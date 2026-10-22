#pragma once
#include "passive_policy.h"
#include <stddef.h>
namespace radiosensor {
struct Frame {uint8_t address[6]={};const uint8_t* service=nullptr;size_t length=0;uint16_t uuid=0;int rssi=0;bool encrypted=false;};
enum class Result {Disabled,NoSelection,WrongDevice,InvalidFrame,NeedsDecryption,Unknown,InvalidReading,Decoded,AuthenticationFailed,AdvertisementOnly};
Result decodeMiBeacon(const Config& config,const Frame& frame,uint32_t now,Reading& output);
Result decode(const Config& config,const Frame& frame,uint32_t now,Reading& output);
}
