#pragma once
#include <stdint.h>
namespace weatherpolicy {
constexpr bool due(bool enabled,bool idle,bool connected,unsigned heap,uint32_t now,uint32_t last,bool attempted){return enabled&&idle&&connected&&heap>65000&&(!attempted||uint32_t(now-last)>=900000);}
constexpr bool fresh(uint32_t now,uint32_t updated){return uint32_t(now-updated)<3600000;}
}
