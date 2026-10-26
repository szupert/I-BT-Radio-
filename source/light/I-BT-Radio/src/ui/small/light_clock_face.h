#pragma once
#include "light_clock_font.h"
#include "light_alarm_icon.h"
#include <initializer_list>
namespace lightclockface {
constexpr uint16_t bg=0,panel=0x18e6,border=0x2988,white=0xf7bf,cyan=0x869b,hinge=0x6b8f;
template<class LCD> void digits(LCD& lcd,unsigned style,const char* value,bool colon){
 lcd.fillRect(0,40,320,144,bg);
 if(!style){
  for(int x:{10,170}){lcd.fillRoundRect(x,45,140,136,10,panel);lcd.drawRoundRect(x,45,140,136,10,border);}
 }
 const int x[]={style?8:12,style?76:80,style?175:172,style?243:240};
 for(unsigned i=0;i<4;++i)lightclock::character(lcd,x[i],52,68,value[i<2?i:i+1],68,style&&i>=2?cyan:white,style?bg:panel);
 if(!style)for(int left:{10,170}){lcd.fillRect(left+2,112,136,2,bg);for(int side:{left,left+137})lcd.fillRoundRect(side,107,3,12,1,hinge);}
 if(colon)lightclock::character(lcd,151,52,18,':',68,hinge,bg);
}
template<class LCD> void colon(LCD& lcd,bool on){lcd.fillRect(151,40,18,144,bg);if(on)lightclock::character(lcd,151,52,18,':',68,hinge,bg);}
template<class LCD> void bell(LCD& lcd){lightalarmicon::paint(lcd,55,198);}
}
