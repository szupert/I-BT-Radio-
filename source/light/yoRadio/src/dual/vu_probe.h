#pragma once
#include <atomic>
#include <stdint.h>
namespace vuprobe {
inline constexpr uint16_t refreshOptions[]={100,200,250,500,1000};
inline bool validRefresh(unsigned ms){for(auto v:refreshOptions)if(v==ms)return true;return false;}
inline std::atomic<uint16_t> refreshMs{500};
inline void setRefresh(unsigned ms){refreshMs.store(validRefresh(ms)?ms:500,std::memory_order_relaxed);}

// -1: original setting; 0: off; 1: legacy; 2: sparse. RAM only.
inline std::atomic<int> mode{-1};
inline std::atomic<bool> dacEnabled{false};
inline std::atomic<bool> localBtEnabled{false}; // only selected KAJON SD-BT; no callback work
inline std::atomic<uint8_t> dacLeft{0},dacRight{0};
// Single DAC producer; UI reads an atomic snapshot without blocking audio.
#if !IBT_NO_PSRAM
inline std::atomic<int16_t> dacPcm[128]{};
#endif
inline std::atomic<uint32_t> dacPcmWrite{0};
struct Meter {
 unsigned divider=0; uint32_t stamp=0; unsigned peakL=0,peakR=0;
 unsigned left=0,right=0;
 bool due(){return (++divider&63u)==0;}
 bool sample(int16_t l,int16_t r,uint32_t now){
  unsigned a=l<0?-int(l):int(l),b=r<0?-int(r):int(r);
  if(a>peakL)peakL=a;if(b>peakR)peakR=b;
  if(uint32_t(now-stamp)<67)return false;
  stamp=now;
  left=left>peakL?(left*3+peakL)/4:peakL;
  right=right>peakR?(right*3+peakR)/4:peakR;
  peakL=peakR=0;return true;
 }
};
// One existing PCM producer samples levels; no FFT, allocation or display work.
inline void observeBlock(const int16_t* pcm,unsigned frames,unsigned channels,uint32_t now){
 if(!pcm||(channels!=1&&channels!=2))return;
 static Meter meter;
 for(unsigned i=0;i<frames;i+=32){
  if(meter.sample(pcm[i*channels],pcm[i*channels+(channels==2?1:0)],now)){
   dacLeft.store(uint8_t(meter.left>32640?255:meter.left>>7),std::memory_order_relaxed);
   dacRight.store(uint8_t(meter.right>32640?255:meter.right>>7),std::memory_order_relaxed);
  }
 }
}
}
