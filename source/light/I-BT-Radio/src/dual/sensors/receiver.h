#pragma once
#include <stddef.h>
#include <stdint.h>
#include "passive_policy.h"
namespace sensors {
bool roleSelected(unsigned);bool roleReading(unsigned,radiosensor::Reading&);
bool roleSavePending();void enableRoles();
void selectRole(unsigned index,unsigned role);void clearRole(unsigned role);
bool bindRole(unsigned role,const char* address,const char* key);
void roleBindingStatus(unsigned,char*,size_t,bool&);
void stackStopped(); // SDK owner notification; callbacks are no longer live
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
