#pragma once
#include <atomic>
#include <stdint.h>
#include <stddef.h>
#include <initializer_list>
namespace dacdiag {
// One audio-owner writer; diagnostics read individual atomic counters.
// Accepted bytes are driver submissions, not proof of audible DMA output.
struct Counters {
    std::atomic<uint32_t> calls{0},accepted{0},shortWrites{0},zeroWrites{0},errors{0};
    std::atomic<uint32_t> callMaxUs{0},gapMaxUs{0},overCapacityGaps{0},rate{0};
    uint32_t previousEnd=0;
    bool continuous=false;
    void breakInterval(){continuous=false;}
    void reset(){
        for(auto* p:{&calls,&accepted,&shortWrites,&zeroWrites,&errors,&callMaxUs,&gapMaxUs,&overCapacityGaps,&rate})
            p->store(0,std::memory_order_relaxed);
        continuous=false;previousEnd=0;
    }
    static void maximum(std::atomic<uint32_t>& field,uint32_t n){
        if(n>field.load(std::memory_order_relaxed))field.store(n,std::memory_order_relaxed);
    }
    void record(uint32_t start,uint32_t end,size_t requested,size_t written,bool ok,uint32_t hz,uint32_t dmaFrames){
        if(continuous&&hz==rate.load(std::memory_order_relaxed)){
            const uint32_t gap=start-previousEnd;
            maximum(gapMaxUs,gap);
            // Exceeds even a completely full DMA reservoir. This is a supply
            // deadline warning, not a hardware underrun interrupt counter.
            if(hz&&uint64_t(gap)*hz>uint64_t(dmaFrames)*1000000u)
                overCapacityGaps.fetch_add(1,std::memory_order_relaxed);
        }
        calls.fetch_add(1,std::memory_order_relaxed);
        accepted.fetch_add(uint32_t(written),std::memory_order_relaxed);
        if(written<requested)shortWrites.fetch_add(1,std::memory_order_relaxed);
        if(!written)zeroWrites.fetch_add(1,std::memory_order_relaxed);
        if(!ok)errors.fetch_add(1,std::memory_order_relaxed);
        maximum(callMaxUs,end-start);rate.store(hz,std::memory_order_relaxed);
        previousEnd=end;continuous=true;
    }
};
}
