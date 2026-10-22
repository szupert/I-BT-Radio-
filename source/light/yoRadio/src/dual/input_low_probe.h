#pragma once
#include <stdint.h>
namespace inputlow {
// Audio-owner updates only, no allocation, logging or change to recovery.
void sample(uint32_t now,bool low,uint32_t input,uint32_t generation,bool pcmSafe);
void telemetry(); // diagnostic/Core0 only
}
