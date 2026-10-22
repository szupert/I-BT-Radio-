#pragma once
#include "../ram_reserve.h"
#include "passive_policy.h"
#include <stddef.h>
namespace radiosensor {
constexpr uint32_t directIntervalMs=60000,directIdleMs=15000,directTimeoutMs=15000,directCleanupMs=5000;
constexpr bool directBudget(bool idle,unsigned heap,unsigned largest,unsigned stack){return idle&&heap>=(ibtram::reserve==0?0u:32768u)&&largest>=(ibtram::reserve==0?0u:16384u)&&stack>=(ibtram::reserve==0?0u:3072u);}
constexpr bool directDue(bool stable,bool attempted,uint32_t now,uint32_t last){return stable&&(!attempted||uint32_t(now-last)>=directIntervalMs);}
constexpr bool directValue(const uint8_t* p,size_t n,uint32_t now,Reading& out){
 if(!p||n!=5)return false;const int t=int16_t(uint16_t(p[0])|(uint16_t(p[1])<<8));const unsigned h=p[2],mv=uint16_t(p[3])|(uint16_t(p[4])<<8);
 if(mv<1800||mv>3600)return false;return updateReading(out,true,t,true,int(h)*100,-1,now);
}
}
