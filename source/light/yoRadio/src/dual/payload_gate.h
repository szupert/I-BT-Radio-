#pragma once
#include <atomic>
#include <cstdint>

// One lifecycle owner closes/reopens; readers never wait for teardown.
// Counter and closed bit share one atomic so a late reader cannot slip between
// the owner's zero-reader check and free. The owner retries on its next tick.
class PayloadGate {
    static constexpr uint32_t Closed=uint32_t(1)<<31,CountMask=Closed-1;
    std::atomic<uint32_t> state{Closed};
public:
    bool acquire(){
        uint32_t observed=state.load(std::memory_order_acquire);
        while(!(observed&Closed)&&(observed&CountMask)!=CountMask){
            if(state.compare_exchange_weak(observed,observed+1,
                std::memory_order_acquire,std::memory_order_relaxed))return true;
        }
        return false;
    }
    void release(){state.fetch_sub(1,std::memory_order_release);}
    bool close(){return (state.fetch_or(Closed,std::memory_order_acq_rel)&CountMask)==0;}
    bool open(){
        uint32_t expected=Closed;
        return state.compare_exchange_strong(expected,0,std::memory_order_release,std::memory_order_relaxed);
    }
    class Use {
        PayloadGate* owner;
    public:
        explicit Use(PayloadGate& gate):owner(gate.acquire()?&gate:nullptr){}
        ~Use(){if(owner)owner->release();}
        explicit operator bool() const{return owner!=nullptr;}
        Use(const Use&)=delete;
        Use& operator=(const Use&)=delete;
    };
};
