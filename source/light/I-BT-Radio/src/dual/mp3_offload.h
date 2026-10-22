#pragma once
#include <stdint.h>
namespace mp3offload {
int imdct(int granule,int channels); bool setImdct(bool on); bool setWorkerPriority(unsigned priority);
void diagnosticMode(bool on); bool prepare(); bool set(bool on); bool setFraction(unsigned fraction); int dequantize(int granule); void diagnostics(); bool timingActive(); void recordYield(uint32_t elapsed);
}
