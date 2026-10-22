#pragma once
#include <stdint.h>
namespace bootsequence {
// Display-owner state only. No task, allocation, delay, network or audio calls.
struct State {
 uint32_t animationMs=0,lastFrame=0,desktopAt=0;
 bool framed=false,desktop=false;
 void reset(){animationMs=lastFrame=desktopAt=0;framed=desktop=false;}
 void frame(uint32_t now){
  if(framed){const uint32_t gap=now-lastFrame;animationMs+=gap>50?50:gap;}
  framed=true;lastFrame=now;
 }
 bool ready(unsigned kind,bool complete)const{return complete&&framed&&animationMs>=(kind==4?3400u:1800u);}
 void shown(uint32_t now){desktop=true;desktopAt=now;}
 bool expired(uint32_t now)const{return desktop&&uint32_t(now-desktopAt)>=2000u;}
};
}
