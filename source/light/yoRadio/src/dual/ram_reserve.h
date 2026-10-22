#pragma once
#include <stdint.h>
namespace ibtram {
#if IBT_STREAM_ART_DIAG
// Explicit user-requested diagnostic exception; production baseline stays10000B.
inline constexpr uint32_t reserve=5000u;
static_assert(IBT_NO_PSRAM && IBT_LIGHT4_DNS_AUTO,"Diagnostic is Light4/noPSRAM only");
#else
inline constexpr uint32_t reserve=10000u;
#endif
}
