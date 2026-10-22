#pragma once
#include <atomic>
#include <stdint.h>
namespace owneryield {
// Auto is a mode, not a millisecond interval. Keep the state in initialized data.
constexpr uint32_t Auto=1;
constexpr bool valid(uint32_t n){return n==Auto||n==2||n==50;}
constexpr bool bufferedWebMp3(bool web,bool playing,unsigned codec,unsigned rate,uint32_t fill){
 return web&&playing&&codec==2&&rate==32000&&fill>=65536;
}
constexpr uint32_t period(bool btReady,uint32_t requested,bool buffered=false){
 if(!btReady||!valid(requested))return 2;
 return requested==Auto?(buffered?50:2):requested;
}
constexpr bool due(uint32_t now,uint32_t last,uint32_t interval){return uint32_t(now-last)>=interval;}
inline std::atomic<uint32_t>& requested(){static std::atomic<uint32_t> value{Auto};return value;}
inline std::atomic<uint32_t>& applied(){static std::atomic<uint32_t> value{2};return value;}
inline bool set(uint32_t n){if(!valid(n))return false;requested().store(n);return true;}
inline void reset(){requested().store(Auto);}
}
