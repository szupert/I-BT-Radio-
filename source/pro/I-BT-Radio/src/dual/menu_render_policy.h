#pragma once
#include <stdint.h>
namespace menurender {
// The PCM gate remains mandatory. Only a BT menu with actual headroom can
// replace the indirect, retriggerable audio-loop-gap quiet period.
inline bool recoveryReady(bool service,bool playing,bool bt,bool menu,bool pcmSafe,bool quietExpired){
    return service||!playing||quietExpired||(bt&&menu&&pcmSafe);
}
// Explicit menu requests and pending slices have already been paced by input
// or the test action scheduler. Never add the periodic idle refresh delay.
// This only selects cadence: PCM headroom and quiet admission remain outside.
inline bool due(bool menu,bool redraw,bool continuation,bool scanRequest,bool stress,uint32_t elapsed,uint32_t interval){
    return continuation||scanRequest||(menu&&redraw)||(!stress&&redraw)||elapsed>=interval;
}
inline unsigned viewport(unsigned selected,unsigned count,unsigned visible,unsigned previous,bool reset){
    if(!visible||count<=visible)return 0;
    const unsigned last=count-visible;
    if(selected>=count)selected=count-1;
    if(previous>last)previous=last;
    if(!reset&&selected>=previous&&selected-previous<visible)return previous;
    const unsigned centered=selected>visible/2?selected-visible/2:0;
    return centered<last?centered:last;
}
// UI-thread-owned continuation. One call clears <=40 scanlines, paints
// the header, or visits one row. No task/notification/priority change.
struct Slices {
    bool active=false,layout=false,header=false;
    unsigned page=0,item=0,row=0,clearY=0;
    void cancel(){*this=Slices{};}
    void begin(unsigned p,unsigned i,bool structural,bool changed,unsigned height){
        if(!active||page!=p||item!=i||structural||changed){
            const bool same=active&&page==p&&!structural;
            // After the first content row, cached unchanged rows need no layout repaint.
            // Clearing/header and selection-change work must still finish.
            const bool carry=same&&layout&&(!header||!row||item!=i);
            const unsigned cleared=same?clearY:height;
            *this=Slices{};active=true;page=p;item=i;
            layout=structural||carry;clearY=structural?0:cleared;
        }
    }
    unsigned clearRows(unsigned height)const{
        return clearY>=height?0:(height-clearY>40?40:height-clearY);
    }
};
struct Wait {
    bool active=false;unsigned page=0,item=0;
    uint32_t since=0,last=0,headroomChecks=0,quietChecks=0,cadenceChecks=0,maxLoopMs=0;
    // Overlapping wall-time buckets, attributed to the previous observation.
    // They explain admission delay; none is task CPU time or an audio verdict.
    uint32_t headroomMs=0,quietMs=0,cadenceMs=0,gapStart=0,gapEvents=0;
    uint8_t previousBlocked=0;
    void observe(uint32_t now,unsigned p,unsigned i,bool pending,bool pcmSafe,bool recovery,bool cadence,uint32_t gaps=0){
        if(!pending){active=false;return;}
        if(!active||page!=p||item!=i){*this=Wait{};active=true;page=p;item=i;since=last=now;gapStart=gaps;}
        const uint32_t gap=now-last;if(gap>maxLoopMs)maxLoopMs=gap;last=now;
        if(previousBlocked&1)headroomMs+=gap;
        if(previousBlocked&2)quietMs+=gap;
        if(previousBlocked&4)cadenceMs+=gap;
        previousBlocked=(!pcmSafe?1u:0u)|(!recovery?2u:0u)|(!cadence?4u:0u);
        gapEvents=gaps-gapStart;
        if(!pcmSafe)++headroomChecks;
        if(!recovery)++quietChecks;
        if(!cadence)++cadenceChecks;
    }
    uint32_t elapsed(uint32_t now)const{return active?uint32_t(now-since):0;}
};
}
