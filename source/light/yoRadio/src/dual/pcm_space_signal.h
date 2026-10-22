#pragma once
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// One waiting producer, any task-context consumer. Storage lives in the
// internal-heap pipeline for its whole lifetime; never allocate in a callback.
// A latched semaphore covers give-before-take. Always arm, recheck the actual
// buffer predicate, then wait. Signals are hints, never ownership of a slot.
class PcmSpaceSignal {
    StaticSemaphore_t storage;
    SemaphoreHandle_t handle;
    std::atomic<bool> armed{false};
public:
    std::atomic<uint32_t> waits{0},signals{0},timeouts{0};
    PcmSpaceSignal():handle(xSemaphoreCreateBinaryStatic(&storage)){}
    ~PcmSpaceSignal(){if(handle)vSemaphoreDelete(handle);}
    PcmSpaceSignal(const PcmSpaceSignal&)=delete;
    PcmSpaceSignal& operator=(const PcmSpaceSignal&)=delete;
    bool valid()const{return handle!=nullptr;}
    void arm(){armed.store(true);}
    void disarm(){armed.store(false);}
    void notify(){
        if(armed.load() && xSemaphoreGive(handle)==pdTRUE)
            signals.fetch_add(1,std::memory_order_relaxed);
    }
    void wait(TickType_t ticks){
        waits.fetch_add(1,std::memory_order_relaxed);
        if(xSemaphoreTake(handle,ticks)!=pdTRUE)
            timeouts.fetch_add(1,std::memory_order_relaxed);
    }
};

class PcmSpaceWait {
    PcmSpaceSignal* signal;
    bool armed=false;
public:
    explicit PcmSpaceWait(PcmSpaceSignal* s):signal(s){}
    ~PcmSpaceWait(){if(armed)signal->disarm();}
    // True means recheck before sleeping. Do not clear a pending notification.
    bool armAndRecheck(){
        if(!signal||armed)return false;
        signal->arm();armed=true;return true;
    }
    void progress(){if(armed){signal->disarm();armed=false;}}
    void wait(uint32_t remainingMs){
        if(!signal){vTaskDelay(1);return;} // Existing allocation-failure fallback.
        const TickType_t ticks=pdMS_TO_TICKS(remainingMs<20?remainingMs:20);
        signal->wait(ticks?ticks:1);
    }
};
