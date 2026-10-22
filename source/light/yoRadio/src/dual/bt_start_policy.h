#pragma once
#include <stdint.h>
#include "ram_reserve.h"
namespace btstart {
constexpr unsigned reserve=ibtram::reserve,stackPlan=65536,hostPlan=24576,enablePlan=8192,profilePlan=24576;
// Conservative admission budgets, not a proof of the SDK's hidden allocation peak.
constexpr bool phaseReady(unsigned free,unsigned largest,unsigned remaining){
 return free>=reserve+remaining&&largest>=(remaining>=stackPlan?32768u:remaining?8192u:4096u);
}
#if IBT_STREAM_ART_DIAG
static_assert(reserve==5000,"User diagnostic reserve; BT stays inactive");
#else
static_assert(reserve==10000,"Final shared BT RAM reserve");
#endif
}
