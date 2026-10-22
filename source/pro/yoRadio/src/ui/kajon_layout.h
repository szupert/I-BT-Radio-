#pragma once
#include "../dual/logic.h"
namespace kajon {
struct Needle { int8_t x0,y0,x1,y1; };
inline constexpr Needle needles[]={{-6,6,-45,47},{-5,6,-43,49},{-5,6,-40,51},{-5,6,-38,53},{-4,7,-35,55},{-4,7,-33,56},{-4,7,-30,58},{-3,7,-27,59},{-3,7,-24,60},{-3,8,-21,61},{-2,8,-18,62},{-2,8,-15,63},{-2,8,-12,64},{-1,8,-9,64},{-1,8,-6,65},{0,8,-3,65},{0,8,0,65},{0,8,3,65},{1,8,6,65},{1,8,9,64},{2,8,12,64},{2,8,15,63},{2,8,18,62},{3,8,21,61},{3,7,24,60},{3,7,27,59},{4,7,30,58},{4,7,33,56},{4,7,35,55},{5,6,38,53},{5,6,40,51},{5,6,43,49},{6,6,45,47}};
inline constexpr unsigned index(unsigned level){
    // Peak mailbox: amplitude-to-dB curve, bounded integer thresholds.
    constexpr uint8_t thresholds[]={2,3,4,5,6,8,10,12,16,20,25,32,40,50,63,80,100,126,159,200,252};
    unsigned i=0;for(auto v:thresholds)if(level>=v)++i;
    return i*32/21;
}
inline constexpr dual::Action hit(dual::Point p){
    using dual::Action;
    if(p.x<0||p.x>=320||p.y<0||p.y>=240)return Action::None;
    if(p.y>=227){if(p.x<80)return Action::Menu;if(p.x>=240)return Action::Source;return Action::None;}
    if(p.y>=173&&p.y<212&&p.x<180){
        if(p.x<49)return Action::Prev;if(p.x<96)return Action::Toggle;
        if(p.x<134)return Action::PlayerPause;return Action::Next;
    }
    return Action::None;
}
inline constexpr bool volume(dual::Point p){return p.x>=208&&p.x<309&&p.y>=184&&p.y<216;}
inline constexpr bool playlist(dual::Point p){return p.x>=8&&p.x<313&&p.y>=113&&p.y<163;}
}
