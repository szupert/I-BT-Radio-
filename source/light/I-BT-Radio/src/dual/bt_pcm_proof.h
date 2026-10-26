#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
namespace btaudio {
// Each instance has one PCM writer. Host snapshots may span adjacent updates.
// At most8 frames per block, first4096 sampled values per boot. No PCM retained.
struct PcmProof {
    std::atomic<uint32_t> sampled{0}, peakNonzero{0}, hash{2166136261u};
    void observe(const void* data,unsigned frames,unsigned channels){
        uint32_t count=sampled.load(std::memory_order_relaxed);
        if(!data||count>=4096||(channels!=1&&channels!=2))return;
        uint32_t packed=peakNonzero.load(std::memory_order_relaxed);
        uint32_t peak=packed>>16,nonzero=packed&65535u,h=hash.load(std::memory_order_relaxed);
        const auto* p=static_cast<const uint8_t*>(data);
        const unsigned n=frames<8?frames:8;
        for(unsigned i=0;i<n*channels&&count<4096;++i){
            int16_t value;std::memcpy(&value,p+i*2,2);
            const uint32_t magnitude=value<0?uint32_t(-int32_t(value)):uint32_t(value);
            if(magnitude>peak)peak=magnitude;
            nonzero+=value!=0;h=(h^uint16_t(value))*16777619u;++count;
        }
        hash.store(h,std::memory_order_relaxed);peakNonzero.store((peak<<16)|nonzero,std::memory_order_relaxed);
        sampled.store(count,std::memory_order_relaxed);
    }
};
}
