#pragma once
#include <stdint.h>
namespace btnavigation {
// Preserve the configured inactivity policy; only the speaker picker gets 3x.
constexpr uint32_t menuTimeoutMs(uint16_t seconds,bool speakerPicker){
    return uint32_t(seconds)*1000u*(speakerPicker?3u:1u);
}
// A connected but silent speaker is waiting for a file, not for a connection.
constexpr bool connectionTimedOut(uint32_t now,uint32_t selectedAt,bool found,bool streaming,bool connected){
    return found&&!streaming&&!connected&&selectedAt&&uint32_t(now-selectedAt)>=90000u;
}
}
