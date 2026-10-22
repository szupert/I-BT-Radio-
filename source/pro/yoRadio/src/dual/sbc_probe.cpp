#include "sbc_probe.h"
#include "bt_audio.h"
#include "../core/player.h"
#include "../core/options.h"
#include "../core/config.h"
#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <esp_timer.h>
#include <sdkconfig.h>
#include "C:/esp32/SYSTEM/EXPERIMENTS/SBC_WORKER_V1/roundtrip_policy.h"
#include "C:/esp32/SYSTEM/EXPERIMENTS/SBC_WORKER_V1/cpu_policy.h"
#if !CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS || !CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER || !CONFIG_FREERTOS_RUN_TIME_COUNTER_TYPE_U32
#error SBC CPU probe requires configured scheduler runtime in microseconds/U32
#endif
static_assert(pdMS_TO_TICKS(4)>0,"SBC pacing requires a tick period shorter than 4ms");
extern "C" unsigned ibt_private_ibt_sbc_probe_32k(const int16_t*,uint8_t*,unsigned);
extern "C" void ibt_private_ibt_sbc_probe_release();
extern "C" bool ibt_private_ibt_sbc_probe_allocation(uint8_t*);
extern "C" bool ibt_private_ibt_sbc_decode_begin();
extern "C" bool ibt_private_ibt_sbc_decode_frame(const uint8_t*,unsigned,int16_t*);
extern "C" void ibt_private_ibt_sbc_decode_release();
namespace sbcdiag {
static std::atomic<bool> busy{false},cancelled{false};
bool active(){return busy.load();}
void cancel(){cancelled.store(true);}
static uint8_t crc8(const uint8_t* b,unsigned bits,uint8_t crc=0x0f){
    for(unsigned i=0;i<bits;++i){bool feedback=((crc>>7)^((b[i/8]>>(7-i%8))&1))&1;
      crc=uint8_t(crc<<1);if(feedback)crc^=0x1d;}
    return crc;
}
static uint32_t runtime(TaskHandle_t handle){
    TaskStatus_t info{};vTaskGetInfo(handle,&info,pdFALSE,eInvalid);return uint32_t(info.ulRunTimeCounter);
}
static void cpuProbe(unsigned stage=1,bool live=false){
    const auto route=btaudio::mode();const auto generation=player.playbackGeneration();
    const char* stageName=stage==0?"BASELINE":stage==1?"ENCODE":"ROUNDTRIP";
    const char* routeName=live?(route==btaudio::Mode::Out?"BT":"DAC"):"QUIET";
    auto release=[](){ibt_private_ibt_sbc_probe_release();ibt_private_ibt_sbc_decode_release();};
    int16_t pcm[256],decoded[256];uint8_t packet[120];
    for(unsigned i=0;i<128;++i)pcm[2*i]=pcm[2*i+1]=(i%32)<16?4096:-4096;
    // Warm-up/allocation is outside the timed steady-state phase.
    if((stage>0&&ibt_private_ibt_sbc_probe_32k(pcm,packet+1,118)!=118)||(stage==2&&!ibt_private_ibt_sbc_decode_begin())){
        release();Serial.printf("[SBC-CPU] result=FAIL stage=%s route=%s reason=warmup\n",stageName,routeName);return;
    }
    vTaskDelay(1); // Flush the preceding running interval into the task counter.
    uint32_t cpuBefore=runtime(nullptr),idleBefore[2]={runtime(xTaskGetIdleTaskHandleForCPU(0)),runtime(xTaskGetIdleTaskHandleForCPU(1))};
    uint32_t timerBefore=uint32_t(esp_timer_get_time());const int64_t start=esp_timer_get_time();
    TickType_t wake=xTaskGetTickCount();unsigned valid=0,misses=0;uint64_t maxLate=0,encodeWall=0;
    for(unsigned frame=0;frame<sbccpu::frames;++frame){
        if(cancelled.load()||(!live&&(player.isRunning()||btaudio::wanted()||!btaudio::idle()))||(live&&(!player.isRunning()||config.getMode()!=PM_WEB||player.playbackGeneration()!=generation||btaudio::mode()!=route||(route==btaudio::Mode::Out&&!btaudio::transmitting())))){
            release();Serial.printf("[SBC-CPU] result=FAIL stage=%s route=%s reason=cancel_or_route_changed\n",stageName,routeName);return;
        }
        packet[0]=0xa5;packet[119]=0x5a;
        const auto before=esp_timer_get_time();unsigned size=stage>0?ibt_private_ibt_sbc_probe_32k(pcm,packet+1,118):0;
        encodeWall+=uint64_t(esp_timer_get_time()-before);
        const auto b=packet+1;
        if(stage==0)++valid;
        else if(size==118){uint8_t crc=crc8(b+1,16);crc=crc8(b+4,64,crc);
            bool good=b[0]==0x9c&&b[1]==0x79&&b[2]==53&&b[3]==crc&&packet[0]==0xa5&&packet[119]==0x5a;
            if(good&&(stage!=2||ibt_private_ibt_sbc_decode_frame(b,size,decoded)))++valid;
        }
        int64_t late=esp_timer_get_time()-(start+int64_t(frame+1)*sbccpu::frame_us);
        if(late>0){++misses;maxLate=std::max(maxLate,uint64_t(late));}
        // Bound busy catch-up; diagnostic must still let other tasks run.
        if(xTaskDelayUntil(&wake,pdMS_TO_TICKS(4))==pdFALSE)vTaskDelay(1);
    }
    vTaskDelay(1); // Account for the final encode interval before reading it.
    const uint64_t wall=uint64_t(esp_timer_get_time()-start);
    const uint32_t cpu=runtime(nullptr)-cpuBefore,timerAfter=uint32_t(esp_timer_get_time());
    const bool accounted=sbccpu::runtime_valid(timerBefore,timerAfter,cpu,wall);
    int busy[2]={-1,-1};
    if(accounted)for(unsigned c=0;c<2;++c){uint32_t idle=runtime(xTaskGetIdleTaskHandleForCPU(c))-idleBefore[c];busy[c]=1000-int(std::min(uint32_t(1000),sbccpu::permille(idle,wall)));}
    const unsigned stackMin=unsigned(uxTaskGetStackHighWaterMark(nullptr));
    release();
    Serial.printf("[SBC-CPU] result=%s frames=%u valid=%u hz=32000 audio_us=10000000 wall_us=%llu task_runtime_us=%lu task_permille=%lu core0_permille=%d core1_permille=%d deadline_misses=%u max_late_us=%llu encode_wall_us=%llu stack_min_bytes=%u scope=DIAG_SHADOW stage=%s route=%s coded_frames=%u decoded_frames=%u runtime=SCHEDULER_US bt_send=NOT_RUN\n",accounted&&valid==sbccpu::frames?"PASS":"FAIL",unsigned(sbccpu::frames),valid,(unsigned long long)wall,(unsigned long)cpu,(unsigned long)sbccpu::permille(cpu,wall),busy[0],busy[1],misses,(unsigned long long)maxLate,(unsigned long long)encodeWall,stackMin,stageName,routeName,stage?valid:0,stage==2?valid:0);
}
static void worker(void*){
    int16_t pcm[256],decoded[256]; uint8_t guarded[120]; unsigned good=0,decodedGood=0; uint32_t encodeWall=0;
    unsigned silencePeak=0,signalPeak=0,channelError=0,periodError=0,signalSamples=0;
    uint64_t absoluteSum=0; uint32_t decodeWall=0;
    unsigned silenceR=0,peakR=0,periodR=0;uint64_t absoluteR=0;
    bool decoderReady=ibt_private_ibt_sbc_decode_begin(),crcRejected=false;
    for(unsigned frame=0;frame<250;++frame){
        for(unsigned i=0;i<128;++i)pcm[2*i]=pcm[2*i+1]=frame<125?0:((i%32)<16?4096:-4096);
        guarded[0]=0xa5;guarded[119]=0x5a;
        auto before=micros();unsigned size=ibt_private_ibt_sbc_probe_32k(pcm,guarded+1,118);
        encodeWall+=uint32_t(micros()-before);
        auto b=guarded+1;
        uint8_t crc=0;if(size==118){crc=crc8(b+1,16);crc=crc8(b+4,64,crc);}
        bool valid=size==118&&b[0]==0x9c&&b[1]==0x79&&b[2]==53&&b[3]==crc&&guarded[0]==0xa5&&guarded[119]==0x5a;
        if(valid)++good;
        if(frame==0&&valid&&decoderReady){
            b[3]^=1;crcRejected=!ibt_private_ibt_sbc_decode_frame(b,size,decoded);b[3]^=1;
            // Failed decoder calls may mutate state: start a clean valid stream.
            decoderReady=ibt_private_ibt_sbc_decode_begin();
        }
        before=micros();bool decodedOk=valid&&decoderReady&&ibt_private_ibt_sbc_decode_frame(b,size,decoded);
        decodeWall+=uint32_t(micros()-before);
        if(decodedOk){
            ++decodedGood;
            for(unsigned i=0;i<128;++i){
                unsigned amplitude=roundtrip::magnitude(decoded[2*i]);
                if(frame<125){silencePeak=std::max(silencePeak,amplitude);silenceR=std::max(silenceR,unsigned(roundtrip::magnitude(decoded[2*i+1])));}
                // Ignore eight signal frames for encoder/decoder filter settling.
                if(frame>=133){signalPeak=std::max(signalPeak,amplitude);absoluteSum+=amplitude;++signalSamples;
                    unsigned right=roundtrip::magnitude(decoded[2*i+1]);peakR=std::max(peakR,right);absoluteR+=right;
                    channelError=std::max(channelError,unsigned(roundtrip::magnitude(int(decoded[2*i])-decoded[2*i+1])));
                    if(i>=32){periodError=std::max(periodError,unsigned(roundtrip::magnitude(int(decoded[2*i])-decoded[2*(i-32)])));periodR=std::max(periodR,unsigned(roundtrip::magnitude(int(decoded[2*i+1])-decoded[2*(i-32)+1])));}
                }
            }
        }
        vTaskDelay(1);
    }
    uint8_t bits[16];bool allocation=ibt_private_ibt_sbc_probe_allocation(bits);
    if(allocation){Serial.print("[SBC-ALLOC] bitpool=53 left=");for(unsigned i=0;i<8;++i)Serial.printf("%s%u",i?",":"",bits[i]);Serial.print(" right=");for(unsigned i=8;i<16;++i)Serial.printf("%s%u",i>8?",":"",bits[i]);Serial.println();}
    ibt_private_ibt_sbc_probe_release();
    ibt_private_ibt_sbc_decode_release();
    bool signal=roundtrip::stereo_ok(silencePeak,signalPeak,absoluteSum,silenceR,peakR,absoluteR,signalSamples,periodError,periodR);
    Serial.printf("[SBC-PCM] decoded=%u silence_peak=%u signal_peak=%u abs_sum=%llu samples=%u channel_error=%u period_error=%u silence_r=%u peak_r=%u abs_r=%llu period_r=%u allocation=%u crc_rejected=%u smoke=%s decode_wall_us=%lu reference_pcm=NOT_RUN\n",decodedGood,silencePeak,signalPeak,(unsigned long long)absoluteSum,signalSamples,channelError,periodError,silenceR,peakR,(unsigned long long)absoluteR,periodR,unsigned(allocation),unsigned(crcRejected),signal?"PASS":"FAIL",(unsigned long)decodeWall);
    Serial.printf("[SBC-PROBE] result=%s frames=250 valid=%u hz=32000 frame_bytes=118 encode_wall_us=%lu cpu_time=NOT_MEASURED decoded_pcm=SMOKE_ONLY bt_send=NOT_RUN\n",good==250&&decodedGood==250&&signal&&crcRejected&&allocation?"PASS":"FAIL",good,(unsigned long)encodeWall);
    if(good==250&&decodedGood==250&&signal&&crcRejected&&allocation){for(unsigned stage=0;stage<3&&!cancelled.load();++stage)cpuProbe(stage,false);}
    else Serial.println("[SBC-CPU] result=NOT_RUN reason=smoke_failed");
    busy.store(false);vTaskDelete(nullptr);
}
static void phaseWorker(void* arg){cpuProbe(unsigned(reinterpret_cast<uintptr_t>(arg)),true);busy.store(false);vTaskDelete(nullptr);}
bool startPhase(unsigned stage){
    if(stage>2||(btaudio::mode()==btaudio::Mode::Off&&(btaudio::wanted()||!btaudio::idle()))||config.getMode()!=PM_WEB||!player.isRunning()||btaudio::inputSelected()||(btaudio::outputSelected()&&!btaudio::transmitting()))return false;
    bool expected=false;if(!busy.compare_exchange_strong(expected,true))return false;
    cancelled.store(false);
    if(xTaskCreatePinnedToCore(phaseWorker,"sbc_phase",6144,reinterpret_cast<void*>(uintptr_t(stage)),1,nullptr,0)!=pdPASS){busy.store(false);return false;}
    return true;
}
void start(){
    if(btaudio::wanted()||!btaudio::idle()||player.isRunning()){Serial.println("[SBC-PROBE] start=DENIED stop_audio_and_bt_first");return;}
    bool expected=false;if(!busy.compare_exchange_strong(expected,true)){Serial.println("[SBC-PROBE] start=DENIED busy");return;}
    cancelled.store(false);
    if(xTaskCreatePinnedToCore(worker,"sbc_probe",6144,nullptr,1,nullptr,0)!=pdPASS){busy.store(false);Serial.println("[SBC-PROBE] start=FAIL allocation");return;}
    Serial.println("[SBC-PROBE] start=QUEUED core=0 private_codec=1 synthetic_pcm=1");
}
}
