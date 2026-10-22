#pragma once
#include "os_boot_assets.h"
#include "os_desktop_assets.h"
#include "packed_pixels.h"
#include "../../dual/service_pcm.h"
#include <JPEGDEC.h>
#include <new>
namespace osboot {
// UI owner only; shares the existing guarded PSRAM frame. No internal fallback.
inline uint16_t*& target(){static uint16_t* p=nullptr;return p;}
inline int tile(JPEGDRAW* p){
 if(!target()||p->x<0||p->y<0||p->iWidth<1||p->iHeight<1||p->x+p->iWidth>320||p->y+p->iHeight>240)return 0;
 for(int y=0;y<p->iHeight;++y)memcpy(target()+(p->y+y)*320+p->x,p->pPixels+y*p->iWidth,p->iWidth*2);
 return 1;
}
inline bool load(unsigned kind,bool desktop=false){
 if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<65536+sizeof(JPEGDEC))return false;
 auto* frame=packedpixels::frame();if(!frame)return false;
 if(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<sizeof(JPEGDEC)+262144)return false;
 void* memory=heap_caps_malloc(sizeof(JPEGDEC),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!memory)return false;
 if(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<262144){heap_caps_free(memory);return false;}
 auto* decoder=new(memory)JPEGDEC();target()=reinterpret_cast<uint16_t*>(frame);packedpixels::cache().loaded=nullptr;
 const uint8_t* bytes=desktop?(kind==3?desktop_xp_jpg:desktop_seven_jpg):(kind==3?boot_xp_jpg:boot_seven_jpg);
 const size_t count=desktop?(kind==3?sizeof(desktop_xp_jpg):sizeof(desktop_seven_jpg)):(kind==3?sizeof(boot_xp_jpg):sizeof(boot_seven_jpg));
 bool ok=false;
 if(decoder->openRAM(const_cast<uint8_t*>(bytes),count,tile)){
  decoder->setPixelType(RGB565_LITTLE_ENDIAN);
  ok=decoder->getWidth()==320&&decoder->getHeight()==240&&decoder->decode(0,0,0)!=0;decoder->close();
 }
 decoder->~JPEGDEC();heap_caps_free(memory);if(!ok)target()=nullptr;return ok;
}
constexpr uint16_t dim(uint16_t p,unsigned level){
 return uint16_t((((p>>11)*level/255)<<11)|((((p>>5)&63)*level/255)<<5)|((p&31)*level/255));
}
template<class LCD> __attribute__((noinline)) void region(LCD& lcd,int x,int y,int width,int height,unsigned level=255){
 if(!target())return;uint16_t row[320];const int w=lcd.width(),h=lcd.height();
 const int left=x*w/320,right=(x+width)*w/320,top=y*h/240,bottom=(y+height)*h/240;
 if(w<1||w>320||h<1||h>320||right-left>320)return;
 for(int yy=top;yy<bottom;++yy){for(int xx=left;xx<right;++xx)row[xx-left]=dim(target()[(yy*240/h)*320+xx*320/w],level);
  lcd.drawRGBBitmap(left,yy,row,right-left,1);if(height>120&&((yy-top)&15)==15)vTaskDelay(1);}
}
template<class LCD> void rect(LCD& lcd,int x,int y,int w,int h,uint16_t c){lcd.fillRect(x*lcd.width()/320,y*lcd.height()/240,w*lcd.width()/320,h*lcd.height()/240,c);}
template<class LCD> void animate(LCD& lcd,unsigned kind,uint32_t elapsed){
 if(kind==4){
  if(elapsed<3200){
   rect(lcd,80,12,160,113,0);
   if(elapsed<2400){const unsigned radius=(2400-elapsed)*60/2400;static constexpr uint16_t colors[]={0xfa44,0x87e5,0xff06,0x249f};
    for(unsigned i=0;i<4;++i){const uint32_t phase=(uint32_t(i)<<30)+uint32_t(uint64_t(elapsed)*0x100000000ull/2200);
     const int x=160+serviceui::sine(phase)*int(radius)/32768;
     const int y=75+serviceui::sine(phase+0x40000000u)*int(radius)*2/(32768*3);
     lcd.fillCircle(x*lcd.width()/320,y*lcd.height()/240,6,dim(colors[i],35));
     lcd.fillCircle(x*lcd.width()/320,y*lcd.height()/240,4,dim(colors[i],100));
     lcd.fillCircle(x*lcd.width()/320,y*lcd.height()/240,2,colors[i]);}
   }else region(lcd,80,12,160,113,(elapsed-2400)*255/800);
  }else region(lcd,80,12,160,113,244+unsigned((serviceui::sine(uint32_t(uint64_t(elapsed)*0x100000000ull/2600))+32768)*11/65536));
 }
 // Replace the entire loader ROI so baked-in segment glow cannot remain.
 const int fx=kind==3?121:124,fy=kind==3?172:161,fw=kind==3?78:72,fh=kind==3?10:7;
 rect(lcd,fx-3,fy-3,fw+6,fh+6,0);
 lcd.drawRoundRect(fx*lcd.width()/320,fy*lcd.height()/240,fw*lcd.width()/320,fh*lcd.height()/240,2,0x8410);
 const int x=fx+2,y=fy+2,w=fw-4,h=fh-4;
 const int phase=int((elapsed%(kind==3?1800u:2400u))*unsigned(w+24)/(kind==3?1800u:2400u))-24;
 for(int segment=0;segment<(kind==3?3:1);++segment){const int a=phase+segment*7,b=a+(kind==3?6:16);const int left=a<0?0:a,right=b>w?w:b;
  if(right>left){rect(lcd,x+left,y,right-left,h,kind==3?0x04ff:0x4eff);rect(lcd,x+left,y+1,right-left,1,0xbfff);}}
}
}
