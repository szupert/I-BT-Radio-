#pragma once
#include <stddef.h>
#include <stdint.h>
namespace wavfmt {
constexpr uint16_t subformat(const uint8_t* data,size_t bytes,uint16_t bits){
    if(bytes<40 || data[16]!=22 || data[17]!=0 ||
       !(uint16_t(data[18])|(uint16_t(data[19])<<8)) ||
       (uint16_t(data[18])|(uint16_t(data[19])<<8))>bits)return 0;
    constexpr uint8_t guid[16]={0,0,0,0,0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};
    for(unsigned i=1;i<16;++i)if(data[24+i]!=guid[i])return 0;
    if(data[24]==3&&(uint16_t(data[18])|(uint16_t(data[19])<<8))!=bits)return 0;
    return data[24]==1||data[24]==3?data[24]:0;
}
constexpr bool extensiblePcm(const uint8_t* data,size_t bytes,uint16_t bits){
    return subformat(data,bytes,bits)==1;
}
}
