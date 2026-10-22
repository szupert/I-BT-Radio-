#pragma once
#include "passive_policy.h"
namespace radiosensor {
constexpr bool directOwnsAcquisition(const Config& c,bool searching,bool requested){return c.enabled&&c.selected&&c.keyValid&&!searching&&!requested;}
}
