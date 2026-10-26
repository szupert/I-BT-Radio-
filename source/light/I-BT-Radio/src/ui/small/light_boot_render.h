#pragma once
#include "light_boot_image.h"
#include "../light_boot_policy.h"
namespace lightboot {
using RowSource=bool(*)(unsigned,uint16_t*);
struct Paint {
 uint16_t row=0;
 uint8_t old[7]={255,255,255,255,255,255,255},focus=255;
 void reset(){*this=Paint{};}
 bool complete(unsigned height)const{return row>=height;}
};
// A single 640-byte stack row, flash source, no heap or framebuffer.
template<class LCD> __attribute__((noinline)) void region(LCD& lcd,int left,int top,int width,int height,RowSource source=nullptr){
 uint16_t line[320];const int w=lcd.width(),h=lcd.height();
 if(w<1||w>320||h<1||h>320)return;
 for(int y=top;y<top+height&&y<h;++y){
  const bool custom=source&&source(unsigned(y*240/h),line);
  for(int x=left;x<left+width&&x<w;++x)line[x-left]=custom?line[x*320/w]:pgm_read_word(lightbootimage::pixels+(y*240/h)*320+x*320/w);
  lcd.drawRGBBitmap(left,y,line,width,1);
 }
}
template<class LCD> bool paint(LCD& lcd,Paint& state,const Signal* signals,uint8_t progress,unsigned focus,RowSource source=nullptr){
 const int w=lcd.width(),h=lcd.height();if(w<1||w>320||h<1||h>320)return true;
 if(!state.complete(h)){
  const unsigned n=unsigned(h-state.row)<sliceRows?unsigned(h-state.row):sliceRows;
  region(lcd,0,state.row,w,n,source);state.row+=n;
  if(!state.complete(h))return false;
 }
 static constexpr uint16_t centers[7][2]={{103,157},{189,157},{275,157},{123,194},{148,194},{173,194},{197,194}};
 static constexpr uint16_t ink[]={0x630c,0xfd20,0x07e0,0xf800};
 if(state.focus!=focus){
  const unsigned previous=state.focus;state.focus=focus;
  state.old[0]=state.old[1]=state.old[2]=255;
  static constexpr int left[]={35,122,208},width[]={81,80,80};
  // Restore the rounded contour, including its arcs, without touching live LEDs.
  for(unsigned i=0;i<3;++i)if(i==previous||i==focus){
   const int x=left[i]*w/320,y=141*h/240,r=(left[i]+width[i])*w/320,b=172*h/240;
   const int radius=7*w/320,band=radius+1;
   region(lcd,x,y,r-x,band,source);region(lcd,x,b-band,r-x,band,source);
   region(lcd,x,y+band,band,b-y-2*band,source);region(lcd,r-band,y+band,band,b-y-2*band,source);
   if(i==focus)lcd.drawRoundRect(x,y,r-x,b-y,radius,0x07ff);
  }
 }
 for(unsigned i=0;i<7;++i){
  const uint8_t value=i<3?uint8_t(signals[i]):uint8_t(progress&(1u<<(i-3))?Signal::Ready:Signal::Off);
  if(state.old[i]==value)continue;state.old[i]=value;
  const int x=centers[i][0]*w/320,y=centers[i][1]*h/240,r=i<3?4:5;
  lcd.fillCircle(x,y,r+1,0x2104);lcd.fillCircle(x,y,r,ink[value]);
 }
 return true;
}
}
