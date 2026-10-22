#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace ibtbackup {
constexpr size_t kSealSize=44;
constexpr uint8_t kMagic[8]={'I','B','T','B','K','0','0','1'};
inline bool encodeSeal(uint8_t* out,size_t cap,uint32_t bytes,const uint8_t hash[32]){
    if(!out||cap<kSealSize||!hash||!bytes)return false;
    memcpy(out,kMagic,sizeof(kMagic));
    for(unsigned i=0;i<4;i++)out[8+i]=uint8_t(bytes>>(8*i));
    memcpy(out+12,hash,32);
    return true;
}
inline bool decodeSeal(const uint8_t* data,size_t size,uint32_t maxBytes,uint32_t& bytes,uint8_t hash[32]){
    if(!data||size!=kSealSize||!hash||memcmp(data,kMagic,sizeof(kMagic)))return false;
    bytes=uint32_t(data[8])|(uint32_t(data[9])<<8)|(uint32_t(data[10])<<16)|(uint32_t(data[11])<<24);
    if(!bytes||bytes>maxBytes)return false;
    memcpy(hash,data+12,32);
    return true;
}
}
