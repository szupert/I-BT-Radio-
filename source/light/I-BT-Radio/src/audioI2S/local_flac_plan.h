#pragma once
#include <stddef.h>
#include <stdint.h>
namespace localflac {
// FLACDecodeBounded has a signed16-bit available-byte counter. One reserved
// byte belongs to the local linear input's empty/full distinction.
constexpr size_t DefaultCapacity=16384,MaxCapacity=32768;
struct Plan { size_t capacity; bool supported; };
constexpr Plan input(uint32_t maximumFrame) {
    return maximumFrame>=MaxCapacity?Plan{0,false}:
        maximumFrame==0?Plan{DefaultCapacity,true}:
        Plan{maximumFrame<2048?2048:((size_t(maximumFrame)+1+511)/512)*512,true};
}
}
