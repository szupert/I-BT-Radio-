#pragma once
#include <stdint.h>
namespace lightcontrols {
// UI-owned feedback. Time starts when the guarded painter actually displays it,
// so a short tap is not lost while audio headroom postpones an LCD update.
struct Feedback {
 int8_t pending=-1,shown=-1;uint32_t until=0;
 void press(int index){if(index>=0&&index<5)pending=int8_t(index);}
 int advance(uint32_t now){
  if(pending>=0){shown=pending;pending=-1;until=now+350u;}
  else if(shown>=0&&int32_t(now-until)>=0)shown=-1;
  return shown;
 }
 bool active()const{return pending>=0||shown>=0;}
 void clear(){pending=shown=-1;until=0;}
};
constexpr uint16_t background=0x0000,panel=0x08e4,border=0x2229,
 accent=0x04ff,ink=0xffff,pressedFill=0x07e0,playingFill=0x0224;
template<class Display,class Text>
void paint(Display& lcd,int x,int y,int w,int h,unsigned id,bool playing,bool pressed,bool bt,Text label){
 const uint16_t fill=pressed?pressedFill:id==1&&playing?playingFill:panel;
 const uint16_t fg=pressed?background:ink,edge=pressed?ink:id==1?accent:border;
 lcd.fillRect(x,y,w,h,background);
 lcd.fillRoundRect(x+3,y+3,w-6,h-6,8,fill);
 lcd.drawRoundRect(x+3,y+3,w-6,h-6,8,edge);
 const int cx=x+w/2,cy=y+13;
 if(id==0||id==2){
  const int sign=id==0?-1:1;
  lcd.fillRect(cx+sign*8-1,cy-5,2,11,fg);
  for(int col=0;col<8;++col)lcd.fillRect(cx+sign*(col-5),cy-5+col/2,1,11-col,fg);
 }else if(id==1){
  if(playing){lcd.fillRect(cx-5,cy-5,3,11,fg);lcd.fillRect(cx+2,cy-5,3,11,fg);}
  else for(int col=0;col<9;++col)lcd.fillRect(cx-3+col,cy-5+col/2,1,11-col,fg);
 }else if(id==3){
  lcd.drawLine(cx-8,cy,cx+8,cy,fg);lcd.drawLine(cx,cy-5,cx,cy+5,fg);
  lcd.fillRect(cx-10,cy-2,5,5,fg);lcd.fillRect(cx+5,cy-7,5,5,fg);lcd.fillRect(cx+5,cy+3,5,5,fg);
 }else for(int row=0;row<3;++row)lcd.fillRect(cx-8,cy-5+row*5,16,2,fg);
 const char* text=id==0?"Előző":id==1?(playing?"Szünet":"Indít"):id==2?"Követk.":id==3?"Forrás":bt?"Scan":"Menü";
 label(cx,y+h-16,text,fg,w-8);
}
}
