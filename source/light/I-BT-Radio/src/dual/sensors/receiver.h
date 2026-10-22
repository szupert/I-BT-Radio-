#pragma once
#include <stddef.h>
#include <stdint.h>
#include "passive_policy.h"
namespace sensors {
bool available();
bool enabled();
void toggle();
void discover(); // deferred until idle clock home; no network or scan in UI handler
unsigned deviceCount();
bool deviceLabel(unsigned index,char*out,size_t size);
void select(unsigned index);
void setBudget(bool idleClock,uint32_t blockers=0); // UI publication only
void service(); // existing Core0 owner; bounded lifecycle, no decoder calls
void audioRequested();
bool quiescent(); // audio start waits only for scan-stop acknowledgement
const char* status();
bool reading(radiosensor::Reading&out);
bool bindDevice(unsigned index,const char* key);
bool bindAddress(const char* address,const char* key);
void listDevices();
void bindingStatus(char* address,size_t size,bool& hasKey,bool& pending);
void diagnostics(); // read-only; never starts scanning or prints address/key
}
