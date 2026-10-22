#pragma once
#include "weather_asset.h"
#include "weather_cloud.h"
#include "clock_design.h"
#include "../../dual/weather.h"
#include <esp_heap_caps.h>
namespace weatherface {
static uint16_t* canvas=nullptr;
static weather::Solar sun{};
static uint8_t phase=0,conditionClass=255;
inline uint8_t classify(unsigned code,bool known){return !known?255:code>=95?5:((code>=71&&code<=77)||code==85||code==86)?4:code>=51?3:(code==45||code==48)?2:code>0?1:0;}
static uint32_t stamp=UINT32_MAX;
static uint16_t composed(uint16_t c,int x,int y){
 // Smooth readability gradients, never opaque UI rectangles.
 unsigned shade=phase==1?11:12;
 if(x<180&&y<110)shade-=unsigned(180-x)*unsigned(110-y)*3/(180*110);
 if(y>140){const unsigned lower=12-unsigned(y-140)*5/100;if(lower<shade)shade=lower;}
 c=clockart::blend(c,0,shade);
 if(conditionClass==1)c=clockart::blend(0x7bef,c,1);
 if(conditionClass==2)c=clockart::blend(0xc638,c,3);
 if(conditionClass==3||conditionClass==5)c=clockart::blend(0x1086,c,conditionClass==5?5:3);
 if(conditionClass==4)c=clockart::blend(0xb6df,c,2);
 // Sparse static precipitation belongs to the cached background, not a new animation.
 if((conditionClass==3||conditionClass==5)&&((x*11+y*3)%137<2)&&(y%31<8))c=clockart::blend(0xb6df,c,3);
 if(conditionClass==4&&((x*13+y*7)%283==0))c=clockart::blend(0xffff,c,6);
 if(sun.visible){const int dx=x-sun.x,dy=y-sun.y,d=dx*dx+dy*dy;
  if(d<225&&y<135){const unsigned alpha=d<25?12:unsigned(225-d)*8/200;c=clockart::blend(0xffb0,c,alpha);}
 }
 return c;
}
inline uint16_t pixel(int x,int y){if(x<0||x>=320||y<0||y>=240)return 0;return canvas?canvas[y*320+x]:0x0863;}
inline bool prepare(time_t now,const weather::Config& c,unsigned code=0,bool known=false){
 const uint32_t slot=now>=1600000000?uint32_t(now/300):0;
 static double lat=999,lon=999;
 const uint8_t category=classify(code,known);
 if(slot==stamp&&lat==c.latitude&&lon==c.longitude&&category==conditionClass)return false;
 conditionClass=category;
 stamp=slot;lat=c.latitude;lon=c.longitude;sun=weather::solar(now,c.latitude,c.longitude);phase=sun.valid?sun.phase:3;
 if(!canvas)canvas=(uint16_t*)heap_caps_malloc(320*240*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(canvas){
  const uint8_t* packed=weatherart::phases[phase];const uint16_t* palette=weatherart::palettes[phase];unsigned at=0,out=0;
  while(out<320*240){unsigned token=packed[at++];if(token>=128){unsigned count=token-127;uint16_t c=palette[packed[at++]];while(count--&&out<320*240)canvas[out++]=c;}else{unsigned count=token+1;while(count--&&out<320*240)canvas[out++]=palette[packed[at++]];}}
  for(int y=0;y<240;++y){for(int x=0;x<320;++x)canvas[y*320+x]=composed(canvas[y*320+x],x,y);if((y&15)==15)vTaskDelay(1);}
 }
 return true;
}
inline uint16_t background(int x,int y){return pixel(x,y);}
template<class LCD> void restore(LCD&lcd,int x,int y,int width,int height){
 uint16_t row[320];int left=x<0?0:x,right=x+width>320?320:x+width;
 for(int yy=y<0?0:y;yy<y+height&&yy<240;++yy){for(int xx=left;xx<right;++xx)row[xx-left]=pixel(xx,yy);if(right>left)lcd.drawRGBBitmap(left,yy,row,right-left,1);if((yy&15)==15)vTaskDelay(1);}
}
template<class LCD> void text(LCD&lcd,int x,int y,int width,const char*s,unsigned size,uint16_t ink=0xffff,bool center=false){
 const int right=x+width;if(center)x+=(width-clockart::width(s,size))/2;
 uint16_t row[96];
 while(*s){const auto*g=clockart::glyph(size,clockart::utf(s));if(!g)continue;
  if(x<0||x+g->w>right||x+g->w>320)break;
  for(unsigned yy=0;yy<g->h&&y+int(yy)<240;++yy){
   for(unsigned xx=0;xx<g->w;++xx){unsigned pos=yy*g->w+xx;uint8_t v=clockart::pixels[g->offset+pos/2];unsigned a=(pos&1)?v&15:v>>4;row[xx]=clockart::blend(ink,pixel(x+xx,y+yy),a);}
   if(y+int(yy)>=0)lcd.drawRGBBitmap(x,y+yy,row,g->w,1);
  }x+=g->advance;
 }
}
inline void fit(char*out,size_t capacity,const char*source,int available,unsigned size){
 if(!capacity)return;out[0]=0;size_t used=0;const char*p=source;
 while(*p){const char*begin=p;clockart::utf(p);const size_t bytes=size_t(p-begin);if(used+bytes>=capacity){p=begin;break;}memcpy(out+used,begin,bytes);out[used+bytes]=0;if(clockart::width(out,size)>available){out[used]=0;p=begin;break;}used+=bytes;}
 if(*p){while(used&&clockart::width(out,size)+clockart::width("...",size)>available){do{--used;}while(used&&(uint8_t(out[used])&0xc0)==0x80);out[used]=0;}if(used+3<capacity)strcat(out,"...");}
}
template<class LCD> void thermometer(LCD&lcd,int x,int y){
 lcd.drawRoundRect(x+3,y,5,11,2,0xff9a);lcd.fillRect(x+5,y+3,1,10,0xfa69);lcd.fillCircle(x+5,y+13,4,0xfa69);
}
template<class LCD> void droplet(LCD&lcd,int x,int y){lcd.fillTriangle(x,y+8,x+8,y+8,x+4,y,0x45ff);lcd.fillCircle(x+4,y+9,4,0x45ff);}
template<class LCD> void transport(LCD&lcd,int x,int y,unsigned type){
 constexpr uint16_t c=0xffff;
 if(type==1){lcd.fillRect(x+2,y,4,18,c);lcd.fillRect(x+11,y,4,18,c);return;}
 if(type==3){lcd.fillTriangle(x+2,y,x+2,y+18,x+17,y+9,c);return;}
 const bool next=type==2;
 lcd.fillTriangle(x+(next?0:16),y,x+(next?0:16),y+18,x+(next?14:2),y+9,c);
 lcd.fillRect(x+(next?16:0),y,2,18,c);
}
template<class LCD> void symbol(LCD&lcd,unsigned code,bool valid){
 restore(lcd,156,28,82,67);
 if(valid&&code<=2&&phase!=3){
  uint16_t row[34];for(int y=0;y<34;++y){for(int x=0;x<34;++x){const int dx=x-17,dy=y-17,d=dx*dx+dy*dy;unsigned alpha=d<100?15:d<289?unsigned(289-d)*9/189:0;row[x]=clockart::blend(0xffb0,pixel(166+x,29+y),alpha);}lcd.drawRGBBitmap(166,29+y,row,34,1);}
 }
 if(valid&&code==0&&phase==3){
  uint16_t row[28];for(int y=0;y<28;++y){for(int x=0;x<28;++x){int dx=x-14,dy=y-14,d=dx*dx+dy*dy,cut=(x-21)*(x-21)+(y-8)*(y-8);unsigned a=d<121&&cut>100?unsigned(std::min(15,(121-d)*3)):0;row[x]=clockart::blend(0xe71c,pixel(167+x,32+y),a);}lcd.drawRGBBitmap(167,32+y,row,28,1);}
 }
 if(!valid||code>0){
  uint16_t row[58];for(int y=0;y<weathercloud::height;++y){for(int x=0;x<weathercloud::width;++x){const unsigned i=y*weathercloud::width+x;row[x]=clockart::blend(weathercloud::color[i],pixel(171+x,45+y),weathercloud::alpha[i]);}lcd.drawRGBBitmap(171,45+y,row,weathercloud::width,1);}
 }
 if(valid&&code>=51)for(int i=0;i<3;++i){int x=184+i*12;lcd.drawLine(x,83,x-3,90,((code>=71&&code<=77)||code==85||code==86)?0xffff:0x45ff);}
}
inline const char* condition(unsigned code){
 if(code==0)return "Derült";if(code<=3)return "Felhős";if(code==45||code==48)return "Ködös";
 if(code>=95)return "Zivatar";if((code>=71&&code<=77)||code==85||code==86)return "Havazás";return "Eső";
}
}
