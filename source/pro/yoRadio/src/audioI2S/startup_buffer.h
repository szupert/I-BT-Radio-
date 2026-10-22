#pragma once
#include <stdint.h>
#include "stream_buffer_policy.h"
namespace ibtaudio {
// Nonblocking, one-shot startup gate for continuous internet streams.
// Metadata bytes never count: caller passes the decoder input-ring fill.
class StartupBuffer {
public:
    enum Result { Waiting, Ready, Stalled, Expired };
    constexpr void begin(uint32_t now,uint32_t capacity){
        target=StreamBufferPolicy::startupTarget(capacity);
        started=progressAt=now;lastFill=0;result=Waiting;
    }
    constexpr Result poll(uint32_t now,uint32_t filled){
        if(result!=Waiting)return result;
        if(target&&filled>=target)return result=Ready;
        if(filled>lastFill)progressAt=now;
        lastFill=filled;
        if(uint32_t(now-started)>=180000)return result=Expired;
        if(uint32_t(now-progressAt)>=15000)return result=Stalled;
        return Waiting;
    }
    constexpr uint32_t threshold()const{return target;}
private:
    uint32_t target=0,started=0,progressAt=0,lastFill=0;
    Result result=Waiting;
};
}
