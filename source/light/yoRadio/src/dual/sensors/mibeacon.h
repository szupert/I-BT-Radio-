#pragma once
#include "passive_policy.h"
#include <stddef.h>
#include <string.h>
namespace radiosensor {
// Xiaomi MiBeacon v5 LYWSD03MMC only. AES-CCM authentication precedes parsing.
struct MiLayout {size_t offset=0,length=0;uint8_t nonce[12]={};};
constexpr bool miPresence(const uint8_t* raw,size_t n,const uint8_t* mac){
 if(!raw||!mac||n<5||n>64||(raw[1]>>4)!=5||raw[2]!=0x5b||raw[3]!=0x05||(raw[0]&0x40))return false;
 size_t off=5;if(raw[0]&0x10){if(n<11)return false;for(unsigned i=0;i<6;++i)if(raw[5+i]!=mac[5-i])return false;off+=6;}
 if(raw[0]&0x20)++off;return n>=off;
}
constexpr bool miLayout(const uint8_t* raw,size_t n,const uint8_t* mac,MiLayout& out){
 if(!raw||!mac||n<16||n>64||(raw[1]>>4)!=5||(raw[0]&0x48)!=0x48||raw[2]!=0x5b||raw[3]!=0x05||(raw[0]&0x20))return false;
 size_t off=(raw[0]&0x10)?11:5;if(n<=off+7)return false;
 if(off==11)for(unsigned i=0;i<6;++i)if(raw[5+i]!=mac[5-i])return false;
 MiLayout c;c.offset=off;c.length=n-off-7;
 for(unsigned i=0;i<6;++i)c.nonce[i]=mac[5-i];
 for(unsigned i=0;i<3;++i){c.nonce[6+i]=raw[2+i];c.nonce[9+i]=raw[n-7+i];}out=c;return true;
}
constexpr bool miObjects(const uint8_t* p,size_t n,uint32_t now,Reading& out){
 Reading c=out;bool updated=false;
 for(size_t at=0;at<n;){
  if(n-at<3)return false;unsigned type=p[at]|(unsigned(p[at+1])<<8),len=p[at+2];at+=3;if(len>n-at)return false;
  bool ht=false,hh=false;int t=0,h=0,b=-1;
  if(type==0x1004&&len==2){ht=true;t=int16_t(p[at]|(unsigned(p[at+1])<<8))*10;}
  else if(type==0x1006&&len==2){hh=true;h=(p[at]|(unsigned(p[at+1])<<8))*10;}
  else if(type==0x100d&&len==4){ht=hh=true;t=int16_t(p[at]|(unsigned(p[at+1])<<8))*10;h=(p[at+2]|(unsigned(p[at+3])<<8))*10;}
  else if(type==0x100a&&len==1){b=p[at];if(b>100)return false;c.battery=b;}
  if(ht||hh){if(!updateReading(c,ht,t,hh,h,b,now))return false;updated=true;}at+=len;
 }
 if(updated)out=c;return updated;
}
}
