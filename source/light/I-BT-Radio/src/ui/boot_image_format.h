#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace bootimage {
constexpr uint32_t Width=320,Height=240,Payload=Width*Height*2,Header=24,Total=Header+Payload,MaxSeconds=120;
inline uint16_t u16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline bool valid(const uint8_t*p,uint32_t total){return p&&total==Total&&!memcmp(p,"IBTB5651",8)&&u16(p+8)==Width&&u16(p+10)==Height&&u32(p+12)==Payload&&u32(p+20)==0;}
inline uint32_t crc(uint32_t value,const uint8_t*p,size_t n){for(size_t i=0;i<n;++i){value^=p[i];for(unsigned b=0;b<8;++b)value=(value>>1)^(0xedb88320u&uint32_t(-int32_t(value&1u)));}return value;}
}
