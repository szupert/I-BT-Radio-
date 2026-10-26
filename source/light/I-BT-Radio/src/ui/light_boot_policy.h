#pragma once
#include <atomic>
#include <stdint.h>
namespace lightboot {
constexpr unsigned page=76,minimumMs=4000,connectedVisibleMs=2000,connectionStableMs=2000,sliceRows=8,sliceIntervalMs=20;
enum class Signal:uint8_t {Off,Waiting,Ready,Error};
enum class Hit:uint8_t {None,Wifi,Sd,Bt,Back};
constexpr Hit hit(int x,int y,int width=320,int height=240){
 if(width<1||height<1||x<0||y<0||x>=width||y>=height)return Hit::None;
 x=x*320/width;y=y*240/height;
 if(y<141||y>=172)return Hit::None;
 return x>=35&&x<116?Hit::Wifi:x>=122&&x<202?Hit::Sd:x>=208&&x<288?Hit::Bt:Hit::None;
}
// A boot probe records actual mount success; the SDK itself is not started for a lamp.
struct Readiness {std::atomic<uint8_t> sd{0};std::atomic<bool> btLoaded{false};};
inline Readiness& readiness(){static Readiness r;return r;}
template<class Card> bool probeSd(Card& card,bool keepMounted){
 const bool already=card.ready,ok=already||card.start();
 if(ok&&!already&&!keepMounted)card.stop();
 readiness().sd.store(ok?2:1,std::memory_order_release);return ok;
}
constexpr uint8_t progress(bool sdReady,bool btLoaded,bool connected){return 1u|(sdReady?2u:0u)|(btLoaded?4u:0u)|(connected?8u:0u);}
constexpr Signal wifi(bool connected,bool failed,bool offline){return connected?Signal::Ready:failed?Signal::Error:offline?Signal::Off:Signal::Waiting;}
constexpr Signal sd(bool ready,bool indexed,bool failed){return failed?Signal::Error:ready?(indexed?Signal::Ready:Signal::Waiting):Signal::Off;}
constexpr Signal bt(bool connected,bool failed,bool wanted){return connected?Signal::Ready:failed?Signal::Error:wanted?Signal::Waiting:Signal::Off;}
struct Session {
 uint32_t started=0,greenAt=0,connectedAt=0;
 uint32_t visibleAt=0,holdMs=0;
 uint8_t phase=0;
 bool automatic=true,greenSeen=false,connectionSeen=false,imageSeen=false;
 std::atomic<bool> wifiMenuPending{false};
 void begin(uint32_t now){started=now;phase=0;greenSeen=false;connectionSeen=false;imageSeen=false;automatic=true;wifiMenuPending.store(false);}
 void reopen(){automatic=false;greenSeen=false;connectionSeen=false;wifiMenuPending.store(false);}
 bool connectionReady(uint32_t now,bool connected){
  if(!connected){connectionSeen=false;return false;}
  if(!automatic)return true;
  if(!connectionSeen){connectedAt=now;connectionSeen=true;}
  return uint32_t(now-connectedAt)>=connectionStableMs;
 }
 void advance(bool sdChecked,bool btLoaded,bool connected){
  // One real milestone per completed render: SD, BT support, then live Wi-Fi.
  if((phase==0&&sdChecked)||(phase==1&&btLoaded)||(phase==2&&connected))++phase;
 }
 void painted(uint32_t now,bool green){
  if(!imageSeen){visibleAt=now;imageSeen=true;}
  if(!green){greenSeen=false;return;}
  if(!greenSeen){greenAt=now;greenSeen=true;}
 }
 bool dismiss(uint32_t now,bool setupDone,bool connected,bool offline,bool rendered){
  if(!connected){greenSeen=false;connectionSeen=false;}
  return automatic&&!wifiMenuPending.load()&&setupDone&&rendered&&uint32_t(now-started)>=minimumMs&&(!holdMs||(imageSeen&&uint32_t(now-visibleAt)>=holdMs))&&
    (connected?(greenSeen&&uint32_t(now-greenAt)>=connectedVisibleMs):(offline&&phase>=2));
 }
};
inline Session& session(){static Session s;return s;}
}
