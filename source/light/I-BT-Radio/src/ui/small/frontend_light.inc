#include "light_clock_face.h"
#include "light_menu_font.h"
#include "../light_menu_layout.h"
#include "../light_player_status.h"
#include "light_clock_font.h"
#include "../light_player_meter.h"
#include "../../core/options.h"
#include "../frontend.h"
#include "../content.h"
#include "../light_ui_policy.h"
#include "../light_menu_rows.h"
#include "../light_player_layout.h"
#include "../light_player_controls.h"
#include "../audio_format_label.h"
#include "../wifi_ip_indicator.h"
#include "../lite_list.h"
#include "../bt_speaker_layout.h"
#include "../setup_layout.h"
#include "../wifi_password_layout.h"
#include "../user_message_hu.h"
#include "../../dual/vu_probe.h"
#include "../alarm_layout.h"
#include "../../dual/bt_audio.h"
#include "../../core/config.h"
#include "../../core/network.h"
#include <SD.h>
#include "../../core/sdmanager.h"
#include "../../core/player.h"
#include "../../dual/boot_progress.h"
#include "light_boot_render.h"
#include "../boot_custom.h"
#include "weather_light_render.h"
#include "../../dual/weather.h"
#include "../../dual/sensors/receiver.h"
#include <Adafruit_ST7789.h>
#include <glcdfont.c>
#include <time.h>
#include <atomic>
namespace front {
static SPIClass bus(HSPI);
static Adafruit_ST7789 lcd(&bus,TFT_CS,TFT_DC,TFT_RST);
static constexpr uint16_t black=0,white=0xffff,cyan=0x7fff,lime=0xe7f0,green=0x5fe0,dim=0x2945;
static bool ready=false,dirty=true,deferred=false,playerCadenceDeferred=false;
static lightboot::Paint bootPaint;
static lightweather::Paint weatherPaint;
static bool bootNeedsLayout=false;
static unsigned shownPage=~0u,firstRow=0,oldSelection=~0u;
static uint8_t shownSurface=255;
static uint32_t frameAt=0,meterAt=0,oldBanner=0,oldMeta=0,oldStatus=0,oldClock=0;
static uint8_t oldStyle=255,levelsL=255,levelsR=255;
static lightmenu::Rows menuRowCache;
static uint32_t rowIpHash=0;
static std::atomic<bool> transportPulsePending{false};
static uint32_t transportPulseUntil=0;
static bool transportPulseShown=false,transportWasPlaying=false;
static lightcontrols::Feedback buttonFeedback;
static uint8_t previousButtons[5]={255,255,255,255,255};
void playerButtonPressed(unsigned index){buttonFeedback.press(int(index));}
void transportAck(){transportPulsePending.store(true,std::memory_order_relaxed);}
static uint32_t hash(const char* s,uint32_t h=2166136261u){while(s&&*s){h^=uint8_t(*s++);h*=16777619u;}return h;}
static uint16_t glyphCode(uint32_t c){unsigned accent=0;
  switch(c){
   case 225:c='a';accent=1;break;case 193:c='A';accent=1;break;
   case 233:c='e';accent=1;break;case 201:c='E';accent=1;break;
   case 237:c='i';accent=1;break;case 205:c='I';accent=1;break;
   case 243:c='o';accent=1;break;case 211:c='O';accent=1;break;
   case 250:c='u';accent=1;break;case 218:c='U';accent=1;break;
   case 246:c='o';accent=2;break;case 214:c='O';accent=2;break;
   case 252:c='u';accent=2;break;case 220:c='U';accent=2;break;
   case 337:c='o';accent=3;break;case 336:c='O';accent=3;break;
   case 369:c='u';accent=3;break;case 368:c='U';accent=3;break;
   case 8594:c='>';break;case 8212:case 8211:c='-';break;case 183:c='.';break;
   default:break;
  }
 return uint16_t((c>=32&&c<127?c:'?')|(accent<<8));
}
// Transparent text uses the same glyph mapping as the cover row compositor.
static void text(int x,int y,const char* s,uint16_t ink=cyan,unsigned scale=1,int width=312){
 lcd.setTextColor(ink);lcd.setTextSize(scale);const int end=std::min(320,x+width);
 for(unsigned count=0;s&&*s&&count<64&&x+int(6*scale)<=end;++count,x+=6*scale){
  const uint16_t g=glyphCode(dual::nextUtf8(s));const unsigned c=g&255,accent=g>>8;
  lcd.setCursor(x,y+2*scale);lcd.write(c>=32&&c<127?c:'?');
  if(accent==1||accent==3){lcd.fillRect(x+3*scale,y,scale,scale,ink);lcd.fillRect(x+2*scale,y+scale,scale,scale,ink);}
  if(accent==2){lcd.fillRect(x+scale,y,scale,scale,ink);lcd.fillRect(x+3*scale,y,scale,scale,ink);}
  if(accent==3){lcd.fillRect(x+5*scale,y,scale,scale,ink);lcd.fillRect(x+4*scale,y+scale,scale,scale,ink);}
 }
}
// Medium9px advance between the old6px and12px fonts; glyph-only writes.
static unsigned glyphCount(const char* s){unsigned n=0;while(s&&*s&&n<64){dual::nextUtf8(s);++n;}return n;}
static void mediumText(int x,int y,const char* s,uint16_t ink=cyan,int width=304,unsigned advance=9){
 const int end=std::min(320,x+width);
 if(!s||!*s||x+int(advance)>end)return;
 // One bounded SPI transaction per string; identical transparent glyph pixels.
 // Horizontal runs avoid a transaction and address window for every lit dot.
 lcd.startWrite();
 for(unsigned count=0;s&&*s&&count<64&&x+int(advance)<=end;++count,x+=advance){
  const uint16_t g=glyphCode(dual::nextUtf8(s));const unsigned c=g&255,a=g>>8;
  for(unsigned gy=0;gy<9;++gy){
   unsigned mask=0;
   if(gy>=2)for(unsigned col=0;col<5;++col)if(pgm_read_byte(font+c*5+col)&(1u<<(gy-2)))mask|=1u<<col;
   if(gy==0){if(a==1||a==3)mask|=1u<<3;if(a==2)mask|=(1u<<1)|(1u<<3);if(a==3)mask|=1u<<5;}
   if(gy==1){if(a==1||a==3)mask|=1u<<2;if(a==3)mask|=1u<<4;}
   for(unsigned col=0;col<6;){
    if(!(mask&(1u<<col))){++col;continue;}
    const unsigned first=col;while(col<6&&(mask&(1u<<col)))++col;
    const int left=advance==6?int(first):int(first*3/2),right=advance==6?int(col):int(col*3/2);
    lcd.writeFillRect(x+left,y+int(gy*3/2),right-left,int((gy+1)*3/2-gy*3/2),ink);
   }
  }
 }
 lcd.endWrite();
}
static void header(const char* label,int width=304){lcd.fillRect(0,0,320,28,lime);text(8,3,label,black,2,width);}
// Fixed menu header: both existing touch targets stay unchanged.
static void menuHeader(const char* ip){
 lcd.fillRect(0,0,320,28,lime);
 const char* title=shownPage==1?"Menü":content::pageTitle(dual::Language::HU,shownPage);
 menufont::text(lcd,8,2,title,black,lime,104,16);
 menufont::text(lcd,248,2,"Vissza",black,lime,48,16);
 const char* address=ip&&*ip?ip:"Nincs IP-cím";
 const int w=menufont::width(address,16);
 menufont::text(lcd,178-w/2,2,address,black,lime,124,16);
}
static void overlayButton(int x,int y,int w,int h,const char* label,bool active=false){
 lcd.drawRect(x,y,w,h,dim);const int width=menufont::width(label,15);
 menufont::text(lcd,x+std::max(2,(w-width)/2),y+(h-20)/2,label,active?green:cyan,black,w-4,15);
}
static void button(int x,int y,int w,int h,const char* label,bool active=false){unsigned n=0;const char* p=label;while(p&&*p&&n<64){dual::nextUtf8(p);++n;}lcd.drawRect(x,y,w,h,dim);
 if((shownPage!=0&&shownPage!=39)||shownSurface!=0){const unsigned advance=n*9<=unsigned(w-4)?9:6;mediumText(x+std::max(2,(w-int(n)*int(advance))/2),y+(h-14)/2,label,active?green:cyan,w-4,advance);}
 else text(x+std::max(2,(w-int(n)*6)/2),y+(h-11)/2,label,active?green:cyan,1,w-4);}
// Setup controls keep the existing touch targets, with readable 2x glyphs.
static void setupButton(setupui::Rect r,const char* first,const char* second=nullptr,bool inHeader=false){
 lcd.fillRect(r.x,r.y,r.w,r.h,inHeader?lime:black);
 lcd.drawRect(r.x,r.y,r.w,r.h,inHeader?black:cyan);
 const auto line=[&](const char* label,int y){unsigned n=0;const char* p=label;while(p&&*p){dual::nextUtf8(p);++n;}
  mediumText(r.x+std::max(2,(r.w-int(n)*9)/2),y,label,inHeader?black:cyan,r.w-4);};
 const int y=r.y+(r.h-(second?38:18))/2;
 line(first,y);if(second)line(second,y+20);
}
bool earlyBoot(){
 if(ready)return true;bus.begin(DUAL_TFT_SCK,DUAL_TFT_MISO,DUAL_TFT_MOSI,TFT_CS);
 lcd.init(240,320);lcd.setRotation(1);lcd.invertDisplay(false);lcd.setTextWrap(false);
 pinMode(BRIGHTNESS_PIN,OUTPUT);digitalWrite(BRIGHTNESS_PIN,HIGH);lcd.fillScreen(black);
 bootcustom::prepare();
 lightboot::session().begin(millis());
 lightboot::session().holdMs=bootcustom::seconds()*1000u;
 const lightboot::Signal signals[3]={lightboot::Signal::Off,lightboot::Signal::Off,lightboot::Signal::Off};
 while(!lightboot::paint(lcd,bootPaint,signals,1,255,bootcustom::row))vTaskDelay(1);
 lightboot::session().painted(millis(),false);
 shownPage=lightboot::page;ready=true;return true;
}
bool begin(){return earlyBoot();}dual::Size nativeSize(){return {320,240};}
void invalidate(){dirty=true;playerCadenceDeferred=false;}bool renderPending(){return deferred||buttonFeedback.active()||transportPulseShown||transportPulsePending.load(std::memory_order_relaxed);}
bool menuViewport(unsigned page,unsigned& first){first=firstRow;return !dirty&&!deferred&&shownPage==page;}
unsigned stationListFirst(){return firstRow;}
void setRotation(uint8_t r){if(lcd.getRotation()!=((r+1)&3))bootNeedsLayout=true;lcd.setRotation((r+1)&3);dirty=true;}
static void digit(int x,int y,unsigned d,int w,int h){
 if(d>9)return;const unsigned size=34;
 lightclock::character(lcd,x,y+(h-int(size+4))/2,w,char('0'+d),size,lime,black);
}
 // Light has no artwork compositor or pixel/glyph cache.
static void metadataLine(const char* line,int y,unsigned scale,unsigned){text(8,y,line,scale==1?cyan:white,scale,304);}
static void metadata(const dual::Snapshot& s,bool speaker){
 metadataLine(s.artist,34,1,0);
 const char* at=s.title;const unsigned rows=1;
 for(unsigned row=0;row<rows&&*at;++row){
  char line[104];unsigned bytes=0,count=0,lastSpace=0;const char* nextSpace=nullptr;
  while(*at&&count<25){const char* start=at;dual::nextUtf8(at);const unsigned n=unsigned(at-start);if(bytes+n>=sizeof(line)){at=start;break;}
   if(n==1&&*start==' '){lastSpace=bytes;nextSpace=at;}for(unsigned i=0;i<n;++i)line[bytes++]=start[i];++count;
  }
  if(*at&&row+1<rows&&nextSpace&&lastSpace>8){bytes=lastSpace;at=nextSpace;}
  line[bytes]=0;metadataLine(line,53+int(row)*23,2,row+1);while(*at==' ')++at;
 }
}
static void clock(bool speaker,bool force){
 time_t t=time(nullptr);const uint32_t key=uint32_t(t/60);if(key==oldClock&&!force)return;oldClock=key;
 char value[6]="--:--";tm local{};if(t>=1600000000&&localtime_r(&t,&local))snprintf(value,sizeof(value),"%02u:%02u",unsigned(local.tm_hour),unsigned(local.tm_min));
 if(speaker){lcd.fillRect(164,99,148,42,black);lightclock::face(lcd,170,101,29,16,34,value,lime,black);return;}
 lcd.fillRect(lightplayer::clockX,lightplayer::clockY,lightplayer::clockW,lightplayer::clockH,black);
 lightclock::face(lcd,104,83,47,18,52,value,white,black);
}
 static void speakerMeters(bool speaker,bool force){
 const unsigned style=lightui::style.load();const uint8_t l=vuprobe::dacLeft.load(),r=vuprobe::dacRight.load();
 if(!force&&oldStyle==style&&l==levelsL&&r==levelsR)return;
 const bool layout=force||oldStyle!=style;if(layout)lcd.fillRect(8,99,144,speaker?42:69,black);
 const uint8_t previousL=levelsL,previousR=levelsR;
 oldStyle=style;levelsL=l;levelsR=r;if(!style)return;
 lightmeter::compactDelta(lcd,lightui::levels(l,lightmeter::steps),lightui::levels(r,lightmeter::steps),lightui::levels(previousL,lightmeter::steps),lightui::levels(previousR,lightmeter::steps),layout);
}
 struct PeakHold{uint8_t level=0;uint32_t at=0;};static PeakHold peaks[2];
static void meters(bool speaker,bool force){
 if(speaker){speakerMeters(true,force);return;}
 const unsigned style=lightui::style.load();const uint8_t values[]={vuprobe::dacLeft.load(),vuprobe::dacRight.load()};
 const uint32_t now=millis();bool decay=false;for(unsigned ch=0;ch<2;++ch)decay|=peaks[ch].level>lightui::levels(values[ch],lightmeter::steps)&&uint32_t(now-peaks[ch].at)>=300;
 if(!force&&oldStyle==style&&values[0]==levelsL&&values[1]==levelsR&&!decay)return;
 const bool layout=force||oldStyle!=style;if(layout){lcd.fillRect(lightplayer::vuX,lightplayer::vuY,lightplayer::vuW,lightplayer::vuH,black);for(auto& p:peaks)p={};}
 const uint8_t previous[]={levelsL,levelsR};
 oldStyle=style;levelsL=values[0];levelsR=values[1];if(!style)return;
 for(unsigned ch=0;ch<2;++ch){const unsigned n=lightui::levels(values[ch],lightmeter::steps);auto& peak=peaks[ch];const unsigned previousPeak=peak.level;
  if(n>=peak.level){peak.level=n;peak.at=now;}else if(uint32_t(now-peak.at)>=300){--peak.level;peak.at=now-200;}
  lightmeter::channelDelta(lcd,style,ch,n,peak.level,lightui::levels(previous[ch],lightmeter::steps),previousPeak,layout);
 }
}
 // Auxiliary painters may run between a network transition and its next frame.
// Only the currently rendered player surface owns those LCD regions.
static bool playerPaintOwned(){
 return ready&&!dirty&&(!deferred||playerCadenceDeferred)&&shownSurface==0&&
   (shownPage==0||shownPage==39)&&!(shownPage==0&&network.status==SOFT_AP);
}
void liteMeterTick(bool allowed){
 if(!allowed||!playerPaintOwned())return;
 const uint32_t now=millis();const unsigned ms=vuprobe::refreshMs.load();if(!lightui::due(now,meterAt,ms))return;meterAt=now;
 meters(shownPage==39,false);
}
// Small independently updated header widgets: no extra ADC task/pin or buffer.
static void playerHeaderStatus(const dual::Snapshot& s,bool force){
 const auto signal=wifiindicator::state(s.connected,s.ip,s.rssi);
 const bool bluetooth=btaudio::connected();const unsigned signalKey=wifiindicator::connectionKey(signal,bluetooth);
 static unsigned oldSignal=~0u;if(!force&&oldSignal==signalKey)return;oldSignal=signalKey;
 lcd.fillRect(240,0,80,28,lightcontrols::panel);
 wifiindicator::drawTransport(lcd,signal,bluetooth,lightcontrols::panel,white);
 // Battery hardware is not fitted: an outlined cell plus dash means unknown.
 lcd.drawRect(282,6,26,15,white);lcd.fillRect(308,10,3,7,white);lcd.fillRect(291,12,8,2,white);lcd.fillRect(240,27,80,1,lightcontrols::border);
}
static void player(const dual::Snapshot&s,bool speaker,bool full,bool force){
 const unsigned volume=std::min(100u,(unsigned(s.volume)*100u+127u)/254u);
 const bool playing=speaker?btaudio::inputPlaying():s.playing;
 if(transportPulsePending.exchange(false,std::memory_order_relaxed))transportPulseUntil=millis()+600;
 const bool pulse=transportPulseUntil&&int32_t(transportPulseUntil-millis())>0;
 char line[160];const char* banner=speaker||btaudio::inputSelected()?(s.station[0]?s.station:"BT-forrás"):btaudio::outputSelected()?"SD > Bluetooth":s.sd?"SD-kártya":s.station[0]?s.station:"I-BT-Radio";
 uint32_t key=hash(banner);const bool headerChanged=full||force||key!=oldBanner;if(headerChanged){oldBanner=key;lcd.fillRect(0,0,240,28,lightcontrols::panel);menufont::text(lcd,10,2,banner,white,lightcontrols::panel,224,16);lcd.fillRect(0,27,320,1,lightcontrols::border);}playerHeaderStatus(s,headerChanged);
 key=hash(s.title,hash(s.artist));const bool metaChanged=full||force||key!=oldMeta;if(metaChanged){oldMeta=key;lcd.fillRect(5,31,310,48,black);lcd.fillRoundRect(5,32,310,46,7,lightcontrols::panel);
  metadata(s,speaker);
 }
 char hud[64];const char* route=btaudio::inputSelected()?"BT > DAC":btaudio::outputSelected()?"SD > BT":s.sd?"SD > DAC":"Wi-Fi > DAC";
 lightplayerstatus::format(hud,sizeof(hud),route,s.audioCodec,s.formatKbps,s.sampleRate,s.sourceBits);
 key=unsigned(s.volume)+unsigned(playing)*256u+unsigned(s.sd)*512u+unsigned(btaudio::connected())*1024u+hash(s.notice)+hash(hud);
 const bool statusChanged=full||force||key!=oldStatus;
 if(statusChanged){oldStatus=key;lcd.fillRect(8,169,304,26,black);
  text(8,169,hud,cyan,1,304);
 }
 {static unsigned previousBuffer=~0u,previousVolume=~0u;static uint16_t previousBufferColor=0;const unsigned fill=s.settings.bufferIndicator&&s.bufferCapacity?unsigned(std::min<uint64_t>(108,uint64_t(s.buffer)*108u/s.bufferCapacity)):0;
  if(statusChanged){text(8,182,"Puffer",cyan,1,36);lcd.drawRect(46,184,110,9,dim);text(164,182,"Hangerő",cyan,1,48);lcd.drawRect(212,184,100,9,dim);}
  const uint16_t bufferInk=lightplayerstatus::bufferColor(s.bufferCapacity?unsigned(std::min<uint64_t>(100,uint64_t(s.buffer)*100u/s.bufferCapacity)):0);
  if(statusChanged||fill!=previousBuffer||bufferInk!=previousBufferColor){previousBuffer=fill;previousBufferColor=bufferInk;lcd.fillRect(47,185,108,7,black);if(fill)lcd.fillRect(47,185,fill,7,bufferInk);}
  if(statusChanged||volume!=previousVolume){previousVolume=volume;lcd.fillRect(213,185,98,7,black);lcd.fillRect(213,185,volume*98u/100u,7,cyan);}
 }
 clock(speaker,full);if(full)meters(speaker,true);
 const int pressed=buttonFeedback.advance(millis());
 const unsigned count=speaker?4:5;
 for(unsigned i=0;i<count;++i){
  const unsigned id=speaker&&i==3?4:i;
  const bool lit=pressed==int(id)||(id==1&&pulse);
  const uint8_t value=uint8_t(unsigned(lit)*2u+(id==1&&playing?1u:0u)+(id==4&&btaudio::outputSelected()?4u:0u));
  if(!full&&previousButtons[id]==value)continue;previousButtons[id]=value;
  int x=int(i)*64,y=196,w=64,h=44;
  if(speaker){const btspeakerui::Rect rects[]={btspeakerui::previous,btspeakerui::toggle,btspeakerui::next,btspeakerui::menu};auto r=rects[i];x=r.x;y=r.y;w=r.w;h=r.h;}
  lightcontrols::paint(lcd,x,y,w,h,id,playing,lit,btaudio::outputSelected(),
   [&](int center,int top,const char* label,uint16_t ink,int width){text(center-int(glyphCount(label))*3,top,label,ink,1,width);});
 }
 transportPulseShown=pulse;transportWasPlaying=playing;
 if(speaker){const char* state=btaudio::connected()?"Telefon csatlakoztatva":"Várakozás a telefonra";const uint32_t stateKey=hash(state);static uint32_t oldState=0;if(full||oldState!=stateKey){oldState=stateKey;lcd.fillRect(8,80,304,14,black);text(8,81,state,btaudio::connected()?green:cyan);}}
}
// Reuse the existing time, alarm settings, controls and ROM glyphs. No heap,
// framebuffer, new timer/task or periodic weather/cover requests.
static void alarmHome(const dual::Snapshot& s,bool full){
 time_t now=time(nullptr);tm local{};const bool valid=now>=1600000000&&localtime_r(&now,&local);
 const unsigned style=lightui::clockStyle.load()?1:0;const uint32_t minute=valid?uint32_t(now/60):~0u;
 const bool blink=!s.settings.alarm.blinkColon||(now&1)==0;
 const bool changed=full||minute!=oldClock||oldStyle/2!=style;
 char digits[6]="--:--";if(valid)snprintf(digits,sizeof(digits),"%02u:%02u",unsigned(local.tm_hour),unsigned(local.tm_min));
 if(changed){oldClock=minute;lightclockface::digits(lcd,style,digits,valid&&blink);}
 else if(oldStyle%2!=unsigned(blink))lightclockface::colon(lcd,valid&&blink);
 oldStyle=uint8_t(style*2+unsigned(blink));
 const bool controls=dual::clockControlsVisible();const unsigned focus=dual::clockHomeFocus();
 const uint32_t controlsKey=1u+unsigned(controls)+focus*4u+unsigned(s.settings.alarm.mode)*64u+unsigned(s.settings.alarm.daysMask)*256u+unsigned(s.settings.alarm.onHour)*65536u+unsigned(s.settings.alarm.onMinute)*16777216u;
 const bool controlChanged=full||controlsKey!=oldBanner;oldBanner=controlsKey;
 if(controlChanged){lcd.fillRect(0,0,320,38,black);lcd.fillRect(0,188,320,52,black);
  if(controls){overlayButton(5,4,150,25,"Menü",focus==9);overlayButton(165,4,150,25,"Ébresztő",focus==10);
   button(0,200,30,40,s.settings.alarm.mode?"Be":"Ki",focus==0);char t[8];snprintf(t,sizeof(t),"%02u:%02u",unsigned(s.settings.alarm.onHour),unsigned(s.settings.alarm.onMinute));button(30,200,72,40,t,focus==1);
   static const char*const names[]={"H","K","Sz","Cs","P","Sz","V"};for(unsigned day=0;day<7;++day){const int x=102+(int(day)*218+6)/7,end=102+(int(day+1)*218+6)/7;button(x,200,end-x,40,names[day],focus==day+2);if(s.settings.alarm.daysMask&(1u<<day))lcd.drawFastHLine(x+4,235,end-x-8,lime);}
  }
 }
 char line[64];static const char*const days[]={"Vasárnap","Hétfő","Kedd","Szerda","Csütörtök","Péntek","Szombat"};
 if(valid)snprintf(line,sizeof(line),"%04u.%02u.%02u. / %s",unsigned(local.tm_year+1900),unsigned(local.tm_mon+1),unsigned(local.tm_mday),days[local.tm_wday]);else snprintf(line,sizeof(line),"Pontos időre vár / Wi-Fi, NTP");
 const auto dateKey=hash(line);if(!controls&&(controlChanged||dateKey!=oldMeta)){lcd.fillRect(0,0,320,38,black);lightweather::text(lcd,160,13,line,12,cyan,black,1,310);}oldMeta=dateKey;
 if(s.settings.alarm.mode){const char* mask=s.settings.alarm.daysMask==31?"H-P":s.settings.alarm.daysMask==127?"Minden nap":s.settings.alarm.daysMask==96?"Sz-V":"Egyéni";snprintf(line,sizeof(line),"Ébresztés %02u:%02u / %s",unsigned(s.settings.alarm.onHour),unsigned(s.settings.alarm.onMinute),mask);}else snprintf(line,sizeof(line),"Ébresztés kikapcsolva");
 const auto statusKey=hash(line);if(!controls&&(controlChanged||statusKey!=oldStatus)){lcd.fillRect(0,188,320,52,black);lcd.fillRoundRect(44,194,232,32,12,lightclockface::panel);lightclockface::bell(lcd);lightweather::text(lcd,174,201,line,12,white,lightclockface::panel,1,232);}oldStatus=statusKey;
}
static void alarmTimeEditor(unsigned selected,bool full,bool force){
 const unsigned hour=dual::alarmDraftHour(),minute=dual::alarmDraftMinute();const bool error=dual::alarmDraftError();
 const uint32_t key=1u+hour+minute*32u+selected*2048u+unsigned(error)*16384u;
 if(!full&&!force&&key==oldStatus)return;oldStatus=key;
 if(full){lcd.fillRect(0,0,320,32,black);button(5,3,73,27,"Vissza");mediumText(91,8,dual::alarmDraftEnding()?"Kikapcsolás ideje":"Ébresztés ideje",cyan,222);}
 lcd.fillRect(8,45,304,140,black);lcd.drawRect(10,62,140,65,selected==0?lime:dim);lcd.drawRect(170,62,140,65,selected==1?lime:dim);
 digit(35,68,hour/10,40,52);digit(85,68,hour%10,40,52);digit(195,68,minute/10,40,52);digit(245,68,minute%10,40,52);
 if(error)text(10,128,"Hibás időablak: javítsd az időt",0xf800,1,304);
 button(10,140,140,44,"-",false);button(170,140,140,44,"+",false);button(10,190,140,44,"Mentés",selected==2);button(170,190,140,44,"Mégse",selected==3);
}

static void menuRows(const dual::Snapshot& s,unsigned page,unsigned selected,unsigned count,unsigned first,unsigned visible,int step,int inset,bool full,char* line,size_t cap){
 auto& rows=menuRowCache;
 const uint32_t geometry=(first<<16)|(count<<8)|visible;
 const int top=lightmenu::top(page);
 if(rows.begin(geometry,full)&&!full)lcd.fillRect(0,top,320,240-top,black);
 firstRow=first;
 for(unsigned i=0;i<visible&&first+i<count;++i){
  const unsigned item=first+i;const bool enabled=content::menuItem(line,cap,s,page,item);
  const bool active=item==selected;const uint32_t key=hash(line,(active?2u:0u)|(enabled?1u:0u));
  if(!rows.changed(i,key))continue;
  const int y=top+int(i)*step;lcd.fillRect(4,y,312,step,active?lime:black);
  menufont::text(lcd,8,y+(step-20)/2,line,active?black:enabled?cyan:dim,active?lime:black,304);
 }
}

static void listRow(unsigned row,const char* label,bool active){
 const int y=litelist::top+int(row)*litelist::step;const uint16_t bg=active?lime:black;
 lcd.fillRect(4,y,312,litelist::step,bg);
 menufont::text(lcd,8,y+(litelist::step-20)/2,label,active?black:cyan,bg,304);
}

static void bluetoothRows(const dual::Snapshot& s,unsigned page,unsigned selected,bool full){
 const unsigned count=page==40?btaudio::pickerCount():btaudio::savedDeviceCount();
 const unsigned visible=page==40?7:4,history=page==40?btaudio::pickerHistoryCount():0;
 unsigned first=page==44?0:selected>=visible/2?selected-visible/2:0;
 if(count>visible&&first+visible>count)first=count-visible;
 firstRow=first;
 const uint32_t geometry=0x80000000u|(page<<24)|(first<<16)|(count<<8)|history;
 const bool changed=menuRowCache.begin(geometry,full);
 const uint32_t ipKey=hash(s.ip);if(full||rowIpHash!=ipKey){menuHeader(s.ip);rowIpHash=ipKey;}
 if(changed&&!full)lcd.fillRect(0,28,320,212,black);
 bool painted=false;
 const int step=page==40?26:30;
 for(unsigned i=0;i<visible&&first+i<count;++i){
  btaudio::Device d{};const unsigned item=first+i;
  const bool ok=page==40?btaudio::pickerDevice(item,d):btaudio::savedDevice(item,d);
  const uint32_t key=hash(ok?d.name:"",item==selected?1u:0u);
  if(!menuRowCache.changed(i,key))continue;
  lcd.fillRect(4,33+int(i)*step,312,step,black);
  if(ok)menufont::text(lcd,8,35+int(i)*step,d.name,item==selected?lime:cyan,black,304);
  painted=true;
 }
 if(!count&&menuRowCache.changed(0,hash(page==40?"scan-empty":"saved-empty"))){
  text(8,55,page==40?"Eszközök keresése...":"Nincs mentett eszköz");painted=true;
 }
 if(page==40&&painted&&history&&history>=first&&history<=first+visible)
  lcd.drawFastHLine(8,33+int(history-first)*26,304,cyan);
 if(page==44&&menuRowCache.changed(4,selected==4?1u:0u)){
  lcd.fillRect(4,156,312,15,black);text(8,156,"Új keresés",selected==4?lime:cyan);
 }
}

// One UI-owned overlay. Clearing it schedules a normal complete repaint so no
// saved pixels/framebuffer or audio-buffer manipulation is needed.
static bool overloadPainted=false;
struct OverloadPaint {bool active;~OverloadPaint(){if(active)lcd.fillCircle(309,14,5,0xf800);overloadPainted=active;}};
static void resetSurfaceCaches(){
 oldBanner=oldMeta=oldStatus=oldClock=0;oldStyle=255;levelsL=levelsR=255;
 oldSelection=~0u;firstRow=0;rowIpHash=0;menuRowCache=lightmenu::Rows{};
 weatherPaint=lightweather::Paint{};for(auto& p:peaks)p={};
 for(auto& value:previousButtons)value=255;
}
void render(const dual::Snapshot&s,unsigned page,unsigned selected,bool force){
 playerCadenceDeferred=false;
 const bool overload=dual::uiOverloadActive();
 const bool restoreWarning=overloadPainted&&!overload;
 if(restoreWarning&&page!=lightboot::page)dirty=true;
 if(page==lightboot::page){
  OverloadPaint warning{overload};
  // Early boot already painted the background. Ordinary invalidation/force
  // updates only LEDs/dots/focus; a real page entry or rotation owns a new layout.
  if(shownPage!=page||bootNeedsLayout){lcd.fillScreen(black);resetSurfaceCaches();bootPaint.reset();bootcustom::open();bootNeedsLayout=false;lightboot::session().painted(millis(),false);}
  else if(restoreWarning&&bootPaint.complete(lcd.height()))lightboot::region(lcd,303,8,13,13,bootcustom::row);
  shownPage=page;shownSurface=0;dirty=false;
  transportPulsePending.store(false,std::memory_order_relaxed);transportPulseShown=false;buttonFeedback.clear();
  auto& boot=lightboot::session();
  const bool connected=network.status==CONNECTED&&WiFi.status()==WL_CONNECTED;
  const uint8_t sdCheck=lightboot::readiness().sd.load(std::memory_order_acquire);
  const bool btLoaded=lightboot::readiness().btLoaded.load(std::memory_order_acquire);
  const bool wifiStable=boot.connectionReady(millis(),connected);
  if(boot.automatic&&bootPaint.complete(lcd.height()))boot.advance(sdCheck!=0,btLoaded,wifiStable);
  const bool sdReady=boot.automatic?sdCheck==2:sdman.ready;
  const bool btReady=btLoaded&&(!boot.automatic||boot.phase>=2);
  const bool wifiReady=connected&&(!boot.automatic||(boot.phase>=3&&wifiStable));
  const lightboot::Signal signals[3]={
   lightboot::wifi(wifiReady,network.status==SOFT_AP||network.setupFailed(),network.offlinePlayer()),
   boot.automatic?(boot.phase<1?lightboot::Signal::Waiting:sdCheck==2?lightboot::Signal::Ready:lightboot::Signal::Error):
     lightboot::sd(sdman.ready,sdman.catalogReady.load(),sdman.indexError.load()!=sdroute::Error::None),
   boot.automatic?(btReady?lightboot::Signal::Ready:lightboot::Signal::Waiting):
     lightboot::bt(btaudio::connected(),btaudio::linkFailed(),btaudio::wanted())};
  const uint8_t dots=lightboot::progress(sdReady&&(!boot.automatic||boot.phase>=1),btReady,wifiReady);
  deferred=!lightboot::paint(lcd,bootPaint,signals,dots,selected,bootcustom::row);
  if(!deferred)boot.painted(millis(),wifiReady);
  return;
 }
 bootcustom::close();
 const uint32_t now=millis();if(!lightui::due(now,frameAt,lightui::frameMs)){
  deferred=true;
  // Only a timing deferral on the already painted player may release its VU.
  // Dirty/layout/menu/clock/weather/setup work keeps exclusive LCD ownership.
  playerCadenceDeferred=!dirty&&shownPage==page&&shownSurface==0&&(page==0||page==39)&&
   !(page==0&&(network.status==SOFT_AP||weather::active()||s.settings.alarm.clockHome));
  return;
 }
 // Hotspot and player share page 0, but own different complete screens.
 // Schedule one full repaint on either transition; keep normal frame/audio pacing.
 const uint8_t surface=network.status==SOFT_AP&&page==0?1:(page==0&&weather::active()?3:page==0&&s.settings.alarm.clockHome?2:0);
 if((page!=0&&page!=39)||surface){transportPulsePending.store(false,std::memory_order_relaxed);transportPulseShown=false;buttonFeedback.clear();transportPulseUntil=0;}
 deferred=false;frameAt=now;const bool full=dirty||shownPage!=page||shownSurface!=surface;dirty=false;shownSurface=surface;
 OverloadPaint warning{overload};
 if(full){lcd.fillScreen(black);resetSurfaceCaches();}shownPage=page;
 if(network.status==SOFT_AP&&page==0){if(full||force){
  lcd.fillRect(0,0,320,48,lime);mediumText(8,15,"Wi-Fi beállítás",black,204);
  setupButton(setupui::scan(320),"Keresés",nullptr,true);
  mediumText(8,58,"Csatlakozz a beállító hálózathoz");mediumText(8,80,"Jelszó megadása a böngészőben");
  mediumText(8,108,s.ip,lime,304);mediumText(8,150,"Vagy válassz hálózat nélküli módot");
  setupButton(setupui::reboot(240),"Újra-","indítás");
  setupButton(setupui::player(320,240),"Lejátszó");setupButton(setupui::qr(320,240),"IP-cím");
 }return;}
 if(page==0&&weather::active()){
  lightweather::Data d;d.forecast=weather::forecast();const auto c=weather::configuration();snprintf(d.location,sizeof(d.location),"%s",c.location);d.connected=s.connected;d.now=now;d.epoch=time(nullptr);d.overlay=weather::controlsVisible(now);d.status=weather::status();
  for(unsigned r=0;r<2;++r){d.assigned[r]=sensors::roleSelected(r);d.fresh[r]=sensors::roleReading(r,d.sensors[r]);}
  lightweather::paint(lcd,weatherPaint,d,full);return;
 }
 if(page==0&&s.settings.alarm.clockHome){alarmHome(s,full);return;}
 if(page==67){alarmTimeEditor(selected,full,force);return;}
 if(page==0||page==39){player(s,page==39,full,force);return;}
 char line[180];uint32_t key=hash(s.notice,unsigned(s.volume)+unsigned(s.playing)*256u);static uint32_t oldPageKey=0;
 // Scan completion has no UI event: poll its visible data, not repaint the LCD.
 if(page==28){
  const unsigned count=network.scanCount(),rows=network.status==SOFT_AP?6:5;
  unsigned first=selected>=rows/2?selected-rows/2:0;if(count>rows&&first+rows>count)first=count-rows;
  key=selected*65537u+count*17u+network.scanStatus()+rows*257u;
  for(unsigned i=0;i<rows&&first+i<count;++i){char name[33];if(network.scanEntry(first+i,name,sizeof(name)))key=hash(name,key);}
 }
 // Index completion changes list content without a selection/input event.
 // On SD this is an allocation-free read of the validated owner catalog.
 const bool stationPage=page==4||page==29;
 const unsigned listTotal=stationPage?(page==29?dual::favoritesCount():config.playlistLength()):0;
 if(stationPage)key=hash(s.sd?"SD":"RADIO",key^(listTotal*16777619u));
 if(page==31||page==32)key=hash(dual::stationSearchQuery(),key^dual::stationSearchCount()^(dual::stationSearchStatus()<<16));
 if(page==75)key=hash(dual::sourceRouteError(),key);
 key=hash(s.ip,key);
 const unsigned rowCount=content::itemCount(page);
 if(page==40||page==44){bluetoothRows(s,page,selected,full);return;}
 const bool ordinaryRows=rowCount&&page!=4&&page!=29&&page!=40&&page!=44&&page!=28&&page!=42&&page!=69&&page!=41;
 if(ordinaryRows){
  const uint32_t ipKey=hash(s.ip);
  if(full||rowIpHash!=ipKey){menuHeader(s.ip);rowIpHash=ipKey;}
  if(page==75&&(full||key!=oldPageKey)){lcd.fillRect(0,28,320,25,black);
   if(dual::sourceRouteError()[0])text(8,32,dual::sourceRouteError(),0xffe0);
  }
  oldPageKey=key;oldSelection=selected;
  const bool alarm=alarmui::menu(page);
  unsigned first=alarm?alarmui::first(selected,rowCount):(rowCount>8&&selected>=4?selected-4:0);
  if(!alarm&&rowCount>8&&first+8>rowCount)first=rowCount-8;
  menuRows(s,page,selected,rowCount,first,alarm?alarmui::visible(rowCount):8,lightmenu::step(page,rowCount,alarm),0,full,line,sizeof(line));return;
 }
 if(!full&&(!force||page==28)&&oldPageKey==key&&oldSelection==selected&&page!=40)return;oldPageKey=key;oldSelection=selected;
 lcd.fillRect(0,28,320,212,black);
 static uint32_t menuIpHash=0;const uint32_t ipHash=hash(s.ip);
 if(full||page!=28||menuIpHash!=ipHash){menuHeader(s.ip);menuIpHash=ipHash;}
 if(page==75&&dual::sourceRouteError()[0])text(8,32,dual::sourceRouteError(),0xffe0);
 if(page==4||page==29){const bool favorites=page==29;unsigned total=listTotal;firstRow=total?(favorites?selected:litelist::first(selected,total)):0;unsigned n=firstRow;for(unsigned i=0;i<litelist::rows&&i<total&&n;++i){snprintf(line,sizeof(line),"%u %s",n,config.stationByNum(n));listRow(i,line,n==selected);n=favorites?dual::nextFavorite(n,true):n<total?n+1:0;}if(!total)text(8,56,favorites?"Nincs kedvenc állomás":"A lista üres");return;}
 if(page==32){const unsigned total=dual::stationSearchCount();firstRow=litelist::searchFirst(selected,total);
  for(unsigned row=0;row<litelist::searchRows&&firstRow+row<total;++row){char name[170]={};const unsigned index=firstRow+row;dual::stationSearchResultName(index,name,sizeof(name));snprintf(line,sizeof(line),"%u %s",dual::stationSearchResult(index),name);listRow(row,line,index==selected);}
  if(!total)menufont::text(lcd,8,40,"Nincs találat",cyan,black,304);return;
 }
 if(page==31){snprintf(line,sizeof(line),"Név: %s",dual::stationSearchQuery());menufont::text(lcd,8,33,line,cyan,black,304);
  for(unsigned k=0;k<30;++k){char label[8]={};if(k<26)label[0]=char('A'+k);else snprintf(label,sizeof(label),"%s",k==26?"Szóköz":k==27?"Töröl":k==28?"Keres":"Vissza");
   const int x=10+int(k%6)*50,y=65+int(k/6)*30;const bool active=k==selected;lcd.fillRect(x,y,48,28,active?lime:black);lcd.drawRect(x,y,48,28,dim);menufont::text(lcd,x+2,y+3,label,active?black:cyan,active?lime:black,44,13);
  }
  const unsigned status=dual::stationSearchStatus();const char* message=status==1?"Keresés előtt állítsd le":status==2?"Hibás vagy hiányzó állomáslista":status==3?"Nincs találat":status==4?"Internet-rádió mód szükséges":s.playing?"Keresés előtt állítsd le":"";
  menufont::text(lcd,8,218,message,cyan,black,304,13);return;
 }
 if(page==28){const unsigned count=network.scanCount(),rows=network.status==SOFT_AP?6:5;firstRow=selected>=rows/2?selected-rows/2:0;if(count>rows&&firstRow+rows>count)firstRow=count-rows;const int y=network.status==SOFT_AP?39:46,h=network.status==SOFT_AP?26:28;
  for(unsigned i=0;i<rows&&firstRow+i<count;++i){char name[33];if(network.scanEntry(firstRow+i,name,sizeof(name)))mediumText(8,y+i*h,name,firstRow+i==selected?lime:cyan,304);}if(!count)mediumText(8,60,network.scanStatus()==1?"Hálózatok keresése...":"Nincs találat");mediumText(8,205,"Jelszó megadása",lime);return;}
 if(page==42||page==69){const auto& w=page==42?dual::wifiPasswordUi():dual::ntpServerUi();mediumText(8,33,w.ssid,white,304);snprintf(line,sizeof(line),"%u karakter  %s",unsigned(strlen(w.password)),w.mode==1?"Nagybetű":w.mode==2?"Jelek":"Kisbetű");mediumText(8,52,page==42&&w.error[0]?w.error:line,cyan,page==42?222:304);
  if(page==42){char preview[36];wifipasswordui::preview(preview,sizeof(preview),w.password,w.visible,24);mediumText(8,70,preview,lime,222);button(230,46,82,36,w.visible?"Elrejt":"Mutat",selected==wifipasswordui::visibilityKey);}
  else text(8,70,w.error,lime);
  // UTF-8 "Mégse" needs seven bytes including NUL; Shift needs six.
  const int width=38,height=28;for(unsigned k=0;k<40;++k){char keytext[8]={};if(k<36){keytext[0]=dual::wifiPasswordKeyChar(w.mode,k);}else snprintf(keytext,sizeof(keytext),"%s",k==36?"Shift":k==37?"Tör":k==38?"OK":"Mégse");button(8+(k%8)*width,91+(k/8)*height,width,height,keytext,k==selected||(k==36&&w.mode==1));}return;}
 if(page==41){text(8,60,dual::sourcePendingSd()?"SD-lista előkészítése...":"Internetrádióra váltás...");text(8,90,"Kérlek várj.");text(8,200,dual::sourcePendingSd()?"Vissza: megszakítás":"Vissza: menü");return;}
 if(page==20){text(8,50,"Firmware:");text(8,70,IBT_FW_VERSION);}
 else if(page==18||page==17){text(8,50,"Hálózati cím:");mediumText(8,74,s.ip,lime);text(8,111,s.connected?"Wi-Fi csatlakoztatva":"Nincs Wi-Fi kapcsolat");}
 else if(page==2||page==24||page==25||page==30){snprintf(line,sizeof(line),"Szabad RAM: %lu B",(unsigned long)s.freeHeap);text(8,60,line);text(8,90,s.sd?"SD mód":"Rádió mód");}
 else if(page==22||page==23){text(8,60,userMessage(btaudio::status()));}
 else text(8,60,s.notice[0]?userMessage(s.notice):"Visszalépés a fejlécben");
}
bool bootFrame(bool,bool,bool,unsigned){return false;}void bootRestart(uint8_t,uint8_t,uint8_t){lightboot::session().reopen();}
void btClockTick(){}bool cassetteReady(){return false;}void skinPreview(int){}bool skinPreviewActive(){return false;}bool skinPreviewInput(dual::Action){return false;}
void service(){}void rgbBootTestTick(){}void renderBattery(const dual::Snapshot&){}
void calibrationTarget(int x,int y,unsigned){lcd.fillScreen(black);lcd.drawCircle(x,y,12,cyan);}void calibrationDone(bool){dirty=true;}
void artworkTestFast(bool){}void artworkTestRows(unsigned,unsigned,const uint16_t*){}
bool artworkBegin(int,int){return false;}
void artworkTile(int,int,int,int,int,const uint16_t*){}
void artworkEnd(){}void artworkClear(){}
}
