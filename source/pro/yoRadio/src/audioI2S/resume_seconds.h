#pragma once
#include <stdint.h>
namespace resumetime {
constexpr uint32_t seconds(uint32_t position,uint32_t dataStart,uint32_t buffered,uint32_t bitrate){
    const uint64_t skipped=uint64_t(dataStart)+buffered;
    return !bitrate||position<=skipped?0:uint32_t((uint64_t(position)-skipped)*8/bitrate);
}
}
