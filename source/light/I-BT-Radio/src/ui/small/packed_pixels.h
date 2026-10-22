#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#ifndef IBT_PACKED_PIXELS_HOST
#include <esp_heap_caps.h>
#endif
namespace packedpixels {
// Raw LZ4 block, restricted to an exact output length. All lengths and back
// references are checked before touching memory; overlapping matches are valid.
inline bool unpack(const uint8_t* src,size_t size,uint8_t* out,size_t expected){
    if(!src||!out)return false;
    size_t ip=0,op=0;
    while(ip<size){
        const unsigned token=src[ip++];size_t literals=token>>4;
        if(literals==15){unsigned n;do{if(ip>=size)return false;n=src[ip++];if(literals>expected-op||n>expected-op-literals)return false;literals+=n;}while(n==255);}
        if(literals>size-ip||literals>expected-op)return false;
        memcpy(out+op,src+ip,literals);ip+=literals;op+=literals;
        if(ip==size)return op==expected;
        if(size-ip<2)return false;
        const size_t offset=size_t(src[ip])|(size_t(src[ip+1])<<8);ip+=2;
        if(offset==0||offset>op)return false;
        size_t count=(token&15)+4;
        if((token&15)==15){unsigned n;do{if(ip>=size)return false;n=src[ip++];if(count>expected||n>expected-count)return false;count+=n;}while(n==255);}
        if(count>expected-op)return false;
        for(size_t i=0;i<count;++i)out[op+i]=out[op+i-offset];
        op+=count;
    }
    return op==expected;
}
struct Asset {const uint8_t* data;size_t bytes;};
struct IndexedAsset {const uint8_t* data;size_t bytes;const uint16_t* palette;size_t colors;};
// Decode the 8-bit indices into the upper half of the same frame. Forward
// expansion reads each index before a 16-bit write can reach that index.
inline bool unpackIndexed(const IndexedAsset& a,uint8_t* frame,size_t bytes){
    if(!frame||bytes!=153600||!a.palette||!a.colors||a.colors>256)return false;
    constexpr size_t count=76800;
    if(!unpack(a.data,a.bytes,frame+count,count))return false;
    for(size_t i=0;i<count;++i){const unsigned n=frame[count+i];if(n>=a.colors)return false;
        const uint16_t v=a.palette[n];frame[i*2]=uint8_t(v);frame[i*2+1]=uint8_t(v>>8);}
    return true;
}
#ifndef IBT_PACKED_PIXELS_HOST
// A single optional PSRAM-only cache, used exclusively by the display owner.
// No files, mutexes, internal-RAM fallback or decoder-task operations.
struct Cache {uint8_t* frame=nullptr;const void* loaded=nullptr;};
inline Cache& cache(){static Cache value;return value;}
inline uint8_t* frame(){
    auto& frame=cache().frame;
    constexpr size_t bytes=320u*240u*2u;
    if(!frame){
        if(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<bytes+262144)return nullptr;
        frame=static_cast<uint8_t*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(frame&&heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<262144){heap_caps_free(frame);frame=nullptr;}
    }
    return frame;
}
inline const uint16_t* pixels(const Asset& asset){
    auto* p=frame();if(!p)return nullptr;auto& loaded=cache().loaded;
    if(loaded!=&asset){loaded=nullptr;if(!unpack(asset.data,asset.bytes,p,153600))return nullptr;loaded=&asset;}
    return reinterpret_cast<const uint16_t*>(p);
}
inline const uint16_t* pixels(const IndexedAsset& asset){
    auto* p=frame();if(!p)return nullptr;auto& loaded=cache().loaded;
    if(loaded!=&asset){loaded=nullptr;if(!unpackIndexed(asset,p,153600))return nullptr;loaded=&asset;}
    return reinterpret_cast<const uint16_t*>(p);
}
#endif
}
