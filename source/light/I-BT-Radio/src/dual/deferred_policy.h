#pragma once
#include <stdint.h>
namespace audiod {
constexpr bool current(uint32_t qe,uint32_t qg,uint32_t e,uint32_t g){return qe==e&&qg==g;}
}
