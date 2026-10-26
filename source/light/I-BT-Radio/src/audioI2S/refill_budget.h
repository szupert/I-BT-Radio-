#pragma once
#include <stdint.h>
namespace ibtaudio {
// Same audio owner/socket, bounded non-waiting batch. One legacy iteration on
// other routes; no new task, socket, scratch buffer or PCM threshold.
struct RefillBudget {
 static constexpr uint32_t maxBytes=4096,maxRounds=16,maxUs=3000,lowUs=12000;
 bool enabled;uint32_t start,bytes=0,rounds=0;
 RefillBudget(bool on,uint32_t now):enabled(on),start(now){}
 void enter(){++rounds;}
 void account(uint32_t n){bytes+=n;}
 uint32_t remaining()const{return enabled?(bytes<maxBytes?maxBytes-bytes:0):UINT32_MAX;}
 bool more(uint32_t now,uint32_t filled,uint32_t capacity)const{
  // Only the enabled Light TLS route: below25%, allow already available short
  // records to form a complete frame instead of stopping after one slow read.
  // No waiting for data, no extra bytes/rounds, no hard preemption guarantee.
  const uint32_t timeBudget=uint64_t(filled)*4u<uint64_t(capacity)?lowUs:maxUs;
  return enabled&&rounds<maxRounds&&bytes<maxBytes&&uint32_t(now-start)<timeBudget&&
   uint64_t(filled)*4u<uint64_t(capacity)*3u;
 }
};
}
