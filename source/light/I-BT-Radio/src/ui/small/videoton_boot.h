#pragma once
#include "videoton_face.h"
#include "../../dual/boot_progress.h"
namespace videotono {
// Early boot: streaming flash RLE, bounded 1280-byte scanlines, no heap/PSRAM.
template<class LCD,class Yield> void bootImage(LCD& lcd,int w,int h,Yield yield){
 if(w<1||w>320||h<1||h>320)return;
 uint16_t row[320],out[320];unsigned run=0,left=0,color=0;int sourceY=-1;
 for(int y=0;y<h;++y){
  const int target=y*240/h;
  while(sourceY<target){
   for(int x=0;x<320;++x){if(!left){left=bootRuns[run++];color=bootRuns[run++];}row[x]=color;--left;}
   ++sourceY;
  }
  for(int x=0;x<w;++x)out[x]=row[x*320/w];
  lcd.drawRGBBitmap(0,y,out,w,1);if((y&15)==15)yield();
 }
}
template<class LCD> void solidText(LCD& lcd,Rect r,const char* value,const Font& f,uint16_t ink,uint16_t bg){
 uint16_t scan[320];if(r.w<1||r.w>320||r.x<0||r.y<0||r.x+r.w>320||r.y+r.h>240)return;
 const int start=(r.w-width(f,value))/2,top=(r.h-int(f.h))/2;
 for(int y=0;y<r.h;++y){
  for(int x=0;x<r.w;++x)scan[x]=bg;
  if(y>=top&&y<top+f.h){int x=start;const char* p=value;while(*p){const auto& g=glyph(f,next(p));for(unsigned xx=0;xx<g.w;++xx){int dx=x+int(xx);unsigned bit=unsigned(y-top)*g.w+xx;if(dx>=0&&dx<r.w)scan[dx]=blend(bg,ink,(f.data[g.offset+bit/4]>>(6-2*(bit%4)))&3);}x+=g.advance;}}
  lcd.drawRGBBitmap(r.x,r.y+y,scan,r.w,1);
 }
}
template<class LCD> void bootField(LCD& lcd,Rect r,const char* value,const Font& f){
 if(r.x<0||r.y<0||r.w<1||r.w>320||r.x+r.w>320||r.y+r.h>240)return;
 uint16_t scan[320];unsigned run=0,left=0,color=0;
 auto advance=[&](unsigned n){while(n){if(!left){left=bootRuns[run++];color=bootRuns[run++];}const unsigned take=n<left?n:left;n-=take;left-=take;}};
 advance(unsigned(r.y*320+r.x));
 const int start=(r.w-width(f,value))/2,top=(r.h-int(f.h))/2;
 for(int y=0;y<r.h;++y){
  for(int x=0;x<r.w;++x){if(!left){left=bootRuns[run++];color=bootRuns[run++];}scan[x]=color;--left;}
  if(y>=top&&y<top+f.h){int x=start;const char* p=value;while(*p){const auto& g=glyph(f,next(p));for(unsigned xx=0;xx<g.w;++xx){const int dx=x+int(xx);const unsigned bit=unsigned(y-top)*g.w+xx;if(dx>=0&&dx<r.w)scan[dx]=blend(scan[dx],green,(f.data[g.offset+bit/4]>>(6-2*(bit%4)))&3);}x+=g.advance;}}
  lcd.drawRGBBitmap(r.x,r.y+y,scan,r.w,1);if(y+1<r.h)advance(320-r.w);
 }
}
template<class LCD> void bootProgress(LCD& lcd,unsigned stage,bool connected,bool offline,unsigned tick,unsigned focus){
 const unsigned done=bootprogress::fill(stage,connected,offline);
 static const char* labels[]={"DISPLAY","CONFIG","HARDWARE","AUDIO","WI-FI","SD LIST","CONTROLS","READY"};
 const bool waiting=!connected&&!offline&&stage>=4;
 // Restore the whole original text halo, including the formerly missed top row.
 bootField(lcd,{90,124,140,17},waiting?"WI-FI CONNECTING":labels[done],font7);
 // Preserve the original thin luminous bar style; restore before every update
 // so the wait cursor never leaves pixels when a phase or connection changes.
 bootField(lcd,{96,141,128,12},"",font7);
 lcd.fillRect(103,143,114,6,0x0121);
 lcd.fillRect(104,144,112,4,0x1ae9);lcd.fillRect(105,145,110,2,0x0020);
 const unsigned filled=done*110/7;
 if(filled){lcd.fillRect(105,145,filled,1,0x77fa);lcd.fillRect(105,146,filled,1,green);}
 if(waiting){const unsigned room=110-filled;const unsigned cursor=tick%(room>3?room-3:1);lcd.fillRect(105+filled+cursor,145,3,2,green);}
 (void)focus; // The approved boot artwork has no extra row of indicator LEDs.
}
}
