#include "lite_memory.h"
#include "pcm_gain_cache.h"
#include "../core/options.h"
#include "bt_audio.h"
#include "ram_reserve.h"
#include "surface_policy.h"
#include "bt_rate_policy.h"
#include "../core/player.h"
#include "../core/config.h"
#include "../core/network.h"
#include "bt_start_policy.h"
#include "bt_discovery_stats.h"
#include "resource_limiter.h"
#include "media_start_gate.h"
#include "bt_media.h"
#include "bt_input_vu.h"
#include "pcm_ring.h"
#include "payload_gate.h"
#include "pcm_work_queue.h"
#include "pcm_space_signal.h"
#include "audio_path_probe.h"
#include "bt_signal.h"
#include "pcm_resampler.h"
#include "../audioI2S/start_fade.h"
#include <Arduino.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <esp_a2dp_legacy_api.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <algorithm>
#include <new>
#include <Preferences.h>
#include <sdkconfig.h>
// Direct IDF A2DP calls bypass Arduino library reservation constructors.
// Reserve Classic controller memory before initArduino; runtime deinit stays reversible.
#include <esp32-hal-alloc-bt-classic-mem.h>
#include <esp32-hal-bt.h>
#if CONFIG_BT_BLE_ENABLED && defined(IBT_SENSOR_EXPERIMENTAL)
// Direct IDF GAP calls do not pull in Arduino BLE's reservation constructor.
// Keep controller BLE EM out of the heap before initArduino() runs.
#include <esp32-hal-alloc-ble-mem.h>
#include <esp32-hal-bt.h>
#endif
extern "C" uint32_t ibt_l2cap_guard_count(void);
#include <strings.h>
namespace btaudio {
static std::atomic<int> txCallbackCore{-1};
static std::atomic<unsigned> txCallbackTask{0};
static Device devices[12]{};static unsigned devicesUsed=0;
static btdiscovery::Stats discoveryStats;
static std::atomic<bool> discoverySummary{false};
static portMUX_TYPE devicesMux=portMUX_INITIALIZER_UNLOCKED;
static std::atomic<bool> controlReady{false},controlGone{false},controlConnected{false};
static std::atomic<int> pendingKey{-1},controlResult{0};
static std::atomic<bool> keyHeld{false};static int heldKey=-1;static uint32_t releaseAt=0;static uint8_t transaction=0;
enum class Stage : uint8_t { Off, Init, Active, Disconnect, Deinit, Fault, AvrcInit, AvrcDeinit };
static std::atomic<Mode> desired{Mode::Off},active{Mode::Off};
static std::atomic<Stage> stage{Stage::Off};
static std::atomic<bool> streaming{false},ready{false},deinitialized{false},failed{false};
static ibtaudio::MediaStartGate mediaGate; // service() owner only
static std::atomic<int> mediaCheckAck{0};
static std::atomic<bool> mediaStartFailed{false};
static std::atomic<bool> found{false},discovering{false};
static std::atomic<bool> connectionRejected{false};
static std::atomic<bool> autoDacFallback{false};
static std::atomic<bool> retryRequested{false};
static std::atomic<bool> restartScanRequested{false},reconnectRequested{false},savePeerPending{false};
static std::atomic<bool> savedValid{false};
static std::atomic<int> connection{ESP_A2D_CONNECTION_STATE_DISCONNECTED};
// Upper 24 bits: last 10-second polling bucket, low byte: signed RSSI delta.
// One word keeps the diagnostic out of the saturated internal DRAM section.
static std::atomic<uint32_t> linkSignal{0xffffff80u};
static std::atomic<uint32_t> requestId{0},epoch{0},delivered{0},received{0},dropped{0},underruns{0},rxRate{44100};
static std::atomic<uint32_t> blockFrames{0},txRate{0},sourceRate{0},requestedRate{32000};
static std::atomic<bool> txCodecConfigured{false},codecUnsupported{false};
static std::atomic<bool> mono{false};
// BT OUT keeps enough decoded audio in PSRAM to absorb radio/decode jitter.
static portMUX_TYPE txMux=portMUX_INITIALIZER_UNLOCKED;
static int16_t* txPcm=nullptr;
#if IBT_NO_PSRAM
static constexpr unsigned txFrames=16384,internalReserve=ibtram::reserve;
#else
static constexpr unsigned txFrames=32768,internalReserve=ibtram::reserve;
#endif
// Bound the PSRAM copy in one spinlock, while reducing callback lock churn.
static constexpr unsigned txReadBatchFrames=256;
static unsigned txHead=0,txTail=0,txUsed=0;
static std::atomic<uint32_t> requestedBytes{0},prefillCallbacks{0},rebufferEvents{0};
static std::atomic<uint32_t> boundaryUnderruns{0},steadyUnderruns{0},boundaryAt{0},sourceGeneration{0},sourceCodec{0},nativeUnityFrames{0};
static std::atomic<bool> sourcePlaying{false};
void markSourceBoundary(){sourcePlaying.store(false);boundaryAt.store(millis());}
void sourceContext(uint32_t generation,unsigned codec,bool playing){
    if(sourceGeneration.load()!=generation){sourceGeneration.store(generation);boundaryAt.store(millis());}
    if(sourceCodec.load()!=codec)sourceCodec.store(codec);
    if(sourcePlaying.load()!=playing)sourcePlaying.store(playing);
}

// Callback telemetry only: no printing, waiting, allocation or new lock in BTU.
static std::atomic<uint32_t> txCallbacks{0},txCallbackMaxUs{0},txCallbackGapMaxUs{0};
static std::atomic<uint32_t> txPrefillBytes{0},txShortageBytes{0};
struct TxCallbackMeasure {
    uint32_t start;
    TxCallbackMeasure():start(micros()){
        static uint32_t last=0; // Single A2DP consumer owns this timestamp.
        const uint32_t gap=start-last;
        if(last&&gap>txCallbackGapMaxUs.load(std::memory_order_relaxed))txCallbackGapMaxUs.store(gap,std::memory_order_relaxed);
        last=start;txCallbacks.fetch_add(1,std::memory_order_relaxed);
    }
    ~TxCallbackMeasure(){const uint32_t elapsed=micros()-start;
        if(elapsed>txCallbackMaxUs.load(std::memory_order_relaxed))txCallbackMaxUs.store(elapsed,std::memory_order_relaxed);}
};
static std::atomic<PcmRing*> rxRing{nullptr};
#if IBT_NO_PSRAM
static PayloadGate litePayloadGate;
struct PayloadUse : PayloadGate::Use {PayloadUse():Use(litePayloadGate){}};
#else
struct PayloadUse {explicit operator bool() const{return true;}};
#endif
static std::atomic<uint32_t> pcmGeneration{0};
static uint32_t consumedGeneration=~0u,consumedEpoch=~0u,legacyRate=0,legacyOutputRate=0,legacyPhase=0;
static int16_t legacyPrevious[2]{};
static esp_bd_addr_t peer{},selectedPeer{};
static esp_bd_addr_t savedPeer{},requestedSavedPeer{};
static portMUX_TYPE peerMux=portMUX_INITIALIZER_UNLOCKED;
static constexpr uint32_t savedHistoryMagic=0x49425431;
static constexpr unsigned savedHistoryCapacity=4;
struct SavedHistory {uint32_t magic;uint8_t count;uint8_t reserved[3];Device entries[savedHistoryCapacity];};
static SavedHistory savedHistory{};
static bool validAddress(const uint8_t* address){
    uint8_t any=0,all=0xff;
    for(unsigned i=0;i<6;++i){any|=address[i];all&=address[i];}
    return any!=0&&all!=0xff;
}
static bool placeholderName(const char* name){
    return !name[0]||!strcmp(name,"Névtelen eszköz")||
        (strlen(name)==20&&!strncmp(name,"BT ",3)&&name[5]==':'&&name[8]==':'&&name[11]==':'&&name[14]==':'&&name[17]==':');
}
static void fallbackName(Device& d){
    if(placeholderName(d.name))strlcpy(d.name,"Névtelen eszköz",sizeof(d.name));
}
static uint32_t networkPrepareAt=0;
static bool initialized=false,attempt=false,connectSent=false,disconnectSent=false,advertised=false;
static bool heapConnectedChecked=false; // BT lifecycle owner only, diagnostic release
static bool routeFallbackLogged=false;
static std::atomic<bool> cancelSent{false};
static std::atomic<uint32_t> selectedAt{0},scanStoppedAt{0},connectedAt{0},streamAt{0};
static bool scanTimingLogged=false,connectTimingLogged=false,streamTimingLogged=false;
static uint32_t appliedRequest=0,stageAt=0,linkAt=0,lastCheck=0,retryAt=0;
static uint8_t connectAttempts=0;
static PcmResampler resampler;
static int16_t block[512];static unsigned frames=0; // PCM owner only
// One decoder producer, one core0 PCM consumer. Allocations live until reboot.
static std::atomic<bool> pcmWorkerReady{false};
struct PcmPipeline : PcmWorkQueue {
    PcmSpaceSignal txSpace,queueSpace;
    // Queue-local diagnostics share its existing internal-heap lifetime.
    std::atomic<uint32_t> taskId{0},queueWaits{0},queuePeak{0},workWallMaxUs{0};
    explicit PcmPipeline(Job* slots):PcmWorkQueue(slots){}
};
static PcmPipeline* pcmJobs=nullptr;
static TaskHandle_t pcmWorkerHandle=nullptr;
static std::atomic<uint32_t> pcmQueuedFrames{0},pcmProcessedFrames{0},pcmCancelledFrames{0};
static std::atomic<uint32_t> pcmQueueLostFrames{0};
static uint32_t processGeneration=0,processEpoch=0; // PCM owner only
static bool pcmWorkerAttempted=false; // service core only
static uint32_t fallbackGeneration=0,fallbackEpoch=0,fallbackRate=0;
static unsigned fallbackCount=0;
static int16_t fallbackPcm[128]; // decoder owner only, already gained samples
static bool allocatePcmWorker();
static void wakePcmProducers(){
    if(pcmJobs&&pcmWorkerReady.load()){
        pcmJobs->txSpace.notify();pcmJobs->queueSpace.notify();
    }
}
static void clearTx(){portENTER_CRITICAL(&txMux);txHead=txTail=txUsed=0;portEXIT_CRITICAL(&txMux);wakePcmProducers();}
void flush(){
    markSourceBoundary();
    portENTER_CRITICAL(&txMux);
    txHead=txTail=txUsed=0;pcmGeneration.fetch_add(1);epoch.fetch_add(1);
    portEXIT_CRITICAL(&txMux);
    wakePcmProducers();
}
void request(Mode next){
    if(next!=Mode::Off&&!surfacepolicy::allows(surfacepolicy::current(),next==Mode::In?surfacepolicy::Route::BtInDac:surfacepolicy::Route::SdBt))return;
    if(next==Mode::Out&&config.getMode()==PM_WEB){
        Serial.println("[BT-ROUTE] result=DENIED source=WEB target=BT_OUT keep=DAC");
        return;
    }
    flush();desired.store(next);requestId.fetch_add(1);
}
void request(bool enabled){request(enabled?Mode::Out:Mode::Off);}
Mode mode(){return desired.load();}
bool wanted(){return mode()!=Mode::Off;}
bool outputSelected(){return mode()==Mode::Out;}
bool inputSelected(){return mode()==Mode::In;}
bool idle(){return stage.load()==Stage::Off;}
unsigned pcmTaskId(){return (pcmJobs&&pcmWorkerReady.load())?pcmJobs->taskId.load():0;}
bool setDiagnosticRate(unsigned hz){
    if(IBT_NO_PSRAM&&hz!=32000)return false;
    if(!idle()||wanted()||(hz!=16000&&hz!=32000&&hz!=44100&&hz!=48000))return false;
    requestedRate.store(btrate::effective(config.getMode()==PM_WEB,hz));return true;
}
unsigned negotiatedOutputRate(){return producerReady()?txRate.load():0;}
unsigned outputRate(){return IBT_NO_PSRAM?32000:btrate::effective(config.getMode()==PM_WEB,requestedRate.load());}
bool setOutputRate(unsigned hz){
    if(IBT_NO_PSRAM&&hz!=32000)return false;
    if(!btrate::selectable(config.getMode()==PM_WEB,hz))return false;
    if(!idle()||wanted())return false;
    if(requestedRate.load()==hz)return true;
    Preferences p;
    if(!p.begin("ibt-bt-out",false))return false;
    const bool saved=p.putUInt("rate",hz)==sizeof(uint32_t);
    p.end();
    if(!saved)return false;
    requestedRate.store(hz);
    flush();
    Serial.printf("[BT-RATE] selected_hz=%u result=SAVED\n",hz);
    return true;
}
static bool flowing(Mode m){return desired.load()==m&&active.load()==m&&stage.load()==Stage::Active&&streaming.load()&&!failed.load()&&(m!=Mode::Out||txCodecConfigured.load());}
bool transmitting(){return flowing(Mode::Out);}
bool uiHeadroomSafe(){
    if(!transmitting())return true; // Pairing/teardown UI must remain available.
    portENTER_CRITICAL(&txMux);
    const unsigned frames=txUsed;
    portEXIT_CRITICAL(&txMux);
    return frames>=std::min<unsigned>(txFrames-1,txRate.load()/2);
}
uint32_t producedFrames(){return blockFrames.load(std::memory_order_relaxed);}
bool connected(){const auto m=desired.load();return m!=Mode::Off&&active.load()==m&&stage.load()==Stage::Active&&connection.load()==ESP_A2D_CONNECTION_STATE_CONNECTED&&!failed.load();}
unsigned signalBars(){
    const int delta=int(int8_t(linkSignal.load(std::memory_order_relaxed)&0xff));
    return connected()?ibtsignal::btBars(delta==-128?-129:delta):0;
}
bool producerReady(){return outputSelected()&&connected()&&(!IBT_NO_PSRAM||txPcm)&&txCodecConfigured.load()&&txRate.load()!=0&&(config.getMode()!=PM_WEB||txRate.load()==32000);}
bool inputPlaying(){return flowing(Mode::In);}
static bool pushTx(const int16_t* samples,unsigned count){
    PayloadUse payload;if(!payload)return false;
    unsigned done=0;const uint32_t started=millis();
    PcmSpaceWait space(pcmJobs&&pcmWorkerReady.load()?&pcmJobs->txSpace:nullptr);
    while(done<count){
        portENTER_CRITICAL(&txMux);
        const bool active=producerReady()&&processGeneration==pcmGeneration.load()&&processEpoch==epoch.load();
        const unsigned n=active?std::min({count-done,txFrames-txUsed,txFrames-txHead}):0;
        if(n){memcpy(txPcm+txHead*2,samples+done*2,n*4);txHead=(txHead+n)%txFrames;txUsed+=n;}
        portEXIT_CRITICAL(&txMux);
        if(!active)return false;
        done+=n;
        if(n)space.progress();
        if(!n){
            const uint32_t elapsed=millis()-started;
            if(elapsed>=100){dropped.fetch_add((count-done)*4);return false;}
            if(space.armAndRecheck())continue;
            space.wait(100-elapsed);
        }
    }
    return true;
}
static void emit(int16_t l,int16_t r){
    block[frames*2]=l;block[frames*2+1]=r;
    if(++frames<64)return;
    pushTx(block,frames);
    frames=0;
}
static void processSample(int16_t l,int16_t r,uint32_t hz){
    if(!producerReady())return;
    const uint32_t generation=processGeneration;
    if(consumedGeneration!=generation||consumedEpoch!=processEpoch){consumedGeneration=generation;consumedEpoch=processEpoch;resampler.reset();frames=0;legacyRate=0;}
    resampler.feed(l,r,hz,txRate.load(),emit);
}
static void processBlock(const int16_t* samples,uint32_t count,unsigned channels,unsigned rate,unsigned volume,int balance){
    if(!samples||!count||!producerReady()||!(channels==1||channels==2)||!(rate==16000||rate==32000||rate==44100||rate==48000))return;
    const uint32_t targetHz=txRate.load();
    if(!targetHz)return;
    sourceRate.store(rate);
    blockFrames.fetch_add(count);
    const uint32_t generation=processGeneration;
    if(consumedGeneration!=generation||consumedEpoch!=processEpoch||legacyRate!=rate||legacyOutputRate!=targetHz){
        consumedGeneration=generation;consumedEpoch=processEpoch;legacyRate=rate;legacyOutputRate=targetHz;legacyPhase=0;frames=0;
        legacyPrevious[0]=samples[0];legacyPrevious[1]=samples[channels==2?1:0];
    }
    // One PCM worker owns this cache; unchanged gain needs no64bit divisions.
    static BlockGainCache gainCache;
    const auto gain=gainCache.get(volume,balance);
    const int32_t leftGainQ15=gain.left,rightGainQ15=gain.right;
    int16_t out[128];unsigned pending=0;
    if(rate==targetHz){
        // Exact unity at native rate: no gain arithmetic or temporary copy.
        // Limit each write to128frames; memory reserve and consumer batches unchanged.
        if(channels==2&&leftGainQ15==32768&&rightGainQ15==32768){
            for(uint32_t done=0;done<count;){const unsigned n=std::min<uint32_t>(128,count-done);if(!pushTx(samples+done*2,n))return;done+=n;}
            nativeUnityFrames.fetch_add(count);legacyPhase=0;
            legacyPrevious[0]=samples[(count-1)*2];legacyPrevious[1]=samples[(count-1)*2+1];return;
        }
        // Native-rate PCM needs gain only.
        for(uint32_t i=0;i<count;++i){
            const int16_t left=samples[i*channels];
            const int16_t right=samples[i*channels+(channels==2?1:0)];
            out[pending*2]=(int32_t(left)*leftGainQ15)/32768;
            out[pending*2+1]=(int32_t(right)*rightGainQ15)/32768;
            if(++pending==64){if(!pushTx(out,pending))return;pending=0;}
        }
        if(pending)pushTx(out,pending);
        legacyPhase=0;
        legacyPrevious[0]=samples[(count-1)*channels];
        legacyPrevious[1]=samples[(count-1)*channels+(channels==2?1:0)];
        return;
    }
    for(uint32_t i=0;i<count;++i){
        const int16_t now[2]={samples[i*channels],samples[i*channels+(channels==2?1:0)]};
        while(legacyPhase<targetHz){
            const int32_t fractionQ15=int32_t((legacyPhase*32768u)/targetHz);
            for(unsigned c=0;c<2;++c){
                // Q15 keeps 16/32/44.1/48 kHz interpolation in signed 32-bit.
                const int32_t value=legacyPrevious[c]+((int32_t(now[c])-legacyPrevious[c])*fractionQ15)/32768;
                out[pending*2+c]=value*(c==0?leftGainQ15:rightGainQ15)/32768;
            }
            legacyPhase+=rate;
            if(++pending==64){if(!pushTx(out,pending))return;pending=0;}
        }
        legacyPhase-=targetHz;legacyPrevious[0]=now[0];legacyPrevious[1]=now[1];
    }
    if(pending)pushTx(out,pending);
}
static void processJob(const PcmWorkQueue::Job& job){
    pathprobe::Scope probe(pathprobe::Worker);probe.units(job.count);
    if(!producerReady()||job.generation!=pcmGeneration.load()||job.epoch!=epoch.load()){
        pcmCancelledFrames.fetch_add(job.count);return;
    }
    processGeneration=job.generation;processEpoch=job.epoch;
    if(job.legacy){
        for(unsigned i=0;i<job.count;++i)processSample(job.pcm[i*2],job.pcm[i*2+1],job.rate);
    }else processBlock(job.pcm,job.count,job.channels,job.rate,job.volume,job.balance);
    pcmProcessedFrames.fetch_add(job.count);
}
// One 1044-byte job on the existing internal task stack, no heap allocation.
// Keep this out of line so the direct control path does not reserve this frame.
static void __attribute__((noinline)) processStagedJob(const PcmWorkQueue::Job& job){
    PcmWorkQueue::Job local;
    memcpy(&local,&job,sizeof(local));
    processJob(local);
}
bool setPcmStaging(bool enabled){
    if(!enabled){if(pcmJobs)pcmJobs->staging.store(false);return true;}
    if(!pcmWorkerReady.load()||!pcmJobs||!pcmWorkerHandle||
       config.getMode()!=PM_WEB||!outputSelected()||!producerReady())return false;
    // R49 physical high-water was 3112 B. Keep >=1536 B after the measured
    // candidate frame; guard is conservative, target frame audited at release.
    const unsigned required=1536+(pcmJobs->stagedJobs.load()==0?sizeof(PcmWorkQueue::Job)+64:0);
    if(uxTaskGetStackHighWaterMark(pcmWorkerHandle)<required)return false;
    pcmJobs->staging.store(true);return true;
}
static void pcmWorkerTask(void*){
    TaskStatus_t info{};vTaskGetInfo(nullptr,&info,pdFALSE,eInvalid);
    pcmJobs->taskId.store(unsigned(info.xTaskNumber));
    for(;;){
        const auto* job=pcmJobs->front();
        if(!job){ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(20));continue;}
        const uint32_t started=micros();
        // Borrowed slot stays immutable until processing completes, in both modes.
        const bool staged=pcmJobs->staging.load();
        if(staged){processStagedJob(*job);pcmJobs->stagedJobs.fetch_add(1);}
        else{processJob(*job);pcmJobs->directJobs.fetch_add(1);}
        pcmJobs->release();
        pcmJobs->queueSpace.notify();
        const uint32_t elapsed=micros()-started;
        if(elapsed>pcmJobs->workWallMaxUs.load())pcmJobs->workWallMaxUs.store(elapsed);
    }
}
static bool allocatePcmWorker(){
#if IBT_NO_PSRAM
    Serial.println("[BT-PCM] init=INLINE_FALLBACK reason=LITE_NO_EXTERNAL_WORK_QUEUE");return false;
#endif
    if(pcmWorkerAttempted)return pcmWorkerReady.load();
    pcmWorkerAttempted=true;
    // Preserve the shared10000B reserve plus room for 4KiB stack and TCB.
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<internalReserve+6144){
        Serial.println("[BT-PCM] init=INLINE_FALLBACK reason=INTERNAL_RESERVE");return false;
    }
    auto slots=(PcmWorkQueue::Job*)heap_caps_malloc(sizeof(PcmWorkQueue::Job)*PcmWorkQueue::Capacity,litememory::payloadCaps);
    if(!slots){Serial.println("[BT-PCM] init=INLINE_FALLBACK reason=PSRAM");return false;}
    void* state=heap_caps_malloc(sizeof(PcmPipeline),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!state){free(slots);Serial.println("[BT-PCM] init=INLINE_FALLBACK reason=QUEUE");return false;}
    pcmJobs=new(state) PcmPipeline(slots);
    if(!pcmJobs->txSpace.valid()||!pcmJobs->queueSpace.valid()){
        pcmJobs->~PcmPipeline();heap_caps_free(pcmJobs);pcmJobs=nullptr;free(slots);
        Serial.println("[BT-PCM] init=INLINE_FALLBACK reason=WAKE");return false;
    }
    if(xTaskCreatePinnedToCore(pcmWorkerTask,"ibt_pcm",4096,nullptr,3,&pcmWorkerHandle,0)!=pdPASS){
        pcmJobs->~PcmPipeline();heap_caps_free(pcmJobs);pcmJobs=nullptr;free(slots);pcmWorkerHandle=nullptr;
        Serial.println("[BT-PCM] init=INLINE_FALLBACK reason=TASK");return false;
    }
    pcmWorkerReady.store(true);
    Serial.printf("[BT-PCM] init=PASS core=0 priority=3 psram_bytes=%u stack=4096\n",unsigned(sizeof(PcmWorkQueue::Job)*PcmWorkQueue::Capacity));
    return true;
}
static bool queueBlock(const int16_t* pcm,unsigned count,unsigned channels,unsigned rate,unsigned volume,int balance,uint32_t generation,uint32_t streamEpoch,bool legacy){
    if(!pcmJobs)return false; // No optional worker exists in the internal-RAM profile.
    const uint32_t started=millis();bool waited=false;
    PcmSpaceWait space(&pcmJobs->queueSpace);
    for(;;){
        if(!producerReady()||generation!=pcmGeneration.load()||streamEpoch!=epoch.load()){
            pcmCancelledFrames.fetch_add(count);return false;
        }
        if(pcmJobs->push(pcm,count,channels,rate,volume,balance,generation,streamEpoch,legacy)){
            pcmQueuedFrames.fetch_add(count);
            const uint32_t size=pcmJobs->size();if(size>pcmJobs->queuePeak.load())pcmJobs->queuePeak.store(size);
            xTaskNotifyGive(pcmWorkerHandle);return true;
        }
        if(!waited){pcmJobs->queueWaits.fetch_add(1);waited=true;}
        const uint32_t elapsed=millis()-started;
        if(elapsed>=100){pcmQueueLostFrames.fetch_add(count);return false;}
        if(space.armAndRecheck())continue;
        space.wait(100-elapsed);
    }
}
bool beginServicePcm(ServiceLease& lease){
    PayloadUse payload;if(!payload)return false;
    lease={};
    if(!player.onAudioTask()||player.isRunning()||!producerReady()||!txPcm)return false;
    // Invalidates every queued decoder job before the generator owns the ring.
    flush();
    portENTER_CRITICAL(&txMux);
    const bool good=producerReady()&&txPcm;
    if(good)lease={pcmGeneration.load(),epoch.load(),txRate.load(),true};
    portEXIT_CRITICAL(&txMux);
    return good;
}
serviceui::Transfer writeServicePcm(const int16_t* samples,uint16_t count,const ServiceLease& lease){
    PayloadUse payload;if(!payload)return {0,true};
    if(!player.onAudioTask()||player.isRunning()||!lease.valid)return {0,true};
    portENTER_CRITICAL(&txMux);
    uint32_t head=txHead,used=txUsed;
    auto result=serviceui::nativeRing(txPcm,txFrames,head,used,samples,count,true,
        producerReady(),lease.generation==pcmGeneration.load()&&lease.epoch==epoch.load(),lease.rate,txRate.load());
    if(!result.fault){txHead=unsigned(head);txUsed=unsigned(used);}
    if(!result.fault&&result.frames){blockFrames.fetch_add(result.frames);sourceRate.store(lease.rate);}
    portEXIT_CRITICAL(&txMux);
    return result;
}
void feedBlock(const int16_t* samples,uint32_t count,unsigned channels,unsigned rate,unsigned volume,int balance){
    if(!samples||!count||!producerReady()||(channels!=1&&channels!=2)||
       !(rate==16000||rate==32000||rate==44100||rate==48000))return;
#if IBT_NO_PSRAM
    if(vuprobe::localBtEnabled.load(std::memory_order_relaxed))vuprobe::observeBlock(samples,count,channels,millis());
#endif
    const uint32_t generation=pcmGeneration.load(),streamEpoch=epoch.load();
    if(!pcmJobs||!pcmWorkerReady.load()){
        processGeneration=generation;processEpoch=streamEpoch;
        processBlock(samples,count,channels,rate,volume,balance);return;
    }
    // Flush any complete legacy fragment before switching processing paths.
    if(fallbackCount){
        queueBlock(fallbackPcm,fallbackCount,2,fallbackRate,254,0,fallbackGeneration,fallbackEpoch,true);
        fallbackCount=0;
    }
    for(uint32_t done=0;done<count;){
        const unsigned n=std::min<uint32_t>(PcmWorkQueue::Frames,count-done);
        if(!queueBlock(samples+done*channels,n,channels,rate,volume,balance,generation,streamEpoch,false)){
            if(generation==pcmGeneration.load()&&streamEpoch==epoch.load()&&producerReady())
                pcmQueueLostFrames.fetch_add(count-done-n);
            return;
        }
        done+=n;
    }
}
void sample(int16_t left,int16_t right,uint32_t rate){
    if(!producerReady())return;
    const uint32_t generation=pcmGeneration.load(),streamEpoch=epoch.load();
    if(!pcmJobs||!pcmWorkerReady.load()){
        processGeneration=generation;processEpoch=streamEpoch;processSample(left,right,rate);return;
    }
    if(fallbackCount&&(fallbackGeneration!=generation||fallbackEpoch!=streamEpoch||fallbackRate!=rate)){
        pcmCancelledFrames.fetch_add(fallbackCount);fallbackCount=0;
    }
    if(!fallbackCount){fallbackGeneration=generation;fallbackEpoch=streamEpoch;fallbackRate=rate;}
    fallbackPcm[fallbackCount*2]=left;fallbackPcm[fallbackCount*2+1]=right;
    if(++fallbackCount==64){
        queueBlock(fallbackPcm,fallbackCount,2,rate,254,0,generation,streamEpoch,true);fallbackCount=0;
    }
}

