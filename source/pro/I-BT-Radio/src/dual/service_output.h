#pragma once
#include <stddef.h>
#include <stdint.h>
namespace serviceui {
struct Transfer { uint16_t frames=0;bool fault=false; };
// The driver returns both actual byte count and status, even on timeout.
// 'retryable' is true only for a documented temporary/timeout status.
constexpr Transfer dacTransfer(uint16_t offered,size_t written,bool ok,bool retryable){
 if(offered>256||written>size_t(offered)*4||written%4)return {0,true};
 return {uint16_t(written/4),!ok&&!retryable};
}
// Called while the actual BT ring lock is held. Native rate only: no hidden
// resampler fragments or worker-queue acknowledgement masquerading as output.
constexpr Transfer nativeRing(int16_t* ring,uint32_t capacity,uint32_t& head,uint32_t& used,
 const int16_t* pcm,uint16_t offered,bool owner,bool ready,bool epochMatches,
 uint32_t sourceRate,uint32_t targetRate){
 if(!owner||!ready||!epochMatches)return {0,true};
 if(!ring||!pcm||!capacity||head>=capacity||used>capacity||offered>256||!sourceRate||sourceRate!=targetRate)return {0,true};
 const uint32_t space=capacity-used;const uint16_t count=uint16_t(space<offered?space:offered);
 for(uint16_t i=0;i<count;++i){
  ring[head*2]=pcm[i*2];ring[head*2+1]=pcm[i*2+1];
  head=(head+1==capacity)?0:head+1;
 }
 used+=count;return {count,false};
}
}

namespace serviceui {
// Same packed stereo order as Audio::Gain/playSample: left high, right low.
constexpr uint32_t dacWord(int16_t left,int16_t right){
 return (uint32_t(uint16_t(left))<<16)|uint16_t(right);
}
constexpr bool supportedRate(uint32_t hz){
 return hz==32000||hz==44100||hz==48000||hz==96000||hz==192000;
}
}
