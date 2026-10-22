#pragma once
#include <stdint.h>
#include <stddef.h>
// Pure NEC learning policy. Driver and persistence live in ir_receiver.cpp.
namespace smallir {
enum class Action:uint8_t {None,VolumeDown,VolumeUp,Previous,Next,PlayPause,Menu,Back,Select,Source,Clock,Mute};
enum class Protocol:uint8_t {Unknown,Nec};
struct Key {Protocol protocol=Protocol::Unknown;uint8_t bits=0;uint16_t address=0,command=0;
 constexpr bool operator==(const Key& r)const{return protocol==r.protocol&&bits==r.bits&&address==r.address&&command==r.command;}
 constexpr bool valid()const{return protocol!=Protocol::Unknown&&bits&&bits<=64;}
};
struct Pulse {uint16_t mark=0,space=0;}; // Microseconds, active-low receiver already normalized.
struct Frame {Key key;bool valid=false,repeat=false;};
constexpr bool near(uint16_t a,unsigned b){return a>=b*3/4&&a<=b*5/4;}
constexpr Frame nec(const Pulse* p,size_t n){
 Frame r;if(!p)return r;
 if(n>=2&&n<=3&&near(p[0].mark,9000)&&near(p[0].space,2250)&&near(p[1].mark,560)){r.valid=r.repeat=true;return r;}
 if(n!=34||!near(p[0].mark,9000)||!near(p[0].space,4500)||!near(p[33].mark,560))return r;
 uint32_t data=0;
 for(unsigned i=0;i<32;++i){if(!near(p[i+1].mark,560))return r;if(near(p[i+1].space,1690))data|=uint32_t(1)<<i;else if(!near(p[i+1].space,560))return r;}
 const uint8_t a=data,b=data>>8,c=data>>16,d=data>>24;if(uint8_t(c^d)!=255)return r;
 r.key={Protocol::Nec,32,uint16_t(uint8_t(a^b)==255?a:uint16_t(a)|(uint16_t(b)<<8)),c};r.valid=true;return r;
}
enum class Learn:uint8_t {Off,Waiting,First,Ready,Collision,Timeout};
struct Binding {Key key;Action action=Action::None;};
class Learner {
 Binding slots[12]{};Key candidate{},last{};Action target=Action::None;
 uint32_t started=0,firstAt=0,lastAt=0,lastSeen=0;bool haveLast=false;
 Learn mode=Learn::Off;
 static constexpr bool repeats(Action a){return a==Action::VolumeDown||a==Action::VolumeUp;}
 public:
 constexpr Learn status()const{return mode;}
 constexpr bool begin(Action a,uint32_t now){if(a==Action::None||unsigned(a)>=12)return false;target=a;candidate={};started=now;mode=Learn::Waiting;haveLast=false;return true;}
 constexpr void cancel(){mode=Learn::Off;target=Action::None;candidate={};haveLast=false;}
 constexpr void tick(uint32_t now){if(mode!=Learn::Off&&uint32_t(now-started)>=30000){mode=Learn::Timeout;candidate={};}}
 constexpr Action receive(const Frame& f,uint32_t now){
  tick(now);if(!f.valid)return Action::None;
  if(mode!=Learn::Off){
   if(mode==Learn::Timeout||mode==Learn::Collision||mode==Learn::Ready)return Action::None;
   if(f.repeat){if(mode==Learn::First)firstAt=now;return Action::None;}
   if(!f.key.valid())return Action::None;
   for(const auto& b:slots)if(b.action!=Action::None&&b.action!=target&&b.key==f.key){mode=Learn::Collision;return Action::None;}
   if(mode==Learn::Waiting||!(candidate==f.key)){candidate=f.key;firstAt=now;mode=Learn::First;}
   else {if(uint32_t(now-firstAt)>=300)mode=Learn::Ready;firstAt=now;}
   return Action::None;
  }
  Key key=f.key;
  if(f.repeat){if(!haveLast||uint32_t(now-lastSeen)>250)return Action::None;key=last;}
  else if(!key.valid())return Action::None;
  const bool quick=haveLast&&last==key&&uint32_t(now-lastSeen)<250;
  last=key;haveLast=true;lastSeen=now;
  if(uint32_t(now-lastAt)<100&&(f.repeat||quick))return Action::None;
  lastAt=now;
  for(const auto& b:slots)if(b.action!=Action::None&&b.key==key)return (f.repeat||quick)&&!repeats(b.action)?Action::None:b.action;
  return Action::None;
 }
 // Only the explicit menu Save may commit. Persistence is a separate owner job.
 constexpr bool commit(){if(mode!=Learn::Ready||!candidate.valid())return false;slots[unsigned(target)]={candidate,target};cancel();return true;}
 constexpr bool erase(Action a){if(a==Action::None||unsigned(a)>=12)return false;slots[unsigned(a)]={};return true;}
 constexpr const Binding* bindings()const{return slots;}
 constexpr bool restore(const Binding* data,size_t count){
  if(!data||count!=12)return false;
  for(unsigned i=0;i<12;++i){
   const auto& b=data[i];if(b.action==Action::None)continue;
   if(i==0||unsigned(b.action)!=i||b.key.protocol!=Protocol::Nec||
      b.key.bits!=32||b.key.command>255)return false;
   for(unsigned j=1;j<i;++j)if(data[j].action!=Action::None&&data[j].key==b.key)return false;
  }
  for(unsigned i=0;i<12;++i)slots[i]=data[i];
  cancel();return true;
 }
};
}
