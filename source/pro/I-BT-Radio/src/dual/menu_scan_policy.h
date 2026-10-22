#pragma once
namespace uicoverage {
constexpr bool renderable(unsigned slot,unsigned normalSlots,unsigned item){
 return (slot<normalSlots&&item<16)||(slot==normalSlots&&item<28);
}
}
