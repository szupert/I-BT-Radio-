#include "mono_dsp.h"
#include "audio_path_probe.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/queue.h>
#include "deferred_audio.h"
namespace pathprobe {
std::atomic<bool> enabled{false};Counters counters[Count];
std::atomic<uint32_t> lowInput{0},decodedFrames{0};
static QueueHandle_t queue=nullptr;
static std::atomic<uint32_t> lost{0};
void begin(){
 if(queue)return;
 auto storage=(uint8_t*)heap_caps_malloc(128*sizeof(Record),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 auto control=(StaticQueue_t*)heap_caps_malloc(sizeof(StaticQueue_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
 if(storage&&control)queue=xQueueCreateStatic(128,sizeof(Record),storage,control);
 if(!queue){heap_caps_free(storage);heap_caps_free(control);}
}
static void aggregate(const Record& r){if(unsigned(r.stage)>=Count)return;auto& c=counters[r.stage];
 c.calls.fetch_add(1);c.us.fetch_add(r.elapsed);c.units.fetch_add(r.units);c.empty.fetch_add(r.empty);c.errors.fetch_add(r.errors);
 if(r.elapsed>c.maxUs.load())c.maxUs.store(r.elapsed);
}
void publish(const Record& r){if(xPortGetCoreID()==0){aggregate(r);return;}if(!queue||xQueueSend(queue,&r,0)!=pdTRUE)lost.fetch_add(1);}
void service(){if(xPortGetCoreID()!=0||!queue)return;Record r;unsigned budget=128;while(budget--&&xQueueReceive(queue,&r,0)==pdTRUE)aggregate(r);}
void diagnostics(){
    service();audiod::diagnostics();monodsp::diagnostics();
    Serial.printf("[PATH-DEFER] core=0 lost=%lu pending=%u complete=%u\n",(unsigned long)lost.load(),unsigned(queue?uxQueueMessagesWaiting(queue):0),unsigned(queue&&lost.load()==0));
    const char* names[]={"net_poll","net_read","decode","output_handoff","pcm_worker"};
    Serial.printf("[PATH-TOTAL] enabled=%u low_input=%lu decoded_frames=%lu\n",unsigned(enabled.load()),(unsigned long)lowInput.load(),(unsigned long)decodedFrames.load());
    for(unsigned i=0;i<Count;++i){const auto& c=counters[i];
        Serial.printf("[PATH-STAGE] name=%s calls=%lu wall_us=%lu max_wall_us=%lu units=%lu empty=%lu errors=%lu\n",names[i],(unsigned long)c.calls.load(),(unsigned long)c.us.load(),(unsigned long)c.maxUs.load(),(unsigned long)c.units.load(),(unsigned long)c.empty.load(),(unsigned long)c.errors.load());
    }
}
}
