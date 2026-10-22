#pragma once
#include <atomic>
#include <stdint.h>
namespace lightnet {
// One stream owner and one optional network worker. No heap, socket or RTOS
// operation under this gate. Cancellation is observed by the worker itself;
// only that worker may close its HTTP/TLS client.
class Gate {
    static constexpr uint32_t opening=1,tlsResident=2,background=4;
    std::atomic<uint32_t> flags{0},epoch{1},allowedEpoch{0};
public:
    void cancel(){epoch.fetch_add(1);}
    bool permitted()const{const uint32_t a=allowedEpoch.load();return a&&a==epoch.load();}
    uint32_t token()const{return epoch.load();}
    bool current(uint32_t t)const{return permitted()&&epoch.load()==t&&!(flags.load()&opening);}
    uint32_t beginStream(){flags.fetch_or(opening);cancel();return token();}
    bool drained()const{return !(flags.load()&background);}
    void endStream(bool ok,bool tls,uint32_t t){
        if(ok&&tls)flags.fetch_or(tlsResident);
        flags.fetch_and(~opening);allowedEpoch.store(ok?t:0);
    }
    void streamClosed(){flags.fetch_and(~tlsResident);}
    bool acquire(bool tls,uint32_t t,bool tlsBudgeted=false){
        if(!current(t))return false;
        uint32_t old=flags.load();
        for(;;){
            if(old&(opening|background)||(tls&&(old&tlsResident)&&!tlsBudgeted))return false;
            if(flags.compare_exchange_weak(old,old|background))break;
        }
        if(!current(t)){release();return false;}return true;
    }
    void release(){flags.fetch_and(~background);}
};
inline Gate& gate(){static Gate value;return value;}
}
