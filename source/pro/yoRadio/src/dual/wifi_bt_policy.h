#pragma once
#include <stdint.h>
namespace ibtcoex {
// One core0 owner; readback catches Wi-Fi reinitialization overriding the setting.
// No radio calls while disconnected/AP setup; no loops, delays, or NVS writes.
struct Policy {
    bool waiting=false,lastBt=false;
    uint32_t attemptedAt=0,interval=1000;
    template<class Apply> bool tick(uint32_t now,bool staReady,bool bt,Apply apply){
        if(!staReady){waiting=false;return false;}
        if(waiting&&bt==lastBt&&uint32_t(now-attemptedAt)<interval)return false;
        waiting=true;lastBt=bt;attemptedAt=now;
        const bool ok=apply(bt);interval=ok?1000:5000;
        return ok;
    }
};
}
