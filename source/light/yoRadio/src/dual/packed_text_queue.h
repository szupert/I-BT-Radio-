#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
namespace audiod {
// One audio-owner producer and one service-owner consumer. Publication fences
// protect descriptors and bytes until the consumer has copied the complete event.
template<class Event,unsigned Slots,unsigned Bytes> class PackedTextQueue {
private:
    struct Header {uint32_t epoch,generation;decltype(Event{}.kind) kind;uint16_t length;};
    Header slots[Slots]{};
    char bytes[Bytes]{};
    std::atomic<uint32_t> published{0},consumed{0},written{0},released{0};
    uint32_t writePosition=0,readPosition=0;
    unsigned writeSlot=0,readSlot=0;
    static_assert(Slots>0 && Bytes>0 && sizeof(Event{}.text)<=65535,"bounded text queue");
public:
    bool push(const Event& e) {
        const auto n=strnlen(e.text,sizeof(e.text));
        if(n==sizeof(e.text))return false;
        const uint32_t size=uint32_t(n+1),w=written.load(std::memory_order_relaxed);
        const uint32_t p=published.load(std::memory_order_relaxed);
        if(size>Bytes || p-consumed.load(std::memory_order_acquire)>=Slots ||
           w-released.load(std::memory_order_acquire)>Bytes-size)return false;
        for(uint32_t i=0;i<size;++i)bytes[(writePosition+i)%Bytes]=e.text[i];
        slots[writeSlot]={e.epoch,e.generation,e.kind,uint16_t(size)};
        writePosition=(writePosition+size)%Bytes;writeSlot=(writeSlot+1)%Slots;
        written.store(w+size,std::memory_order_relaxed);
        published.store(p+1,std::memory_order_release);
        return true;
    }
    bool pop(Event& e) {
        const uint32_t r=consumed.load(std::memory_order_relaxed);
        if(r==published.load(std::memory_order_acquire))return false;
        const auto h=slots[readSlot];
        if(!h.length || h.length>sizeof(e.text) || h.length>Bytes)return false;
        const uint32_t start=released.load(std::memory_order_relaxed);
        e.epoch=h.epoch;e.generation=h.generation;e.kind=h.kind;
        for(uint32_t i=0;i<h.length;++i)e.text[i]=bytes[(readPosition+i)%Bytes];
        readPosition=(readPosition+h.length)%Bytes;readSlot=(readSlot+1)%Slots;
        released.store(start+h.length,std::memory_order_release);
        consumed.store(r+1,std::memory_order_release);
        return true;
    }
    unsigned pending()const{return published.load(std::memory_order_acquire)-consumed.load(std::memory_order_acquire);}
};
}
