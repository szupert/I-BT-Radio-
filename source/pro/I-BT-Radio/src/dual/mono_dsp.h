#pragma once
#include "mono_dsp_engine.h"
namespace monodsp {
void begin();
void service(bool maySave,unsigned volume);
Config configuration();
bool change(unsigned parameter,int direction);
bool set(unsigned parameter,int value);
bool pendingSave();
void block(uint32_t rate);
bool active();
uint32_t process(int16_t l,int16_t r,uint32_t dry,unsigned volume,uint32_t rate);
void measuredBlock(unsigned frames,uint32_t wallUs);
void diagnostics();
}