static int32_t data(uint8_t* bytes,int32_t n){
    TxCallbackMeasure callbackMeasure;
    PayloadUse payload;
    if(!payload){if(bytes&&n>0){memset(bytes,0,n);return n;}return 0;}
    if(txCallbackCore.load()<0){
        TaskStatus_t info{};vTaskGetInfo(nullptr,&info,pdFALSE,eInvalid);
        txCallbackTask.store(unsigned(info.xTaskNumber));
        txCallbackCore.store(xPortGetCoreID());
    }
    // Bounded critical sections; silence on missing input.
    static uint32_t seen=0;
    static bool primed=false;
    static ibtaudio::StartFade startFade; // Owned only by this A2DP callback.
    const uint32_t generation=epoch.load();
    if(n<0){clearTx();primed=false;return 0;}
    if(!bytes||n==0)return 0;
    memset(bytes,0,n);
    if(config.getMode()==PM_WEB&&txRate.load()!=32000)return 0;
    requestedBytes.fetch_add(n);
    // flush() already cleared stale PCM. Preserve newly primed PCM on first START.
    if(generation!=seen){seen=generation;primed=false;}
    if(!primed){
        portENTER_CRITICAL(&txMux);
        primed=txUsed>=std::min<unsigned>(txFrames-1,unsigned(txRate.load()/2));
        portEXIT_CRITICAL(&txMux);
        if(!primed){prefillCallbacks.fetch_add(1);txPrefillBytes.fetch_add(unsigned(n),std::memory_order_relaxed);return n;}
        startFade.begin(txRate.load()); // Initial start and each underflow recovery.
    }
    const unsigned wanted=unsigned(n)/4;unsigned done=0;
    while(done<wanted){
        portENTER_CRITICAL(&txMux);
        const unsigned count=txPcm&&transmitting()?std::min({wanted-done,txUsed,txReadBatchFrames,txFrames-txTail}):0;
        if(count){memcpy(bytes+done*4,txPcm+txTail*2,count*4);txTail=(txTail+count)%txFrames;txUsed-=count;delivered.fetch_add(count*4);}
        portEXIT_CRITICAL(&txMux);
        if(!count)break;
        done+=count;
    }
    // Consumer never waits. One coalesced notification after the bounded copy,
    // outside txMux; only a producer that armed itself on a full ring is woken.
    if(done&&pcmJobs&&pcmWorkerReady.load())pcmJobs->txSpace.notify();
    // Fade actual PCM only, after copying and outside the ring critical section.
    // Silence while prebuffering never consumes the fade; steady state is one check.
    if(startFade.active())startFade.apply(bytes,done);
    if(done<wanted){const bool boundary=!sourcePlaying.load()||uint32_t(millis()-boundaryAt.load())<1000u;(boundary?boundaryUnderruns:steadyUnderruns).fetch_add(1);txShortageBytes.fetch_add((wanted-done)*4,std::memory_order_relaxed);underruns.fetch_add(1);rebufferEvents.fetch_add(1);primed=false;}
    return n;
}

