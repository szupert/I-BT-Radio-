#pragma once
#include <time.h>
#include <stddef.h>
#include <stdio.h>
namespace dual {
inline void formatLocalTime(char *out,size_t cap,const tm &t,bool clock24){
    if(!cap)return;
    if(clock24)strftime(out,cap,"%H:%M",&t);
    else strftime(out,cap,"%I:%M %p",&t);
}
inline bool validEpoch(time_t now){return now>1600000000;}
}
