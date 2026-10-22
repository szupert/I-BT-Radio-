#pragma once
#include <stddef.h>
#include <stdint.h>
namespace localsd {
constexpr size_t ReadQuantum = 8192;
inline size_t request(size_t room, size_t position, size_t end) {
    if(position >= end) return 0;
    const size_t left = end - position;
    if(room > ReadQuantum) room = ReadQuantum;
    return room > left ? left : room;
}
inline bool stalled(uint32_t now, uint32_t since) {
    return uint32_t(now - since) >= 5000;
}
}
