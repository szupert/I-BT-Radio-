#pragma once
#include <stdint.h>
#include "../dual/alarm_policy.h"
namespace alarmui {
constexpr bool menu(unsigned page){return (page>=59&&page<=64)||page==68;}
constexpr unsigned visible(unsigned count){return count<4?count:4;}
constexpr unsigned first(unsigned selected,unsigned count){
 return count<=4?0:selected<2?0:selected+2>=count?count-4:selected-2;
}
constexpr int rowTop=64,rowHeight=44;
constexpr uint8_t adjust(uint8_t value,int delta,unsigned modulus){
 return uint8_t((int(value)+delta+int(modulus))%int(modulus));
}
struct TimeDraft {
 bool ending=false;uint8_t hour=0,minute=0;
 constexpr void change(unsigned field,int delta){if(field==0)hour=adjust(hour,delta,24);else if(field==1)minute=adjust(minute,delta,60);}
 constexpr unsigned rotate(unsigned field,int delta){
  if(field<2){change(field,delta);return field;}
  return unsigned((int(field)+delta+4)%4);
 }
 constexpr bool commit(alarmclock::Setting &setting) const {
  auto candidate=setting;
  if(ending){candidate.offHour=hour;candidate.offMinute=minute;}
  else{candidate.onHour=hour;candidate.onMinute=minute;}
  if(!alarmclock::validWindow(candidate))return false;
  setting=candidate;return true;
 }
};
constexpr void toggle(alarmclock::Setting& a){if(a.mode){a.enabledProfile=a.mode;a.mode=0;}else a.mode=a.enabledProfile==1?1:2;}
constexpr void profile(alarmclock::Setting& a){a.enabledProfile=(a.mode?a.mode:a.enabledProfile)==1?2:1;if(a.mode)a.mode=a.enabledProfile;}
enum class Hit {None,Back,Hour,Minute,Minus,Plus,Save,Cancel};
constexpr Hit hit(int x,int y,int width=320,int height=240){
 if(x<0||x>=width||y<0||y>=height)return Hit::None;
 if(y<32&&x<80)return Hit::Back;
 const int middle=width/2;
 if(y>=62&&y<127){
  if(x>=width/4-70&&x<width/4+70)return Hit::Hour;
  if(x>=3*width/4-70&&x<3*width/4+70)return Hit::Minute;
 }
 if(y>=140&&y<184){
  if(x>=10&&x<middle-10)return Hit::Minus;
  if(x>=middle+10&&x<width-10)return Hit::Plus;
 }
 if(y>=190&&y<234){
  if(x>=10&&x<middle-10)return Hit::Save;
  if(x>=middle+10&&x<width-10)return Hit::Cancel;
 }
 return Hit::None;
}
}
