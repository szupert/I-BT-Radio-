#pragma once
#include <stdint.h>
#include "../ram_reserve.h"
namespace radiosensor {
struct Config { bool enabled=false; uint8_t address[6]={}; bool selected=false; uint8_t bindkey[16]={}; bool keyValid=false; };
// Any foreground work vetoes BLE, regardless of how long reception was deferred.
// Scan/direct acquisition shares a minimum 60-second start interval.
// Cooldown is owned by receiver::service; direct::service is called by it.
struct AcquisitionCooldown {
 bool attempted=false; uint32_t last=0;
 constexpr bool due(uint32_t now)const{return !attempted||uint32_t(now-last)>=60000u;}
 bool claim(uint32_t now){if(!due(now))return false;attempted=true;last=now;return true;}
};
inline AcquisitionCooldown& acquisitionCooldown(){static AcquisitionCooldown gate;return gate;}
constexpr uint32_t staleMs=300000, scanBurstMs=1000, scanRestMs=59000;
constexpr bool receiveAllowed(const Config& c,bool stackReady,bool audioBusy,bool foregroundBusy,unsigned heap,unsigned largest,unsigned loadPermille){
 return c.enabled&&c.selected&&stackReady&&!audioBusy&&!foregroundBusy&&heap>=(ibtram::reserve==0?0u:32768u)&&largest>=(ibtram::reserve==0?0u:16384u)&&loadPermille<800;
}
constexpr bool scanDue(const Config& c,bool ready,bool audio,bool foregroundBusy,unsigned heap,unsigned largest,unsigned load,uint32_t now,uint32_t last,bool attempted){
 return receiveAllowed(c,ready,audio,foregroundBusy,heap,largest,load)&&(!attempted||uint32_t(now-last)>=scanBurstMs+scanRestMs);
}
constexpr bool fresh(uint32_t now,uint32_t received){return uint32_t(now-received)<staleMs;}
struct Reading {bool valid=false;int16_t temperatureCenti=0;uint16_t humidityCenti=0;int16_t battery=-1;uint32_t receivedAt=0;bool temperatureValid=false,humidityValid=false;uint32_t temperatureAt=0,humidityAt=0;};
constexpr bool hasFreshPair(const Reading&r,uint32_t now){return r.temperatureValid&&r.humidityValid&&fresh(now,r.temperatureAt)&&fresh(now,r.humidityAt);}
constexpr bool updateReading(Reading&r,bool hasT,int temperature,bool hasH,int humidity,int battery,uint32_t now){
 if((!hasT&&!hasH)||(hasT&&(temperature<-5000||temperature>8500))||(hasH&&(humidity<0||humidity>10000))||battery < -1||battery>100)return false;
 Reading c=r;
 if(hasT){c.temperatureCenti=int16_t(temperature);c.temperatureAt=now;c.temperatureValid=true;}
 if(hasH){c.humidityCenti=uint16_t(humidity);c.humidityAt=now;c.humidityValid=true;}
 if(battery>=0)c.battery=int16_t(battery);c.receivedAt=now;c.valid=hasFreshPair(c,now);r=c;return true;
}
constexpr bool plausible(float t,float h){return t>=-50&&t<=85&&h>=0&&h<=100;}
}
