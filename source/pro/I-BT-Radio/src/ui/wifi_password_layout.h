#pragma once
#include <cstddef>
#include <cstdio>
#include <cstring>
namespace wifipasswordui {
static constexpr unsigned visibilityKey=40;
inline unsigned keyCount(bool wifi){return wifi?41u:40u;}
inline unsigned move(unsigned selected,bool forward,bool wifi){
    const unsigned count=keyCount(wifi);
    return (selected+(forward?1u:count-1u))%count;
}
inline bool visibilityHit(int x,int y,int width){
    return x>=width-90&&x<width-8&&y>=46&&y<82;
}
// A bounded, allocation-free local preview; never used by diagnostics or NVS.
inline void preview(char* out,size_t capacity,const char* password,bool visible,unsigned limit=24){
    if(!out||!capacity)return;
    size_t count=0;while(count<63&&password[count])++count;
    const size_t shown=count<limit?count:limit;
    if(visible)std::snprintf(out,capacity,"%s%.*s",count>shown?"...":"",int(shown),password+count-shown);
    else{const size_t n=shown<capacity-1?shown:capacity-1;std::memset(out,'*',n);out[n]=0;}
}
}
