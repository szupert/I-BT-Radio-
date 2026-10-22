#pragma once
#include <stdint.h>
namespace ibtlimit {
// Optional network artwork: 75% to start, 50% to continue, fresh audio heartbeat.
// Ratio uses 64-bit arithmetic; unknown/invalid capacity fails closed when playing.
constexpr bool artworkBufferReady(bool active,bool fresh,uint32_t filled,uint32_t capacity,bool starting) {
    return fresh && (!active || (capacity && filled<=capacity &&
        uint64_t(filled)*100>=uint64_t(capacity)*(starting?75:50)));
}
}
