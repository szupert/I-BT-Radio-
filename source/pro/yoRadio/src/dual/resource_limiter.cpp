#include "resource_limiter.h"
#include "artwork_buffer_policy.h"
#include <Arduino.h>
#include <atomic>
#include <new>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
namespace ibtlimit {
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static Policy policy;
// One tick writer (Arduino Core0); diagnostic readers use aligned relaxed loads.
// Allocate once before worker startup. Failure denies optional work; no audio
// buffer is reduced to make room for diagnostic state in the static DRAM region.
struct Cadence {
    std::atomic<unsigned> requested{IBT_LIMITER_BT_SAMPLE_MS}, applied{0};
    std::atomic<uint32_t> ticks{0}, full{0}, cheap{0};
    uint32_t lastFullAt=0;
    unsigned lastRequested=0;
    bool wasBt=false;
    uint32_t buffer=0, audioAt=0, capacity=0;
};
static Cadence* cadence=nullptr;
void begin(){
    if(cadence)return;
    void* memory=heap_caps_malloc(sizeof(Cadence),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(memory)cadence=new(memory)Cadence;
    else Serial.printf("[LIMITER-CADENCE] result=NO_INTERNAL_STATE optional_work=DENIED\n");
}
bool setCadence(unsigned ms){if(!cadence||(ms!=0&&ms!=100))return false;cadence->requested.store(ms,std::memory_order_relaxed);return true;}
void resetCadence(){if(cadence)cadence->requested.store(IBT_LIMITER_BT_SAMPLE_MS,std::memory_order_relaxed);}
void cadenceDiagnostics(){
    if(!cadence){Serial.printf("[LIMITER-CADENCE] result=NO_INTERNAL_STATE optional_work=DENIED\n");return;}
    Serial.printf("[LIMITER-CADENCE] requested_ms=%u applied_ms=%u ticks=%lu full=%lu cheap=%lu reserve=%lu default_ms=%u\n",
        cadence->requested.load(),cadence->applied.load(),(unsigned long)cadence->ticks.load(),
        (unsigned long)cadence->full.load(),(unsigned long)cadence->cheap.load(),
        (unsigned long)Policy::reserve,unsigned(IBT_LIMITER_BT_SAMPLE_MS));
}
static bool active = false, seenAudio = false;
static Sample sample(uint32_t* audioCapacity=nullptr,bool largestFresh=true) {
    Sample s{};
    s.freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    // Omitted on cheap ticks; never reused as a cached health claim.
    s.largestInternal = largestFresh ? heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT) : 0;
    portENTER_CRITICAL(&mux);
    s.audioActive = active; s.buffer = cadence?cadence->buffer:0;
    if(audioCapacity)*audioCapacity=cadence?cadence->capacity:0;
    const bool hadAudio=seenAudio;
    const uint32_t seenAt=cadence?cadence->audioAt:0;
    portEXIT_CRITICAL(&mux);
    // Capture time after the producer timestamp: a concurrent audio update
    // between heap reads must not look like an unsigned 49-day-old heartbeat.
    s.now=millis();
    s.audioFresh=cadence && (!hadAudio || uint32_t(s.now-seenAt)<1500);
    return s;
}
void publishAudio(bool running,uint32_t buffered,uint32_t now,uint32_t totalCapacity) {
    // The audio path never acquires a permit or waits on a background queue.
    portENTER_CRITICAL(&mux);
    active = running;seenAudio = true;
    if(cadence){cadence->buffer=buffered;cadence->capacity=totalCapacity;cadence->audioAt=now;}
    portEXIT_CRITICAL(&mux);
}
void tick(bool btMode) {
    if(!cadence)return;
    // tick has one owner: main.cpp loop. Admission callers do not touch this
    // cadence state and always take their own full sample outside the mux.
    const uint32_t now = millis();
    const unsigned requested=cadence->requested.load(std::memory_order_relaxed);
    // Pace only the background largest-block walk on DAC and BT. Free bytes
    // and audio health stay per-tick; all admission calls take a fresh sample.
    const bool largestFresh = !requested || btMode!=cadence->wasBt ||
        requested!=cadence->lastRequested || uint32_t(now-cadence->lastFullAt)>=requested;
    auto s=sample(nullptr,largestFresh);
    cadence->wasBt=btMode;cadence->lastRequested=requested;
    if(largestFresh)cadence->lastFullAt=s.now;
    cadence->applied.store(requested,std::memory_order_relaxed);
    cadence->ticks.store(cadence->ticks.load(std::memory_order_relaxed)+1,std::memory_order_relaxed);
    auto& count=largestFresh?cadence->full:cadence->cheap;
    count.store(count.load(std::memory_order_relaxed)+1,std::memory_order_relaxed);
    portENTER_CRITICAL(&mux);
    policy.observe(s,largestFresh);
    portEXIT_CRITICAL(&mux);
}
bool acquire(Job j,size_t cost) { auto s=sample(); portENTER_CRITICAL(&mux); bool ok=policy.acquire(j,s,cost); portEXIT_CRITICAL(&mux); return ok; }
void release(Job j) { portENTER_CRITICAL(&mux); policy.release(j); portEXIT_CRITICAL(&mux); }
bool acquireHttp() { auto s=sample(); portENTER_CRITICAL(&mux); bool ok=policy.acquireHttp(s); portEXIT_CRITICAL(&mux); return ok; }
bool acquireControlHttp(){auto s=sample();portENTER_CRITICAL(&mux);bool ok=policy.acquireControlHttp(s);portEXIT_CRITICAL(&mux);return ok;}
void releaseHttp(bool control) { portENTER_CRITICAL(&mux); policy.releaseHttp(control); portEXIT_CRITICAL(&mux); }
bool backgroundBusy() { portENTER_CRITICAL(&mux);bool busy=policy.http||policy.ws||policy.heavy!=Job::None;portEXIT_CRITICAL(&mux);return busy; }
bool acquireWs() { auto s=sample(); portENTER_CRITICAL(&mux); bool ok=policy.acquireWs(s); portEXIT_CRITICAL(&mux); return ok; }
void releaseWs() { portENTER_CRITICAL(&mux); policy.releaseWs(); portEXIT_CRITICAL(&mux); }
bool allowAllocation(size_t cost) { auto s=sample(); portENTER_CRITICAL(&mux); policy.observe(s); bool ok=!policy.pressured&&policy.budget(s,cost); portEXIT_CRITICAL(&mux); return ok; }
static bool artworkReady(bool start) {
    uint32_t c=0;const auto s=sample(&c);
    return s.freeInternal>=Policy::reserve && artworkBufferReady(s.audioActive,s.audioFresh,s.buffer,c,start);
}
bool allowArtworkStart(){return artworkReady(true);}
bool allowArtworkProgress(){return artworkReady(false);}

bool allowRedraw() { return allowAllocation(1024); }
void telemetry() {
    uint32_t c=0;auto s=sample(&c); Policy p;
    portENTER_CRITICAL(&mux); p=policy; portEXIT_CRITICAL(&mux);
    Serial.printf("[HTTP-ADMISSION] denied=%lu active=%u free=%lu largest=%lu required_first=%lu pressure=%u\n",
        (unsigned long)p.deniedHttp,unsigned(p.http),(unsigned long)s.freeInternal,
        (unsigned long)s.largestInternal,(unsigned long)(Policy::reserve+Policy::httpCost),unsigned(p.pressured));
    Serial.printf("[ART-BUFFER] active=%u fresh=%u filled=%lu capacity=%lu start_pct=75 continue_pct=50 start_ok=%u progress_ok=%u free_internal=%lu reserve=%lu\n",
        s.audioActive,s.audioFresh,(unsigned long)s.buffer,(unsigned long)c,
        unsigned(s.freeInternal>=Policy::reserve&&artworkBufferReady(s.audioActive,s.audioFresh,s.buffer,c,true)),
        unsigned(s.freeInternal>=Policy::reserve&&artworkBufferReady(s.audioActive,s.audioFresh,s.buffer,c,false)),
        (unsigned long)s.freeInternal,(unsigned long)Policy::reserve);
    Serial.printf("[R10-LIMITER] version=1 enabled=1 pressure=%u heavy=%u http=%u ws=%u denied=%lu admitted=%lu reserve=%lu audio_fresh=%u free_internal=%lu largest_internal=%lu audio_active=%u audio_buffer=%lu recovery_free=%lu bad_free=%lu\n",p.pressured?1:0,unsigned(p.heavy),p.http,p.ws,(unsigned long)p.denied,(unsigned long)p.admitted,(unsigned long)Policy::reserve,s.audioFresh?1:0,(unsigned long)s.freeInternal,(unsigned long)s.largestInternal,s.audioActive?1:0,(unsigned long)s.buffer,(unsigned long)Policy::recoveryInternal,(unsigned long)Policy::badInternal);
}
}
