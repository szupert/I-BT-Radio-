#pragma once
#include <atomic>
#include <stdint.h>
class DeferredFlags {
    std::atomic<uint32_t> pending{0};
public:
    void add(uint32_t bits){pending.fetch_or(bits);}
    uint32_t take(){
        uint32_t mask=pending.load();if(!mask)return 0;
        uint32_t bit=mask&(~mask+1);
        pending.fetch_and(~bit);return bit;
    }
};
