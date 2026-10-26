#pragma once
#include <atomic>
#include <stdint.h>
namespace lightui {
inline std::atomic<uint8_t> clockStyle{0}; // lapozós / digitális, shared flash glyphs
inline std::atomic<uint8_t> style{1}; // OFF, vertical stereo, horizontal stereo, boombox
inline constexpr unsigned frameMs=200;
inline constexpr bool meterMode(bool artwork,bool dedicatedSpeaker){return dedicatedSpeaker||!artwork;}
inline uint8_t validStyle(unsigned v){return v<=3?uint8_t(v):1;}
struct Rect {int x,y,w,h;constexpr bool contains(int px,int py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;}};
inline constexpr Rect title{0,0,320,28},metadata{8,33,304,55},meter{8,80,80,88},clock{102,80,210,88},status{8,170,304,22},controls{0,196,320,44};
inline int homeButton(int x,int y){return controls.contains(x,y)?x/64:-1;}
inline unsigned levels(uint8_t value,unsigned segments){
#if IBT_NO_PSRAM
 const unsigned boosted=unsigned(value)+(unsigned(value)>>1);
 const unsigned level=boosted>255?255:boosted; // Display sensitivity only; PCM and volume are untouched.
#else
 const unsigned level=value;
#endif
 return (level*segments+254u)/255u;
}
inline bool due(uint32_t now,uint32_t previous,unsigned ms){return uint32_t(now-previous)>=ms;}
}
