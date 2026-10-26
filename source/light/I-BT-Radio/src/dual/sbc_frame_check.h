#pragma once
#include <stddef.h>
#include <stdint.h>
// Standard SBC header/scale-factor CRC, read-only and allocation-free.
static inline int ibt_sbc_frame_crc_ok(const uint8_t *p, size_t size) {
    if (!p || size < 4 || p[0] != 0x9c) return 0;
    const unsigned bands=(p[1]&1)?8:4, mode=(p[1]>>2)&3;
    const unsigned channels=mode?2:1, blocks=4*((p[1]>>4&3)+1);
    const unsigned pool=p[2];
    if (pool<2 || pool>(mode<=1?16u:32u)*bands) return 0;
    const size_t audio_bits=blocks*pool*(mode==1?2:1)+(mode==3?bands:0);
    const size_t bytes=4+bands*channels/2+(audio_bits+7)/8;
    const unsigned extra=(mode==3?bands:0)+bands*channels*4;
    if (size!=bytes || 4+(extra+7)/8>size) return 0;
    uint8_t crc=0x0f;
    for (unsigned i=0;i<16+extra;++i) {
        const unsigned bit=i<16 ? ((p[1+i/8]>>(7-i%8))&1) :
                                  ((p[4+(i-16)/8]>>(7-(i-16)%8))&1);
        const unsigned feedback=(crc>>7)^bit;
        crc=(uint8_t)(crc<<1);
        if (feedback) crc^=0x1d;
    }
    return crc==p[3];
}
