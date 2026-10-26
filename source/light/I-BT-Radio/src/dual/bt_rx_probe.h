#pragma once
#include <atomic>
#include <stdint.h>
namespace btrxprobe {
inline void maximum(std::atomic<uint32_t>& target,uint32_t value){auto old=target.load(std::memory_order_relaxed);while(old<value&&!target.compare_exchange_weak(old,value,std::memory_order_relaxed)){} }
struct Stats {
 std::atomic<uint32_t> callbacks{0},inputBytes{0},maxBytes{0},overCapacity{0};
 std::atomic<uint32_t> le1024{0},le2048{0},le4096{0},le8192{0},larger{0};
 std::atomic<uint32_t> callbackWall{0},pops{0},popBytes{0},popMax{0},primeWaits{0};
 std::atomic<uint32_t> writes{0},writeWall{0},writeCalls{0},writeFailures{0};
 void callback(unsigned bytes,unsigned capacity){callbacks.fetch_add(1,std::memory_order_relaxed);inputBytes.fetch_add(bytes,std::memory_order_relaxed);maximum(maxBytes,bytes);if(bytes>capacity)overCapacity.fetch_add(1,std::memory_order_relaxed);
  (bytes<=1024?le1024:bytes<=2048?le2048:bytes<=4096?le4096:bytes<=8192?le8192:larger).fetch_add(1,std::memory_order_relaxed);}
 void pop(unsigned bytes){pops.fetch_add(1,std::memory_order_relaxed);popBytes.fetch_add(bytes,std::memory_order_relaxed);maximum(popMax,bytes);}
 void write(unsigned calls,unsigned wall,bool ok){writes.fetch_add(1,std::memory_order_relaxed);writeCalls.fetch_add(calls,std::memory_order_relaxed);maximum(writeWall,wall);if(!ok)writeFailures.fetch_add(1,std::memory_order_relaxed);}
};
inline Stats& stats(){static Stats value;return value;}
}
