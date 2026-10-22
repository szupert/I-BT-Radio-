#pragma once
#include "videoton_face.h"
#include "service_asset.h"
namespace serviceface {
struct View {uint32_t hz,step,rate;uint64_t frames;uint32_t heap,largest;unsigned wave,channel,route,stage,fault,denied,btRate;int c0,c1;bool info;};
template<class LCD> void field(LCD& lcd,const uint16_t* plate,videotono::Rect r,const char* value,bool large=false,uint16_t color=0x07ff){
 const auto& f=large&&r.h>=18&&videotono::width(videotono::font18,value)<=r.w?videotono::font18:r.h>=11&&videotono::width(videotono::font11,value)<=r.w?videotono::font11:videotono::font7;
 videotono::field(lcd,plate,r,value,f,color,true);
}
template<class LCD> void paint(LCD& lcd,const uint16_t* plate,const View& v,int db,bool full,bool background=true){
 char text[64];
 if(full){
  if(background)for(int y=0;y<240;++y)lcd.drawRGBBitmap(0,y,plate+y*320,320,1);
  lcd.drawRoundRect(v.info?162:6,26,152,24,2,0x07ff);
  if(!v.info){
   snprintf(text,sizeof(text),"%lu Hz",(unsigned long)v.hz);field(lcd,plate,{13,66,108,22},text,true);
   snprintf(text,sizeof(text),"+/- %lu Hz",(unsigned long)v.step);field(lcd,plate,{13,91,108,12},text);
   const char* names[]={"SINE","SQUARE","RAMP +","RAMP -","TRIANGLE"};field(lcd,plate,{216,62,88,15},names[v.wave%5]);
   int last=0;for(int i=0;i<80;++i){int n=0;switch(v.wave%5){case 0:n=int(8*sin(i*6.2831853/79));break;case 1:n=i<40?8:-8;break;case 2:n=i*16/79-8;break;case 3:n=8-i*16/79;break;default:n=i<40?i*16/39-8:8-(i-40)*16/39;}
    if(i)lcd.drawLine(219+i-1,91-last,219+i,91-n,0x07ff);last=n;}
   snprintf(text,sizeof(text),"%d dBFS",db);field(lcd,plate,{63,117,92,19},text);
   const char* channels[]={"L+R","L","R"};field(lcd,plate,{248,117,59,19},channels[v.channel%3]);
   snprintf(text,sizeof(text),"%lu",(unsigned long)v.rate);field(lcd,plate,{65,149,84,17},text);
   lcd.drawRoundRect(v.route?270:219,147,v.route?42:45,22,2,0x07ff);
  }
  lcd.fillCircle(126,208,2,v.stage==3?0x07e0:0x4208);
  lcd.fillCircle(230,208,2,v.stage==3?0x4208:0xf800);
 }
 if(v.info){
  // Restore the complete diagnostic panel on an explicit value change.
  lcd.fillRect(5,55,310,115,0x0841);
 }
 // Status updates touch only this strip, preserving the metal and button faces.
 snprintf(text,sizeof(text),"S%u F%u D%u %llu frames UNCAL",v.stage,v.fault,v.denied,(unsigned long long)v.frames);
 field(lcd,plate,{9,175,302,9},text,false,0xfd20);
}
}
