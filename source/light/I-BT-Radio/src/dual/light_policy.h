#pragma once
#include <stdint.h>

namespace light {
constexpr uint8_t next(uint8_t value,bool battery){return value>=100?(battery?10:0):value==0?10:uint8_t((value/10+1)*10);}
// The CYD GPIO34 CdS divider is about 1.28 V covered and near 0 V lit.
// Keep a visible 10% floor; calibration on the actual board remains required.
constexpr uint8_t brightness(uint16_t millivolts){
    if(millivolts<=150)return 100;
    if(millivolts>=1280)return 10;
    return uint8_t(10+(uint32_t(1280-millivolts)*90+565)/1130);
}
static_assert(brightness(0)==100&&brightness(150)==100&&brightness(1280)==10&&brightness(2450)==10,"LDR end points");
}
