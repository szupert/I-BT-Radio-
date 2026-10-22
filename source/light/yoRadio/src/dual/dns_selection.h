#pragma once
#include <stdint.h>
namespace radiodnsauto {
// Prefer a resolver which answered both independent questions. Ties retain DHCP order.
struct Score {uint8_t successes=0;uint32_t elapsed=0;};
inline int winner(const Score* s,unsigned n){int best=-1;for(unsigned i=0;i<n;++i)if(s[i].successes==2&&(best<0||s[i].elapsed<s[best].elapsed))best=int(i);return best;}
constexpr bool retryDue(uint32_t now,uint32_t previous){return uint32_t(now-previous)>=60000u;}
}
