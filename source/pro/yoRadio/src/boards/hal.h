#pragma once
#include "../dual/logic.h"
namespace board {
void begin();
void poll(void (*emit)(dual::Action));
struct TouchCalibration { int xMin,xMax,yMin,yMax; bool swapXY,invertX,invertY; };
bool touch(dual::Point &physical);
bool touchRaw(dual::Point &raw);
bool touchCalibrationNeeded();
bool touchSaveCalibration(const TouchCalibration &cal);
bool touchAvailable();
bool expanderAvailable();
void brightness(uint8_t percent);
bool sensorSample(uint8_t mode,uint32_t now,uint16_t &mv);
}
