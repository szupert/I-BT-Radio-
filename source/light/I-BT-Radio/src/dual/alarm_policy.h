#pragma once
#include <stdint.h>
#include <time.h>

namespace alarmclock {
constexpr bool requestAllowed(uint32_t token,uint32_t current){return token!=0&&token==current;}
constexpr bool playbackTimedOut(bool running,uint32_t now,uint32_t lastProgress){
    return !running&&uint32_t(now-lastProgress)>30000u;
}
constexpr bool cleanupAllowed(uint32_t token,uint32_t occurrence,bool ending,bool manualTakeover){
    return requestAllowed(token,occurrence)&&ending&&!manualTakeover;
}
// Mode 0: disabled, 1: fixed level, 2: scheduled rise and fall.
struct Setting {
    uint8_t mode=0;
    uint8_t enabledProfile=2; // retained while the alarm is disabled
    uint8_t onHour=7, onMinute=0, offHour=7, offMinute=30;
    uint16_t station=1;
    uint8_t volumeStart=10, volumePeak=70, volumeEnd=10;
    uint8_t volumeRise=5, volumeFall=5; // minutes
    uint8_t lightStart=100, lightPeak=100, lightEnd=100;
    uint8_t lightRise=5, lightFall=5; // minutes
    bool nightClock=false;
    bool blinkColon=true;
    bool clockHome=false; // explicit Radio / Alarm Clock home-page switch
    uint8_t daysMask=0x7f; // bit 0=Monday ... bit 6=Sunday
    uint8_t clockStyle=0,clockColor=0,textColor=0;
    uint8_t clockDisplay=0; // 0=time, 1=countdown to next alarm, 2=both
    uint8_t source=0; // 0=internet radio, 1=SD /Ébresztő
    uint8_t sdPlayMode=0; // 0=sequential loop, 1=random loop, 2=one file
};
constexpr bool samePlayback(const Setting& a,const Setting& b){
    return a.mode==b.mode&&a.onHour==b.onHour&&a.onMinute==b.onMinute&&
        a.offHour==b.offHour&&a.offMinute==b.offMinute&&a.daysMask==b.daysMask&&
        a.source==b.source&&(a.source?a.sdPlayMode==b.sdPlayMode:a.station==b.station)&&
        a.volumeStart==b.volumeStart&&a.volumePeak==b.volumePeak&&a.volumeEnd==b.volumeEnd&&
        a.volumeRise==b.volumeRise&&a.volumeFall==b.volumeFall&&
        a.lightStart==b.lightStart&&a.lightPeak==b.lightPeak&&a.lightEnd==b.lightEnd&&
        a.lightRise==b.lightRise&&a.lightFall==b.lightFall;
}
inline void sanitize(Setting& a) {
    if(a.mode>2)a.mode=0;
    if(a.onHour>23)a.onHour=7;
    if(a.offHour>23)a.offHour=7;
    if(a.onMinute>59)a.onMinute=0;
    if(a.offMinute>59)a.offMinute=30;
    if(a.station<1||a.station>480)a.station=1;
    if(a.volumeStart>100)a.volumeStart=10;
    if(a.volumePeak>100)a.volumePeak=70;
    if(a.volumeEnd>100)a.volumeEnd=10;
    if(a.lightStart<5||a.lightStart>100)a.lightStart=100;
    if(a.lightPeak<5||a.lightPeak>100)a.lightPeak=100;
    if(a.lightEnd<5||a.lightEnd>100)a.lightEnd=100;
    if(a.volumeRise>120)a.volumeRise=5;
    if(a.volumeFall>120)a.volumeFall=5;
    if(a.lightRise>120)a.lightRise=5;
    if(a.lightFall>120)a.lightFall=5;
    a.daysMask&=0x7f;
    if(a.clockStyle>3)a.clockStyle=0;
    if(a.clockColor>8)a.clockColor=0;
    if(a.textColor>8)a.textColor=0;
    if(a.clockDisplay>2)a.clockDisplay=0;
    if(a.source>1)a.source=0;
    if(a.sdPlayMode>2)a.sdPlayMode=0;
}
constexpr bool validWindow(const Setting& a) {
    return a.onHour!=a.offHour||a.onMinute!=a.offMinute;
}
constexpr uint32_t dayKey(int year,int dayOfYear);
inline time_t nextStart(time_t now,const Setting& a,uint32_t lastDay=0) {
    if(a.mode==0||a.daysMask==0||!validWindow(a)||now<=0)return 0;
    tm local{};
    if(!localtime_r(&now,&local)||local.tm_year+1900<2024)return 0;
    for(int offset=0;offset<=7;++offset){
        tm candidate=local;
        candidate.tm_mday+=offset;
        candidate.tm_hour=a.onHour;
        candidate.tm_min=a.onMinute;
        candidate.tm_sec=0;
        candidate.tm_isdst=-1;
        const time_t at=mktime(&candidate);
        // mktime normalizes the missing hour at the spring DST transition.
        // That normalized time is not a slot the due() scheduler can fire.
        if(candidate.tm_hour!=a.onHour||candidate.tm_min!=a.onMinute)continue;
        if(at>now&&dayKey(candidate.tm_year+1900,candidate.tm_yday)>lastDay&&
           (a.daysMask&(uint8_t(1u)<<((candidate.tm_wday+6)%7))))return at;
    }
    return 0;
}
constexpr uint32_t dayKey(int year,int dayOfYear) {
    return year>=2024&&dayOfYear>=0&&dayOfYear<=365
        ?uint32_t(year)*366u+uint32_t(dayOfYear)+1u:0u;
}
constexpr bool due(const Setting& a,int year,int dayOfYear,int weekDay,int hour,int minute,
                uint32_t lastDay) {
    const uint32_t day=dayKey(year,dayOfYear);
    return a.mode!=0&&validWindow(a)&&day>lastDay&&weekDay>=0&&weekDay<=6&&
        (a.daysMask&(uint8_t(1u)<<((weekDay+6)%7)))!=0&&
        hour==a.onHour&&minute==a.onMinute;
}
constexpr uint8_t playerVolume(uint8_t percent) {
    return uint8_t((unsigned(percent)*254u+50u)/100u);
}
constexpr uint8_t nextMinutes(uint8_t value){
    if(value>=120)return 0;
    if(value<10)return value+1;
    return value>115?120:value+5;
}
constexpr uint8_t nextLevel(uint8_t value,uint8_t minimum){
    return value>=100?minimum:value>95?100:value+5;
}
constexpr uint32_t elapsedSeconds(uint32_t now,uint32_t started,uint32_t total){
    const uint32_t elapsed=uint32_t(now-started)/1000u;
    return elapsed<total?elapsed:total;
}
constexpr uint8_t interpolate(uint8_t from,uint8_t to,uint32_t elapsed,uint32_t duration) {
    if(!duration||elapsed>=duration)return to;
    return uint8_t((uint32_t(from)*(duration-elapsed)+uint32_t(to)*elapsed+duration/2)/duration);
}
constexpr uint8_t level(uint8_t from,uint8_t peak,uint8_t to,uint32_t elapsed,
                     uint32_t total,uint8_t riseMinutes,uint8_t fallMinutes,bool ramp) {
    if(!ramp||!total)return peak;
    // Keep requested durations when they fit. If they overlap, scale both
    // proportionally so a long rise with a short fall retains its ratio.
    uint32_t rise=uint32_t(riseMinutes)*60u,fall=uint32_t(fallMinutes)*60u;
    if(uint64_t(rise)+fall>total){
        const uint32_t sum=rise+fall;
        rise=uint32_t(uint64_t(total)*rise/sum);fall=total-rise;
    }
    if(rise&&elapsed<rise)return interpolate(from,peak,elapsed,rise);
    if(fall&&elapsed>=total-fall)return interpolate(peak,to,elapsed-(total-fall),fall);
    return peak;
}
}
