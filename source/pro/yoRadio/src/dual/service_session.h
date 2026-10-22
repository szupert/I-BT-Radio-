#pragma once
#include "service_pcm.h"
namespace serviceui {
enum class Stage:uint8_t { Idle,WaitStop,WaitOutput,Running,WaitRestore,Fault };
enum class Effect:uint8_t { None,StopPlayer,PrepareOutput,RestoreOriginal,Mute };
enum class Failure:uint8_t { None,StopTimeout,OutputTimeout,LinkLost,WriteContract,WriteTimeout,RestoreTimeout };
struct Snapshot { uint16_t station=0;uint8_t volume=0;bool sd=false,playing=false;Route route=Route::Dac;uint32_t rate=44100,filePosition=0,savedSdResume=0,savedResume=0; };
struct Inputs { bool owner=true,stopped=false,outputReady=false,restored=false,linkLost=false; };
struct Session {
 Stage stage=Stage::Idle;Effect effect=Effect::None;Failure failure=Failure::None;
 Snapshot original{};State settings{};Limit limit{};Oscillator osc{};
 uint32_t entered=0,lastProgress=0;uint64_t acceptedFrames=0;uint16_t offset=0,count=0;
 int16_t pcm[512]={}; // 256 stereo frames, fixed 1024 bytes, no allocation.
 constexpr bool begin(State desired,Limit measured,Snapshot before,uint32_t now,bool owner,bool busy){
  if(stage!=Stage::Idle||!owner||busy||!desired.running||!mayStart(desired,measured,true))return false;
  original=before;settings=desired;limit=measured;entered=now;failure=Failure::None;
  offset=count=0;acceptedFrames=0;lastProgress=now;osc.stop();stage=Stage::WaitStop;effect=Effect::StopPlayer;return true;
 }
 constexpr Effect takeEffect(){const Effect e=effect;effect=Effect::None;return e;}
 constexpr void restore(uint32_t now,Failure reason=Failure::None){
  if(stage==Stage::Idle||stage==Stage::WaitRestore||stage==Stage::Fault)return;
  osc.stop();offset=count=0;failure=reason;entered=now;stage=Stage::WaitRestore;effect=Effect::RestoreOriginal;
 }
 constexpr void tick(uint32_t now,Inputs input){
  if(!input.owner)return; // Only the existing audio owner may drive transitions.
  const uint32_t age=now-entered; // unsigned subtraction handles millis wrap.
  if(stage==Stage::WaitStop){
   if(input.stopped){stage=Stage::WaitOutput;entered=now;effect=Effect::PrepareOutput;}
   else if(age>=10000)restore(now,Failure::StopTimeout);
  }else if(stage==Stage::WaitOutput){
   if(input.linkLost)restore(now,Failure::LinkLost);
   else if(input.outputReady){
    if(osc.begin(settings,limit,true)){stage=Stage::Running;entered=lastProgress=now;}
    else restore(now,Failure::WriteContract);
   }else if(age>=30000)restore(now,Failure::OutputTimeout);
  }else if(stage==Stage::Running){
   if(input.linkLost||!input.outputReady)restore(now,Failure::LinkLost);
   else if(uint32_t(now-lastProgress)>=5000)restore(now,Failure::WriteTimeout);
  }else if(stage==Stage::WaitRestore){
   if(input.restored){stage=Stage::Idle;effect=Effect::None;}
   else if(age>=30000){stage=Stage::Fault;failure=Failure::RestoreTimeout;effect=Effect::Mute;}
  }
 }
 constexpr bool prepare(){
  if(stage!=Stage::Running)return false;
  if(!count){offset=0;count=uint16_t(osc.render(pcm,512));}
  return count>0;
 }
 constexpr const int16_t* pending()const{return pcm+offset*2;}
 constexpr bool accepted(uint16_t frames,uint32_t now){
  if(stage!=Stage::Running||frames>count){restore(now,Failure::WriteContract);return false;}
  offset+=frames;count-=frames;if(frames){lastProgress=now;acceptedFrames+=frames;}return true;
 }
 // Fault recovery is explicit, only after confirmed silent/idle output.
 constexpr bool clearFault(bool silent){if(stage!=Stage::Fault||!silent)return false;stage=Stage::Idle;effect=Effect::None;return true;}
};
}
