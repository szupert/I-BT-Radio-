#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace ibtaudio {
// Audio-owner only. Logical16KiB input, two independent8KiB allocations.
// Decoders still get a contiguous frame; only a page-crossing frame is copied.
class LightInputRing {
public:
    static constexpr uint32_t Page=8192,Capacity=Page*2;
    void attach(uint8_t* a,uint8_t* b,uint8_t* window,uint32_t windowBytes){
        pages_[0]=a;pages_[1]=b;window_=window;windowBytes_=windowBytes;reset();
    }
    bool attached()const{return pages_[0]&&pages_[1]&&window_&&windowBytes_;}
    uint8_t* page(unsigned i)const{return pages_[i];}
    uint8_t* window()const{return window_;}
    void detach(){pages_[0]=pages_[1]=window_=nullptr;windowBytes_=0;reset();}
    void reset(){read_=write_=0;}
    uint32_t readPos()const{return read_;}
    uint32_t writePos()const{return write_;}
    uint32_t filled()const{return attached()?(write_+Capacity-read_)%Capacity:0;}
    uint32_t free()const{return attached()?Capacity-1-filled():0;}
    uint32_t writeSpace()const{const uint32_t end=Page-write_%Page,space=free();return space<end?space:end;}
    uint8_t* writePtr()const{return attached()?pages_[write_/Page]+write_%Page:nullptr;}
    bool written(size_t n){if(n>writeSpace())return false;write_=(write_+uint32_t(n))%Capacity;return true;}
    bool consumed(size_t n){if(n>filled())return false;read_=(read_+uint32_t(n))%Capacity;return true;}
    uint32_t span(uint32_t block)const{const uint32_t n=filled(),bound=block<windowBytes_?block:windowBytes_;return n<bound?n:bound;}
    uint8_t* readPtr(uint32_t block){
        if(!attached())return nullptr;
        const uint32_t n=span(block),end=Page-read_%Page;
        uint8_t* p=pages_[read_/Page]+read_%Page;
        if(end>=n)return p;
        memcpy(window_,p,end);memcpy(window_+end,pages_[(read_/Page)^1],n-end);
        return window_;
    }
private:
    uint8_t* pages_[2]{},*window_=nullptr;
    uint32_t windowBytes_=0,read_=0,write_=0;
};
}
