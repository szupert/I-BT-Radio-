#pragma once
#include "weather_model.h"
namespace weather {
struct Day {uint32_t epoch=0;int16_t low=0,high=0;uint16_t code=0;};
struct Forecast {Reading current{};Day days[5]{};int16_t apparent=0;int32_t utcOffset=0;uint32_t epoch=0;bool daylight=true;};
// Complete-response parser. Publication is all-or-nothing; failed refreshes keep cache.
bool decodeForecast(const char*,size_t,Forecast&);
const char* condition(uint16_t);
}
