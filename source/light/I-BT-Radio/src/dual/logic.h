#pragma once
#ifndef IBT_COMPACT_UI
#define IBT_COMPACT_UI 0
#endif
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "battery_policy.h"
#include "eq_presets.h"
#include "dns_policy.h"
#include "alarm_policy.h"
namespace dual {
enum class Language : uint8_t { HU, English, DE };
enum class Theme : uint8_t { Dark, Light, Auto };
enum class Action : uint8_t { None, Prev, Next, Toggle, VolDown, VolUp, Source, Menu, Select, RotCW, RotCCW, EncoderDown, EncoderShort, EncoderLong, EqBass, EqMiddle, EqTreble, Balance, SourceWeb, SourceSd, PlayerPlay, PlayerPause };
enum class Text : uint8_t { Settings, Radio, SD, Language, Theme, Rotation, Rate, Brightness,
    Bluetooth, Disabled, Diagnostics, Back, Source, Playing, Stopped, NoCover, SavePending,
    Artist, Album, Title, Connecting, Count };
struct Settings {
    alarmclock::Setting alarm{};
    bool dnsManual=false;
    uint8_t dnsPrimary[4]={1,1,1,1}, dnsSecondary[4]={1,0,0,1};
    Language language = Language::HU;
    Theme theme = Theme::Dark;
    uint8_t rotation = 0;
    uint8_t brightness = 100; // 0=Auto with GPIO34 LDR selected; otherwise 10..100%
    eq10::Settings graphicEq{};
    bool batteryIndicator=false;
    uint8_t batteryStep=1, batteryAppearance=0; // percent, icon, both
    uint16_t batteryMv[5]={1500,1650,1800,1950,2100};
    uint8_t sensorMode = 0; // 0 disabled, 1 light voltage, 2 single-cell battery (2:1 divider)
    uint16_t menuIdleSeconds = 10; // 0 disables automatic home navigation
    uint16_t maxKbps = 0; // unlimited by default; optional admission limit, never a transcoder
    uint8_t rotaryMode = 1; // 1=volume, 2=station preview/confirm
    uint8_t volumeStep = 1; // 1..10 percentage points per rotary detent
    bool debug = false; // serial diagnostics/reporting; OFF by default
    bool artworkEnabled = true;
    bool cassetteEnabled = true;
    bool bufferIndicator = true;
    bool reelAnimation = false;
    bool wifiIndicator = true;
    bool btQuickScan = true;
    bool playerEnabled = true; // two-page player/list; cassette remains available
    bool kajonSkin = false; // separately persisted; existing skins unchanged by default
    bool mediaPlayerSkin = false; // optional silver/blue skin; separate persisted key
    uint8_t networkMode = 0; // 0 online, 1 offline SD, 2 offline BT-IN
    uint8_t bootScreen = 0; // 0 original, 1 Videoton, 2 follow, 3 XP, 4 Windows 7
    uint8_t bootSound = 0; // 0 off (safe migration), 1 built-in, 2 /boot.wav
    uint8_t vintageSkin = 0; // 0=off, 1=retro neon, 2=Hi-Fi, 3=classic wood
    uint8_t retroColor = 0; // 0..8; original green is the default
    bool swipeSkins = false; // touch-only home swipe; rotary remains unchanged
    bool tapSelected = true; // Light menus: touch confirms highlighted row; optional direct row activation.
    bool hlsEnabled = false; // experimental M3U8/HLS support; disabled by default
};
inline bool validMenuIdle(uint16_t s){return s==0||s==10||s==20||s==30||s==60||s==120||s==300;}
inline bool validRate(uint16_t k) { return k==0 || k==64 || k==96 || k==128 || k==192 || k==320; }
inline void sanitize(Settings &s) {
    alarmclock::sanitize(s.alarm);
    if(!radiodns::validServer(s.dnsPrimary)||!radiodns::validServer(s.dnsSecondary))s.dnsManual=false;
    if (uint8_t(s.language)>2) s.language=Language::HU;
    if (uint8_t(s.theme)>2) s.theme=Theme::Dark;
    if(!validMenuIdle(s.menuIdleSeconds))s.menuIdleSeconds=10;
    if(s.sensorMode>2)s.sensorMode=0;
    if(s.batteryStep!=1&&s.batteryStep!=5&&s.batteryStep!=10&&s.batteryStep!=25)s.batteryStep=1;
    if(s.batteryAppearance>2)s.batteryAppearance=0;
    // Invalid saved calibration stays visibly invalid; never silently substitute another curve.
    s.rotation &= 3;
    if (s.brightness>100 || (s.brightness>0 && s.brightness<10)) s.brightness=100;
    if (s.brightness==0 && s.sensorMode!=1) s.brightness=100;
    if(unsigned(s.graphicEq.preset)>=eq10::PresetCount && s.graphicEq.preset!=eq10::Preset::Custom)
        s.graphicEq.preset=eq10::Preset::Custom;
    for(auto& gain:s.graphicEq.db)if(gain< -12||gain>6)gain=0;
    if (!validRate(s.maxKbps)) s.maxKbps=0;
    if (s.rotaryMode<1 || s.rotaryMode>2) s.rotaryMode=1;
    if (s.volumeStep<1 || s.volumeStep>10) s.volumeStep=1;
    if (s.vintageSkin>6) s.vintageSkin=0;
    if (s.bootScreen>4) s.bootScreen=0;
    if (s.bootSound>2) s.bootSound=0;
    if (s.networkMode>2) s.networkMode=0;
    if (s.retroColor>8) s.retroColor=0;
    // Retired Basic player: keep saved IDs stable, migrate only its old flags.
    if(!s.playerEnabled&&!s.cassetteEnabled&&!s.vintageSkin){s.playerEnabled=true;s.kajonSkin=false;s.mediaPlayerSkin=false;}
#if IBT_COMPACT_UI
    s.playerEnabled=true;s.cassetteEnabled=false;s.mediaPlayerSkin=true;s.kajonSkin=false;
    if(s.vintageSkin!=5)s.vintageSkin=0;
    s.artworkEnabled=false;s.reelAnimation=false;s.bootScreen=0;s.bootSound=0;s.hlsEnabled=false;
#else
    s.playerEnabled=true;s.cassetteEnabled=false;s.mediaPlayerSkin=false;s.kajonSkin=false;s.vintageSkin=0;
    s.language=Language::HU;s.rotation&=2;s.reelAnimation=false;
#if !IBT_NO_PSRAM
    s.swipeSkins=false;
#endif
    s.bootScreen=0;s.bootSound=0;
#if !defined(IBT_SENSOR_EXPERIMENTAL)
    s.alarm.clockHome=false;
#endif
    s.alarm.mode=0;s.hlsEnabled=false;
#if defined(FEATURE_ARTWORK) && !FEATURE_ARTWORK
    s.artworkEnabled=false;
#endif
#endif

}
inline unsigned playerSkinId(const Settings& s){return s.vintageSkin==6?15:s.vintageSkin==5?12:s.vintageSkin==4?11:s.vintageSkin?5+s.vintageSkin:s.playerEnabled?(s.kajonSkin?4:s.mediaPlayerSkin?3:2):s.cassetteEnabled?1:2;}
inline uint8_t vintageForSkin(unsigned id){return id==15?6:id==12?5:id==11?4:id>=6&&id<=8?uint8_t(id-5):0;}
struct Point { int x, y; };
struct Size { int w, h; };
inline Size logicalSize(Size p, unsigned r) { return (r&1)?Size{p.h,p.w}:p; }
// A deliberate horizontal gesture on the home screen. Keep the middle and
// lower artwork available for swiping; taps still require very little travel.
inline bool skinSwipeGesture(Point from, Point to, Size screen, uint32_t elapsedMs) {
    const int dx=to.x-from.x,dy=to.y-from.y;
    return screen.w==320&&screen.h==240&&from.y>=26&&from.y<=218&&
           to.y>=26&&to.y<=218&&(dx>=70||dx<=-70)&&
           dy>=-42&&dy<=42&&elapsedMs>=40&&elapsedMs<=2000;
}
inline Point toPhysical(Point p, Size native, unsigned r) {
    switch(r&3) {
    case 1:return {native.w-1-p.y,p.x};
    case 2:return {native.w-1-p.x,native.h-1-p.y};
    case 3:return {p.y,native.h-1-p.x};
    default:return p;
    }
}
inline Point toLogical(Point p, Size native, unsigned r) {
    switch(r&3) {
    case 1:return {p.y,native.w-1-p.x};
    case 2:return {native.w-1-p.x,native.h-1-p.y};
    case 3:return {native.h-1-p.y,p.x};
    default:return p;
    }
}
inline bool isDark(const Settings &s, int hour, bool validTime) {
    return s.theme==Theme::Dark || (s.theme==Theme::Auto && (!validTime || hour<7 || hour>=19));
}
inline Action adcKey(int mv) {
    const int centres[]={0,595,1055,1650};
    const Action actions[]={Action::Prev,Action::Next,Action::VolDown,Action::VolUp};
    for(unsigned i=0;i<4;i++) if(mv>=centres[i]-180 && mv<=centres[i]+180) return actions[i];
    return Action::None;
}
inline bool repeatable(Action a) { return a==Action::VolDown || a==Action::VolUp; }
class Debounce {
    int raw_=0, stable_=0;
    uint32_t changed_=0;
public:
    int value() const { return stable_; }
    bool update(int raw, uint32_t now, unsigned ms=35) {
        if(raw!=raw_) { raw_=raw; changed_=now; }
        if(raw_!=stable_ && uint32_t(now-changed_)>=ms) { stable_=raw_; return true; }
        return false;
    }
};
class Quadrature {
    uint8_t previous_=3; int8_t sum_=0;
public:
    int update(uint8_t next) {
        static const int8_t t[]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
        next &= 3;
        if((previous_^next)==3) sum_=0;
        else sum_+=t[(previous_<<2)|next];
        previous_=next;
        if(sum_>=4) {sum_=0;return 1;} if(sum_<=-4) {sum_=0;return -1;} return 0;
    }
};
// Strict enough for bounded metadata display; malformed sequences consume at least one byte.
inline uint32_t nextUtf8(const char *&s) {
    unsigned c=uint8_t(*s++); if(c<128) return c;
    unsigned count=(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:(c&0xf8)==0xf0?3:0;
    if(!count) return '?';
    uint32_t n=c & ((1u<<(6-count))-1);
    for(unsigned i=0;i<count;i++) {unsigned b=uint8_t(*s);if((b&0xc0)!=0x80)return '?';s++;n=(n<<6)|(b&63);}
    if(n>0x10ffff || (n>=0xd800&&n<=0xdfff) || (count==1&&n<128) || (count==2&&n<2048) || (count==3&&n<65536))return '?';
    return n;
}
inline void textCopy(char *to,size_t capacity,const char *from) {
    if(!capacity)return;
    size_t n=0; const char *s=from?from:"";
    while(*s) {const char *start=s;nextUtf8(s);size_t k=size_t(s-start);if(n+k>=capacity)break;memcpy(to+n,start,k);n+=k;}
    to[n]=0;
}
const char *tr(Language language, Text key);
}
