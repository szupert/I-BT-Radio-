#pragma once
#include "weather_model.h"
#if IBT_NO_PSRAM
#include "weather_light_model.h"
#endif
namespace weather {
#if IBT_NO_PSRAM
bool active();void setActive(bool);Forecast forecast();void service();void refresh();void wake(uint32_t);bool controlsVisible(uint32_t);
#else
inline bool active(){return false;}inline void setActive(bool){}inline void service(){}
#endif
bool enabled();void toggle();void tick(bool idle);bool temperature(int &tenths);
bool busy();
void audioRequested();
Config configuration();
Reading reading();
bool configure(const Config& config);
bool pending();
const char* status();
}
