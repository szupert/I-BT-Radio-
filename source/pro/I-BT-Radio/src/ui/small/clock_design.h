#pragma once
#include <cstring>
#include <algorithm>
#include "clock_sensor_icons.h"
#include <cstdio>
#include <cstdlib>
#include "clock_art_font.h"
#include "clock_native_digits.h"
#include "clock_layer_assets.h"
namespace clockart {
constexpr uint16_t bg=0x0863,fg=0xff9d,accent=0xfd88,card=0x1926,edge=0x52cc;
static bool photoBackdrop=false,homeTypography=false;
static unsigned family=0;static bool compactClock=false;
static uint16_t clockInk=fg,textInk=fg;
inline void style(unsigned f,unsigned clockColor,unsigned textColor){
 static constexpr uint16_t colors[]={0xff9d,0xffff,0xfd20,0x07ff,0x07e0,0x5dff,0xf81f,0xf800,0xbdf7};
 family=f<4?f:0;clockInk=colors[clockColor<9?clockColor:0];textInk=colors[textColor<9?textColor:0];
}
inline uint16_t backgroundPixel(int x,int y){return photoBackdrop&&x>=0&&x<320&&y>=0&&y<240?clocklayers::palette[clocklayers::background[y*320+x]]:bg;}
template<class LCD> void restore(LCD&lcd,int x,int y,int width,int height){
 uint16_t scan[320];for(int row=y;row<y+height&&row<240;++row){if(row<0)continue;int left=x<0?0:x,right=x+width>320?320:x+width;for(int col=left;col<right;++col)scan[col-left]=backgroundPixel(col,row);if(right>left)lcd.drawRGBBitmap(left,row,scan,right-left,1);}
}
inline uint32_t utf(const char*&s){unsigned c=(unsigned char)*s++;if(c<128)return c;unsigned n=(c&0xe0)==0xc0?1:2;c&=n==1?31:15;while(n--&&*s)c=(c<<6)|((unsigned char)*s++&63);return c;}
inline int width(const char*s,unsigned size){
 int w=0;while(*s){const auto cp=utf(s);
  if(homeTypography&&size<=16){auto g=clocknative::glyph(family,size<=12?12:14,cp);if(g)w+=g->advance;}
  else{auto g=glyph(size,cp);if(g)w+=g->advance;}
 }return w;
}
inline uint16_t blend(uint16_t a,uint16_t b,unsigned k){return ((((a>>11)*k+(b>>11)*(15-k))/15)<<11)|(((((a>>5)&63)*k+((b>>5)&63)*(15-k))/15)<<5)|(((a&31)*k+(b&31)*(15-k))/15);}
template<class LCD> void text(LCD&lcd,int x,int y,int limit,const char*s,unsigned size,uint16_t color=fg,uint16_t back=bg,bool center=false){
 if(homeTypography&&size<=16){
  if(color==fg)color=textInk;
  auto nativeWidth=[&](){int total=0;const char*p=s;while(*p){auto g=clocknative::glyph(family,size<=12?12:14,utf(p));if(g)total+=g->advance;}return total;};
  const int right=x+limit;if(center)x+=std::max(0,(limit-nativeWidth())/2);uint16_t scan[104];
  while(*s){auto g=clocknative::glyph(family,size<=12?12:14,utf(s));if(!g)continue;if(x<0||x+int(g->w)>right||x+int(g->w)>320)break;clocknative::Reader alpha(g->offset);
   for(unsigned row=0;row<g->h;++row){for(unsigned col=0;col<g->w;++col)scan[col]=blend(color,photoBackdrop&&back==bg?backgroundPixel(x+col,y+row):back,alpha.next());if(y+int(row)>=0&&y+int(row)<240)lcd.drawRGBBitmap(x,y+row,scan,g->w,1);}x+=g->advance;
  }return;
 }
 if(homeTypography&&color==fg)color=textInk;
 const int right=std::min(320,x+limit);
 if(center)x+=std::max(0,(limit-width(s,size))/2);
 uint16_t scan[96];
 while(*s){auto g=glyph(size,utf(s));if(!g)continue;
  if(x+g->w>right||x+g->w>320)break;
  if(x>=0)for(unsigned row=0;row<g->h&&y+int(row)<240;++row){
   for(unsigned col=0;col<g->w;++col){unsigned pos=row*g->w+col;uint8_t v=pixels[g->offset+pos/2];unsigned a=(pos&1)?v&15:v>>4;scan[col]=blend(color,photoBackdrop&&back==bg?backgroundPixel(x+col,y+row):back,a);}
   if(y+int(row)>=0)lcd.drawRGBBitmap(x,y+row,scan,g->w,1);
  }x+=g->advance;
 }
}
// The small menu fonts fit in the existing UI-owner PSRAM scratch (max480px).
// Submit the same glyph rectangle once, rather than one SPI window per row.
// The mutable bitmap overload is synchronous; scratch is reused only after it returns.
template<class LCD> bool textBatch(LCD&lcd,uint16_t*scratch,unsigned capacity,int x,int y,int limit,const char*s,unsigned size,uint16_t color=fg,uint16_t back=bg,bool center=false){
 if(!scratch||capacity<480||homeTypography||(size!=14&&size!=16&&size!=20))return false;
 const char*check=s;while(*check){auto g=glyph(size,utf(check));if(g&&unsigned(g->w)*g->h>capacity)return false;}
 const int right=std::min(320,x+limit);
 if(center)x+=std::max(0,(limit-width(s,size))/2);
 while(*s){auto g=glyph(size,utf(s));if(!g)continue;
  if(x+g->w>right||x+g->w>320)break;
  if(x>=0){
   const unsigned first=y<0?std::min(unsigned(-y),unsigned(g->h)):0;
   const unsigned last=y<240?std::min(unsigned(240-y),unsigned(g->h)):0;
   if(last>first){
    for(unsigned row=first;row<last;++row)for(unsigned col=0;col<g->w;++col){unsigned pos=row*g->w+col;uint8_t v=pixels[g->offset+pos/2];unsigned a=(pos&1)?v&15:v>>4;scratch[(row-first)*g->w+col]=blend(color,photoBackdrop&&back==bg?backgroundPixel(x+col,y+row):back,a);}
    lcd.drawRGBBitmap(x,y+first,scratch,g->w,last-first);
   }
  }x+=g->advance;
 }return true;
}
// Native-size antialiased numerals: no nearest-neighbour enlargement.
inline int clockAdvance(char c){auto g=clocknative::glyph(family,compactClock?72:104,c);return g?g->advance:0;}
inline int clockY(){auto g=clocknative::glyph(family,compactClock?72:104,'0');return 56+(92-int(g?g->h:79))/2;}
inline int clockWidth(const char*s){compactClock=false;int n=0;for(const char*p=s;*p;++p)n+=clockAdvance(*p);if(n>304){compactClock=true;n=0;while(*s)n+=clockAdvance(*s++);}return n;}
template<class LCD> void clockDigit(LCD&lcd,int x,int y,char c,bool visible){
 auto g=clocknative::glyph(family,compactClock?72:104,c);if(!g)return;uint16_t scan[104];clocknative::Reader alpha(g->offset);
 for(unsigned row=0;row<g->h&&y+int(row)<240;++row){
  for(unsigned col=0;col<g->w&&col<104;++col){unsigned a=alpha.next();scan[col]=visible?blend(clockInk,backgroundPixel(x+col,y+row),a):backgroundPixel(x+col,y+row);}
  lcd.drawRGBBitmap(x,y+row,scan,g->w,1);
 }
}
// Native-size antialiased colored miniatures, composited onto the backdrop.
template<class LCD> void sensorIcon(LCD&lcd,int x,int y,const uint16_t*rgb,const uint8_t*alpha,unsigned height){
 uint16_t scan[9];
 for(unsigned row=0;row<height;++row){
  for(unsigned col=0;col<9;++col){unsigned at=row*9+col;scan[col]=blend(rgb[at],backgroundPixel(x+col,y+row),(unsigned(alpha[at])*15+127)/255);}
  lcd.drawRGBBitmap(x,y+row,scan,9,1);
 }
}
template<class LCD> void thermometer(LCD&lcd,int x,int y,uint16_t=fg){sensorIcon(lcd,x,y,clocksensor::thermRGB,clocksensor::thermAlpha,17);}
template<class LCD> void drop(LCD&lcd,int x,int y,uint16_t=fg){sensorIcon(lcd,x,y,clocksensor::dropRGB,clocksensor::dropAlpha,14);}
template<class LCD> void sensorLine(LCD&lcd,int left,int width,const char*label,bool valid,int temperature,unsigned humidity,uint16_t ink=fg){
 // 150 px, one baseline; degree symbol and icons have dedicated footprints.
 const int x=left+(width-156)/2;char value[18];
 text(lcd,x,10,34,label,12,ink);
 thermometer(lcd,x+35,10,ink);
 if(valid)snprintf(value,sizeof(value),"%s%d.%d",temperature<0?"-":"",abs(temperature)/10,abs(temperature)%10);else strcpy(value,"--.-");
 text(lcd,x+47,10,43,value,12,ink);
 lcd.drawCircle(x+93,12,2,ink);drop(lcd,x+101,11,ink);
 if(valid)snprintf(value,sizeof(value),"%u%%",humidity);else strcpy(value,"--%%");
 text(lcd,x+115,10,41,value,12,ink);
}
template<class LCD> void alarmIcon(LCD&lcd,int x,int y,bool enabled){
 uint16_t scan[24];for(unsigned row=0;row<24;++row){for(unsigned col=0;col<24;++col){unsigned pos=row*24+col;uint16_t c=clocklayers::icon[pos];if(!enabled){unsigned grey=(((c>>11)*2+((c>>5)&63)+(c&31)*2)/6);c=uint16_t((grey<<11)|((grey*2)<<5)|grey);}unsigned alpha=(unsigned(clocklayers::alpha[pos])*15+127)/255;scan[col]=blend(c,backgroundPixel(x+col,y+row),alpha);}lcd.drawRGBBitmap(x,y+row,scan,24,1);}
 if(!enabled){lcd.drawLine(x,y+1,x+23,y+22,0x8410);lcd.drawLine(x,y+2,x+23,y+23,0x8410);}
}
template<class LCD> void button(LCD&lcd,int x,int y,int w,int h,const char*label,bool gold,bool focus=false){
 const uint16_t fill=gold?accent:card;
 lcd.fillRoundRect(x,y,w,h,6,fill);lcd.drawRoundRect(x,y,w,h,6,focus?accent:gold?accent:edge);
 const unsigned size=(!strcmp(label,"+")||!strcmp(label,"–"))?32:16;
 if(focus)lcd.drawRoundRect(x+2,y+2,w-4,h-4,4,fg);
 text(lcd,x,y+(h-int(size))/2,w,label,size,gold?bg:fg,fill,true);
}
}
