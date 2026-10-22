#pragma once
#include <stdint.h>
namespace ibtaudio {
struct StreamBufferPolicy {
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
