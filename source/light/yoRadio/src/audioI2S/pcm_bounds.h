#pragma once
#include <stddef.h>
namespace pcmbounds {
inline bool aac(size_t capacity,unsigned channels,bool sbr) {
    return channels>=1&&channels<=2&&capacity>=size_t(channels)*1024u*(sbr?2u:1u);
}
inline unsigned flacFrames(size_t capacity,unsigned channels,unsigned remaining) {
    if(channels<1||channels>2)return 0;
    size_t frames=capacity/channels;
    if(frames>2048)frames=2048;
    if(frames>remaining)frames=remaining;
    return unsigned(frames);
}
}
