#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
namespace pcmaudit {
// First 4096 stereo-normalized frames only: bounded work, no PCM capture buffer.
struct Probe {
 static constexpr uint32_t Limit=4096,Seed=2166136261u;
 std::atomic<uint32_t> active{0},rate{0},channels{0},sourceFrames{0},packedFrames{0},acceptedBytes{0};
 std::atomic<uint32_t> rawHash{Seed},packedHash{Seed},acceptedHash{Seed},rawPeak{0},packedPeak{0};
 static uint32_t byte(uint32_t h,uint8_t b){return (h^b)*16777619u;}
 static uint32_t word(uint32_t h,uint32_t w){for(unsigned n=0;n<4;++n){h=byte(h,uint8_t(w));w>>=8;}return h;}
 static uint32_t peak(int16_t v){return v<0?uint32_t(-int32_t(v)):uint32_t(v);}
 void begin(uint32_t hz,uint32_t ch){active=0;rate=hz;channels=ch;sourceFrames=packedFrames=acceptedBytes=0;rawHash=packedHash=acceptedHash=Seed;rawPeak=packedPeak=0;active=1;}
 void raw(const int16_t*p,size_t frames,unsigned ch){
  if(!active.load(std::memory_order_relaxed))return;
  uint32_t count=sourceFrames.load(),h=rawHash.load(),top=rawPeak.load();
  for(size_t i=0;count<Limit&&i<frames&&i<Limit-count;++i){int16_t l=p[i*ch],r=p[i*ch+ch-1];h=word(h,(uint32_t(uint16_t(l))<<16)|uint16_t(r));uint32_t a=peak(l),b=peak(r);if(a>top)top=a;if(b>top)top=b;}
  rawHash=h;rawPeak=top;sourceFrames=frames>UINT32_MAX-count?UINT32_MAX:count+uint32_t(frames);
 }
 void packed(const uint8_t*p,size_t frames){
  if(!active.load(std::memory_order_relaxed))return;
  uint32_t n=packedFrames.load(),h=packedHash.load(),top=packedPeak.load();
  for(size_t i=0;n<Limit&&i<frames&&i<Limit-n;++i){
   uint32_t w=uint32_t(p[i*4])|(uint32_t(p[i*4+1])<<8)|(uint32_t(p[i*4+2])<<16)|(uint32_t(p[i*4+3])<<24);
   h=word(h,w);uint32_t a=peak(int16_t(w>>16)),b=peak(int16_t(w));if(a>top)top=a;if(b>top)top=b;
  }
  packedHash=h;packedPeak=top;packedFrames=frames>UINT32_MAX-n?UINT32_MAX:n+uint32_t(frames);
 }
 void accepted(const uint8_t*p,size_t bytes){
  if(!active.load(std::memory_order_relaxed))return;
  uint32_t count=acceptedBytes.load(),h=acceptedHash.load();for(size_t i=0;count<Limit*4&&i<bytes&&i<Limit*4-count;++i)h=byte(h,p[i]);acceptedHash=h;acceptedBytes=bytes>UINT32_MAX-count?UINT32_MAX:count+uint32_t(bytes);
 }
};
}
