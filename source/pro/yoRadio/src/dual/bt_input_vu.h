#pragma once
#include <stddef.h>
#include "vu_probe.h"
#include "resource_policy.h"
// Optional observer on the existing single PCM consumer, never a BT callback.
// No allocation, resampling, blocking, I2S write or modification of PCM bytes.
namespace btinputvu {
inline std::atomic<bool> enabled{false};
inline std::atomic<unsigned> lastReasons{255};
// loadprobe::cached supplies permille (0..1000); 80% is 800.
inline bool budget(bool eligible,bool input,bool playing,bool known,int c0Permille,int c1Permille,bool headroom){
 const unsigned why=(!eligible?1u:0u)|(!input?2u:0u)|(!playing?4u:0u)|(!known?8u:0u)|(c0Permille<0||c0Permille>=800?16u:0u)|(c1Permille<0||c1Permille>=800?32u:0u)|(!headroom?64u:0u);
 lastReasons.store(why,std::memory_order_relaxed);return why==0;
}
inline void observe(const uint8_t* pcm,size_t bytes,uint32_t now){
 if(!enabled.load(std::memory_order_relaxed)||!pcm)return;
 static vuprobe::Meter meter;
 uint32_t write=vuprobe::dacPcmWrite.load(std::memory_order_relaxed);
 for(size_t i=0;i+4<=bytes;i+=32){ // Every eighth stereo frame, existing 128-sample UI ring.
  const int16_t l=int16_t(uint16_t(pcm[i])|(uint16_t(pcm[i+1])<<8));
  const int16_t r=int16_t(uint16_t(pcm[i+2])|(uint16_t(pcm[i+3])<<8));
  vuprobe::dacPcm[write++&127u].store(l,std::memory_order_relaxed);
  if(meter.sample(l,r,now)){
   vuprobe::dacLeft.store(uint8_t(meter.left>32640?255:meter.left>>7),std::memory_order_relaxed);
   vuprobe::dacRight.store(uint8_t(meter.right>32640?255:meter.right>>7),std::memory_order_relaxed);
  }
 }
 vuprobe::dacPcmWrite.store(write,std::memory_order_release);
}
}
