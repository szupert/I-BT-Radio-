#include "stage_cpu.h"
#include "../core/player.h"
#include <Arduino.h>
#include <atomic>
#include <esp_freertos_hooks.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <freertos/task.h>
namespace stagecpu {
// Xtensa libstdc++ does not promise lock-free read/modify/write atomics.
// Never use fetch_add in the tick ISR: each aligned counter has one ISR writer.
// Aligned load/store builtins below are checked in the target disassembly.
static uint32_t enabled=0,current=Other;
static TaskHandle_t owner=nullptr;
static volatile uint32_t samples[6]={};
// Lifetime allocation, internal RAM only: the tick ISR must never access PSRAM.
static volatile uint32_t* decoderSamples=nullptr;
static bool registered=false;
static void IRAM_ATTR tick(){
 if(!__atomic_load_n(&enabled,__ATOMIC_RELAXED))return;
 const auto target=__atomic_load_n(&owner,__ATOMIC_RELAXED);
 const bool isOwner=target&&target==xTaskGetCurrentTaskHandle();
 const unsigned phase=__atomic_load_n(&current,__ATOMIC_RELAXED);
 const bool decoderSub=phase>=Huffman&&phase<=Polyphase;
 const unsigned index=bucket(phase,isOwner);
 samples[index]=samples[index]+1;
 if(isOwner&&(phase==2||decoderSub)){
   const unsigned sub=decoderSub?phase-Huffman+1:0;
   decoderSamples[sub]=decoderSamples[sub]+1;
 }
}
void begin(){
 if(!decoderSamples)decoderSamples=static_cast<uint32_t*>(heap_caps_calloc(7,sizeof(uint32_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
 if(decoderSamples&&!registered)registered=esp_register_freertos_tick_hook_for_cpu(tick,1)==ESP_OK;
}
bool set(bool on){
 if(on){begin();__atomic_store_n(&owner,static_cast<TaskHandle_t>(player.audioTaskHandle()),__ATOMIC_RELAXED);if(!registered||!owner)return false;}
 __atomic_store_n(&enabled,uint32_t(on),__ATOMIC_RELEASE);return true;
}
unsigned enter(unsigned stage){
 if(!__atomic_load_n(&enabled,__ATOMIC_ACQUIRE)||xPortGetCoreID()!=1||xTaskGetCurrentTaskHandle()!=__atomic_load_n(&owner,__ATOMIC_RELAXED))return 0xffffffffu;
 const unsigned previous=__atomic_load_n(&current,__ATOMIC_RELAXED);
 __atomic_store_n(&current,stage,__ATOMIC_RELAXED);return previous;
}
bool decoderActive(){
 return __atomic_load_n(&enabled,__ATOMIC_ACQUIRE)&&xPortGetCoreID()==1&&
   xTaskGetCurrentTaskHandle()==__atomic_load_n(&owner,__ATOMIC_RELAXED)&&
   __atomic_load_n(&current,__ATOMIC_RELAXED)==2;
}
void leave(unsigned previous){if(previous!=0xffffffffu)__atomic_store_n(&current,previous,__ATOMIC_RELAXED);}
void diagnostics(){
 // Read each ISR counter once: parent and child totals must share a snapshot.
 uint32_t d[7]={};
 if(decoderSamples)for(unsigned i=0;i<7;++i)d[i]=decoderSamples[i];
 Serial.printf("[CPU-STAGES] enabled=%u registered=%u core=1 method=TICK_SAMPLE_ESTIMATE tick_hz=%u net_poll=%lu net_read=%lu decode=%lu handoff=%lu audio_other=%lu other_tasks=%lu mp3_other=%lu mp3_huffman=%lu mp3_dequant=%lu mp3_imdct=%lu mp3_synthesis=%lu mp3_fdct=%lu mp3_polyphase=%lu mp3_synthesis_other=%lu\n",
 unsigned(__atomic_load_n(&enabled,__ATOMIC_ACQUIRE)),unsigned(registered),unsigned(configTICK_RATE_HZ),(unsigned long)samples[0],(unsigned long)samples[1],(unsigned long)samples[2],(unsigned long)samples[3],(unsigned long)samples[4],(unsigned long)samples[5],(unsigned long)d[0],(unsigned long)d[1],(unsigned long)d[2],(unsigned long)d[3],(unsigned long)(uint32_t(d[4]+d[5]+d[6])),(unsigned long)d[5],(unsigned long)d[6],(unsigned long)d[4]);
}
}
