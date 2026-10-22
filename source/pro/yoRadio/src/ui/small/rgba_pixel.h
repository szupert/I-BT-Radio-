#pragma once
#include <stdint.h>
// Composite straight RGBA over black, then convert to the panel transport format.
inline uint16_t bootRgbaTo565(const uint8_t* p){
    const unsigned a=p[3];
    const unsigned r=(unsigned(p[0])*a+127)/255;
    const unsigned g=(unsigned(p[1])*a+127)/255;
    const unsigned b=(unsigned(p[2])*a+127)/255;
    return uint16_t(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
