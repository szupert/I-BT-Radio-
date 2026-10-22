#pragma once
#include <stdint.h>
namespace pcmgain {
// Preserve legacy coefficient rounding. Recompute only when volume/balance changes.
class Cached {
    unsigned volume=256; int balance=127; int left=0,right=0;
public:
    uint32_t apply(int16_t l,int16_t r,unsigned v,int b){
        if(v!=volume||b!=balance){
            float step=float(v)/254;uint8_t dl=0,dr=0;
            if(b<0){step=step*float((-b)*16);dl=uint8_t(step);}
            if(b>0){step=step*b*16;dr=uint8_t(step);}
            left=int(v)-dl;right=int(v)-dr;volume=v;balance=b;
        }
        const int32_t a=(int32_t(l)*left)>>8,c=(int32_t(r)*right)>>8;
        // Unsigned packing avoids shifting a negative signed value.
        return (uint32_t(uint16_t(a))<<16)|uint16_t(c);
    }
};
}
