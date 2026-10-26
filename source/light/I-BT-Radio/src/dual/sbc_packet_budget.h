#pragma once
#include <stddef.h>
#include <stdint.h>

// SBC standard channel modes: mono0, dual1, stereo2, joint3.
// Includes the packing routine's possible final partial-byte store.
static inline size_t ibt_sbc_packet_bytes(unsigned mtu, unsigned blocks,
        unsigned subbands, unsigned channels, unsigned mode, unsigned bitpool,
        unsigned frames, size_t header, size_t offset, size_t maximum) {
    if (!frames || (blocks!=4 && blocks!=8 && blocks!=12 && blocks!=16) ||
        (subbands!=4 && subbands!=8) || mode>3 ||
        channels!=(mode==0?1u:2u) || bitpool>255 ||
        bitpool>(mode<=1?16u:32u)*subbands ||
        header>maximum || offset>maximum-header) return 0;
    const size_t bits=(size_t)blocks*bitpool*(mode==1?2u:1u)+(mode==3?subbands:0u);
    const size_t frame=4u+subbands*channels/2u+bits/8u+1u;
    if (frames>15) frames=15; // Existing SBC media-header frame count bound.
    size_t payload=frame*frames;
    if (payload>mtu) payload=mtu;
    if (payload<frame) payload=frame; // Existing loop always encodes one frame.
    if (payload>maximum-header-offset) return 0;
    return header+offset+payload;
}
