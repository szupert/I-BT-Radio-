#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include <algorithm>
// One producer and one consumer. For BT OUT: audio task -> BT callback. Capacity is a power
// of two; counters wrap unsigned. No allocation or waiting in either path.
class PcmRing {
    uint8_t* bytes; const uint32_t capacity;
    std::atomic<uint32_t> written{0}, read{0};
public:
    PcmRing(uint8_t* data,uint32_t size):bytes(data),capacity(size){}
    uint32_t size() const {return written.load(std::memory_order_acquire)-read.load(std::memory_order_acquire);}
    uint32_t push(const uint8_t* data,uint32_t n){
        auto w=written.load(std::memory_order_relaxed);
        n=std::min(n,capacity-(w-read.load(std::memory_order_acquire)))&~uint32_t(3);
        auto first=std::min(n,capacity-(w&(capacity-1)));
        memcpy(bytes+(w&(capacity-1)),data,first);memcpy(bytes,data+first,n-first);
        written.store(w+n,std::memory_order_release);return n;
    }
    uint32_t pop(uint8_t* data,uint32_t n){
        auto r=read.load(std::memory_order_relaxed);
        n=std::min(n,written.load(std::memory_order_acquire)-r)&~uint32_t(3);
        auto first=std::min(n,capacity-(r&(capacity-1)));
        memcpy(data,bytes+(r&(capacity-1)),first);memcpy(data+first,bytes,n-first);
        read.store(r+n,std::memory_order_release);return n;
    }
    // Consumer only. Never reset producer's counter while a callback can run.
    void discard(){read.store(written.load(std::memory_order_acquire),std::memory_order_release);}
};