bool inputArtworkSafe(){PayloadUse payload;if(!payload)return false;auto q=rxRing.load();return inputSelected()&&(!streaming.load()||(q&&q->size()>=16384));}
static void sinkData(const uint8_t* bytes,uint32_t n){
    PayloadUse payload;if(!payload)return;
    auto q=rxRing.load();if(!q||!flowing(Mode::In))return;
    if(!mono.load()){
        auto accepted=q->push(bytes,n);received.fetch_add(accepted);if(accepted<n)dropped.fetch_add(n-accepted);
    }else{
        // Expand mono SBC in bounded stack chunks. Never write I2S in callback.
        int16_t stereo[128];uint32_t offset=0;
        while(offset+1<n){unsigned count=0;
            while(count<64&&offset+1<n){int16_t v;memcpy(&v,bytes+offset,2);offset+=2;stereo[count*2]=v;stereo[count*2+1]=v;++count;}
            auto accepted=q->push((uint8_t*)stereo,count*4);received.fetch_add(accepted);if(accepted<count*4)dropped.fetch_add(count*4-accepted);
        }
    }
}
size_t pcm(uint8_t* out,size_t capacity,uint32_t& rate){
    static uint32_t seen=0;static bool primed=false;
    PayloadUse payload;if(!payload)return 0;
    auto q=rxRing.load();if(!q)return 0;auto current=epoch.load();
    if(seen!=current){seen=current;q->discard();primed=false;}
    if(!flowing(Mode::In)){q->discard();primed=false;return 0;}
    if(!primed){if(q->size()<32768)return 0;primed=true;}
    rate=rxRate.load();auto n=q->pop(out,capacity);delivered.fetch_add(n);
    if(n)btinputvu::observe(out,n,millis());
    if(!n){primed=false;underruns.fetch_add(1);}return n;
}
unsigned deviceCount(){portENTER_CRITICAL(&devicesMux);auto n=devicesUsed;portEXIT_CRITICAL(&devicesMux);return n;}
bool device(unsigned i,Device& out){portENTER_CRITICAL(&devicesMux);bool ok=i<devicesUsed;if(ok)out=devices[i];portEXIT_CRITICAL(&devicesMux);return ok;}
bool scanning(){return discovering.load();}
bool linkFailed(){return failed.load();}
bool linkRequestPending(){return reconnectRequested.load()||restartScanRequested.load();}
bool connecting(){return outputSelected()&&found.load()&&!streaming.load()&&!failed.load();}
bool takeAutoDacFallback(){return autoDacFallback.exchange(false);}
void loadSavedDevice(){
    uint8_t address[6]{};bool valid=false;uint32_t savedRate=32000;
    SavedHistory loaded{};loaded.magic=savedHistoryMagic;
    Preferences p;
    if(p.begin("ibt-bt-out",true)){
        savedRate=p.getUInt("rate",32000);
        if(p.getBytesLength("history")==sizeof(loaded)){
            SavedHistory disk{};
            if(p.getBytes("history",&disk,sizeof(disk))==sizeof(disk)&&
               disk.magic==savedHistoryMagic&&disk.count<=savedHistoryCapacity){
                bool good=true;
                for(unsigned i=0;i<disk.count;++i)good=good&&validAddress(disk.entries[i].address);
                if(good){loaded=disk;for(unsigned i=0;i<loaded.count;++i){
                    loaded.entries[i].name[sizeof(loaded.entries[i].name)-1]=0;
                    fallbackName(loaded.entries[i]);
                }}
            }
        }
        if(p.getBytesLength("lastmac")==sizeof(address)){
            valid=p.getBytes("lastmac",address,sizeof(address))==sizeof(address);
            valid=valid&&validAddress(address);
        }
        p.end();
    }
    if(!loaded.count&&valid){
        loaded.count=1;memcpy(loaded.entries[0].address,address,6);
        fallbackName(loaded.entries[0]);
    }
    if(savedRate!=16000&&savedRate!=32000&&savedRate!=44100&&savedRate!=48000)savedRate=32000;
    requestedRate.store(savedRate);
    portENTER_CRITICAL(&peerMux);
    savedHistory=loaded;
    if(loaded.count)memcpy(savedPeer,loaded.entries[0].address,6);
    portEXIT_CRITICAL(&peerMux);
    savedValid.store(loaded.count!=0);
}
bool hasSavedDevice(){return savedValid.load();}
unsigned savedDeviceCount(){
    portENTER_CRITICAL(&peerMux);unsigned count=savedHistory.count;portEXIT_CRITICAL(&peerMux);
    return count;
}
bool savedDevice(unsigned index,Device& out){
    portENTER_CRITICAL(&peerMux);
    bool ok=index<savedHistory.count;
    if(ok)out=savedHistory.entries[index];
    portEXIT_CRITICAL(&peerMux);
    if(ok)fallbackName(out);
    return ok;
}
bool targetDevice(Device& out){
    out=Device{};
    if(!outputSelected())return false;
    uint8_t address[6];
    portENTER_CRITICAL(&peerMux);
    memcpy(address,reconnectRequested.load()?requestedSavedPeer:peer,6);
    for(unsigned i=0;i<savedHistory.count;++i)
        if(!memcmp(savedHistory.entries[i].address,address,6)){out=savedHistory.entries[i];break;}
    portEXIT_CRITICAL(&peerMux);
    if(!validAddress(address))return false;
    portENTER_CRITICAL(&devicesMux);
    for(unsigned i=0;i<devicesUsed;++i)
        if(!memcmp(devices[i].address,address,6)&&!placeholderName(devices[i].name)){out=devices[i];break;}
    portEXIT_CRITICAL(&devicesMux);
    memcpy(out.address,address,6);
    fallbackName(out);
    return true;
}
bool connectedDevice(Device& out){

    out=Device{};

    if(!outputSelected()||!connected())return false;

    uint8_t address[6];

    portENTER_CRITICAL(&peerMux);

    memcpy(address,peer,6);

    for(unsigned i=0;i<savedHistory.count;++i)

        if(!memcmp(savedHistory.entries[i].address,address,6)){out=savedHistory.entries[i];break;}

    portEXIT_CRITICAL(&peerMux);

    portENTER_CRITICAL(&devicesMux);

    for(unsigned i=0;i<devicesUsed;++i)

        if(!memcmp(devices[i].address,address,6)&&!placeholderName(devices[i].name)){out=devices[i];break;}

    portEXIT_CRITICAL(&devicesMux);

    memcpy(out.address,address,6);

    fallbackName(out);

    return true;

}

