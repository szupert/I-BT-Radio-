#pragma once
#include <stdint.h>
namespace dual {
struct RotaryPending {
    int16_t pending=0;uint32_t dropped=0;
    void add(long delta){
        if(delta>16384)delta-=32768;else if(delta<-16384)delta+=32768;
        long total=pending-delta;long bounded=total>128?128:total<-128?-128:total;
        uint32_t lost=uint32_t(total>bounded?total-bounded:bounded-total);
        dropped=UINT32_MAX-dropped<lost?UINT32_MAX:dropped+lost;pending=int16_t(bounded);
    }
    int take(){int out=pending>0?1:pending<0?-1:0;pending-=out;return out;}
};
}
