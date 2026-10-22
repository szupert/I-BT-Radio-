#pragma once
#include <stdint.h>
#include <atomic>
namespace timingdiag {
// One audio-owner writer per bucket; diagnostic reader may observe an in-flight
// update. Unsigned totals wrap modulo 2^32; use bounded interval deltas.
inline constexpr uint32_t outsideWork(uint32_t roundtrip,uint32_t work){return roundtrip>=work?roundtrip-work:0;}
struct Bucket {
 std::atomic<uint32_t> calls{0},total{0},maximum{0};
 void add(uint32_t us){total.store(total.load(std::memory_order_relaxed)+us,std::memory_order_relaxed);calls.store(calls.load(std::memory_order_relaxed)+1,std::memory_order_relaxed);if(us>maximum.load(std::memory_order_relaxed))maximum.store(us,std::memory_order_relaxed);}
};
}
