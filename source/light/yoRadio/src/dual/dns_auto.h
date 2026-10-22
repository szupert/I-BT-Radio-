#pragma once
#include <stdint.h>
namespace radiodnsauto {
#if defined(IBT_LIGHT4_DNS_AUTO)
void tick();
void failed();
void artworkFailed();
void succeeded();
const char* status();
#else
inline void tick(){} inline void failed(){} inline void artworkFailed(){} inline void succeeded(){} inline const char* status(){return "NOT_MEASURED";}
#endif
}
