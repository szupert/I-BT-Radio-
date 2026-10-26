#pragma once
#include <atomic>
#include <stdint.h>
namespace ibtaudio {
// Callback mailbox only. API commands run in existing service owner, never PCM.
class PeerVolume {
 std::atomic<uint32_t> state{0};
 unsigned seen=0,attempts[3]{},label=10;uint32_t due=0;
 static constexpr uint32_t Link=1,Query=2,Supported=4,Register=8,Set=16,Value=127u<<5,Epoch=0xffff0000u;
public:
 enum Action{None=0,Capabilities=1,Notification=2,Absolute=3};
 void connection(bool up){uint32_t old=state.load();while(!state.compare_exchange_weak(old,((old+0x10000u)&Epoch)|(up?(Link|Query):0))){} }
 void capabilities(bool supported){uint32_t old=state.load();while(old&Link){uint32_t next=old&~(Supported|Register);if(supported)next|=Supported|Register;if(state.compare_exchange_weak(old,next))return;}}
 void changed(unsigned volume){if(volume>127)return;uint32_t old=state.load();while((old&(Link|Supported))==(Link|Supported)){uint32_t next=(old&~Value)|(volume<<5)|Set|Register;if(state.compare_exchange_weak(old,next))return;}}
 struct Command{Action action;unsigned label,volume;uint32_t epoch;};
 Command next(uint32_t now){uint32_t old=state.load();unsigned epoch=old&Epoch;if(epoch!=seen){seen=epoch;for(auto& n:attempts)n=0;due=now+250;}
  if(!(old&Link)||int32_t(now-due)<0)return {None,0,0,epoch};
  Action a=(old&Query)?Capabilities:(old&Set)?Absolute:(old&Register)?Notification:None;if(a==None)return {None,0,0,epoch};
  const unsigned flag=a==Capabilities?Query:a==Absolute?Set:Register;
  if(!state.compare_exchange_strong(old,old&~flag))return {None,0,0,epoch};
  due=now+20;return {a,a==Capabilities?12u:a==Notification?13u:(label=label==10?11:10),(old&Value)>>5,epoch};
 }
 void result(Command c,bool ok,uint32_t now){if(c.action==None)return;unsigned i=unsigned(c.action)-1;
  if(ok){attempts[i]=0;return;}if(++attempts[i]>=3)return;
  uint32_t old=state.load();while((old&Epoch)==c.epoch&&(old&Link)){unsigned flag=c.action==Capabilities?Query:c.action==Absolute?Set:Register;if(state.compare_exchange_weak(old,old|flag)){due=now+1000;return;}}
 }
 unsigned snapshot()const{return state.load();}
};
}
