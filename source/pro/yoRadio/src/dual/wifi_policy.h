#pragma once
#include <stdint.h>
namespace dual {
struct WifiFailoverPolicy {
    uint8_t failuresBeforeRotate=3;
    uint32_t retryBackoffMs=2500;
    uint32_t scanBackoffMs=10000;
};
inline uint8_t nextKnownSsid(uint8_t current,uint8_t count){return count?uint8_t((current+1)%count):0;}
inline bool shouldRotateSsid(uint8_t consecutiveFailures,const WifiFailoverPolicy &p={}){return consecutiveFailures>=p.failuresBeforeRotate;}
inline uint32_t reconnectDelay(uint8_t failures,const WifiFailoverPolicy &p={}){
    uint32_t mult=failures>4?4:failures?failures:1; return p.retryBackoffMs*mult;
}
}
