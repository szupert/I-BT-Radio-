#pragma once
#include <stdint.h>
#include <algorithm>
namespace btaudio {
struct BlockGains {int32_t left,right;};
class BlockGainCache {
 unsigned lastLevel=~0u;int lastBalance=0;BlockGains value{0,0};
public:
 BlockGains get(unsigned volume,int balance){
  const unsigned level=std::min(volume,254u);
  if(level==lastLevel&&balance==lastBalance)return value;
  lastLevel=level;lastBalance=balance;
  if(level==254&&balance==0){value={32768,32768};return value;}
  const unsigned left=unsigned(std::max(0,std::min(16,-balance)));
  const unsigned right=unsigned(std::max(0,std::min(16,balance)));
  constexpr uint32_t divisor=254u*254u*16u;
  value={int32_t((uint64_t(level)*level*(16-left)*32768u+divisor/2)/divisor),int32_t((uint64_t(level)*level*(16-right)*32768u+divisor/2)/divisor)};
  return value;
 }
};
}
