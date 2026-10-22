#include "deferred_audio.h"
#include "icy_fields.h"
#include "deferred_policy.h"
#include "../audioI2S/AudioEx.h"
#include "../core/player.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <freertos/queue.h>
namespace audiod {
struct Event {uint32_t epoch,generation;Kind kind;char text[640];};
static QueueHandle_t queue=nullptr;
static std::atomic<uint32_t> epoch{0},dropped{0},stale{0},processed{0},wrongCore{0};
static uint32_t titleEpoch=~0u;static char* lastTitle=nullptr;
void begin(){
 if(queue)return;
 auto storage=(uint8_t*)heap_caps_malloc(12*sizeof(Event),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 auto control=(StaticQueue_t*)heap_caps_malloc(sizeof(StaticQueue_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
 lastTitle=(char*)heap_caps_calloc(640,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(storage&&control&&lastTitle)queue=xQueueCreateStatic(12,sizeof(Event),storage,control);
 if(!queue){heap_caps_free(storage);heap_caps_free(control);heap_caps_free(lastTitle);lastTitle=nullptr;}
 Serial.printf("[AUDIO-DEFER] core=0 psram_queue=%u bytes=%u\n",unsigned(queue!=nullptr),unsigned(12*sizeof(Event)));
}
void newStream(){epoch.fetch_add(1,std::memory_order_release);}
bool defer(Kind kind,const char* text){
 if(xPortGetCoreID()!=1)return false;
 Event e{};e.epoch=epoch.load(std::memory_order_acquire);e.generation=player.playbackGeneration();e.kind=kind;
 const size_t n=text?strnlen(text,sizeof(e.text)):0;
 if(n==sizeof(e.text)||!queue){dropped.fetch_add(1);return true;}
 if(n)memcpy(e.text,text,n);e.text[n]=0;
 if(xQueueSend(queue,&e,0)!=pdTRUE)dropped.fetch_add(1);
 return true;
}
void service(){
 if(xPortGetCoreID()!=0){wrongCore.fetch_add(1);return;}
 if(!queue)return;
 Event e;unsigned budget=3;
 while(budget--&&xQueueReceive(queue,&e,0)==pdTRUE){
  if(!current(e.epoch,e.generation,epoch.load(std::memory_order_acquire),player.playbackGeneration())){stale.fetch_add(1);continue;}
  if(e.kind==Kind::Icy){
   if(titleEpoch!=e.epoch){titleEpoch=e.epoch;lastTitle[0]=0;}
   char value[640];
   if(icyfields::get(e.text,"StreamTitle=",value,sizeof(value))&&strcmp(value,lastTitle)){
    memcpy(lastTitle,value,strlen(value)+1);
    if(audio_showstreamtitle)audio_showstreamtitle(value);
   }
   if(icyfields::get(e.text,"StreamUrl=",value,sizeof(value))&&audio_info)audio_info(value);
   if(icyfields::get(e.text,"durationMilliseconds=",value,sizeof(value))&&audio_commercial)audio_commercial(value);
  }else switch(e.kind){
   case Kind::Info:if(audio_info)audio_info(e.text);break;
   case Kind::Bitrate:if(audio_bitrate)audio_bitrate(e.text);break;
   case Kind::Title:if(audio_showstreamtitle)audio_showstreamtitle(e.text);break;
   case Kind::Station:if(audio_showstation)audio_showstation(e.text);break;
   case Kind::Artist:if(audio_id3artist)audio_id3artist(e.text);break;
   case Kind::Album:if(audio_id3album)audio_id3album(e.text);break;
   case Kind::Id3Title:if(audio_id3title)audio_id3title(e.text);break;
   case Kind::Id3Data:if(audio_id3data)audio_id3data(e.text);break;
   default:break;
  }
  processed.fetch_add(1);
 }
}
void diagnostics(){Serial.printf("[AUDIO-DEFER] core=0 processed=%lu dropped=%lu stale=%lu wrong_core=%lu pending=%u\n",(unsigned long)processed.load(),(unsigned long)dropped.load(),(unsigned long)stale.load(),(unsigned long)wrongCore.load(),unsigned(queue?uxQueueMessagesWaiting(queue):0));}
}
