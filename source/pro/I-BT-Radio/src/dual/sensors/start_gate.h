#pragma once
#include <stdint.h>
namespace radiosensor {
// Preserve an explicit pending search across the initial saved-setting read.
constexpr bool enabledAfterLoad(bool saved,bool searchPending){return saved||searchPending;}
// Discovery never requires model decoding, even with a previously saved sensor.
constexpr bool needsDecoderTask(bool selected,bool pending,bool searching,bool native=false){return selected&&!pending&&!searching&&!native;}
}
