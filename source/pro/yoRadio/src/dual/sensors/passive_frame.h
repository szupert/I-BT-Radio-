#pragma once
#include <stdint.h>
#include <stddef.h>
namespace radiosensor {
// Own all payload bytes: GAP callback memory is borrowed and expires on return.
struct Packet {uint8_t address[6]={};uint8_t addressType=0;int8_t rssi=0;uint16_t uuid=0;uint8_t length=0;uint8_t data[64]={};uint32_t epoch=0;uint32_t capturedAt=0;bool discovery=false;};
enum class Extract {Found,Absent,Malformed,TooLarge};
constexpr bool supportedService(uint16_t uuid){return uuid==0x181a||uuid==0xfe95||uuid==0xfcd2||uuid==0xfdcd;}
// Validate the ENTIRE AD stream before committing, including trailing elements.
constexpr Extract extract(const uint8_t* adv,size_t length,Packet& output){
 if(!adv||length>62)return Extract::Malformed;
 Packet candidate=output;bool found=false;
 for(size_t pos=0;pos<length;){
  unsigned count=adv[pos++];if(!count)break;
  if(count>length-pos)return Extract::Malformed;
  if(adv[pos]==0x16){
   if(count<3)return Extract::Malformed;
   uint16_t uuid=uint16_t(adv[pos+1])|(uint16_t(adv[pos+2])<<8);
   if(supportedService(uuid)){
    if(found)return Extract::Malformed;
    size_t n=count-3;if(!n)return Extract::Malformed;if(n>sizeof(candidate.data))return Extract::TooLarge;
    candidate.uuid=uuid;candidate.length=uint8_t(n);
    for(size_t i=0;i<n;++i)candidate.data[i]=adv[pos+3+i];found=true;
   }
  }pos+=count;
 }
 if(found){output=candidate;return Extract::Found;}return Extract::Absent;
}
constexpr bool encrypted(uint16_t uuid,const uint8_t* data,size_t n){
 return data&&n&&((uuid==0xfcd2&&(data[0]&1))||(uuid==0xfe95&&(data[0]&8))||(uuid==0x181a&&n==11));
}
constexpr bool samePayload(const Packet&a,const Packet&b){
 if(a.length>64||b.length>64)return false;
 if(a.uuid!=b.uuid||a.length!=b.length||a.addressType!=b.addressType||a.epoch!=b.epoch)return false;
 for(unsigned i=0;i<6;++i)if(a.address[i]!=b.address[i])return false;
 for(unsigned i=0;i<a.length;++i)if(a.data[i]!=b.data[i])return false;return true;
}
}
