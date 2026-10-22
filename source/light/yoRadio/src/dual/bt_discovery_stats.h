#pragma once
#include <atomic>
#include <stdint.h>
namespace btdiscovery {
// Fixed counters only: raw is callback events, not unique devices. No heap/logging.
struct Stats {
    std::atomic<uint32_t> generation{0},raw{0},accepted{0},added{0},updated{0};
    std::atomic<uint32_t> state{0},selected{0},invalid{0},full{0},badProps{0};
    std::atomic<uint32_t> codUnknown{0},codNonRendering{0},started{0},completed{0};
    std::atomic<int> startError{-1};
    void reset(){
        raw=0;accepted=0;added=0;updated=0;state=0;selected=0;invalid=0;full=0;badProps=0;
        codUnknown=0;codNonRendering=0;started=0;completed=0;startError=-1;++generation;
    }
};
// Respect the supplied property length; complete name takes priority over short.
inline const uint8_t* eirName(const uint8_t* data,unsigned size,uint8_t& len,bool& valid){
    const uint8_t* shortName=nullptr;uint8_t shortLen=0;len=0;valid=true;
    if(!data||size>240){valid=false;return nullptr;}
    for(unsigned at=0;at<size;){
        const unsigned n=data[at];if(!n)break;
        if(n>size-at-1){valid=false;return nullptr;}
        if(n>1&&data[at+1]==0x09){len=uint8_t(n-1);return data+at+2;}
        if(n>1&&data[at+1]==0x08){shortName=data+at+2;shortLen=uint8_t(n-1);}
        at+=n+1;
    }
    len=shortLen;return shortName;
}
}
