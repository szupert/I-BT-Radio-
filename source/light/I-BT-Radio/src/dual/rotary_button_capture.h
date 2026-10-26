#pragma once
#include <stdint.h>
namespace dual {
// GPIO ISR captures edges; the UI owner alone emits actions. A completed
// press survives a slow LCD slice, without a task, allocation or audio call.
class RotaryButtonCapture {
    uint32_t started_=0,released_=0;
    bool tracking_=false,down_=false,downSent_=false,longSent_=false;
    uint8_t ready_=0;
public:
    enum : unsigned { Down=1,Short=2,Long=4 };
    inline __attribute__((always_inline)) void edge(bool down,uint32_t now){
        if(down){
            // A second distinct contact must not lengthen a completed short
            // press into a false long press while LCD work delays the consumer.
            if(tracking_&&!down_&&uint32_t(now-released_)>=35){
                const uint32_t held=released_-started_;
                if(held>=35&&!ready_)ready_=uint8_t((downSent_?0:Down)|(longSent_?0:held>=800?Long:Short));
                tracking_=false;
            }
            if(!tracking_){started_=now;tracking_=true;downSent_=longSent_=false;}down_=true;
        }
        else if(down_){down_=false;released_=now;}
    }
    unsigned poll(uint32_t now){
        if(ready_){const unsigned out=ready_;ready_=0;return out;}
        if(!tracking_)return 0;
        unsigned out=0;
        if(!down_){
            if(uint32_t(now-released_)<35)return 0;
            const uint32_t held=released_-started_;
            if(held>=35){if(!downSent_)out|=Down;if(!longSent_)out|=held>=800?Long:Short;}
            tracking_=false;downSent_=longSent_=false;return out;
        }
        if(!downSent_&&uint32_t(now-started_)>=35){downSent_=true;out|=Down;}
        if(!longSent_&&uint32_t(now-started_)>=800){longSent_=true;out|=Long;}
        return out;
    }
};
}
