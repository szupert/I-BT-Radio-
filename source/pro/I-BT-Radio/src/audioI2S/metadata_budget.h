#pragma once
#include <stdint.h>

namespace ibtmeta {
// The prefetch ring can report 65536 bytes, but readMetadata accepts uint16_t.
// Saturate instead of wrapping that full-ring size to zero.
constexpr uint16_t readBudget(uint32_t available) {
    return available > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(available);
}
}
