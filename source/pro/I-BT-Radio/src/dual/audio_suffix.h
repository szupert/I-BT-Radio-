#pragma once
#include <string.h>
#include <ctype.h>
namespace ibtaudio {
inline bool exactAudioSuffix(const char* base,const char* suffix){
    if(!base||!suffix)return false;
    size_t end=strlen(base),length=strlen(suffix);
    while(end&&isspace(static_cast<unsigned char>(base[end-1])))--end;
    return end>=length&&!memcmp(base+end-length,suffix,length);
}
}
