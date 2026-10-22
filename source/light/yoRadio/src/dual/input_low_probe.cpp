#include "input_low_probe.h"
#include <Arduino.h>
#include <atomic>
namespace inputlow {
static std::atomic<uint32_t> entries{0},exits{0},maxMs{0},lastMs{0},minimum{UINT32_MAX},unsafe{0},lastGeneration{0};
static bool wasLow=false;
static uint32_t began=0;
void sample(uint32_t now,bool low,uint32_t input,uint32_t generation,bool pcmSafe){
 if(low&&!wasLow){began=now;entries.fetch_add(1,std::memory_order_relaxed);lastGeneration.store(generation,std::memory_order_relaxed);if(!pcmSafe)unsafe.fetch_add(1,std::memory_order_relaxed);}
 if(low){uint32_t old=minimum.load(std::memory_order_relaxed);if(input<old)minimum.store(input,std::memory_order_relaxed);}
 if(wasLow&&!low){uint32_t elapsed=now-began;exits.fetch_add(1,std::memory_order_relaxed);lastMs.store(elapsed,std::memory_order_relaxed);if(elapsed>maxMs.load(std::memory_order_relaxed))maxMs.store(elapsed,std::memory_order_relaxed);}
 wasLow=low;
}
void telemetry(){Serial.printf("[INPUT-LOW] threshold=1600 sampling_ms=500 entries=%lu exits=%lu max_sampled_ms=%lu last_sampled_ms=%lu min_sampled_input=%lu pcm_guard_unsafe_entries=%lu generation=%lu not_pcm_underrun_counter=1\n",(unsigned long)entries.load(),(unsigned long)exits.load(),(unsigned long)maxMs.load(),(unsigned long)lastMs.load(),(unsigned long)minimum.load(),(unsigned long)unsafe.load(),(unsigned long)lastGeneration.load());}
}
