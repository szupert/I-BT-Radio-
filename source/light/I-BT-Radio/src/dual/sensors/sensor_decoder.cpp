#include "sensor_decoder.h"
#include "passive_frame.h"
#include "mibeacon.h"
#include <mbedtls/ccm.h>
#if !IBT_BLE_NATIVE_ONLY
#include "decoder.h"
#endif
#include <cstring>
#include <cstdio>
#include <cmath>
namespace radiosensor {
Result decodeMiBeacon(const Config& c,const Frame& f,uint32_t now,Reading& output){
 if(!c.enabled)return Result::Disabled;
 if(!c.selected)return Result::NoSelection;
 if(memcmp(c.address,f.address,6))return Result::WrongDevice;
 if(!f.service||!f.length||f.length>64)return Result::InvalidFrame;
 if(f.uuid==0xfe95&&miPresence(f.service,f.length,f.address))return Result::AdvertisementOnly;
 if(f.uuid==0xfe95&&encrypted(f.uuid,f.service,f.length)){
  if(!c.keyValid)return Result::NeedsDecryption;
  MiLayout layout;if(!miLayout(f.service,f.length,f.address,layout))return Result::InvalidFrame;
  uint8_t plain[64]={},aad=0x11;mbedtls_ccm_context ctx;mbedtls_ccm_init(&ctx);
  int error=mbedtls_ccm_setkey(&ctx,MBEDTLS_CIPHER_ID_AES,c.bindkey,128);
  if(!error)error=mbedtls_ccm_auth_decrypt(&ctx,layout.length,layout.nonce,12,&aad,1,f.service+layout.offset,plain,f.service+f.length-4,4);
  mbedtls_ccm_free(&ctx);
  if(error)return Result::AuthenticationFailed;
  return miObjects(plain,layout.length,now,output)?Result::Decoded:Result::InvalidReading;
 }
 return Result::Unknown;
}
Result decode(const Config& c,const Frame& f,uint32_t now,Reading& output){
 if(!c.enabled)return Result::Disabled;
 if(!c.selected)return Result::NoSelection;
 if(memcmp(c.address,f.address,6))return Result::WrongDevice;
 if(!f.service||!f.length||f.length>64)return Result::InvalidFrame;
 if(f.uuid==0xfe95&&encrypted(f.uuid,f.service,f.length))return decodeMiBeacon(c,f,now,output);
 // Other encrypted protocols are not silently accepted.
 if(f.encrypted||encrypted(f.uuid,f.service,f.length))return Result::NeedsDecryption;
#if IBT_BLE_NATIVE_ONLY
 // Public BTHome v2 and ATC/PVVX formats, with fixed bounds and no JSON heap.
 bool ht=false,hh=false;int t=0,h=0,battery=-1;const uint8_t* p=f.service;
 if(f.uuid==0x181a&&(f.length==13||f.length==15)){
  ht=hh=true;
  if(f.length==13){t=int16_t((unsigned(p[6])<<8)|p[7])*10;h=int(p[8])*100;battery=p[9];}
  else{t=int16_t(unsigned(p[6])|(unsigned(p[7])<<8));h=unsigned(p[8])|(unsigned(p[9])<<8);battery=p[12];}
 }else if(f.uuid==0xfcd2){
  if((p[0]>>5)!=2)return Result::Unknown;if(p[0]&1)return Result::NeedsDecryption;
  for(size_t i=1;i<f.length;){const unsigned id=p[i++];unsigned n=id==0||id==1||id==0x2e?1:id==2||id==3?2:0;if(!n||i+n>f.length)return Result::InvalidFrame;
   if(id==1)battery=p[i];else if(id==2){ht=true;t=int16_t(unsigned(p[i])|(unsigned(p[i+1])<<8));}else if(id==3){hh=true;h=unsigned(p[i])|(unsigned(p[i+1])<<8);}else if(id==0x2e){hh=true;h=int(p[i])*100;}i+=n;
  }
 }else return decodeMiBeacon(c,f,now,output);
 Reading next=output;if((!ht&&!hh)||!updateReading(next,ht,t,hh,h,battery,now))return Result::InvalidReading;output=next;return Result::Decoded;
#else
 char mac[18],uuid[7],hex[129];
 snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",f.address[0],f.address[1],f.address[2],f.address[3],f.address[4],f.address[5]);
 snprintf(uuid,sizeof(uuid),"0x%04x",f.uuid);
 const char* digits="0123456789abcdef";
 for(size_t i=0;i<f.length;++i){hex[2*i]=digits[f.service[i]>>4];hex[2*i+1]=digits[f.service[i]&15];}hex[2*f.length]=0;
 StaticJsonDocument<1536> doc;JsonObject object=doc.to<JsonObject>();
 object["id"]=mac;object["rssi"]=f.rssi;object["servicedatauuid"]=uuid;object["servicedata"]=hex;
 if(doc.overflowed())return Result::InvalidFrame;
 TheengsDecoder decoder;
 // Theengs returns a zero-based model index; -1 means no match.
 if(decoder.decodeBLEJson(object)<0)return Result::Unknown;
 if(doc.overflowed())return Result::InvalidFrame;
 const bool hasT=!object["tempc"].isNull(),hasH=!object["hum"].isNull();
 if(!hasT&&!hasH)return Result::InvalidReading;
 float t=0,h=0;
 if(hasT){if(!object["tempc"].is<float>())return Result::InvalidReading;t=object["tempc"].as<float>();if(!std::isfinite(t)||t<-50||t>85)return Result::InvalidReading;}
 if(hasH){if(!object["hum"].is<float>())return Result::InvalidReading;h=object["hum"].as<float>();if(!std::isfinite(h)||h<0||h>100)return Result::InvalidReading;}
 // Some MiBeacon devices advertise temperature and humidity separately.
 // The worker resets output on selection/epoch change; never combine devices.
 Reading candidate=output;int battery=-1;
 if(object["batt"].is<float>()){float batt=object["batt"].as<float>();if(batt>=0&&batt<=100)battery=int(lroundf(batt));}
 if(!updateReading(candidate,hasT,int(lroundf(t*100)),hasH,int(lroundf(h*100)),battery,now))return Result::InvalidReading;
 output=candidate;return Result::Decoded;
#endif
}
}
