#pragma once
#include "service_model.h"
#include <stddef.h>
namespace serviceui {
// Quarter-wave LUT with linear interpolation; no heap, floating point or I/O.
constexpr int16_t sineQuarter[]={0,3212,6393,9512,12540,15446,18205,20788,23170,25330,27245,28899,30274,31357,32138,32610,32767};
constexpr int16_t gains[]={33,58,104,184,328,583,1036,1843,3277,5827,10362,18426,32767};
constexpr int32_t sine(uint32_t phase){
 const uint32_t quadrant=phase>>30,offset=phase&0x3fffffff;
 const uint32_t pos=(quadrant&1)?0x40000000-offset:offset;
 const uint32_t index=pos>>26,fraction=(pos>>10)&65535;
 const int32_t value=index>=16?32767:sineQuarter[index]+int32_t((int64_t(sineQuarter[index+1]-sineQuarter[index])*fraction)>>16);
 return quadrant>=2?-value:value;
}
constexpr int32_t waveform(Wave wave,uint32_t phase){
 switch(wave){
 case Wave::Sine:return sine(phase);
 case Wave::Square:return phase<0x80000000?32767:-32767;
 case Wave::RampUp:return int32_t(phase>>16)-32768;
 case Wave::RampDown:return 32767-int32_t(phase>>16);
 case Wave::Triangle:{const uint32_t half=phase&0x7fffffff;const int32_t rise=int32_t(half>>15);return phase<0x80000000?-32768+rise:32767-rise;}
 }
 return 0;
}
struct Oscillator {
 uint32_t phase=0,increment=0;Wave wave=Wave::Sine;Channel channel=Channel::Stereo;
 int16_t gain=0;bool active=false,alarmPattern=false;uint32_t patternFrame=0,rate=48000;
 constexpr void stop(){active=false;phase=0;increment=0;gain=0;}
 constexpr bool begin(const State& state,Limit limit,bool ready){
  stop();if(!state.running||!mayStart(state,limit,ready)||state.db%5!=0)return false;
  increment=uint32_t((uint64_t(state.hz)<<32)/state.rate);
  wave=state.wave;channel=state.channel;gain=gains[(state.db+60)/5];active=true;
  alarmPattern=state.alarmPattern;patternFrame=0;rate=state.rate;return true;
 }
 // Capacity is int16 samples; returned count is stereo frames.
 // Odd final sample is deliberately untouched. Inactive output is silence.
 constexpr size_t render(int16_t* output,size_t capacity){
  if(!output)return 0;
  const size_t frames=capacity/2;
  for(size_t i=0;i<frames;++i){
   unsigned envelope=1024;
   if(alarmPattern){
    const uint32_t ms=uint32_t(uint64_t(patternFrame)*1000/rate);
    const uint32_t slot=ms%250;
    envelope=ms<750&&slot<120?stdEnvelope(slot):0;
    if(++patternFrame>=rate*3/2)patternFrame=0;
   }
   const int16_t value=active?int16_t(int64_t(waveform(wave,phase))*gain*envelope/(32767*1024)):0;
   output[i*2]=channel==Channel::Right?0:value;
   output[i*2+1]=channel==Channel::Left?0:value;
   if(active)phase+=increment;
  }
  return frames;
 }
 static constexpr unsigned stdEnvelope(uint32_t ms){return ms<5?ms*1024/5:ms>115?(120-ms)*1024/5:1024;}
};
}
