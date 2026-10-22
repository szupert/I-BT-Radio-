#include "load_probe.h"
#include "load_epoch.h"
#include "bt_audio.h"
#include "../core/player.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <atomic>
#include "../audioI2S/net_prefetch.h"
namespace loadprobe {
static constexpr unsigned capacity=80;
struct Previous { UBaseType_t id; configRUN_TIME_COUNTER_TYPE runtime; };
struct Store { TaskStatus_t tasks[capacity]; Previous previous[capacity]; uint32_t delta[capacity]; unsigned previousCount;int64_t previousTime;int cachedBusy[2];uint32_t cachedAt;Epoch epoch; uint64_t window;int64_t endedAt;uint32_t sequence;uint32_t snapshotUs; };
static Store* store=nullptr;

// No ISR/audio caller. Try-lock prevents a concurrent diagnostic request from
// corrupting the UI baseline; never hold a scheduler/interrupt lock for I/O.
static std::atomic_flag collecting=ATOMIC_FLAG_INIT;
struct Guard {
 bool acquired=!collecting.test_and_set(std::memory_order_acquire);
 ~Guard(){if(acquired)collecting.clear(std::memory_order_release);}
};
static void invalidate(){
 store->previousTime=0;store->previousCount=0;store->cachedAt=0;store->window=0;
 store->cachedBusy[0]=store->cachedBusy[1]=-1;
}
static void reportWindow(bool replay){
 const auto age=esp_timer_get_time()-store->endedAt;
 Serial.printf("[LOAD] result=OK window_us=%llu core0_permille=%d core1_permille=%d tasks=%u snapshot_us=%u probe_psram=%u window_id=%lu age_us=%llu end_us=%llu cached=%u generation=%lu route_state=%u\n",
   (unsigned long long)store->window,store->cachedBusy[0],store->cachedBusy[1],store->previousCount,
   unsigned(store->snapshotUs),unsigned(sizeof(Store)),(unsigned long)store->sequence,
   (unsigned long long)age,(unsigned long long)store->endedAt,unsigned(replay),
   (unsigned long)store->epoch.generation,store->epoch.state);
 const auto audioHandle=player.audioTaskHandle();
 for(unsigned i=0;i<store->previousCount;++i){const auto& t=store->tasks[i];
   if(audioHandle&&t.xHandle==audioHandle)Serial.printf("[LOAD-AUDIO] id=%u core=%d priority=%u cpu_permille=%u stack_min_bytes=%u\n",unsigned(t.xTaskNumber),int(t.xCoreID),unsigned(t.uxCurrentPriority),unsigned(uint64_t(store->delta[i])*1000/store->window),unsigned(t.usStackHighWaterMark));
   if(t.xTaskNumber==btaudio::pcmTaskId())Serial.printf("[LOAD-PCM] id=%u core=%d cpu_permille=%u\n",unsigned(t.xTaskNumber),int(t.xCoreID),unsigned(uint64_t(store->delta[i])*1000/store->window));
 }
 // Do not mutate delta[]: another read must describe the same exact window.
 // Never dereference stale task names/handles after the system snapshot.
 bool used[capacity]{};
 for(unsigned rank=0;rank<6&&rank<store->previousCount;++rank){
   unsigned best=capacity;
   for(unsigned i=0;i<store->previousCount;++i)if(!used[i]&&(best==capacity||store->delta[i]>store->delta[best]))best=i;
   used[best]=true;const auto& t=store->tasks[best];
   Serial.printf("[LOAD-TASK] id=%u core=%d priority=%u cpu_permille=%u stack_min_bytes=%u\n",unsigned(t.xTaskNumber),int(t.xCoreID),unsigned(t.uxCurrentPriority),unsigned(uint64_t(store->delta[best])*1000/store->window),unsigned(t.usStackHighWaterMark));
 }
}
static void reportMemory(int64_t start){
 Serial.printf("[LOAD-MEM] internal_free=%u internal_largest=%u internal_min=%u psram_free=%u psram_largest=%u psram_min=%u probe_us=%llu\n",
   unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
   unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)),unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)),(unsigned long long)(esp_timer_get_time()-start));
 netprefetch::diagnostics();
}
static void collect(bool verbose){
    Guard guard;if(!guard.acquired){if(verbose)Serial.println("[LOAD] result=BUSY");return;}

    const auto start=esp_timer_get_time();
    if(!store)store=static_cast<Store*>(heap_caps_calloc(1,sizeof(Store),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!store){if(verbose)Serial.println("[LOAD] result=NO_PSRAM");return;}
    const auto generation=player.playbackGeneration();
    const unsigned state=unsigned(btaudio::mode())|(btaudio::transmitting()?0x100u:0u);
    // Start a new baseline on route/source/transmission changes. A long startup
    // window must not be presented as steady playback CPU consumption.
    if(store->epoch.update(generation,state)||(store->previousTime&&start-store->previousTime>30000000)) {
        invalidate();
    }
    if(store->previousTime&&start-store->previousTime<9000000){
        if(verbose){if(store->window){reportWindow(true);reportMemory(start);}else Serial.println("[LOAD] result=TOO_SOON");}
        return;
    }
    configRUN_TIME_COUNTER_TYPE total=0;
    const unsigned count=uxTaskGetSystemState(store->tasks,capacity,&total);
    const auto stamp=esp_timer_get_time();
    if(!count){invalidate();if(verbose)Serial.println("[LOAD] result=TASK_CAPACITY");return;}
    const uint64_t elapsed=store->previousTime?uint64_t(stamp-store->previousTime):0;
    const bool unchanged=generation==player.playbackGeneration()&&state==(unsigned(btaudio::mode())|(btaudio::transmitting()?0x100u:0u));
    const uint64_t window=unchanged&&validWindow(elapsed)?elapsed:0;
    int busy[2]={-1,-1};
    for(unsigned i=0;i<count;i++){
        const auto& task=store->tasks[i];store->delta[i]=0;
        bool known=false;
        for(unsigned j=0;j<store->previousCount;j++)if(store->previous[j].id==task.xTaskNumber){
            store->delta[i]=uint32_t(task.ulRunTimeCounter-store->previous[j].runtime);known=true;break;
        }
        for(unsigned core=0;core<2;core++)if(known&&window&&task.xHandle==xTaskGetIdleTaskHandleForCPU(core))
            busy[core]=1000-int(std::min(uint64_t(1000),uint64_t(store->delta[i])*1000/window));
    }

    store->window=window&&busy[0]>=0&&busy[1]>=0?window:0;
    store->cachedBusy[0]=busy[0];store->cachedBusy[1]=busy[1];store->cachedAt=millis();
    store->endedAt=stamp;store->snapshotUs=uint32_t(stamp-start);
    if(store->window)++store->sequence;
    for(unsigned i=0;i<count;i++)store->previous[i]={store->tasks[i].xTaskNumber,store->tasks[i].ulRunTimeCounter};
    store->previousCount=count;store->previousTime=stamp;
    if(!unchanged){invalidate();if(verbose)Serial.println("[LOAD] result=EPOCH_CHANGED");return;}
    if(verbose){
        if(store->window)reportWindow(false);else Serial.println("[LOAD] result=BASELINE");
        reportMemory(start);
    }
}
void sample(){collect(true);}
void tick(){collect(false);}
bool cached(int &c0,int &c1,bool *samplerBusy){
    c0=c1=-1;Guard guard;if(samplerBusy)*samplerBusy=!guard.acquired;
    if(!guard.acquired||!store||!store->window||!store->cachedAt||
       uint32_t(millis()-store->cachedAt)>20000||
       store->epoch.generation!=player.playbackGeneration()||
       store->epoch.state!=(unsigned(btaudio::mode())|(btaudio::transmitting()?0x100u:0u)))return false;
    c0=store->cachedBusy[0];c1=store->cachedBusy[1];return c0>=0&&c1>=0;
}
}
