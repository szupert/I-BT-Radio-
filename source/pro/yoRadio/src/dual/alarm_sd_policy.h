#pragma once
#include <stddef.h>
#include <stdint.h>

namespace alarmsd {
enum class EndEvent : uint8_t { None, Complete, Failed };
// Even a sub-second valid file is complete. An empty/undecodable file is not
// a successful EOF and must never create an automatic repeat loop.
constexpr EndEvent endEvent(bool decodedAudio){return decodedAudio?EndEvent::Complete:EndEvent::Failed;}
// The accented folder is canonical. The ASCII spelling works on cards whose
// host cannot create accented FAT names. Only direct children are selected.
constexpr bool inFolder(const char* path,size_t length) {
    if(!path||!length)return false;
    const char* const folders[]={"/Ébresztő/","/ébresztő/","/Ebreszto/","/ebreszto/"};
    for(const char* prefix:folders){
        size_t n=0;while(prefix[n])++n;
        if(length<=n)continue;
        bool same=true;
        for(size_t i=0;i<n;++i)if(path[i]!=prefix[i]){same=false;break;}
        if(!same)continue;
        for(size_t i=n;i<length;++i)if(path[i]=='/'){same=false;break;}
        if(same)return true;
    }
    return false;
}
// 0=sequential loop, 1=random loop, 2=one file then stop.
constexpr size_t next(size_t current,size_t count,uint8_t mode,uint32_t randomValue) {
    if(!count||current>=count||mode==2)return count;
    if(mode!=1)return (current+1)%count;
    if(count==1)return 0;
    // Choose among all *other* tracks, so shuffle never immediately repeats.
    const size_t pick=randomValue%(count-1);
    return pick>=current?pick+1:pick;
}
}
