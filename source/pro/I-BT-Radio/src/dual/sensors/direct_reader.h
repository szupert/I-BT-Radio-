#pragma once
#include "passive_policy.h"
namespace sensors {namespace direct {
void service(const radiosensor::Config& c,bool idle,bool stackReady,bool scanQuiet,uint32_t now);
bool exclusive();bool quiet();bool faulted();void audioRequested();
bool take(radiosensor::Reading& out);void diagnostics();
}}
