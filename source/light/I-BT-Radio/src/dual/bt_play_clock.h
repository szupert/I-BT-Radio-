#pragma once
#include <stdint.h>
namespace btplayclock {
inline uint32_t trackKey(const char* title,const char* artist){
 uint32_t h=2166136261u;
 for(const char* p=title;*p;++p)h=(h^uint8_t(*p))*16777619u;
 h=(h^0xffu)*16777619u;
 for(const char* p=artist;*p;++p)h=(h^uint8_t(*p))*16777619u;
 return h;
}
// UI-only approximate elapsed playback, never the source's AVRCP position.
struct Clock {
 uint32_t previous=0,generation=0,seconds=0,remainder=0;
 bool connected=false,playing=false;
 uint32_t tick(uint32_t now,bool link,bool run,uint32_t gen){
  if(!link||!connected||generation!=gen){seconds=0;remainder=0;}
  else if(playing&&run){uint32_t delta=uint32_t(now-previous);seconds+=delta/1000;remainder+=delta%1000;seconds+=remainder/1000;remainder%=1000;}
  previous=now;generation=gen;connected=link;playing=link&&run;return seconds;
 }
};
}
