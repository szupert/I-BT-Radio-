#pragma once
#include "weather_model.h"
namespace weather {
bool enabled();void toggle();void tick(bool idle);bool temperature(int &tenths);
bool busy();
void audioRequested();
Config configuration();
Reading reading();
bool configure(const Config& config);
bool pending();
const char* status();
}
