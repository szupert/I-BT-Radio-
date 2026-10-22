#pragma once
#include <stdint.h>
namespace ibtaudio {
// Core-0 owner. Callbacks only publish ACKs; they never issue START.
struct MediaStartGate {
    enum Action { None, Check, Start, Fail };
    enum State { Idle, Checking, Starting, Failed };
    State state=Idle;
    uint32_t at=0, generation=0;
    constexpr void reset(){state=Idle;at=0;generation=0;}
    constexpr Action poll(uint32_t now,bool connected,bool primed,bool running,
                          uint32_t token,int checkAck,bool startFailed){
        if(!connected){reset();return None;}
        if(running){reset();return None;}
        if(state==Failed)return None;
        if(state==Checking){
            if(checkAck){
                state=Idle;
                // A flush while awaiting ACK invalidates that START request.
                if(token!=generation||!primed)return None;
                if(checkAck<0){state=Failed;return Fail;}
                state=Starting;at=now;return Start;
            }
            if(uint32_t(now-at)>=10000){state=Failed;return Fail;}
            return None;
        }
        if(state==Starting){
            if(startFailed||uint32_t(now-at)>=10000){state=Failed;return Fail;}
            return None;
        }
        if(primed){generation=token;state=Checking;at=now;return Check;}
        return None;
    }
};
}
