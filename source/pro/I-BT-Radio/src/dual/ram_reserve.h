#pragma once
#include <stdint.h>
namespace ibtram {
// Final user policy: exactly10000bytes for every edition, no profile override.
inline constexpr uint32_t reserve=10000u;
static_assert(reserve==10000u,"User RAM reserve must remain exactly10000bytes");
}
