#include "mp3_offload.h"
#include "audio_path_probe.h"
#include "timing_bucket.h"
#include "bt_audio.h"
#include "../core/options.h"
#include "../core/config.h"
#include "../audioI2S/mp3_decoder/mp3_decoder.h"
#include "../core/player.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include <new>
namespace mp3offload {
// R49 candidate: bounded decoder job outranks the Core0 PCM worker (3).
// BT stack/network priorities and all core affinities stay unchanged.
static constexpr unsigned DefaultWorkerPriority=4;
enum class Work {Dequant, RightImdct};
struct Request {int granule;bool measure;Work work=Work::Dequant;uint32_t sentAt=0;};
struct Reply {int result;uint32_t workUs;uint32_t startOffset=0,endOffset=0;};
struct Store {
 timingdiag::Bucket inlineTime,workerTime,roundtripTime,dispatchTime,yieldTime;
 timingdiag::Bucket joinDispatch,joinQueue,joinWorker,joinLeft,joinWait,joinWake,joinTotal;
 QueueHandle_t requests=nullptr,replies=nullptr;
 TaskHandle_t task=nullptr;
 std::atomic<bool> enabled{false},ready{false};
 std::atomic<uint32_t> jobs{0},inlineJobs{0},imdctJobs{0},imdctFallback{0};
 std::atomic<unsigned> fraction{8}; // Store already lives in PSRAM; no new static DRAM
 std::atomic<int> actualCore{-1};
 unsigned sequence=0; // audio owner only
};
static std::atomic<Store*> published{nullptr};
static std::atomic<bool> diagnosticOverride{false};
static std::atomic<bool> imdctEnabled{false};
// R46: reset once per session transition, not on every audio-loop iteration.
// R51: R50 quiet ABA measured 14.6% more PCM with inline IMDCT.
// Normal playback and session exit both use inline; split stays diagnostic-only.
void diagnosticMode(bool on){
 diagnosticOverride.store(false);
 imdctEnabled.store(false);
 auto* s=published.load();
 if(s){s->enabled.store(false);s->fraction.store(8);if(s->task)vTaskPrioritySet(s->task,DefaultWorkerPriority);}
 diagnosticOverride.store(on);
}

// Single audio-owner producer. A synchronous reply is the buffer lifetime fence.
// Never timeout and then decode/free inline while an accepted job still owns state.
static void worker(void* arg) {
    auto* s=static_cast<Store*>(arg);
    s->actualCore.store(xPortGetCoreID());
    for(;;){Request request{};if(xQueueReceive(s->requests,&request,portMAX_DELAY)!=pdTRUE)continue;
        const uint32_t start=request.measure?uint32_t(esp_timer_get_time()):0;
        Reply reply{request.work==Work::RightImdct?IMDCT(request.granule,1):MP3Dequantize(request.granule),0};
        if(request.measure){
          const uint32_t end=uint32_t(esp_timer_get_time());
          reply.workUs=end-start;reply.startOffset=start-request.sentAt;reply.endOffset=end-request.sentAt;
        }
        if(request.work==Work::RightImdct)s->imdctJobs.fetch_add(1);else s->jobs.fetch_add(1);
        xQueueSend(s->replies,&reply,portMAX_DELAY);
    }
}
bool prepare(){
    if(published.load())return true;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<28672||
       heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<8192)return false;
    void* memory=heap_caps_malloc(sizeof(Store),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!memory)return false;
    auto* s=new(memory)Store();
    s->requests=xQueueCreate(1,sizeof(Request));s->replies=xQueueCreate(1,sizeof(Reply));
    if(!s->requests||!s->replies||xTaskCreatePinnedToCore(worker,"ibt_mp3_dq",4096,s,DefaultWorkerPriority,&s->task,0)!=pdPASS){
        if(s->requests)vQueueDelete(s->requests);if(s->replies)vQueueDelete(s->replies);
        s->~Store();free(s);return false;
    }
    s->ready.store(true);published.store(s);return true;
}
bool set(bool on){if(on&&!diagnosticOverride.load())return false;auto* s=published.load();if(!s)return !on;s->enabled.store(on);return true;}
bool setFraction(unsigned fraction){
    if(fraction!=1&&fraction!=2&&fraction!=4&&fraction!=8)return false;
    auto* s=published.load();if(!s||!s->ready.load())return false;
    s->fraction.store(fraction);return true;
}
bool timingActive(){return diagnosticOverride.load()&&pathprobe::enabled.load()&&published.load()!=nullptr;}
void recordYield(uint32_t elapsed){auto* s=published.load();if(s)s->yieldTime.add(elapsed);}
static int inlineDequant(Store* s,int granule,bool measure){
 const uint32_t start=measure?uint32_t(esp_timer_get_time()):0;
 const int result=MP3Dequantize(granule);
 if(measure)s->inlineTime.add(uint32_t(esp_timer_get_time())-start);return result;
}
int dequantize(int granule){
    // Common path: no PSRAM store access, owner lookup, timer or counter update.
    if(!diagnosticOverride.load())return MP3Dequantize(granule);
    auto* s=published.load();
    if(!s)return MP3Dequantize(granule);
    const bool profiling=pathprobe::enabled.load();
    if(!profiling&&!s->enabled.load())return MP3Dequantize(granule);
    const bool measure=profiling&&player.onAudioTask();
    if(!s->ready.load()||!s->enabled.load()||!player.onAudioTask()||(++s->sequence&(s->fraction.load()-1u))!=0){
        if(measure)s->inlineJobs.fetch_add(1);return inlineDequant(s,granule,measure);
    }
    // Diagnostic ratio 1/8..1/1; default OFF, 1/8. Every accepted granule waits for its reply.
    // No decoder math/PCM mutation, TLS access, task priority or I2S change.
    const uint32_t start=measure?uint32_t(esp_timer_get_time()):0;
    Request request{granule,measure};
    if(xQueueSend(s->requests,&request,0)!=pdTRUE){if(measure)s->inlineJobs.fetch_add(1);return inlineDequant(s,granule,measure);}
    Reply reply{};
    while(xQueueReceive(s->replies,&reply,portMAX_DELAY)!=pdTRUE){}
    if(measure){const uint32_t elapsed=uint32_t(esp_timer_get_time())-start;
      s->roundtripTime.add(elapsed);s->workerTime.add(reply.workUs);
      s->dispatchTime.add(timingdiag::outsideWork(elapsed,reply.workUs));}
    return reply.result;
}
// R47: the two IMDCT channels have separate mutable arrays. The frame owner
// keeps all headers/tables alive and unchanged until the accepted right job replies.
// It computes the left channel concurrently, unlike the old synchronous dequant handoff.
bool setWorkerPriority(unsigned priority){
 if(!diagnosticOverride.load()||(priority!=3&&priority!=4))return false;
 auto* s=published.load();if(!s||!s->ready.load()||!s->task)return false;
 // FreeRTOS priority change is safe across cores; never cancel/requeue an accepted job.
 vTaskPrioritySet(s->task,priority);return true;
}
bool setImdct(bool on){if(!diagnosticOverride.load())return false;imdctEnabled.store(on);return true;}
int imdct(int granule,int channels){
 auto* s=published.load();
 const bool split=channels==2&&imdctEnabled.load()&&s&&s->ready.load()&&
   !s->enabled.load()&&player.onAudioTask()&&config.getMode()==PM_WEB&&btaudio::outputSelected();
 if(split){
  const bool measure=diagnosticOverride.load()&&pathprobe::enabled.load();
  const uint32_t sent=measure?uint32_t(esp_timer_get_time()):0;
  Request request{granule,measure,Work::RightImdct,sent};
  if(xQueueSend(s->requests,&request,0)==pdTRUE){
   const uint32_t dispatched=measure?uint32_t(esp_timer_get_time()):0;
   const int left=IMDCT(granule,0);
   const uint32_t leftDone=measure?uint32_t(esp_timer_get_time()):0;
   Reply reply{};
   // Join even when left failed or STOP arrived. No decoder clear/free/next frame
   // or Subband is allowed while the other channel still owns these arrays.
   while(xQueueReceive(s->replies,&reply,portMAX_DELAY)!=pdTRUE){}
   if(measure){
    const uint32_t done=uint32_t(esp_timer_get_time());
    s->joinDispatch.add(dispatched-sent);s->joinQueue.add(reply.startOffset);
    s->joinWorker.add(reply.workUs);s->joinLeft.add(leftDone-dispatched);
    s->joinWait.add(done-leftDone);s->joinTotal.add(done-sent);
    // Post-worker elapsed includes reply publication and owner scheduling; not pure CPU.
    s->joinWake.add(timingdiag::outsideWork(done-sent,reply.endOffset));
   }
   return left<0?left:reply.result;
  }
  s->imdctFallback.fetch_add(1);
 }
 for(int ch=0;ch<channels;++ch){const int result=IMDCT(granule,ch);if(result<0)return result;}
 return 0;
}
void diagnostics(){auto* s=published.load();
 Serial.printf("[MP3-IMDCT] requested=%u ready=%u worker_core=%d worker_priority=%u right_jobs=%lu fallbacks=%lu scope=WEB_BT_STEREO default=OFF method=PARALLEL_CHANNEL_JOIN\n",unsigned(imdctEnabled.load()),unsigned(s&&s->ready.load()),s?s->actualCore.load():-1,unsigned(s&&s->task?uxTaskPriorityGet(s->task):0),(unsigned long)(s?s->imdctJobs.load():0),(unsigned long)(s?s->imdctFallback.load():0));
if(!s){Serial.println("[MP3-OFFLOAD] ready=0 enabled=0 core=-1 jobs=0 inline_jobs=0 fraction=8 stack_min=0");return;}Serial.printf("[MP3-OFFLOAD] ready=%u enabled=%u core=%d jobs=%lu inline_jobs=%lu fraction=%u stack_min=%u inline_count_scope=PROFILED_ONLY\n",
    unsigned(s->ready.load()),unsigned(s->enabled.load()),s->actualCore.load(),(unsigned long)s->jobs.load(),
    (unsigned long)s->inlineJobs.load(),s->fraction.load(),s->task?unsigned(uxTaskGetStackHighWaterMark(s->task)):0);
 // Only the existing, low-frequency diagnostic request takes a task runtime snapshot.
 // No task enumeration, heap allocation or runtime query in the audio loop.
 TaskStatus_t taskInfo{};if(s->task)vTaskGetInfo(s->task,&taskInfo,pdFALSE,eInvalid);
 Serial.printf("[MP3-JOIN] enabled=%u calls=%lu dispatch_us=%lu queue_us=%lu worker_us=%lu left_us=%lu wait_us=%lu wake_us=%lu total_us=%lu worker_runtime_us=%lu method=WALL_PLUS_TASK_RUNTIME\n",
 unsigned(timingActive()),(unsigned long)s->joinTotal.calls.load(),
 (unsigned long)s->joinDispatch.total.load(),(unsigned long)s->joinQueue.total.load(),
 (unsigned long)s->joinWorker.total.load(),(unsigned long)s->joinLeft.total.load(),
 (unsigned long)s->joinWait.total.load(),(unsigned long)s->joinWake.total.load(),
 (unsigned long)s->joinTotal.total.load(),(unsigned long)taskInfo.ulRunTimeCounter);
 const char* names[]={"inline_dequant","worker_dequant","worker_roundtrip","dispatch_wait","owner_yield"};
 const timingdiag::Bucket* buckets[]={&s->inlineTime,&s->workerTime,&s->roundtripTime,&s->dispatchTime,&s->yieldTime};
 for(unsigned i=0;i<5;++i){const auto& t=*buckets[i];Serial.printf("[MP3-TIMING] enabled=%u name=%s calls=%lu wall_us=%lu max_wall_us=%lu method=ELAPSED_NOT_CPU\n",unsigned(timingActive()),names[i],(unsigned long)t.calls.load(),(unsigned long)t.total.load(),(unsigned long)t.maximum.load());}
}
}
