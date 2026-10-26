#pragma once
#include <stdint.h>
#include <stdio.h>
namespace audioformat {
// Bounded stack strings only. Lossy codec PCM output depth is not source resolution.
struct Label {char detail[24]={},rate[20]={},line[48]={};};
inline Label label(unsigned codec,unsigned bits,unsigned kbps,uint32_t hz){
 Label r{};const char* name=codec==1?"WAV":codec==2?"MP3":codec==3?"AAC":codec==4?"M4A":codec==5?"FLAC":codec==7?"FLAC":codec==8?"OPUS":codec==9?"AAC+":codec==13?"WAV":nullptr;
 if(!name){snprintf(r.line,sizeof(r.line),"Formátum: --");snprintf(r.detail,sizeof(r.detail),"Formátum: --");return r;}
 if((codec==1||codec==5||codec==7)&&bits)snprintf(r.detail,sizeof(r.detail),"%s %u bit",name,bits);
 else if(kbps)snprintf(r.detail,sizeof(r.detail),"%s %u kbit/s",name,kbps);
 else snprintf(r.detail,sizeof(r.detail),"%s",name);
 if(hz){if(hz%100==0&&hz%1000)snprintf(r.rate,sizeof(r.rate),"%lu,%lu kHz",(unsigned long)(hz/1000),(unsigned long)((hz%1000)/100));
 else if(hz%10==0&&hz%1000)snprintf(r.rate,sizeof(r.rate),"%lu,%02lu kHz",(unsigned long)(hz/1000),(unsigned long)((hz%1000)/10));
 else if(hz%1000)snprintf(r.rate,sizeof(r.rate),"%lu,%03lu kHz",(unsigned long)(hz/1000),(unsigned long)(hz%1000));
 else snprintf(r.rate,sizeof(r.rate),"%lu kHz",(unsigned long)(hz/1000));}
 snprintf(r.line,sizeof(r.line),"%s%s%s",r.detail,hz?" / ":"",r.rate);return r;
}
}