bool connectSavedDevice(unsigned index){
    if(!surfacepolicy::allows(surfacepolicy::current(),surfacepolicy::Route::SdBt))return false;
    if(config.getMode()==PM_WEB)return false;
    portENTER_CRITICAL(&peerMux);
    bool ok=index<savedHistory.count;
    if(ok)memcpy(requestedSavedPeer,savedHistory.entries[index].address,6);
    portEXIT_CRITICAL(&peerMux);
    if(ok)reconnectRequested.store(true);
    return ok;
}
bool reconnectSaved(){
    return connectSavedDevice(0);
}
bool restartScan(){
    if(!surfacepolicy::allows(surfacepolicy::current(),surfacepolicy::Route::SdBt))return false;
    if(!outputSelected()||active.load()!=Mode::Out||stage.load()!=Stage::Active||
       discovering.load()||deviceCount()!=0||connection.load()!=ESP_A2D_CONNECTION_STATE_DISCONNECTED)return false;
    restartScanRequested.store(true);
    return true;
}
bool selectDevice(unsigned i){
    if(!surfacepolicy::allows(surfacepolicy::current(),surfacepolicy::Route::SdBt))return false;
    if(!outputSelected()||active.load()!=Mode::Out||stage.load()!=Stage::Active||(found.load()&&!failed.load()))return false;
    Device chosen;if(!device(i,chosen))return false;
    portENTER_CRITICAL(&peerMux);memcpy(peer,chosen.address,6);memcpy(selectedPeer,chosen.address,6);portEXIT_CRITICAL(&peerMux);
    selectedAt.store(millis());
    if(!discovering.load())scanStoppedAt.store(selectedAt.load());
    bool retry=found.exchange(true);
    if(retry){retryRequested.store(true);return true;}
    // Stop inquiry at selection time. The main loop may be busy with network/UI
    // work; A2DP connect still waits for the GAP stopped callback below.
    if(discovering.load()&&esp_bt_gap_cancel_discovery()==ESP_OK)cancelSent.store(true);
    return true;
}
bool control(unsigned key){
    if(!surfacepolicy::remoteTransport(surfacepolicy::current())||key>3||!inputSelected()||stage.load()!=Stage::Active||!controlConnected.load()){controlResult.store(-1);return false;}
    if(keyHeld.load()){controlResult.store(-2);return false;}
    int empty=-1;bool ok=pendingKey.compare_exchange_strong(empty,int(key));controlResult.store(ok?1:-2);return ok;
}
const char* controlStatus(){return !controlConnected.load()?"Telefon: I-BT-Radio-Audio":controlResult.load()<0?"Vezerles nem elerheto":streaming.load()?"Lejatszas / telefon":"Csatlakozva / szunet";}
static void avrc(esp_avrc_ct_cb_event_t e,esp_avrc_ct_cb_param_t* p){
    if(inputSelected())btmedia::event(e,p);
    if(e==ESP_AVRC_CT_PROF_STATE_EVT){controlReady.store(p->avrc_ct_init_stat.state==ESP_AVRC_INIT_SUCCESS);controlGone.store(p->avrc_ct_init_stat.state==ESP_AVRC_DEINIT_SUCCESS);}
    if(e==ESP_AVRC_CT_CONNECTION_STATE_EVT)controlConnected.store(p->conn_stat.connected);
    if(e==ESP_AVRC_CT_PASSTHROUGH_RSP_EVT)controlResult.store(p->psth_rsp.rsp_code==ESP_AVRC_RSP_ACCEPT?2:-3);
}
static void serviceControl(){
    if(!surfacepolicy::remoteTransport(surfacepolicy::current())||!inputSelected()||stage.load()!=Stage::Active||!controlConnected.load()){
        if(keyHeld.load()&&controlConnected.load())esp_avrc_ct_send_passthrough_cmd(transaction,heldKey,ESP_AVRC_PT_CMD_STATE_RELEASED);
        transaction=(transaction+1)%14;pendingKey.store(-1);keyHeld.store(false);heldKey=-1;return;
    }
    if(keyHeld.load()){
        if(uint32_t(millis()-releaseAt)<40)return;
        auto err=esp_avrc_ct_send_passthrough_cmd(transaction,heldKey,ESP_AVRC_PT_CMD_STATE_RELEASED);
        transaction=(transaction+1)%14;heldKey=-1;keyHeld.store(false);if(err!=ESP_OK)controlResult.store(-3);return;
    }
    int k=pendingKey.exchange(-1);if(k<0)return;
    const uint8_t keys[]={ESP_AVRC_PT_CMD_PLAY,ESP_AVRC_PT_CMD_PAUSE,ESP_AVRC_PT_CMD_BACKWARD,ESP_AVRC_PT_CMD_FORWARD};
    heldKey=keys[k];
    if(esp_avrc_ct_send_passthrough_cmd(transaction,heldKey,ESP_AVRC_PT_CMD_STATE_PRESSED)!=ESP_OK){controlResult.store(-3);heldKey=-1;return;}
    transaction=(transaction+1)%14;releaseAt=millis();keyHeld.store(true);
}
static bool pairingAllowed(const uint8_t* address){
      if(surfacepolicy::current()==surfacepolicy::Surface::Clock)return false;
    if(stage.load()!=Stage::Active||mode()!=active.load())return false;
    if(inputSelected())return true; // phone pairing is explicitly enabled in IN mode
    esp_bd_addr_t expected;portENTER_CRITICAL(&peerMux);memcpy(expected,selectedPeer,6);portEXIT_CRITICAL(&peerMux);
    return outputSelected()&&found.load()&&!memcmp(expected,address,6);
}
static void discoveryResult(esp_bt_gap_cb_param_t* p){
    ++discoveryStats.raw;
    if(mode()!=Mode::Out||active.load()!=Mode::Out||stage.load()!=Stage::Active){++discoveryStats.state;return;}
    if(found.load()){++discoveryStats.selected;return;}
    if(!p||p->disc_res.num_prop<0||p->disc_res.num_prop>16||(p->disc_res.num_prop&&!p->disc_res.prop)){++discoveryStats.invalid;return;}
    if(!validAddress(p->disc_res.bda)){++discoveryStats.invalid;return;}
    Device entry{};memcpy(entry.address,p->disc_res.bda,6);entry.rssi=-127;uint32_t cod=0;bool hasCod=false,hasRssi=false;
    for(int i=0;i<p->disc_res.num_prop;++i){
        auto& prop=p->disc_res.prop[i];uint8_t len=0;const uint8_t* value=nullptr;
        if(!prop.val||prop.len<=0){++discoveryStats.badProps;continue;}
        if(prop.type==ESP_BT_GAP_DEV_PROP_COD&&prop.len<4){++discoveryStats.badProps;continue;}
        if(prop.type==ESP_BT_GAP_DEV_PROP_COD&&prop.len>=4){memcpy(&cod,prop.val,4);hasCod=true;}
        if(prop.type==ESP_BT_GAP_DEV_PROP_RSSI&&prop.len>=1){memcpy(&entry.rssi,prop.val,1);hasRssi=true;}
        if(prop.type==ESP_BT_GAP_DEV_PROP_BDNAME&&prop.len>0){len=uint8_t(std::min(prop.len,int(sizeof(entry.name)-1)));value=(uint8_t*)prop.val;}
        if(prop.type==ESP_BT_GAP_DEV_PROP_EIR){
            bool valid=true;value=btdiscovery::eirName((const uint8_t*)prop.val,unsigned(prop.len),len,valid);
            if(!valid)++discoveryStats.badProps;
        }
        if(value&&len){len=std::min<unsigned>(len,sizeof(entry.name)-1);memcpy(entry.name,value,len);entry.name[len]=0;}
    }
    // CoD is a hint, not proof of A2DP capability. Keep all Classic results.
    if(!hasCod||!cod)++discoveryStats.codUnknown;
    else if(!(esp_bt_gap_get_cod_srvc(cod)&ESP_BT_COD_SRVC_RENDERING))++discoveryStats.codNonRendering;
    portENTER_CRITICAL(&devicesMux);
    unsigned at=0;while(at<devicesUsed&&memcmp(devices[at].address,entry.address,6))++at;
    if(at<12){
        // Inquiry RSSI-only updates must not erase an already discovered name.
        if(!entry.name[0]&&at<devicesUsed)memcpy(entry.name,devices[at].name,sizeof(entry.name));
        if(!hasRssi&&at<devicesUsed)entry.rssi=devices[at].rssi;
        fallbackName(entry);devices[at]=entry;++discoveryStats.accepted;
        if(at==devicesUsed){++devicesUsed;++discoveryStats.added;}else ++discoveryStats.updated;
    }else ++discoveryStats.full;
    portEXIT_CRITICAL(&devicesMux);
}
static void printDiscoveryStats(const char* reason){
    const auto& d=discoveryStats;
    Serial.printf("[BT-DISCOVERY] reason=%s generation=%lu raw=%lu accepted=%lu added=%lu updated=%lu filtered_state=%lu filtered_selected=%lu filtered_invalid=%lu filtered_full=%lu malformed_props=%lu cod_unknown_seen=%lu cod_nonrender_seen=%lu started=%lu completed=%lu start_error=%d listed=%u capacity=12 transport=CLASSIC compatibility=NOT_PROBED\n",
        reason,(unsigned long)d.generation.load(),(unsigned long)d.raw.load(),(unsigned long)d.accepted.load(),(unsigned long)d.added.load(),(unsigned long)d.updated.load(),
        (unsigned long)d.state.load(),(unsigned long)d.selected.load(),(unsigned long)d.invalid.load(),(unsigned long)d.full.load(),(unsigned long)d.badProps.load(),
        (unsigned long)d.codUnknown.load(),(unsigned long)d.codNonRendering.load(),(unsigned long)d.started.load(),(unsigned long)d.completed.load(),d.startError.load(),deviceCount());
}
static void gap(esp_bt_gap_cb_event_t e,esp_bt_gap_cb_param_t* p){
    // Control events only, no additional queries and no address/key disclosure.
    if(e==ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT)
        Serial.printf("[BT-CONNECT-DIAG] phase=ACL sdk_status=%d\n",int(p->acl_conn_cmpl_stat.stat));
    if(e==ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT)
        Serial.printf("[BT-CONNECT-DIAG] phase=ACL_DISCONNECT sdk_reason=%d\n",int(p->acl_disconn_cmpl_stat.reason));
    if(e==ESP_BT_GAP_AUTH_CMPL_EVT)
        Serial.printf("[BT-CONNECT-DIAG] phase=AUTH sdk_status=%d\n",int(p->auth_cmpl.stat));
    if(e==ESP_BT_GAP_READ_RSSI_DELTA_EVT){
        esp_bd_addr_t currentPeer;portENTER_CRITICAL(&peerMux);memcpy(currentPeer,peer,6);portEXIT_CRITICAL(&peerMux);
        if(p->read_rssi_delta.stat==ESP_BT_STATUS_SUCCESS&&
           connection.load()==ESP_A2D_CONNECTION_STATE_CONNECTED&&
           !memcmp(currentPeer,p->read_rssi_delta.bda,6)){
            const uint32_t delta=uint8_t(p->read_rssi_delta.rssi_delta);
            uint32_t old=linkSignal.load(std::memory_order_relaxed);
            while(!linkSignal.compare_exchange_weak(old,(old&0xffffff00u)|delta,std::memory_order_relaxed)){}
        }
        return;
    }
    if(e==ESP_BT_GAP_CFM_REQ_EVT)esp_bt_gap_ssp_confirm_reply(p->cfm_req.bda,pairingAllowed(p->cfm_req.bda));
    if(e==ESP_BT_GAP_PIN_REQ_EVT){esp_bt_pin_code_t code;memset(code,'0',sizeof(code));esp_bt_gap_pin_reply(p->pin_req.bda,pairingAllowed(p->pin_req.bda),p->pin_req.min_16_digit?16:4,code);}
    if(e==ESP_BT_GAP_DISC_STATE_CHANGED_EVT){
        bool running=p->disc_st_chg.state==ESP_BT_GAP_DISCOVERY_STARTED;
        discovering.store(running);
        if(running)++discoveryStats.started;else{++discoveryStats.completed;discoverySummary.store(true);}
        if(!running&&found.load())scanStoppedAt.store(millis());
    }
    if(e==ESP_BT_GAP_DISC_RES_EVT)discoveryResult(p);
}
static void event(esp_a2d_cb_event_t e,esp_a2d_cb_param_t* p){
    if(e==ESP_A2D_PROF_STATE_EVT){
        if(p->a2d_prof_stat.init_state==ESP_A2D_INIT_SUCCESS)ready.store(true);
        if(p->a2d_prof_stat.init_state==ESP_A2D_DEINIT_SUCCESS){ready.store(false);deinitialized.store(true);}
    }
    if(e==ESP_A2D_CONNECTION_STATE_EVT){
        portENTER_CRITICAL(&peerMux);memcpy(peer,p->conn_stat.remote_bda,6);portEXIT_CRITICAL(&peerMux);
        Serial.printf("[BT-CONNECT-DIAG] phase=A2DP state=%d disconnect_reason=%d\n",int(p->conn_stat.state),
            p->conn_stat.state==ESP_A2D_CONNECTION_STATE_DISCONNECTED?int(p->conn_stat.disc_rsn):-1);
        connection.store(p->conn_stat.state);
        linkSignal.store(0xffffff80u,std::memory_order_relaxed);
        if(p->conn_stat.state==ESP_A2D_CONNECTION_STATE_CONNECTED){
            connectedAt.store(millis());connectionRejected.store(false);
            if(active.load()==Mode::Out)savePeerPending.store(true);
        }
        else {
            if(p->conn_stat.state==ESP_A2D_CONNECTION_STATE_DISCONNECTED&&mode()==Mode::Out&&found.load())connectionRejected.store(true);
            streaming.store(false);txCodecConfigured.store(false);txRate.store(0);flush();
            mediaCheckAck.store(0);mediaStartFailed.store(false);
        }
    }
    if(e==ESP_A2D_AUDIO_STATE_EVT){
        bool playing=p->audio_stat.state==ESP_A2D_AUDIO_STATE_STARTED;
        streaming.store(playing);if(playing)streamAt.store(millis());
        // Output START must retain PCM prepared before the media channel opened.
        if(active.load()!=Mode::Out||!playing)epoch.fetch_add(1);
    }
    if(e==ESP_A2D_AUDIO_CFG_EVT){
        const auto& codec=p->audio_cfg.mcc;
        const uint8_t frequency=codec.cie.sbc_info.samp_freq;
        if(active.load()==Mode::In){
            mono.store((codec.cie.sbc_info.ch_mode&ESP_A2D_SBC_CIE_CH_MODE_MONO)!=0);
            rxRate.store((frequency&ESP_A2D_SBC_CIE_SF_16K)?16000:(frequency&ESP_A2D_SBC_CIE_SF_32K)?32000:(frequency&ESP_A2D_SBC_CIE_SF_44K)?44100:48000);epoch.fetch_add(1);
            Serial.printf("[BT-IN-CODEC] type=%u rate=%lu channel_mode=0x%02x bitpool_min=%u bitpool_max=%u subbands=0x%02x blocks=0x%02x allocation=0x%02x\n",
                unsigned(codec.type),(unsigned long)rxRate.load(),unsigned(codec.cie.sbc_info.ch_mode),
                unsigned(codec.cie.sbc_info.min_bitpool),unsigned(codec.cie.sbc_info.max_bitpool),
                unsigned(codec.cie.sbc_info.num_subbands),unsigned(codec.cie.sbc_info.block_len),unsigned(codec.cie.sbc_info.alloc_mthd));
        }else if(active.load()==Mode::Out){
            const uint32_t targetHz=outputRate();
            const uint32_t negotiatedHz=frequency==ESP_A2D_SBC_CIE_SF_16K?16000:
                frequency==ESP_A2D_SBC_CIE_SF_32K?32000:
                frequency==ESP_A2D_SBC_CIE_SF_44K?44100:
                frequency==ESP_A2D_SBC_CIE_SF_48K?48000:0;
            const bool accepted=codec.type==ESP_A2D_MCT_SBC&&btrate::accepted(config.getMode()==PM_WEB,negotiatedHz);
            // Reconfiguration invalidates old-rate PCM before producers resume.
            txCodecConfigured.store(false);flush();
            txRate.store(accepted?negotiatedHz:0);txCodecConfigured.store(accepted);
            Serial.printf("[BT-CODEC] target_hz=%lu negotiated_hz=%lu negotiated_mask=0x%02x result=%s\n",(unsigned long)targetHz,(unsigned long)negotiatedHz,unsigned(frequency),accepted?(targetHz==negotiatedHz?"PASS":"COMMON_RATE"):"UNSUPPORTED_BT_WAIT");
            if(!accepted){codecUnsupported.store(true);failed.store(true);autoDacFallback.store(true);}
        }
    }
    if(e==ESP_A2D_MEDIA_CTRL_ACK_EVT&&mode()==Mode::Out&&stage.load()==Stage::Active){
        if(p->media_ctrl_stat.cmd==ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY)
            mediaCheckAck.store(p->media_ctrl_stat.status==ESP_A2D_MEDIA_CTRL_ACK_SUCCESS?1:-1);
        if(p->media_ctrl_stat.cmd==ESP_A2D_MEDIA_CTRL_START&&p->media_ctrl_stat.status!=ESP_A2D_MEDIA_CTRL_ACK_SUCCESS)
            mediaStartFailed.store(true);
    }
}
#if IBT_NO_PSRAM
// Classic profiles are mutually exclusive. Close admission after deinit ACK and
// wait for all existing borrowers before returning the shared arena to DRAM.
static uint8_t* liteBtPayload=nullptr;
static size_t liteBtBytes=0;
static bool releaseLiteBtPayload(){
    if(!litePayloadGate.close())return false; // No spin/wait in the service owner.
    auto q=rxRing.exchange(nullptr);
    portENTER_CRITICAL(&txMux);
    txPcm=nullptr;txHead=txTail=txUsed=0;
    portEXIT_CRITICAL(&txMux);
    auto bytes=liteBtPayload;const size_t released=liteBtBytes;liteBtPayload=nullptr;liteBtBytes=0;
    delete q;heap_caps_free(bytes);
    if(bytes)Serial.printf("[BT-PAYLOAD] released=%u reason=QUIESCENT\n",unsigned(released));
    return true;
}
static bool allocateLiteBtPayload(size_t bytes=16384*4){
    if(liteBtPayload)return liteBtBytes>=bytes;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<bytes+internalReserve)return false;
    auto p=(uint8_t*)heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!p)return false;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<internalReserve){free(p);return false;}
    liteBtPayload=p;liteBtBytes=bytes;return true;
}
static_assert(16384>32000/2,"Lite TX must retain at least500ms PCM");
#endif
static bool allocateRx(){
    auto& target=rxRing;
    if(target.load())return true;
    const uint32_t capacity=IBT_NO_PSRAM?32768:131072;
    const uint32_t caps=litememory::payloadCaps;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<internalReserve)return false;
