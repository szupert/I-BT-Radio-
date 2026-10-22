#pragma once
#include <stdint.h>
namespace ibtaudio {
struct StreamBufferPolicy {
    static constexpr uint32_t LightBase=16u*1024u;
    static constexpr uint32_t LightPreferred=18u*1024u;
    static constexpr bool admitLight(uint32_t free,uint32_t largest,uint32_t tail,uint32_t reserve){
        return uint64_t(free)>=uint64_t(LightPreferred)+tail+reserve &&
               uint64_t(largest)>=uint64_t(LightPreferred)+tail;
    }
    static constexpr uint32_t LightExpanded=32u*1024u,LightIntermediate=22u*1024u;
    // Separate future receive cost, never another retained-RAM floor. Account
    // for a queued TCP window and its copy into the TLS/receive processing path.
    static constexpr uint32_t receiveCost(uint32_t tcpWindow){return tcpWindow<=UINT32_MAX/2u?tcpWindow*2u:UINT32_MAX;}
    static constexpr bool admitExpanded(uint32_t free,uint32_t largest,uint32_t tail,uint32_t reserve,uint32_t tcpWindow,uint32_t capacity){
        return uint64_t(free)>=uint64_t(capacity)+tail+64u+reserve+receiveCost(tcpWindow)&&
               uint64_t(largest)>=uint64_t(capacity)+tail+64u;
    }
    static constexpr bool admitSegmented(uint32_t free,uint32_t largest,uint32_t tail,uint32_t reserve,uint32_t tcpWindow){
        // Three actual allocations:8KiB+8KiB+frame window; keep receive cost.
        return uint64_t(free)>=uint64_t(LightBase)+tail+3u*64u+reserve+receiveCost(tcpWindow)&&
               largest>=reserve; // User's10000B general contiguous-RAM floor.
    }
    static constexpr uint32_t chooseExpanded(uint32_t free,uint32_t largest,uint32_t tail,uint32_t reserve,uint32_t tcpWindow){
        // Light4 user policy: keep the6KiB formerly used by22KiB as headroom.
        // Do not grow again on a later reconnect even if more memory is free.
        return admitSegmented(free,largest,tail,reserve,tcpWindow)?LightBase:0;
    }
    static constexpr uint32_t Large=1024u*1024u;
    static constexpr uint32_t Fallback=320u*1024u;
    static constexpr uint32_t Reserve=512u*1024u;
    static constexpr uint32_t StartLimit=512u*1024u;
    static constexpr bool admitLarge(uint32_t free,uint32_t largest,uint32_t tail){
        return uint64_t(free)>=uint64_t(Large)+tail+Reserve &&
               uint64_t(largest)>=uint64_t(Large)+tail;
    }
    static constexpr uint32_t startupTarget(uint32_t capacity){
        const uint32_t fraction=uint32_t((uint64_t(capacity)+3u)/4u);
        return fraction<StartLimit?fraction:StartLimit;
    }
};
}
