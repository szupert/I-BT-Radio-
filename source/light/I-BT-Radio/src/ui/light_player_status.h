#pragma once
#include <stdint.h>
#include <stdio.h>
namespace lightplayerstatus {
inline const char* codec(unsigned c){return c==16?"SBC":c==1||c==13?"WAV":c==2?"MP3":c==3?"AAC":c==4?"M4A":c==5||c==7?"FLAC":c==8?"OPUS":c==9?"AAC+":nullptr;}
inline void format(char* out,unsigned capacity,const char* route,unsigned c,unsigned kbps,unsigned rate=0,unsigned bits=0){
 if(c==16&&rate&&bits){if(rate%1000)snprintf(out,capacity,"%s  SBC %u.%u kHz %u bit",route,rate/1000,(rate%1000)/100,bits);else snprintf(out,capacity,"%s  SBC %u kHz %u bit",route,rate/1000,bits);return;}
 if(c==16){snprintf(out,capacity,"%s  SBC --",route);return;}
 const char* name=codec(c);if(!name)snprintf(out,capacity,"%s  Formátum: --",route);
 else if(kbps)snprintf(out,capacity,"%s  %s %u kbit/s",route,name,kbps);
 else snprintf(out,capacity,"%s  %s -- kbit/s",route,name);
}
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b){return uint16_t(((uint32_t(r)>>3)<<11)|((uint32_t(g)>>2)<<5)|(uint32_t(b)>>3));}
constexpr uint16_t bufferColor(unsigned percent){
 if(percent<20)return rgb(243,76,88);
 if(percent>=50)return rgb(40,200,173);
 const unsigned n=percent-20;return rgb(248-(183*n)/30,180+(24*n)/30,45+(102*n)/30);
}
}
