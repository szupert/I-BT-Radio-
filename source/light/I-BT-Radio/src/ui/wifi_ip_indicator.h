#pragma once
#include <stdint.h>
namespace wifiindicator {
enum class Tone:uint8_t {None,Red,Yellow,Green};
struct State {uint8_t bars;Tone tone;unsigned key()const{return bars|(unsigned(tone)<<3);}};
inline bool validIp(const char* text){
 if(!text)return false;
 unsigned value=0,digits=0,parts=0,first=0;
 for(unsigned i=0;i<16;++i){const char c=text[i];
  if(c>='0'&&c<='9'){if(++digits>3)return false;value=value*10+unsigned(c-'0');if(value>255)return false;continue;}
  if(c!='.'&&c!=0)return false;
  if(!digits||parts>=4)return false;if(parts==0)first=value;++parts;
  if(c==0)return parts==4&&first>0&&first<224&&first!=127;
  value=digits=0;
 }return false;
}
inline State state(bool connected,const char* ip,int rssi){
 if(!connected||!validIp(ip)||rssi>=0||rssi<-127)return {0,Tone::None};
 const uint8_t bars=rssi>=-55?4:rssi>=-67?3:rssi>=-80?2:1;
 return {bars,rssi>=-67?Tone::Green:rssi>=-80?Tone::Yellow:Tone::Red};
}
inline uint16_t color(Tone tone){return tone==Tone::Green?0x07e0:tone==Tone::Yellow?0xffe0:tone==Tone::Red?0xf800:0;}
// Paint only the existing RSSI area; battery/menu/player geometry is untouched.
template<class Display> void draw(Display& lcd,State value,uint16_t background,uint16_t outline){
 for(unsigned i=0;i<4;++i){const int x=244+int(i)*7;
  const int h=value.bars?4+int(i)*3:5;const int y=value.bars?20-h:11;
  if(i<value.bars)lcd.fillRect(x,y,5,h,color(value.tone));
  else lcd.fillRect(x,y,5,h,background);
  lcd.drawRect(x,y,5,h,outline);
 }
}
inline unsigned connectionKey(State signal,bool bluetooth){return bluetooth?0x100u:signal.key();}
template<class Display> void drawTransport(Display& lcd,State signal,bool bluetooth,uint16_t background,uint16_t outline){
 if(!bluetooth){draw(lcd,signal,background,outline);return;}
 constexpr uint16_t blue=0x001f;
 lcd.fillRect(256,3,2,22,blue);
 // Two-pixel strokes, contained in the existing RSSI slot; no bitmap buffer.
 for(int offset=0;offset<2;++offset){
  lcd.drawLine(256+offset,3,264+offset,9,blue);
  lcd.drawLine(264+offset,9,248+offset,20,blue);
  lcd.drawLine(248+offset,7,264+offset,18,blue);
  lcd.drawLine(264+offset,18,256+offset,24,blue);
 }
}

}
