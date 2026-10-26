#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace ibtaudio {
constexpr unsigned LightBtRate=32000, LightTxFrames=8192, LightRxBytes=8192;
static_assert(LightRxBytes>=15*128*2*2,"RX must hold the SDK maximum SBC PCM callback");
constexpr unsigned rxPrime(unsigned capacity){return capacity/2;}
constexpr unsigned LightBtPrimeMs=250;
constexpr unsigned btPrimeMs(bool light){return light?LightBtPrimeMs:500u;}
constexpr unsigned btPrimeFrames(unsigned rate,bool light){return rate*btPrimeMs(light)/1000u;}
static_assert(LightTxFrames>=btPrimeFrames(LightBtRate,true),"Retain250ms stereo PCM");
// IRAM pages are accessed exclusively as aligned volatile32-bit stereo words.
// Normal DRAM ingress/egress may be byte aligned; no DMA uses this storage.
struct LightStereoPages {
 static constexpr unsigned PageFrames=1024, Pages=LightTxFrames/PageFrames;
 uint32_t* pages[Pages]{};unsigned execBytes=0;
 bool ready()const{return pages[Pages-1]!=nullptr;}
 template<class Free> void release(Free free){for(auto& p:pages){if(p)free(p);p=nullptr;}execBytes=0;}
 template<class Alloc,class Free> bool allocate(Alloc alloc,Free free){
  if(ready())return true;
  for(unsigned i=0;i<Pages;++i){
   // Every page is accessed as32-bit words; try EXEC for all of them before
   // DRAM fallback, rather than wasting free EXEC after the first four pages.
   void* p=alloc(PageFrames*4,true);
   bool exec=p!=nullptr;
   if(!p)p=alloc(PageFrames*4,false);
   if(!p||(reinterpret_cast<uintptr_t>(p)&3)){if(p)free(p);release(free);return false;}
   pages[i]=static_cast<uint32_t*>(p);if(exec)execBytes+=PageFrames*4;
  }return true;
 }
 void write(unsigned frame,const int16_t* src,unsigned n){
  for(unsigned i=0;i<n;++i){unsigned at=(frame+i)%LightTxFrames;
   volatile uint32_t* p=pages[at/PageFrames]+at%PageFrames;
   *p=uint32_t(uint16_t(src[i*2]))|(uint32_t(uint16_t(src[i*2+1]))<<16);
  }
 }
 void read(unsigned frame,uint8_t* dst,unsigned n)const{
  for(unsigned i=0;i<n;++i){unsigned at=(frame+i)%LightTxFrames;
   const volatile uint32_t* p=pages[at/PageFrames]+at%PageFrames;
   uint32_t word=*p;memcpy(dst+i*4,&word,4);
  }
 }
};
}