#if IBT_NO_PSRAM
    if(!allocateLiteBtPayload(capacity))return false;
    auto q=new(std::nothrow) PcmRing(liteBtPayload,capacity);if(!q)return false;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<internalReserve){delete q;return false;}
#else
    auto bytes=(uint8_t*)heap_caps_malloc(capacity,caps);if(!bytes)return false;
    auto q=new(std::nothrow) PcmRing(bytes,capacity);if(!q){free(bytes);return false;}
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<internalReserve){delete q;free(bytes);return false;}
#endif
    target.store(q);return true;
}
static bool allocateTx(){
    if(txPcm)return true;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<internalReserve)return false;
#if IBT_NO_PSRAM
    if(!allocateLiteBtPayload())return false;
    txPcm=(int16_t*)liteBtPayload;
#else
    txPcm=(int16_t*)heap_caps_malloc(txFrames*4,litememory::payloadCaps);
    if(!txPcm)return false;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<internalReserve){free(txPcm);txPcm=nullptr;return false;}
#endif
    return true;
}
static bool stackResult(const char* step,esp_err_t error){
    if(error!=ESP_OK)Serial.printf("[BT-STACK] step=%s error=%d host=%u controller=%u free=%u largest=%u\n",step,int(error),unsigned(esp_bluedroid_get_status()),unsigned(esp_bt_controller_get_status()),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
    return error==ESP_OK;
}
#if IBT_NO_PSRAM
// Only before any profile starts, or after A2DP+AVRCP deinit ACK and a closed
// payload gate. Ordinary deinit is reversible; never call *_mem_release().
static bool stopLiteStack(){
    if(esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_ENABLED && !stackResult("host_disable",esp_bluedroid_disable()))return false;
    if(esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_INITIALIZED && !stackResult("host_deinit",esp_bluedroid_deinit()))return false;
    if(esp_bluedroid_get_status()!=ESP_BLUEDROID_STATUS_UNINITIALIZED)return false;
    if(esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_ENABLED && !stackResult("controller_disable",esp_bt_controller_disable()))return false;
    if(esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_INITED && !stackResult("controller_deinit",esp_bt_controller_deinit()))return false;
    if(esp_bt_controller_get_status()!=ESP_BT_CONTROLLER_STATUS_IDLE)return false;
    initialized=false;return true;
}
#endif
static bool initMemoryPhase(const char* phase,unsigned remaining){
#if IBT_NO_PSRAM
    const unsigned free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    const unsigned largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    const bool ok=btstart::phaseReady(free,largest,remaining);
    Serial.printf("[BT-INIT-MEM] phase=%s free=%u largest=%u remaining=%u reserve=%u result=%s\n",phase,free,largest,remaining,internalReserve,ok?"PASS":"DENIED");
    if(!ok&&remaining==btstart::stackPlan){
        Serial.println("[BT-HEAP-REGIONS] reason=ADMISSION_DENIED scope=INTERNAL_8BIT");
        heap_caps_print_heap_info(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    }
    return ok;
#else
    (void)phase;(void)remaining;return true;
#endif
}
static bool initializeStack(bool audit=false){
    // Diagnostic path is used only by the explicitly requested BLE startup.
    // Audio startup keeps its existing checks and priorities.
    static bool auditFailed=false;
    auto checkpoint=[&](const char* step){
        if(!audit)return true;
        if(auditFailed)return false;
        Serial.printf("[BLE-CHECK] stage=%s result=BEGIN\n",step);
        const bool healthy=heap_caps_check_integrity_all(true);
        Serial.printf("[BLE-CHECK] stage=%s result=%s\n",step,healthy?"PASS":"FAIL");
        if(!healthy)auditFailed=true;
        return healthy;
    };
    if(audit&&auditFailed)return false;
#if CONFIG_BT_BLE_ENABLED && defined(IBT_SENSOR_EXPERIMENTAL)
    const bool bleMemoryLost=btMemReleased(BT_MODE_BLE);
    if(audit)Serial.printf("[BLE-MEMORY] reserved_ble=%u reserved_classic=%u released_ble=%u suspect=0x3ffb15c0 ble_em=0x3ffb09a8..0x3ffb1ddc\n",unsigned(bleInUse()),unsigned(btClassicInUse()),unsigned(bleMemoryLost));
    if(!bleInUse()||bleMemoryLost){Serial.println("[BLE-MEMORY] result=DENIED reason=controller_memory_not_reserved");return false;}
#endif
    if(!checkpoint("sdk_entry"))return false;
    if(btMemReleased(BT_MODE_CLASSIC_BT)){
        Serial.println("[BT-MEMORY] result=DENIED reason=classic_memory_released reboot_with_fixed_firmware=1");
        return false;
    }
    if(esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_IDLE){
        if(!initMemoryPhase("before_controller",btstart::stackPlan))return false;
        esp_bt_controller_config_t cfg=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        const esp_err_t error=esp_bt_controller_init(&cfg);
        if(!checkpoint("controller_init"))return false;if(!stackResult("controller_init",error))return false;
    }
#if CONFIG_BTDM_CTRL_MODE_BTDM
    constexpr esp_bt_mode_t controllerMode=ESP_BT_MODE_BTDM;
#else
    constexpr esp_bt_mode_t controllerMode=ESP_BT_MODE_CLASSIC_BT;
#endif
    if(esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_INITED){
        if(!initMemoryPhase("before_controller_enable",btstart::hostPlan))return false;
        const esp_err_t error=esp_bt_controller_enable(controllerMode);
        if(!checkpoint("controller_enable"))return false;if(!stackResult("controller_enable",error))return false;
    }
    if(esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_UNINITIALIZED){
        if(!initMemoryPhase("before_host",btstart::hostPlan))return false;
        const esp_err_t error=esp_bluedroid_init();
        if(!checkpoint("bluedroid_init"))return false;if(!stackResult("bluedroid_init",error))return false;
    }
    if(esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_INITIALIZED){
        if(!initMemoryPhase("before_host_enable",btstart::enablePlan))return false;
        const esp_err_t error=esp_bluedroid_enable();
        if(!checkpoint("bluedroid_enable"))return false;if(!stackResult("bluedroid_enable",error))return false;
    }
    return checkpoint("sdk_ready")&&initMemoryPhase("sdk_ready",0);
}
bool preparePassiveStack(){
#if CONFIG_BT_BLE_ENABLED
    if(wanted()||!idle())return false;
    return initializeStack(true);
#else
    return false;
#endif
}
static bool startStack(){
    if(!initializeStack())return false;
#if !IBT_NO_PSRAM
    // Allocate after the controller/host so their startup cost precedes the guard.
    // This is an allocation-time reserve, not a guarantee against future SDK use.
    if(!allocateTx()||!allocateRx())return false;
#endif
    allocatePcmWorker(); // Logged inline fallback on allocation failure; never half-active.
    if(esp_bt_gap_set_device_name("I-BT-Radio-Audio")!=ESP_OK||esp_bt_gap_register_callback(gap)!=ESP_OK)return false;
    esp_bt_io_cap_t cap=ESP_BT_IO_CAP_NONE;
    if(esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE,&cap,sizeof(cap))!=ESP_OK)return false;
    esp_bt_pin_code_t pin={'0','0','0','0'};
    if(esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED,4,pin)!=ESP_OK)return false;
    return esp_a2d_register_callback(event)==ESP_OK&&esp_avrc_ct_register_callback(avrc)==ESP_OK;
}
static void fault(const char* why){failed.store(true);stage.store(Stage::Fault);if(outputSelected())autoDacFallback.store(true);Serial.printf("[BT-AUDIO] lifecycle_fault=%s restart_required=1\n",why);}
static void enter(Stage next){stageAt=millis();stage.store(next);}
static void heapCheckpoint(const char* point){
    // Inspect the internal heap at lifecycle boundaries, never in the audio
    // callback. A later SDP free can only show where corruption was detected.
    Serial.printf("[BT-HEAP] point=%s check=BEGIN free=%u largest=%u\n",point,
      unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
      unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
    const bool ok=heap_caps_check_integrity(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,true);
    Serial.printf("[BT-HEAP] point=%s check=%s\n",point,ok?"PASS":"FAIL");
}
static void initProfile(Mode target){
    enter(Stage::Init);
    auto callback=target==Mode::Out?esp_a2d_source_register_data_callback(data):esp_a2d_sink_register_data_callback(sinkData);
    if(callback!=ESP_OK){fault("callback_register");return;}
    if((target==Mode::Out?esp_a2d_source_init():esp_a2d_sink_init())!=ESP_OK)fault("profile_init");
}
static void deinitProfile(Mode current){
    heapCheckpoint("before_profile_deinit");
    deinitialized.store(false);enter(Stage::Deinit);
    if((current==Mode::Out?esp_a2d_source_deinit():esp_a2d_sink_deinit())!=ESP_OK)fault("profile_deinit");
}
#if IBT_NO_PSRAM
static void rollbackLiteStartup(const char* reason){
    failed.store(true);desired.store(Mode::Off);
    if(!releaseLiteBtPayload()||!stopLiteStack()){fault(reason);return;}
    active.store(Mode::Off);streaming.store(false);ready.store(false);epoch.fetch_add(1);enter(Stage::Off);
    autoDacFallback.store(true);
    Serial.printf("[BT-STARTUP] rollback=PASS reason=%s DAC_RESTORE=SAFE\n",reason);
}
#endif
void service(){
    if(discoverySummary.exchange(false))printDiscoveryStats(discoveryStats.startError.load()==ESP_OK?"COMPLETE":"START_ERROR");
#if IBT_NO_PSRAM
    // Off is published only after payload readers and SDK teardown acknowledge.
    // Leave transitions/faults/start requests on the full owner state machine.
    if(stage.load()==Stage::Off&&mode()==Mode::Off&&active.load()==Mode::Off&&
       !initialized&&!liteBtPayload&&!pcmJobs)return;
#endif
      if(wanted()&&!surfacepolicy::allows(surfacepolicy::current(),inputSelected()?surfacepolicy::Route::BtInDac:surfacepolicy::Route::SdBt))request(Mode::Off);
    btmedia::service(inputSelected()&&controlConnected.load());
    serviceControl();
    auto s=stage.load();auto target=mode();auto current=active.load();uint32_t now=millis();
    if(s!=Stage::Active||current!=Mode::Out){mediaGate.reset();mediaCheckAck.store(0);mediaStartFailed.store(false);}
    if(s==Stage::Fault)return;
    if(s==Stage::Active&&current==target&&connected()){
        // Asynchronous GAP query, once per ten-second bucket; never in an
        // audio callback or display draw. A missing reply hides the bars.
        const uint32_t bucket=now/10000u;
        if(uint32_t(now-connectedAt.load())>=3000&&
           (linkSignal.load(std::memory_order_relaxed)>>8)!=bucket){
            esp_bd_addr_t address;portENTER_CRITICAL(&peerMux);memcpy(address,peer,6);portEXIT_CRITICAL(&peerMux);
            uint32_t old=linkSignal.load(std::memory_order_relaxed);
            while(!linkSignal.compare_exchange_weak(old,(bucket<<8)|(old&0xffu),std::memory_order_relaxed)){}
            esp_bt_gap_read_rssi_delta(address);
        }
    }
    if(s==Stage::Off){
        if(target==Mode::Off){networkPrepareAt=0;return;}
        if(target==Mode::Out&&config.getMode()==PM_WEB){
            desired.store(Mode::Off);
            Serial.println("[BT-ROUTE] result=DENIED startup=WEB_BT_OUT");
            return;
        }
#if IBT_NO_PSRAM
        if(!network.prepareLocalBluetooth()||ibtlimit::backgroundBusy()){
            if(!networkPrepareAt)networkPrepareAt=now;
            if(uint32_t(now-networkPrepareAt)>=15000){networkPrepareAt=0;rollbackLiteStartup("network_quiescence_timeout");}
            return;
        }
        networkPrepareAt=0;
#endif
        heapCheckpoint("before_stack_init");
        if(!initialized){initialized=startStack();if(!initialized){
#if IBT_NO_PSRAM
            rollbackLiteStartup("stack_init");
#else
            fault("stack_init");
#endif
            return;
        }}
#if IBT_NO_PSRAM
        // Mutually exclusive profiles retain their original buffer capacities.
        // IN32KiB has no unused OUT64KiB arena; OUT still protects500ms at32kHz.
        if(target==Mode::In&&!allocateRx()){
            rollbackLiteStartup("payload_allocation");return;
        }
        // Profile callbacks may allocate more queues after controller/host setup.
        // Keep their budget in addition to the full payload and retained reserve.
        if(!initMemoryPhase("before_profile",btstart::profilePlan)){
            rollbackLiteStartup("profile_memory_admission");return;
        }
        if(!litePayloadGate.open()){fault("payload_not_quiescent");return;}
        if(target==Mode::Out)Serial.println("[BT-PAYLOAD] TX=DEFERRED_UNTIL_CONNECTED capacity=65536 prefill_ms=500");
#endif
        heapCheckpoint("after_stack_init");
        appliedRequest=requestId.load();active.store(target);ready.store(false);deinitialized.store(false);failed.store(false);
        heapConnectedChecked=false;
        routeFallbackLogged=false;
        streaming.store(false);connection.store(ESP_A2D_CONNECTION_STATE_DISCONNECTED);mono.store(false);rxRate.store(44100);txRate.store(0);txCodecConfigured.store(false);sourceRate.store(0);
        linkSignal.store(0xffffff80u,std::memory_order_relaxed);
        attempt=false;connectSent=false;disconnectSent=false;advertised=false;cancelSent.store(false);found.store(false);codecUnsupported.store(false);epoch.fetch_add(1);
        connectionRejected.store(false);retryRequested.store(false);connectAttempts=0;retryAt=0;
        autoDacFallback.store(false);
        selectedAt.store(0);scanStoppedAt.store(0);connectedAt.store(0);streamAt.store(0);
        scanTimingLogged=false;connectTimingLogged=false;streamTimingLogged=false;
        controlConnected.store(false);controlResult.store(0);
        portENTER_CRITICAL(&devicesMux);devicesUsed=0;portEXIT_CRITICAL(&devicesMux);
        if(target==Mode::In){
            controlReady.store(false);controlGone.store(false);enter(Stage::AvrcInit);
            if(esp_avrc_ct_init()!=ESP_OK)fault("avrc_init");
        }else initProfile(target);
        return;
    }
    if(s==Stage::AvrcInit){
        if(controlReady.load())initProfile(current);else if(uint32_t(now-stageAt)>10000)fault("avrc_init_timeout");return;
    }
    if(s==Stage::AvrcDeinit){
        if(controlGone.load())deinitProfile(current);else if(uint32_t(now-stageAt)>10000)fault("avrc_deinit_timeout");return;
    }
    if(s==Stage::Init){
        if(ready.load()){heapCheckpoint("after_profile_init");linkAt=now;lastCheck=0;enter(Stage::Active);}
        else if(uint32_t(now-stageAt)>10000)fault("init_timeout");
        return;
    }
    if(s==Stage::Active&&(target!=current||requestId.load()!=appliedRequest)){
        heapCheckpoint("before_disconnect");
        if(esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE,ESP_BT_NON_DISCOVERABLE)!=ESP_OK){fault("scan_disable");return;}
        if(discovering.load())esp_bt_gap_cancel_discovery();
        enter(Stage::Disconnect);s=Stage::Disconnect;
        // enter() reads millis() again; use that fresh time for the timeout.
        // The stale value can underflow now-stageAt and fault immediately.
        now=stageAt;
    }
    if(s==Stage::Disconnect){
        if(connection.load()!=ESP_A2D_CONNECTION_STATE_DISCONNECTED&&!disconnectSent){
            esp_bd_addr_t address;portENTER_CRITICAL(&peerMux);memcpy(address,peer,6);portEXIT_CRITICAL(&peerMux);
            disconnectSent=true;
            auto result=current==Mode::Out?esp_a2d_source_disconnect(address):esp_a2d_sink_disconnect(address);
            if(result!=ESP_OK){fault("disconnect");return;}
        }
        if(connection.load()==ESP_A2D_CONNECTION_STATE_DISCONNECTED&&!discovering.load()){
            if(current==Mode::In){controlGone.store(false);enter(Stage::AvrcDeinit);if(esp_avrc_ct_deinit()!=ESP_OK)fault("avrc_deinit");}
            else deinitProfile(current);
        }else if(uint32_t(now-stageAt)>10000)fault("disconnect_timeout");
        return;
    }
    if(s==Stage::Deinit){
        if(deinitialized.load()){
#if IBT_NO_PSRAM
            if(!releaseLiteBtPayload())return;
            if(!stopLiteStack()){fault("sdk_deinit");return;}
#endif
            heapCheckpoint("after_profile_deinit");active.store(Mode::Off);streaming.store(false);epoch.fetch_add(1);enter(Stage::Off);
        }
        else if(uint32_t(now-stageAt)>10000)fault("deinit_timeout");
        return;
    }
    if(s!=Stage::Active)return;
#if IBT_NO_PSRAM
    if(current==Mode::Out&&connected()&&txCodecConfigured.load()&&!txPcm){
        if(!allocateTx()){
            failed.store(true);desired.store(Mode::Off);autoDacFallback.store(true);
            Serial.println("[BT-PAYLOAD] TX=DENIED reserve_retained=10000 teardown=QUEUED");return;
        }
        Serial.println("[BT-PAYLOAD] TX=READY capacity=65536 prefill_ms=500");
    }
#endif
    if(connected()&&!heapConnectedChecked){heapConnectedChecked=true;heapCheckpoint("after_connection");}
    if(current==Mode::Out&&config.getMode()==PM_WEB){
        if(!routeFallbackLogged){
            Serial.println("[BT-ROUTE] result=DENIED active=BT_OUT source=WEB fallback=DAC");
            routeFallbackLogged=true;
        }
        autoDacFallback.store(true);return;
    }
    routeFallbackLogged=false;
    if(current==Mode::Out&&connected()&&txCodecConfigured.load()&&config.getMode()==PM_WEB&&txRate.load()!=32000){
        Serial.println("[BT-RATE-GATE] result=FAIL expected=32000 fallback=DAC");
        txCodecConfigured.store(false);flush();codecUnsupported.store(true);failed.store(true);autoDacFallback.store(true);return;
    }
    // The CONNECTED callback is the admission event. A quick peer disconnect
    // before the next service tick must not erase this successful connection.
    if(current==Mode::Out&&savePeerPending.exchange(false)){
        uint8_t address[6];SavedHistory next{};
        portENTER_CRITICAL(&peerMux);memcpy(address,peer,6);next=savedHistory;portEXIT_CRITICAL(&peerMux);
        const SavedHistory before=next;
        if(validAddress(address)){
            Device entry{};memcpy(entry.address,address,6);
            portENTER_CRITICAL(&devicesMux);
            for(unsigned i=0;i<devicesUsed;++i)if(!memcmp(devices[i].address,address,6)){
                memcpy(entry.name,devices[i].name,sizeof(entry.name));break;
            }
            portEXIT_CRITICAL(&devicesMux);
            unsigned old=0;while(old<next.count&&memcmp(next.entries[old].address,address,6))++old;
            if(placeholderName(entry.name)&&old<next.count)memcpy(entry.name,next.entries[old].name,sizeof(entry.name));
            fallbackName(entry);
            unsigned count=std::min<unsigned>(next.count+unsigned(old==next.count),savedHistoryCapacity);
            for(unsigned i=count-1;i>0;--i){unsigned from=i<=old?i-1:i;next.entries[i]=next.entries[from];}
            next.entries[0]=entry;next.count=count;next.magic=savedHistoryMagic;
            bool ok=true;
            if(memcmp(&before,&next,sizeof(next))){
                Preferences p;ok=p.begin("ibt-bt-out",false);
                if(ok){ok=p.putBytes("history",&next,sizeof(next))==sizeof(next);p.end();}
            }
            if(ok){portENTER_CRITICAL(&peerMux);savedHistory=next;memcpy(savedPeer,address,6);portEXIT_CRITICAL(&peerMux);savedValid.store(true);}
            Serial.printf("[BT-LINK] saved_peer=%s history_count=%u\n",ok?"PASS":"FAIL",unsigned(next.count));
        }
    }
    if(failed.load()&&!reconnectRequested.load()&&!restartScanRequested.load()){
        if(current!=Mode::Out||!retryRequested.exchange(false))return;
        failed.store(false);connectionRejected.store(false);connectAttempts=0;connectSent=false;retryAt=now;
        connectedAt.store(0);streamAt.store(0);connectTimingLogged=false;streamTimingLogged=false;
        Serial.printf("[BT-LINK] manual_retry=1\n");
    }
    if(current==Mode::In){
        if(!advertised){
            if(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,ESP_BT_GENERAL_DISCOVERABLE)!=ESP_OK){fault("scan_enable");return;}
            advertised=true;
        }
        return; // Waiting for the phone is not a failure; no scan/reconnect loop.
    }
    if(reconnectRequested.load()){
        // Publish cleared old failure before dropping the pending-request flag.
        if(savedValid.load()&&connection.load()==ESP_A2D_CONNECTION_STATE_DISCONNECTED){
            portENTER_CRITICAL(&peerMux);memcpy(selectedPeer,requestedSavedPeer,6);memcpy(peer,requestedSavedPeer,6);portEXIT_CRITICAL(&peerMux);
            selectedAt.store(now);scanStoppedAt.store(now);found.store(true);
            attempt=true;discovering.store(false);cancelSent.store(false);
            connectSent=false;connectionRejected.store(false);failed.store(false);connectAttempts=0;retryAt=now;
            scanTimingLogged=false;connectTimingLogged=false;streamTimingLogged=false;
            Serial.printf("[BT-LINK] reconnect_saved=REQUESTED scan_skipped=1 history_count=%u\n",savedDeviceCount());
        }
        reconnectRequested.store(false);
    }
    if(restartScanRequested.exchange(false)){
        if(!discovering.load()&&deviceCount()==0&&connection.load()==ESP_A2D_CONNECTION_STATE_DISCONNECTED){
            attempt=false;found.store(false);cancelSent.store(false);connectSent=false;
            connectionRejected.store(false);failed.store(false);connectAttempts=0;retryAt=0;
            Serial.println("[BT-LINK] empty_scan_restart=REQUESTED");
        }
    }
    if(found.load()&&!streaming.load()&&selectedAt.load()&&uint32_t(now-selectedAt.load())>=90000){
        failed.store(true);autoDacFallback.store(true);
        Serial.printf("[BT-LINK] auto_dac=CONNECT_TIMEOUT attempts=%u elapsed_ms=%lu\n",unsigned(connectAttempts),(unsigned long)(now-selectedAt.load()));
        return;
    }
    if(streaming.load())linkAt=now;
    if(!attempt){
        discoveryStats.reset();discoverySummary.store(false);
        attempt=true;discovering.store(true);
        const esp_err_t error=esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY,10,0);
        discoveryStats.startError.store(int(error));
        if(error!=ESP_OK){discovering.store(false);failed.store(true);discoverySummary.store(true);}
        printDiscoveryStats("REQUESTED");
    }
    if(found.load()&&discovering.load()&&!cancelSent.load()&&esp_bt_gap_cancel_discovery()==ESP_OK)cancelSent.store(true);
    if(found.load()&&!discovering.load()&&!scanTimingLogged){
        scanTimingLogged=true;
        Serial.printf("[BT-LINK] selected_to_scan_stop_ms=%lu\n",(unsigned long)(scanStoppedAt.load()-selectedAt.load()));
    }
    if(connectionRejected.exchange(false)&&connectSent){
        if(connectAttempts>=9){
            failed.store(true);
            autoDacFallback.store(true);
            Serial.printf("[BT-LINK] failed_after_attempts=%u\n",unsigned(connectAttempts));
            return;
        }
        connectSent=false;
        retryAt=now+uint32_t(connectAttempts)*1500;
        Serial.printf("[BT-LINK] connect_failed_attempt=%u retry_in_ms=%lu\n",unsigned(connectAttempts),(unsigned long)(retryAt-now));
    }
    if(found.load()&&!discovering.load()&&!connectSent&&int32_t(now-retryAt)>=0){
        esp_bd_addr_t address;portENTER_CRITICAL(&peerMux);memcpy(address,selectedPeer,6);portEXIT_CRITICAL(&peerMux);
        connectSent=true;++connectAttempts;linkAt=now;connection.store(ESP_A2D_CONNECTION_STATE_CONNECTING);
        Serial.printf("[BT-LINK] connect_attempt=%u connect_request_ms=%lu\n",unsigned(connectAttempts),(unsigned long)(now-selectedAt.load()));
        const esp_err_t error=esp_a2d_source_connect(address);
        Serial.printf("[BT-CONNECT-DIAG] phase=CONNECT_SUBMIT esp_err=%d\n",int(error));
        if(error!=ESP_OK){connection.store(ESP_A2D_CONNECTION_STATE_DISCONNECTED);connectionRejected.store(true);}
    }
    if(connectedAt.load()&&!connectTimingLogged){
        connectTimingLogged=true;
        Serial.printf("[BT-LINK] selected_to_connected_ms=%lu\n",(unsigned long)(connectedAt.load()-selectedAt.load()));
    }
    if(streamAt.load()&&!streamTimingLogged){
        streamTimingLogged=true;
        Serial.printf("[BT-LINK] selected_to_audio_ms=%lu\n",(unsigned long)(streamAt.load()-selectedAt.load()));
    }
    unsigned buffered=0;
    portENTER_CRITICAL(&txMux);buffered=txUsed;portEXIT_CRITICAL(&txMux);
    const bool primed=producerReady()&&buffered>=std::min<unsigned>(txFrames-1,unsigned(txRate.load()/2));
    const auto action=mediaGate.poll(now,connected(),primed,streaming.load(),epoch.load(),
                                     mediaCheckAck.exchange(0),mediaStartFailed.exchange(false));
    if(action==ibtaudio::MediaStartGate::Check||action==ibtaudio::MediaStartGate::Start){
        const bool start=action==ibtaudio::MediaStartGate::Start;
        const auto result=esp_a2d_media_ctrl(start?ESP_A2D_MEDIA_CTRL_START:ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
        Serial.printf("[BT-MEDIA-GATE] action=%s pcm_frames=%u result=%d\n",start?"START":"CHECK",buffered,int(result));
        if(result!=ESP_OK){mediaGate.state=ibtaudio::MediaStartGate::Failed;failed.store(true);autoDacFallback.store(true);}
    }else if(action==ibtaudio::MediaStartGate::Fail){
        failed.store(true);autoDacFallback.store(true);
        Serial.println("[BT-MEDIA-GATE] failed=ACK_OR_START_TIMEOUT fallback=DAC");
    }
}
const char* status(){
    if(stage.load()==Stage::Fault)return "BT valtasi hiba: ujrainditas kell";
    if(stage.load()==Stage::Disconnect||stage.load()==Stage::Deinit||stage.load()==Stage::AvrcDeinit)return "BT atvaltas...";
    if(!wanted())return "BT KI / DAC";
    if(failed.load())return codecUnsupported.load()?"BT HIBA: nincs kozos hangformatum":"BT HIBA: csatlakozas (9 proba)";
    if(stage.load()!=Stage::Active||mode()!=active.load())return "BT indul...";
    if(inputSelected())return streaming.load()?"BT be: telefon -> DAC":"Parositas: I-BT-Radio-Audio";
    return streaming.load()?"BT ki: lejatszas":connection.load()==ESP_A2D_CONNECTION_STATE_CONNECTED?"BT ki: csatlakozva":found.load()?"Csatlakozas...":discovering.load()?"Kereses...":"Valassz eszkozt";
}
void diagnostics(){
    printDiscoveryStats("STATUS");
#if IBT_NO_PSRAM
    Serial.printf("[BT-LIFECYCLE] sdk_initialized=%u controller=%u host=%u payload_bytes=%u pcm_worker=%u release=REVERSIBLE_SDK_DEINIT static_memory=RETAINED\n",
      unsigned(initialized),unsigned(esp_bt_controller_get_status()),unsigned(esp_bluedroid_get_status()),
      unsigned(liteBtBytes),unsigned(pcmWorkerReady.load()));
#endif
    PayloadUse payload;auto diagnosticRx=payload?rxRing.load():nullptr;
    Serial.printf("[BT-VU] enabled=%u reasons=%u cpu_ceiling=80 ram_floor=%lu largest_floor=8192 rx_floor=16384 observer=CONSUMER_ONLY shared_pcm_index=%lu\n",
        unsigned(btinputvu::enabled.load()),btinputvu::lastReasons.load(),(unsigned long)ibtlimit::Policy::badInternal,(unsigned long)vuprobe::dacPcmWrite.load());
    Serial.printf("[BT-UNDERRUN-CLASSIFY] scope=LIFETIME generation=%lu codec=%lu playing=%u boundary_window=%lu steady=%lu grace_ms=1000 native_unity_frames=%lu classification=TIME_WINDOW_NOT_CAUSAL_PROOF\n",(unsigned long)sourceGeneration.load(),(unsigned long)sourceCodec.load(),unsigned(sourcePlaying.load()),(unsigned long)boundaryUnderruns.load(),(unsigned long)steadyUnderruns.load(),(unsigned long)nativeUnityFrames.load());
    Serial.printf("[BT-TX-TIMING] callbacks=%lu callback_max_wall_us=%lu callback_gap_max_us=%lu prefill_silence_bytes=%lu underrun_silence_bytes=%lu scope=LIFETIME gap_includes_stops=1 timing_includes_preemption=1\n",
        (unsigned long)txCallbacks.load(),(unsigned long)txCallbackMaxUs.load(),(unsigned long)txCallbackGapMaxUs.load(),
        (unsigned long)txPrefillBytes.load(),(unsigned long)txShortageBytes.load());
    if(pcmJobs&&pcmWorkerReady.load())Serial.printf("[PCM-WAKE] ready=1 tx_waits=%lu tx_signals=%lu tx_timeouts=%lu queue_waits=%lu queue_signals=%lu queue_timeouts=%lu state_bytes=%u timeout_ms=100 method=CONSUMER_BINARY_SIGNAL\n",
        (unsigned long)pcmJobs->txSpace.waits.load(),(unsigned long)pcmJobs->txSpace.signals.load(),(unsigned long)pcmJobs->txSpace.timeouts.load(),
        (unsigned long)pcmJobs->queueSpace.waits.load(),(unsigned long)pcmJobs->queueSpace.signals.load(),(unsigned long)pcmJobs->queueSpace.timeouts.load(),unsigned(sizeof(PcmPipeline)));
    else Serial.println("[PCM-WAKE] ready=0 method=INLINE_POLL_FALLBACK");
    Serial.printf("[BT-L2CAP-GUARD] callbacks=%lu detail_logs=0 scope=RECURSION_GUARD other_errors=ON\n",(unsigned long)ibt_l2cap_guard_count());
    Serial.printf("[PCM-STAGE] ready=%u requested=%u staged_jobs=%lu direct_jobs=%lu job_bytes=%u stack_min=%u default=OFF memory=EXISTING_INTERNAL_STACK\n",
      unsigned(pcmWorkerReady.load()),unsigned(pcmJobs&&pcmJobs->staging.load()),
      (unsigned long)(pcmJobs?pcmJobs->stagedJobs.load():0),(unsigned long)(pcmJobs?pcmJobs->directJobs.load():0),
      unsigned(sizeof(PcmWorkQueue::Job)),pcmWorkerHandle?unsigned(uxTaskGetStackHighWaterMark(pcmWorkerHandle)):0);

    Serial.printf("[BT-PCM] worker=%u core=0 task_id=%u queue_blocks=%u queue_capacity=32 queue_peak=%lu queued_frames=%lu processed_frames=%lu cancelled_frames=%lu lost_frames=%lu waits=%lu work_wall_max_us=%lu stack_min=%u\n",
        unsigned(pcmWorkerReady.load()),pcmTaskId(),pcmJobs?pcmJobs->size():0,
        (unsigned long)((pcmJobs&&pcmWorkerReady.load())?pcmJobs->queuePeak.load():0),(unsigned long)pcmQueuedFrames.load(),(unsigned long)pcmProcessedFrames.load(),
        (unsigned long)pcmCancelledFrames.load(),(unsigned long)pcmQueueLostFrames.load(),(unsigned long)((pcmJobs&&pcmWorkerReady.load())?pcmJobs->queueWaits.load():0),
        (unsigned long)((pcmJobs&&pcmWorkerReady.load())?pcmJobs->workWallMaxUs.load():0),pcmWorkerHandle?unsigned(uxTaskGetStackHighWaterMark(pcmWorkerHandle)):0);
    Serial.printf("[BT-CORE] btc_target_core=%u pcm_callback_core=%d pcm_task_id=%u decoder_core=1 controller_core=%u btu_target_core=%u\n",
        unsigned(CONFIG_BT_BLUEDROID_PINNED_TO_CORE),txCallbackCore.load(),txCallbackTask.load(),unsigned(CONFIG_BTDM_CTRL_PINNED_TO_CORE),unsigned(CONFIG_BT_BLUEDROID_PINNED_TO_CORE));
    portENTER_CRITICAL(&txMux);const unsigned buffered=txUsed*4;portEXIT_CRITICAL(&txMux);
    Serial.printf("[BT-AUDIO] direction=%s active=%u stage=%u state=%s delivered_bytes=%lu requested_bytes=%lu received_bytes=%lu dropped_bytes=%lu underruns=%lu prefill_callbacks=%lu rebuffer_events=%lu tx_buffer=%lu rx_buffer=%lu rate=%lu source_rate=%lu codec_ok=%u block_frames=%lu tx_internal=%u tx_capacity=%u tx_prefill=%u tx_prefill_ms=500 target_rate=%lu eq_enabled=0\n",inputSelected()?"IN":outputSelected()?"OUT":"OFF",unsigned(active.load()),unsigned(stage.load()),status(),(unsigned long)delivered.load(),(unsigned long)requestedBytes.load(),(unsigned long)received.load(),(unsigned long)dropped.load(),(unsigned long)underruns.load(),(unsigned long)prefillCallbacks.load(),(unsigned long)rebufferEvents.load(),(unsigned long)buffered,(unsigned long)(diagnosticRx?diagnosticRx->size():0),(unsigned long)(inputSelected()?rxRate.load():txRate.load()),(unsigned long)sourceRate.load(),unsigned(txCodecConfigured.load()),(unsigned long)blockFrames.load(),unsigned(IBT_NO_PSRAM!=0),txFrames*4,(txRate.load()/2)*4,(unsigned long)outputRate());
}
}

extern "C" unsigned ibt_bt_output_rate_hz(void){return btaudio::outputRate();}


// Internal-encoder IDF path ignores the external-codec SEP registration API.
// The version-locked SDK hook calls this only while initializing/resetting codec state.
extern "C" uint32_t ibt_sbc_output_rate(void){
    if(btaudio::mode()!=btaudio::Mode::Out)return 0;
    return config.getMode()==PM_WEB?32000:btaudio::outputRate();
}
