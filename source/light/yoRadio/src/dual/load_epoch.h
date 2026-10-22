#pragma once
#include <stdint.h>
namespace loadprobe {
struct Epoch {
    uint32_t generation=0; unsigned state=0; bool known=false;
    constexpr bool update(uint32_t g,unsigned s){
        const bool change=!known||g!=generation||s!=state;
        generation=g;state=s;known=true;return change;
    }
};
constexpr bool validWindow(uint64_t us){return us>=9000000&&us<=30000000;}
}
