#pragma once
#include <stdint.h>
namespace uiinteraction {
// A contact keeps ownership until release, even after the page/skin changes.
// Returning to the start point does not turn a drag back into a tap.
struct Contact {
 int x=0,y=0;unsigned page=0,skin=0,rotation=0;
 uint32_t blockedAt=0;bool cooling=false,moved=false,consumed=false;
 void begin(int px,int py,unsigned p,unsigned s,unsigned r,uint32_t now){
  x=px;y=py;page=p;skin=s;rotation=r;moved=false;
  consumed=cooling&&uint32_t(now-blockedAt)<200;
  if(!consumed)cooling=false;
 }
 void observe(int px,int py){if(px-x>12||px-x< -12||py-y>12||py-y< -12)moved=true;}
 bool tap(unsigned currentPage)const{return !moved&&!consumed&&currentPage==page;}
 void consume(){consumed=true;}
 void finish(uint32_t now){if(consumed){blockedAt=now;cooling=true;}}
};
constexpr bool verticalDrag(int dx,int dy){return (dy>=16||dy<=-16)&&dy*dy>dx*dx;}
constexpr unsigned scroll(unsigned selected,unsigned count,int delta){return !count?0:delta<0?(unsigned(-delta)>selected?0:selected-unsigned(-delta)):(selected+unsigned(delta)>=count?count-1:selected+unsigned(delta));}
constexpr unsigned spectrum(unsigned index,const unsigned char* bins){return index>=94?bins[47]:(index%2?(unsigned(bins[index/2])+bins[index/2+1]+1)/2:bins[index/2]);}
}
