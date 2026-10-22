#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "service_pcm.h"
namespace bootsound {
constexpr bool eligible(unsigned mode,bool wifi,bool ready,bool offline,bool clock,bool bt){return mode>0&&mode<3&&wifi&&ready&&!offline&&!clock&&!bt;}
struct Wav {uint32_t rate=48000,offset=0,bytes=0;uint16_t channels=2;};
constexpr uint16_t le16(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
constexpr uint32_t le32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
// Strict bounded PCM WAV, <=3 seconds. Read/seek is supplied by the audio owner.
template<class Read> bool wav(Read read,uint32_t length,Wav& out){
 uint8_t h[24];if(length<44||length>1048576||!read(0,h,12)||memcmp(h,"RIFF",4)||memcmp(h+8,"WAVE",4))return false;
 const uint32_t riff=le32(h+4);if(riff<36||uint64_t(riff)+8>length)return false;
 const uint32_t end=riff+8;uint32_t at=12;bool format=false;Wav value;
 for(unsigned chunk=0;chunk<32&&at<=end-8;++chunk){
  if(!read(at,h,8))return false;const uint32_t n=le32(h+4),body=at+8;
  if(uint64_t(body)+n>end)return false;
  if(!memcmp(h,"fmt ",4)){
   if(n<16||!read(body,h,16))return false;value.channels=le16(h+2);value.rate=le32(h+4);
   if(le16(h)!=1||le16(h+14)!=16||(value.channels!=1&&value.channels!=2)||
      (value.rate!=32000&&value.rate!=44100&&value.rate!=48000)||le16(h+12)!=value.channels*2||le32(h+8)!=value.rate*value.channels*2)return false;
   format=true;
  }else if(!memcmp(h,"data",4)){
   if(!format||!n||n%(value.channels*2)||n>value.rate*value.channels*2*3)return false;
   value.offset=body;value.bytes=n;out=value;return true;
  }
  const uint64_t next=uint64_t(body)+n+(n&1);if(next>end)return false;at=uint32_t(next);
 }
 return false;
}
constexpr unsigned duration=1200;
// Original, quiet startup motif. Not a bundled Microsoft audio recording.
constexpr int16_t sample(uint32_t frame,unsigned theme){
 constexpr uint32_t rate=48000;const uint32_t ms=frame*1000/rate;int32_t sum=0;
 constexpr unsigned xp[]={659,831,988,1319},seven[]={392,587,740,988};
 for(unsigned voice=0;voice<4;++voice){const unsigned begin=voice*(theme==4?65:140);
  if(ms<begin||ms>=duration)continue;const unsigned age=ms-begin;
  unsigned envelope=age<25?age*1024/25:1024;if(ms>900)envelope=envelope*(duration-ms)/300;
  const uint32_t phase=uint32_t((uint64_t(frame-begin*48)*(theme==4?seven[voice]:xp[voice])<<32)/rate);
  sum+=int32_t(int64_t(serviceui::sine(phase))*envelope/(1024*32));
 }
 return int16_t(sum);
}
}
