#pragma once
#include <stdint.h>
namespace battery {
constexpr bool valid(const uint16_t *p) {
    for(unsigned i=0;i<5;i++)if(p[i]>2450||(i&&p[i]<=p[i-1]))return false;
    return true;
}
// Calibrated GPIO millivolts, not cell volts. A piecewise estimate, not coulomb counting.
constexpr int percent(uint16_t mv,const uint16_t *p) {
    if(!valid(p))return -1;
    if(mv<=p[0])return 0;
    if(mv>=p[4])return 100;
    for(unsigned i=1;i<5;i++)if(mv<p[i])return int((i-1)*25+(uint32_t(mv-p[i-1])*25)/(p[i]-p[i-1]));
    return -1;
}
constexpr int quantize(int percent,unsigned step){return percent<0?-1:percent>=100?100:int((unsigned(percent)/(step?step:1))*(step?step:1));}
constexpr uint16_t adjust(uint16_t mv,int delta) {
    int next=int(mv)+delta;return uint16_t(next<0?0:next>2450?2450:next);
}
}
