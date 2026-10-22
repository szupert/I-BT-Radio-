#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
namespace dual {
inline bool ieq(char a,char b) {
    return tolower((unsigned char)a)==tolower((unsigned char)b);
}
inline bool endsWithIgnoreCase(const char *name,const char *suffix) {
    if(!name||!suffix) return false;
    size_t n=strlen(name),s=strlen(suffix);
    if(s>n) return false;
    const char *p=name+n-s;
    for(size_t i=0;i<s;i++) if(!ieq(p[i],suffix[i])) return false;
    return true;
}
inline bool supportedAudioName(const char *name) {
    return endsWithIgnoreCase(name,".mp3")||endsWithIgnoreCase(name,".m4a")||endsWithIgnoreCase(name,".aac")||
           endsWithIgnoreCase(name,".wav")||endsWithIgnoreCase(name,".flac")||
           endsWithIgnoreCase(name,".ogg")||endsWithIgnoreCase(name,".oga")||endsWithIgnoreCase(name,".opus")||
           endsWithIgnoreCase(name,".amr")||endsWithIgnoreCase(name,".awb")||endsWithIgnoreCase(name,".ts")||
           endsWithIgnoreCase(name,".mp1")||endsWithIgnoreCase(name,".mp2");
}
inline void basenameWithoutExtension(char *out,size_t cap,const char *path) {
    if(!cap) return;
    out[0]=0;
    if(!path) return;
    const char *base=strrchr(path,'/');
    base=base?base+1:path;
    size_t n=strlen(base);
    const char *dot=strrchr(base,'.');
    if(dot&&dot!=base) n=size_t(dot-base);
    if(n>=cap) n=cap-1;
    memcpy(out,base,n);
    out[n]=0;
}
inline void formatClock(char *out,size_t cap,uint32_t sec) {
    if(!cap) return;
    snprintf(out,cap,"%02lu:%02lu",(unsigned long)(sec/60),(unsigned long)(sec%60));
}
inline unsigned percentU32(uint32_t part,uint32_t total) {
    return total?unsigned((uint64_t(part)*100u)/total):0u;
}
}
