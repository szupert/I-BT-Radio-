#pragma once
#include <atomic>
namespace btuiwake {
// One permanent display task. Only it arms/cancels; the PCM producer claims
// a waiter after filling actual headroom. No stored admission permission.
template<class Handle> class Waiter {
    std::atomic<Handle> handle{nullptr};
public:
    bool armed()const{return handle.load(std::memory_order_acquire)!=nullptr;}
    void arm(Handle value){handle.store(value,std::memory_order_release);}
    Handle take(){return handle.exchange(nullptr,std::memory_order_acq_rel);}
    void cancel(Handle value){handle.compare_exchange_strong(value,nullptr,std::memory_order_acq_rel);}
};
}
