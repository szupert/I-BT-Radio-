#pragma once
#include "videoton_assets.h"
#include "videoton_font.h"
#include "../videoton_layout.h"
#include <stddef.h>
namespace videotono {
constexpr uint16_t green=0x47f5,red=0xf92c,glass=0x0020;
inline const Glyph& glyph(const Font& font,uint32_t c){for(unsigned i=0;i<font.count;++i)if(font.glyphs[i].code==c)return font.glyphs[i];return font.glyphs['?'-32];}
inline uint32_t next(const char*& p){
 const unsigned char* b=reinterpret_cast<const unsigned char*>(p);uint32_t c=*b;++p;
 if(c<128)return c;
 if((c&224)==192&&b[1]&&(b[1]&192)==128){++p;return ((c&31)<<6)|(b[1]&63);}
 if((c&240)==224&&b[1]&&b[2]&&(b[1]&192)==128&&(b[2]&192)==128){p+=2;return ((c&15)<<12)|((b[1]&63)<<6)|(b[2]&63);}
 return '?';
}
inline int width(const Font& f,const char* p){int n=0;while(*p)n+=glyph(f,next(p)).advance;return n?n-1:0;}
inline uint16_t blend(uint16_t a,uint16_t b,unsigned k){
 const unsigned r=(((a>>11)&31)*(3-k)+((b>>11)&31)*k+1)/3;
 const unsigned g=(((a>>5)&63)*(3-k)+((b>>5)&63)*k+1)/3;
 const unsigned v=((a&31)*(3-k)+(b&31)*k+1)/3;
 return uint16_t((r<<11)|(g<<5)|v);
}
template<class LCD> void restore(LCD& lcd,const uint16_t* plate,Rect r){
 if(!plate||r.x<0||r.y<0||r.w<1||r.h<1||r.x+r.w>320||r.y+r.h>240)return;
 for(int y=0;y<r.h;++y)lcd.drawRGBBitmap(r.x,r.y+y,plate+(r.y+y)*320+r.x,r.w,1);
}
// Scanline composition, no background rectangles, heap or work in audio tasks.
template<class LCD> void field(LCD& lcd,const uint16_t* plate,Rect r,const char* value,const Font& f,uint16_t color=green,bool center=false){
 if(!plate||r.w<1||r.w>320||r.h<1||r.x<0||r.y<0||r.x+r.w>320||r.y+r.h>240)return;
 uint16_t scan[320];const int ty=(r.h-int(f.h))/2;int total=width(f,value);const int start=center&&total<r.w?(r.w-total)/2:0;
 for(int y=0;y<r.h;++y){
  for(int x=0;x<r.w;++x)scan[x]=plate[(r.y+y)*320+r.x+x];
  if(y>=ty&&y<ty+f.h){int x=start;const char* p=value;
   while(*p){const auto& g=glyph(f,next(p));if(x+g.w>r.w)break;
    for(unsigned xx=0;xx<g.w;++xx){unsigned bit=unsigned(y-ty)*g.w+xx;unsigned alpha=(f.data[g.offset+bit/4]>>(6-2*(bit%4)))&3;scan[x+xx]=blend(scan[x+xx],color,alpha);}
    x+=g.advance;
   }
  }
  lcd.drawRGBBitmap(r.x,r.y+y,scan,r.w,1);
 }
}
template<class LCD> void textFit(LCD& lcd,const uint16_t* plate,Rect r,const char* value,bool large=false,uint16_t color=green){
 const Font& f=large&&width(font18,value)<=r.w?font18:width(font14,value)<=r.w?font14:width(font11,value)<=r.w?font11:width(font9,value)<=r.w?font9:font7;
 field(lcd,plate,r,value,f,color,true);
}
inline const Font& infoFont(const char* value,int w){return width(font11,value)<=w?font11:width(font9,value)<=w?font9:font7;}
inline const Font& presetFont(const char* value){return width(font18,value)<=34?font18:infoFont(value,34);}
template<class LCD> void play(LCD& lcd,const uint16_t* plate,bool running){
 restore(lcd,plate,{70,207,24,11});
 if(running){lcd.fillRect(79,209,3,8,0x1082);lcd.fillRect(85,209,3,8,0x1082);}
 else lcd.fillTriangle(79,208,79,218,89,213,0x1082);
}
template<class LCD> void level(LCD& lcd,const uint16_t* plate,unsigned n){
 restore(lcd,plate,{225,210,79,9});if(n>10)n=10;
 for(unsigned i=0;i<n;++i)lcd.fillRect(226+int(i)*8,212,6,6,green);
}
template<class LCD> void sourceLeds(LCD& lcd,const uint16_t* plate,unsigned active){
 for(unsigned i=0;i<4;++i){
  const Rect r=sourceLed[i];restore(lcd,plate,r);
  if(i==active)lcd.fillRect(r.x+1,r.y+1,r.w-2,1,0x07e0);
 }
}
}
