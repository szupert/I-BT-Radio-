#include "mono_dsp.h"
#if IBT_STREAM_ART_DIAG
#include <Arduino.h>
namespace monodsp {
void begin(){} void service(bool,unsigned){} Config configuration(){return Config{};}
bool change(unsigned,int){return false;}bool set(unsigned,int){return false;}bool pendingSave(){return false;}
void block(uint32_t){}bool active(){return false;}uint32_t process(int16_t,int16_t,uint32_t dry,unsigned,uint32_t){return dry;}
void measuredBlock(unsigned,uint32_t){}void diagnostics(){Serial.println("[MONO-DSP] enabled=0 compiled_engine=0 diagnostic=STREAM_ART_ONLY");}
}
#else
#include <Arduino.h>
#include <Preferences.h>
#include <atomic>
namespace monodsp {
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static Config desired;
static Plan mailbox;
static Processor processor;
static std::atomic<uint32_t> version{1},published{0},saved{1};
static uint32_t seen=0,configuredVersion=0;
static std::atomic<uint32_t> changedAt{0};
static std::atomic<uint32_t> wantedRate{44100},blocks{0},workMax{0},frames{0};
static uint32_t preparedRate=0;static unsigned preparedVolume=999;static std::atomic<bool> saveOk{true};
static Config snapshot(uint32_t& revision){Config c;portENTER_CRITICAL(&mux);c=desired;revision=version.load();portEXIT_CRITICAL(&mux);return c;}
Config configuration(){uint32_t revision;return snapshot(revision);}
bool pendingSave(){return version!=saved;}
void begin(){
    uint8_t data[RecordBytes];Preferences p;Config restored;
    if(p.begin("ibt-mono-v1",true)){
        if(p.getBytesLength("profile")==sizeof(data)&&p.getBytes("profile",data,sizeof(data))==sizeof(data)&&decode(data,sizeof(data),restored))desired=restored;
        p.end();
    }
    desired.v[0]=0;
    service(false,254);
}
bool set(unsigned parameter,int value){
    if(parameter>=Parameters||value<Minimum[parameter]||value>Maximum[parameter])return false;
    portENTER_CRITICAL(&mux);
    if(desired.v[parameter]!=value){desired.v[parameter]=int16_t(value);++version;changedAt=millis();}
    portEXIT_CRITICAL(&mux);return true;
}
bool change(unsigned parameter,int direction){
    if(parameter>=Parameters||!direction)return false;
    const auto c=configuration();int n=c.v[parameter]+(direction>0?Step[parameter]:-Step[parameter]);
    if(n>Maximum[parameter])n=Minimum[parameter];if(n<Minimum[parameter])n=Maximum[parameter];
    return set(parameter,n);
}
void service(bool maySave,unsigned volume){
    const uint32_t hz=wantedRate.load(std::memory_order_relaxed);
    if(hz<8000||hz>192000)return;
    if(configuredVersion!=version.load()||hz!=preparedRate||volume!=preparedVolume){
        uint32_t cfg;const auto c=snapshot(cfg);const auto plan=prepare(c,hz,volume);
        portENTER_CRITICAL(&mux);mailbox=plan;++published;configuredVersion=cfg;portEXIT_CRITICAL(&mux);
        preparedRate=hz;preparedVolume=volume;
    }
    // Same flash-write policy as existing settings: never stall live audio.
    if(maySave&&version!=saved&&uint32_t(millis()-changedAt)>=1500){
        uint32_t target;const auto c=snapshot(target);uint8_t data[RecordBytes],check[RecordBytes];
        encode(c,data);Preferences p;saveOk=p.begin("ibt-mono-v1",false);
        if(saveOk){saveOk=p.putBytes("profile",data,sizeof(data))==sizeof(data)&&p.getBytes("profile",check,sizeof(check))==sizeof(check)&&!memcmp(data,check,sizeof(data));p.end();}
        if(saveOk)saved=target;else changedAt=millis();
    }
}
void block(uint32_t rate){
    wantedRate.store(rate,std::memory_order_relaxed);
    if(processor.busy())return;
    Plan p;bool fresh=false;
    portENTER_CRITICAL(&mux);
    if((seen!=published||processor.engines[processor.current].plan.rate!=mailbox.rate)&&mailbox.rate==rate){p=mailbox;seen=published.load();fresh=true;}
    portEXIT_CRITICAL(&mux);
    if(fresh){if(!p.config.v[0]&&!processor.active()){processor.engines[0].set(p);processor.current=0;processor.initialized=true;}else processor.install(p);}
}
bool active(){return processor.active();}
uint32_t process(int16_t l,int16_t r,uint32_t dry,unsigned volume,uint32_t rate){
    return processor.process(l,r,dry,volume,rate);
}
void measuredBlock(unsigned count,uint32_t wallUs){
    frames.fetch_add(count,std::memory_order_relaxed);blocks.fetch_add(1,std::memory_order_relaxed);
    if(wallUs>workMax.load(std::memory_order_relaxed))workMax.store(wallUs,std::memory_order_relaxed);
}
void diagnostics(){const auto c=configuration();Serial.printf("[MONO-DSP] enabled=%u pending_save=%u save_ok=%u rate=%lu frames=%lu transform_block_max_wall_us=%lu float_bits=32 scope=DAC_BLOCK_GAIN_VU_MONO_INCLUDES_PREEMPTION legacy_eq=OFF\n",unsigned(c.v[0]),unsigned(pendingSave()),unsigned(saveOk.load()),(unsigned long)wantedRate.load(),(unsigned long)frames.load(),(unsigned long)workMax.load());}
}

#endif
