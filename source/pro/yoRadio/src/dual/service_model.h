#pragma once
#include <stdint.h>
// Isolated UI model. No GPIO, audio driver, task, heap or persistent writes.
namespace serviceui {
enum class Wave:uint8_t { Sine,Square,RampUp,RampDown,Triangle };
enum class Route:uint8_t { Dac,Bt };
enum class Channel:uint8_t { Stereo,Left,Right };
enum class Control:uint8_t { None,Back,Start,Stop,Wave,Minus,Plus,Level,Channel,Rate,Dac,Bt,Generator,Diagnostics,Step };
struct Rect { int x,y,w,h; constexpr bool contains(int px,int py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;} };
struct Hit { Control control;Rect rect; };
constexpr Hit hits[]={
 {Control::Step,{9,58,116,49}},
 {Control::Back,{6,190,98,36}}, {Control::Start,{110,190,98,36}}, {Control::Stop,{216,190,98,36}},
 {Control::Wave,{211,58,99,49}}, {Control::Minus,{132,68,29,31}}, {Control::Plus,{169,68,34,31}},
 {Control::Level,{59,115,100,23}}, {Control::Channel,{244,115,67,23}}, {Control::Rate,{61,147,92,22}},
 {Control::Dac,{219,147,45,22}}, {Control::Bt,{270,147,42,22}},
 {Control::Generator,{6,26,152,24}}, {Control::Diagnostics,{162,26,152,24}} };
constexpr Control hit(int x,int y,bool diagnostics=false){for(const auto& h:hits)if(h.rect.contains(x,y)){
 if(diagnostics&&h.control!=Control::Back&&h.control!=Control::Start&&h.control!=Control::Stop&&h.control!=Control::Generator&&h.control!=Control::Diagnostics)return Control::None;
 return h.control;}return Control::None;}
struct State {
 Wave wave=Wave::Sine;Route route=Route::Dac;Channel channel=Channel::Stereo;
 uint32_t hz=1000,rate=48000,step=1;
 int8_t db=-20;bool running=false,alarmPattern=false;
};
struct Limit { uint32_t hz=0;bool measured=false;bool bench=false; };
// Limit is supplied for the exact route/wave/rate after physical measurement.
constexpr bool mayStart(const State& s,Limit limit,bool routeReady){
 return uint8_t(s.wave)<5&&uint8_t(s.route)<2&&uint8_t(s.channel)<3&&routeReady&&(limit.measured||limit.bench)&&limit.hz>=1&&s.hz>=1&&s.hz<=limit.hz&&
        s.rate>=2&&s.hz<s.rate/2&&s.db>=-60&&s.db<=0;
}
constexpr bool start(State& s,Limit limit,bool routeReady){s.running=mayStart(s,limit,routeReady);return s.running;}
constexpr void stop(State& s){s.running=false;}
constexpr void route(State& s,Route r){stop(s);s.route=r;}
constexpr void nextWave(State& s){stop(s);s.wave=Wave((uint8_t(s.wave)+1)%5);}
constexpr void nextChannel(State& s){stop(s);s.channel=Channel((uint8_t(s.channel)+1)%3);}
constexpr void nextLevel(State& s){stop(s);s.db=s.db>=0?-60:int8_t(s.db+5);}
constexpr void frequency(State& s,bool up){
 stop(s);const uint32_t max=s.rate>2?s.rate/2-1:1;
 if(up)s.hz=s.step>=max||s.hz>=max-s.step?max:s.hz+s.step;
 else s.hz=s.hz<=s.step?1:s.hz-s.step;
}
constexpr void sampleRate(State& s,uint32_t rate){
 stop(s);s.rate=rate;const uint32_t max=rate>2?rate/2-1:1;if(s.hz>max)s.hz=max;
}
}
