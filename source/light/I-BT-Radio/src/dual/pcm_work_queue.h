#pragma once
#include <atomic>
#include <stdint.h>
#include <string.h>
#include <algorithm>
// Strict SPSC: audio decoder owns push; PCM worker owns front/release.
// A borrowed front slot stays immutable until release. No cross-core reset.
class PcmWorkQueue {
public:
    static constexpr uint32_t Capacity=32,Frames=256;
    struct Job {
        uint32_t generation,epoch,rate;
        uint16_t count,volume;
        int16_t balance;
        uint8_t channels,legacy;
        int16_t pcm[Frames*2];
    };
private:
    Job* slots;
    std::atomic<uint32_t> written{0},read{0};
public:
    // R50 diagnostic-only: control/counters share the existing internal queue object.
    // Payload storage remains PSRAM; only the current processing job is staged.
    std::atomic<bool> staging{false};
    std::atomic<uint32_t> stagedJobs{0},directJobs{0};
    explicit PcmWorkQueue(Job* storage):slots(storage){}
    bool push(const int16_t* pcm,uint32_t count,unsigned channels,unsigned rate,unsigned volume,int balance,uint32_t generation,uint32_t epoch,bool legacy){
        if(!pcm||!count||count>Frames||(channels!=1&&channels!=2))return false;
        const uint32_t w=written.load(std::memory_order_relaxed);
        if(w-read.load(std::memory_order_acquire)>=Capacity)return false;
        Job& j=slots[w&(Capacity-1)];
        j.generation=generation;j.epoch=epoch;j.rate=rate;j.count=count;
        j.channels=channels;j.volume=volume;j.balance=balance;j.legacy=legacy;
        memcpy(j.pcm,pcm,count*channels*sizeof(int16_t));
        written.store(w+1,std::memory_order_release);return true;
    }
    const Job* front()const {
        const uint32_t r=read.load(std::memory_order_relaxed);
        return r==written.load(std::memory_order_acquire)?nullptr:&slots[r&(Capacity-1)];
    }
    void release(){read.store(read.load(std::memory_order_relaxed)+1,std::memory_order_release);}
    uint32_t size()const {
        return std::min(Capacity,written.load(std::memory_order_acquire)-read.load(std::memory_order_acquire));
    }
};
