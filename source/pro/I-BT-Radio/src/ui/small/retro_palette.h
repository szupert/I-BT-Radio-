#pragma once
#include <stdint.h>

namespace retrocolor {
static constexpr uint8_t count=9;
static constexpr uint16_t colors[count]={
    0x07e0, // green: original artwork
    0xfe00, // amber
    0xfc80, // orange
    0xf800, // red
    0x001f, // blue
    0x87ff, // ice blue
    0x07ff, // turquoise
    0xa81f, // purple
    0xffff  // white
};
inline uint16_t accent(uint8_t choice){return colors[choice<count?choice:0];}
inline uint16_t dim(uint16_t color){
    return uint16_t((((color>>11)&31u)/5u)<<11|(((color>>5)&63u)/5u)<<5|(color&31u)/5u);
}
inline uint16_t pixel(uint16_t original,uint8_t choice){
    if(choice==0||choice>=count)return original;
    const unsigned r=(original>>11)&31u,g=(original>>5)&63u,b=original&31u;
    // Tint only the source artwork's luminous green pixels. Dark chassis and
    // unrelated details keep their original RGB565 values.
    if(g<3u||g<2u*r||g<2u*b)return original;
    const uint16_t target=accent(choice);
    return uint16_t(((((target>>11)&31u)*g+31u)/63u)<<11 |
                    (((((target>>5)&63u)*g+31u)/63u)<<5) |
                    (((target&31u)*g+31u)/63u));
}
}
