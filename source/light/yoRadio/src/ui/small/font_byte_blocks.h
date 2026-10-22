#pragma once
#include <stddef.h>
#include <stdint.h>
// Lossless bounded PackBits blocks. One display owner, no heap/DMA/codec buffer.
namespace fontblocks {
class Reader {
    const uint8_t* data;const uint32_t* offsets;size_t packedSize,total;
    mutable uint32_t cached=UINT32_MAX;
    mutable uint8_t cache[256]{};
public:
    Reader(const uint8_t* d,const uint32_t* o,size_t p,size_t t):data(d),offsets(o),packedSize(p),total(t){}
    uint8_t operator[](size_t index) const {
        if(index>=total)return 0;
        const uint32_t block=uint32_t(index/256);
        if(cached!=block){
            cached=UINT32_MAX; // A malformed block must not leave a valid old cache tag.
            const size_t start=size_t(block)*256,need=total-start<256?total-start:256;
            size_t source=offsets[block],end=offsets[block+1],used=0;
            if(source>end||end>packedSize)return 0;
            while(source<end&&used<need){
                const uint8_t tag=data[source++];const size_t n=tag<128?size_t(tag)+1:size_t(tag&127)+3;
                if(n>need-used)return 0;
                if(tag<128){if(n>end-source)return 0;for(size_t i=0;i<n;++i)cache[used++]=data[source++];}
                else{if(source>=end)return 0;const uint8_t value=data[source++];for(size_t i=0;i<n;++i)cache[used++]=value;}
            }
            if(used!=need||source!=end)return 0;
            cached=block;
        }
        return cache[index%256];
    }
};
}
