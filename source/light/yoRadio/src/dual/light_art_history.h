#pragma once
#include <stdint.h>
#include <stddef.h>
namespace lightart {
inline uint64_t key(const char* s){uint64_t h=14695981039346656037ull;while(s&&*s){h^=uint8_t(*s++);h*=1099511628211ull;}return h;}
struct History {
 uint64_t ids[4]{};
 bool contains(uint64_t id)const{if(!id)return false;for(auto v:ids)if(v==id)return true;return false;}
 bool heard(uint64_t id){
  if(!id||ids[0]==id)return false;
  unsigned at=3;for(unsigned i=1;i<4;++i)if(ids[i]==id){at=i;break;}
  for(unsigned i=at;i;--i)ids[i]=ids[i-1];ids[0]=id;return true;
 }
};
// Count advancing decoded-media seconds, never selection/scrolling or wall time
// spent buffering. Stop, source/generation change, counter rewind and a stalled
// progress clock all restart the continuous 60-second qualification window.
struct Listening {
 uint64_t station=0;uint32_t generation=0,lastElapsed=0,lastProgress=0,started=0,seconds=0;
 bool seen=false,qualified=false;
 bool observe(uint64_t id,uint32_t g,uint32_t elapsed,bool playing,uint32_t now){
  if(!playing||!id){*this={};return false;}
  if(!seen||station!=id||generation!=g||elapsed<lastElapsed||uint32_t(now-lastProgress)>2500){
   station=id;generation=g;lastElapsed=elapsed;lastProgress=now;started=now;seconds=0;seen=true;qualified=false;return false;
  }
  if(elapsed==lastElapsed)return false;
  const uint32_t advance=elapsed-lastElapsed;lastElapsed=elapsed;lastProgress=now;
  if(advance>2){seconds=0;qualified=false;return false;}
  if(qualified)return false;
  seconds+=advance;if(seconds<60||uint32_t(now-started)<60000)return false;qualified=true;return true;
 }
};
inline uint32_t checksum(const uint8_t* p,size_t n){uint32_t h=2166136261u;while(n--){h^=*p++;h*=16777619u;}return h;}
}
