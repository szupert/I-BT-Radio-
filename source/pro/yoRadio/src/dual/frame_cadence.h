#pragma once
#include <stdint.h>
namespace dual {
// Render cadence only; never sleeps or blocks the audio owner.
class FrameCadence {
    uint8_t interval_=100;
    uint32_t goodAt_=0;
    bool good_=false;
public:
    constexpr uint32_t goodMs(uint32_t now) const {return good_?uint32_t(now-goodAt_):0;}
    constexpr unsigned update(uint32_t now,bool known,int c0,int c1,bool pressure,bool samplerBusy=false){
        if(pressure){interval_=200;good_=false;}
        else if(!known){interval_=100;if(!samplerBusy)good_=false;}
        else if(c0>800||c1>800){interval_=100;good_=false;}
        else if(c0<750&&c1<750){
            if(interval_==200)interval_=100;
            if(!good_){good_=true;goodAt_=now;}
            if(uint32_t(now-goodAt_)>=5000){interval_=50;}
        }else {if(interval_==200)interval_=100;good_=false;}
        return interval_;
    }
};
}
