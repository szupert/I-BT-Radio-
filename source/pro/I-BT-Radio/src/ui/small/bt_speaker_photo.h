#pragma once
#include "videoton_face.h"
#include "bt_speaker_asset.h"
#include "../bt_speaker_layout.h"
namespace btspeakerface {
template<class LCD> void field(LCD& lcd,const uint16_t* plate,videotono::Rect r,const char* text,bool large=false){
 const auto& f=large&&r.h>=18&&videotono::width(videotono::font18,text)<=r.w?videotono::font18:r.h>=14&&videotono::width(videotono::font14,text)<=r.w?videotono::font14:r.h>=11&&videotono::width(videotono::font11,text)<=r.w?videotono::font11:videotono::font7;
 videotono::field(lcd,plate,r,text,f,0x07ff,true);
}
template<class LCD> void play(LCD& lcd,const uint16_t* plate,bool playing){
 videotono::restore(lcd,plate,{113,163,20,14});
 if(playing){lcd.fillRect(116,164,4,12,0x0000);lcd.fillRect(124,164,4,12,0x0000);}
 else lcd.fillTriangle(116,164,116,176,129,170,0x0000);
}
template<class LCD> void volume(LCD& lcd,const uint16_t* plate,unsigned percent){
 if(percent>100)percent=100;videotono::restore(lcd,plate,{60,200,241,18});
 for(unsigned i=0;i<32;++i)lcd.fillRect(61+int(i)*6,202,4,14,i*100<percent*32?0x07ff:0x18c3);
 char text[8];snprintf(text,sizeof(text),"%u%%",percent);field(lcd,plate,{264,200,36,18},text);
}
template<class LCD> void bars(LCD& lcd,const uint16_t* plate,const uint8_t* values,uint8_t* previous,bool full){
 for(unsigned i=0;i<48;++i){const unsigned h=unsigned(values[i/2])*24/16;
  if(!full&&previous[i]==h)continue;
  const int x=42+int(i)*5;videotono::restore(lcd,plate,{x,108,4,24});
  for(unsigned y=0;y<h;y+=2)lcd.fillRect(x,131-int(y),4,1,0x07ff);
  previous[i]=h;
 }
}
}
