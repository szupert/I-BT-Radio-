#include "../../core/options.h"
#include "../frontend.h"
#include "../content.h"
#include "../light_ui_policy.h"
#include "../light_player_layout.h"
#include "../lite_list.h"
#include "../bt_speaker_layout.h"
#include "../setup_layout.h"
#include "../wifi_password_layout.h"
#include "../user_message_hu.h"
#include "../../dual/vu_probe.h"
#include "../../dual/online_artwork.h"
#include "../../dual/bt_audio.h"
#include "../../core/config.h"
#include "../../core/network.h"
#include <Adafruit_ST7789.h>
#include <glcdfont.c>
#include <time.h>
#include <atomic>
namespace front {
static SPIClass bus(HSPI);
static Adafruit_ST7789 lcd(&bus,TFT_CS,TFT_DC,TFT_RST);
static constexpr uint16_t black=0,white=0xffff,cyan=0x7fff,lime=0xe7f0,green=0x5fe0,dim=0x2945;
static bool ready=false,dirty=true,deferred=false,artVisible=false;
static unsigned shownPage=~0u,firstRow=0,oldSelection=~0u;
static uint8_t shownSurface=255;
static uint32_t frameAt=0,meterAt=0,oldBanner=0,oldMeta=0,oldStatus=0,oldClock=0;
static uint8_t oldStyle=255,levelsL=255,levelsR=255;
static std::atomic<bool> transportPulsePending{false};
static uint32_t transportPulseUntil=0;
static bool transportPulseShown=false,transportWasPlaying=false;
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
 for(unsigned count=0;s&&*s&&count<64&&x+int(advance)<=end;++count,x+=advance){
  const uint16_t g=glyphCode(dual::nextUtf8(s));const unsigned c=g&255,a=g>>8;
  const auto point=[&](int gx,int gy){const int left=advance==6?gx:gx*3/2,top=gy*3/2;
   lcd.fillRect(x+left,y+top,(advance==6?gx+1:(gx+1)*3/2)-left,(gy+1)*3/2-top,ink);};
  for(int col=0;col<5;++col){const uint8_t bits=pgm_read_byte(font+c*5+col);for(int row=0;row<7;++row)if(bits&(1u<<row))point(col,row+2);}
  if(a==1||a==3){point(3,0);point(2,1);}if(a==2){point(1,0);point(3,0);}if(a==3){point(5,0);point(4,1);}
 }
}
static void header(const char* label,int width=304){lcd.fillRect(0,0,320,28,lime);text(8,3,label,black,2,width);}
// Fixed menu header: both existing touch targets stay unchanged.
static void menuHeader(const char* ip){
 lcd.fillRect(0,0,320,28,lime);
 mediumText(8,6,"Menü",black,48);mediumText(258,6,"Vissza",black,60);
 const char* address=ip&&*ip?ip:"Nincs IP-cím";
 const unsigned length=std::min(size_t(15),strlen(address));
 mediumText(154-int(length)*9/2,6,address,black,180);
}
static void button(int x,int y,int w,int h,const char* label,bool active=false){unsigned n=0;const char* p=label;while(p&&*p&&n<64){dual::nextUtf8(p);++n;}lcd.drawRect(x,y,w,h,dim);
 if((shownPage!=0&&shownPage!=39)||shownSurface==1){const unsigned advance=n*9<=unsigned(w-4)?9:6;mediumText(x+std::max(2,(w-int(n)*int(advance))/2),y+(h-14)/2,label,active?green:cyan,w-4,advance);}
 else text(x+std::max(2,(w-int(n)*6)/2),y+(h-11)/2,label,active?green:cyan,1,w-4);}
static void transportButton(int x,int y,int w,int h,bool playing,bool pulse){
 const uint16_t bg=pulse?0xfd20:playing?0x0300:0x0010,ink=pulse?black:white;
 lcd.fillRect(x+1,y+1,w-2,h-2,bg);lcd.drawRect(x,y,w,h,pulse?white:playing?green:cyan);
 const int cy=y+h/2;
 if(playing){lcd.fillRect(x+7,cy-6,3,12,ink);lcd.fillRect(x+12,cy-6,3,12,ink);}
 else for(int col=0;col<6;++col)lcd.fillRect(x+8+col,cy-6+col,1,12-2*col,ink);
 text(x+20,y+(h-11)/2,playing?"Szünet":"Indít",ink,1,w-22);
}
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
 header("I-BT-Radio");text(16,100,"Indítás...",lime,2);text(16,132,"Magyar kiadás");ready=true;return true;
}
bool begin(){return earlyBoot();}dual::Size nativeSize(){return {320,240};}
void invalidate(){dirty=true;}bool renderPending(){return deferred||transportPulseShown||transportPulsePending.load(std::memory_order_relaxed);}
bool menuViewport(unsigned page,unsigned& first){first=firstRow;return !dirty&&!deferred&&shownPage==page;}
unsigned stationListFirst(){return firstRow;}
void setRotation(uint8_t r){lcd.setRotation((r+1)&3);dirty=true;}
static void digit(int x,int y,unsigned d,int w,int h){
 static const uint8_t segments[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
 if(d>9)return;const uint8_t m=segments[d];const int t=3,half=h/2;
 if(m&1)lcd.fillRect(x+t,y,w-2*t,t,lime);
 if(m&2)lcd.fillRect(x+w-t,y+t,t,half-t,lime);
 if(m&4)lcd.fillRect(x+w-t,y+half,t,half-t,lime);
 if(m&8)lcd.fillRect(x+t,y+h-t,w-2*t,t,lime);
 if(m&16)lcd.fillRect(x,y+half,t,half-t,lime);
 if(m&32)lcd.fillRect(x,y+t,t,half-t,lime);
 if(m&64)lcd.fillRect(x+t,y+half-t/2,w-2*t,t,lime);
}
static bool coverMode=false;
struct OverlayLine{int16_t y=0;uint8_t count=0,scale=1;uint16_t ink=0;};
static OverlayLine overlays[3];
// 50 artist glyphs and two25-glyph title rows; no bitmap/framebuffer.
static uint16_t overlayGlyphs[100];
static unsigned glyphOffset(unsigned slot){return slot?50+(slot-1)*25:0;}
static bool overlayInk(unsigned slot,int x,int y){
 const auto& r=overlays[slot];if(!r.count||x<8||y<r.y)return false;
 const int cell=6*r.scale,n=(x-8)/cell,dy=(y-r.y)/r.scale;
 if(n>=r.count||dy>=10)return false;
 const int dx=((x-8)%cell)/r.scale;const uint16_t g=overlayGlyphs[glyphOffset(slot)+n];
 const unsigned accent=g>>8;
 if(dy<2)return ((accent==1||accent==3)&&((dy==0&&dx==3)||(dy==1&&dx==2)))||
    (accent==2&&dy==0&&(dx==1||dx==3))||(accent==3&&((dy==0&&dx==5)||(dy==1&&dx==4)));
 return dx<5&&(pgm_read_byte(font+(g&255)*5+dx)&(1u<<(dy-2)));
}
// Composite strokes and a one-pixel contrast outline into the existing scaled
// row. Every other pixel retains its original cover color.
static void coverRow(int y,uint16_t* pixels){
 for(unsigned slot=0;slot<3;++slot){
  const auto& r=overlays[slot];if(!r.count||y<r.y-1||y>=r.y+10*r.scale+1)continue;
  const int first=std::max(lightplayer::coverX,7),end=std::min(lightplayer::coverX+lightplayer::coverW,9+int(r.count)*6*r.scale);
  for(int x=first;x<end;++x){
   if(overlayInk(slot,x,y)){pixels[x-lightplayer::coverX]=r.ink;continue;}
   bool outline=false;
   for(int dy=-1;dy<=1&&!outline;++dy)for(int dx=-1;dx<=1;++dx)if(overlayInk(slot,x+dx,y+dy)){outline=true;break;}
   if(outline)pixels[x-lightplayer::coverX]=black;
  }
 }
 lcd.drawRGBBitmap(lightplayer::coverX,y,pixels,lightplayer::coverW,1);
}
static void metadataLine(const char* line,int y,unsigned scale,unsigned slot){
 auto& r=overlays[slot];r={int16_t(y),0,uint8_t(scale),slot?white:cyan};
 const unsigned limit=slot?25:50;const char* at=line;
 while(*at&&r.count<limit)overlayGlyphs[glyphOffset(slot)+r.count++]=glyphCode(dual::nextUtf8(at));
 text(8,y,line,r.ink,scale,304);
}
static void metadata(const dual::Snapshot& s,bool speaker){
 for(auto& r:overlays)r={};metadataLine(s.artist,34,1,0);
 const char* at=s.title;const unsigned rows=coverMode&&!speaker?2:1;
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
 if(speaker){
  const int y=101,h=35;lcd.fillRect(164,99,148,42,black);
  if(t<1600000000){text(171,y+8,"--:--",lime,3);return;}
  tm local{};localtime_r(&t,&local);unsigned values[]={unsigned(local.tm_hour/10),unsigned(local.tm_hour%10),unsigned(local.tm_min/10),unsigned(local.tm_min%10)};
  int x=168;for(unsigned i=0;i<4;++i){if(i==2){lcd.fillRect(x,y+h/3,3,3,lime);lcd.fillRect(x,y+2*h/3,3,3,lime);x+=9;}digit(x,y,values[i],26,h);x+=30;}return;
 }
 const int x=coverMode?32:lightplayer::clockX,y=coverMode?120:103,scale=coverMode?3:7;
 lcd.fillRect(coverMode?8:lightplayer::clockX,coverMode?110:lightplayer::clockY,coverMode?124:lightplayer::clockW,coverMode?54:lightplayer::clockH,black);
 char value[6]="--:--";tm local{};if(t>=1600000000&&localtime_r(&t,&local))snprintf(value,sizeof(value),"%02u:%02u",unsigned(local.tm_hour),unsigned(local.tm_min));
 // Existing ROM5x7 bitmap digits: large, crisp, and right-aligned; no font allocation.
 lcd.setTextColor(lime);lcd.setTextSize(scale);lcd.setCursor(x,y);for(const char* c=value;*c;++c)lcd.write(*c);
}
static void speakerMeters(bool speaker,bool force){
 const unsigned style=lightui::style.load();const uint8_t l=vuprobe::dacLeft.load(),r=vuprobe::dacRight.load();
 if(!force&&oldStyle==style&&l==levelsL&&r==levelsR)return;
 const bool layout=force||oldStyle!=style;if(layout)lcd.fillRect(8,99,144,speaker?42:69,black);
 oldStyle=style;levelsL=l;levelsR=r;if(!style)return;
 const uint8_t value[]={l,r};const int height=speaker?32:52;
 if(style==1){
  for(unsigned ch=0;ch<2;++ch){text(16+ch*34,101,ch?"J":"B");const unsigned n=lightui::levels(value[ch],12);
   for(unsigned j=0;j<12;++j){const int h=std::max(1,(height-12)/12);lcd.fillRect(16+ch*34,114+(11-j)*(h+1),20,h,j<n?(j>9?green:cyan):dim);}}
 }else{
  for(unsigned ch=0;ch<2;++ch){text(9,106+ch*21,ch?"J":"B");const unsigned n=lightui::levels(value[ch],16);
   for(unsigned j=0;j<16;++j)lcd.fillRect(24+j*8,109+ch*21,style==3?6:7,8,j<n?(j>=14?lime:j>=10?green:cyan):dim);}
 }
}
struct PeakHold{uint8_t level=0;uint32_t at=0;};static PeakHold peaks[2];
static void meters(bool speaker,bool force){
 if(speaker){speakerMeters(true,force);return;}
 const unsigned style=lightui::style.load();const uint8_t values[]={vuprobe::dacLeft.load(),vuprobe::dacRight.load()};
 const uint32_t now=millis();bool decay=false;for(unsigned ch=0;ch<2;++ch)decay|=peaks[ch].level>lightui::levels(values[ch],16)&&uint32_t(now-peaks[ch].at)>=700;
 if(!force&&oldStyle==style&&values[0]==levelsL&&values[1]==levelsR&&!decay)return;
 const bool layout=force||oldStyle!=style;if(layout){lcd.fillRect(lightplayer::vuX,lightplayer::vuY,lightplayer::vuW,lightplayer::vuH,black);for(auto& p:peaks)p={};}
 oldStyle=style;levelsL=values[0];levelsR=values[1];if(!style)return;
 constexpr uint16_t yellow=0xffe0,red=0xf800,well=0x1082;
 for(unsigned ch=0;ch<2;++ch){const unsigned n=lightui::levels(values[ch],16);auto& peak=peaks[ch];
  if(n>=peak.level){peak.level=n;peak.at=now;}else if(uint32_t(now-peak.at)>=700){--peak.level;peak.at=now-600;}
  if(style==1){const int x=10+int(ch)*42;if(layout){text(x+6,81,ch?"J":"B");lcd.drawRect(x,97,26,70,dim);}
   // Redraw only the well; its border and channel labels remain unchanged.
   lcd.fillRect(x+1,98,24,68,black);
   for(unsigned j=0;j<16;++j){uint16_t ink=j>=14?red:j>=12?yellow:j>=8?green:cyan;lcd.fillRect(x+3,160-int(j)*4,20,3,j<n?ink:well);}
   if(peak.level)lcd.fillRect(x+1,160-int(peak.level-1)*4+1,24,1,white);
  }else{const int y=107+int(ch)*27;if(layout){text(8,y-13,ch?"J":"B");if(style==3)lcd.drawRect(20,y-2,68,13,dim);}
   lcd.fillRect(22,y,64,9,black);
   for(unsigned j=0;j<16;++j){uint16_t ink=j>=14?red:j>=12?yellow:j>=8?green:cyan;lcd.fillRect(22+int(j)*4,y,style==3?2:3,8,j<n?ink:well);}
   if(peak.level)lcd.fillRect(22+int(peak.level-1)*4,y,1,8,white);
  }
 }
}
// Auxiliary painters may run between a network transition and its next frame.
// Only the currently rendered player surface owns those LCD regions.
static bool playerPaintOwned(){
 return ready&&!dirty&&!deferred&&shownSurface==0&&
   (shownPage==0||shownPage==39)&&!(shownPage==0&&network.status==SOFT_AP);
}
void liteMeterTick(bool allowed){
 if(!allowed||!playerPaintOwned()||(shownPage==0&&coverMode))return;
 const uint32_t now=millis();const unsigned ms=vuprobe::refreshMs.load();if(!lightui::due(now,meterAt,ms))return;meterAt=now;
 meters(shownPage==39,false);
}
// Small independently updated header widgets: no extra ADC task/pin or buffer.
static void playerHeaderStatus(const dual::Snapshot& s,bool force){
 const unsigned bars=lightplayer::signalBars(s.connected,s.rssi);
 static unsigned oldBars=255;if(!force&&oldBars==bars)return;oldBars=bars;
 lcd.fillRect(240,0,80,28,lime);
 for(unsigned i=0;i<4;++i){const int h=4+i*3;lcd.fillRect(244+i*5,20-h,3,h,i<bars?black:dim);}
 // Battery hardware is not fitted: an outlined cell plus dash means unknown.
 lcd.drawRect(282,6,26,15,black);lcd.fillRect(308,10,3,7,black);lcd.fillRect(291,12,8,2,black);
}
static void player(const dual::Snapshot&s,bool speaker,bool full,bool force){
 const bool nextCover=!lightui::meterMode(s.settings.artworkEnabled,speaker);
 const bool coverChanged=nextCover!=coverMode;
 if(coverChanged){coverMode=nextCover;lcd.fillRect(8,32,304,136,black);oldClock=0;oldStyle=255;}
 const unsigned volume=std::min(100u,(unsigned(s.volume)*100u+127u)/254u);
 const bool playing=speaker?btaudio::inputPlaying():s.playing;
 const bool previousPulse=transportPulseShown;
 if(transportPulsePending.exchange(false,std::memory_order_relaxed))transportPulseUntil=millis()+600;
 const bool pulse=transportPulseUntil&&int32_t(transportPulseUntil-millis())>0;
 char line[160];const char* banner=speaker||btaudio::inputSelected()?"Bluetooth-hangszóró":btaudio::outputSelected()?"SD > Bluetooth":s.sd?"SD-kártya":s.station[0]?s.station:"I-BT-Radio";
 uint32_t key=hash(banner);const bool headerChanged=full||force||key!=oldBanner;if(headerChanged){oldBanner=key;header(banner,224);}playerHeaderStatus(s,headerChanged);
 key=hash(s.title,hash(s.artist));if(full||force||coverChanged||key!=oldMeta){oldMeta=key;lcd.fillRect(8,32,304,65,black);artVisible=false;
  if(coverMode){lcd.fillRect(lightplayer::coverX,lightplayer::coverY,lightplayer::coverW,lightplayer::coverH,black);lcd.drawRect(lightplayer::coverX,lightplayer::coverY,lightplayer::coverW,lightplayer::coverH,dim);text(190,115,"Borító",dim,1,80);}
  metadata(s,speaker);if(coverMode&&!s.sd)onlineart::redraw(s.generation);
 }
 char date[8]="--.--.";time_t now=time(nullptr);tm local{};if(now>=1600000000&&localtime_r(&now,&local))snprintf(date,sizeof(date),"%02u.%02u.",unsigned(local.tm_mon+1),unsigned(local.tm_mday));
 key=unsigned(s.volume)+unsigned(playing)*256u+unsigned(s.sd)*512u+unsigned(btaudio::connected())*1024u+hash(s.notice)+hash(date);
 const bool statusChanged=full||force||key!=oldStatus;
 if(statusChanged){oldStatus=key;const int y=speaker?188:169;lcd.fillRect(8,y,304,speaker?50:26,black);
  if(speaker){snprintf(line,sizeof(line),"Hangerő %u%%",volume);text(60,188,line);lcd.drawRect(60,210,244,10,dim);lcd.fillRect(61,211,volume*242u/100u,8,cyan);}
  else{const char* source=btaudio::inputSelected()?"Telefon > DAC":btaudio::outputSelected()?"SD > BT":s.sd?"SD > DAC":"Wi-Fi > DAC";
   snprintf(line,sizeof(line),"%s  Hangerő %u%%  %s",source,volume,date);text(8,169,line,cyan,1,304);}
 }
 if(!speaker){static unsigned previousBuffer=~0u,previousVolume=~0u;const unsigned fill=s.settings.bufferIndicator&&s.bufferCapacity?unsigned(std::min<uint64_t>(108,uint64_t(s.buffer)*108u/s.bufferCapacity)):0;
  if(statusChanged){text(8,182,"Puffer",cyan,1,36);lcd.drawRect(46,184,110,9,dim);text(164,182,"Hangerő",cyan,1,48);lcd.drawRect(212,184,100,9,dim);}
  if(statusChanged||fill!=previousBuffer){previousBuffer=fill;lcd.fillRect(47,185,108,7,black);if(fill)lcd.fillRect(47,185,fill,7,cyan);}
  if(statusChanged||volume!=previousVolume){previousVolume=volume;lcd.fillRect(213,185,98,7,black);lcd.fillRect(213,185,volume*98u/100u,7,cyan);}
 }
 clock(speaker,full||coverChanged);if((full||coverChanged)&&!coverMode)meters(speaker,true);
 if(full){
  if(speaker){const btspeakerui::Rect rects[]={btspeakerui::previous,btspeakerui::toggle,btspeakerui::next,btspeakerui::menu};const char* names[]={"Előző",playing?"Szünet":"Indít","Követk.","Csatl."};for(unsigned i=0;i<4;++i){auto r=rects[i];button(r.x,r.y,r.w,r.h,names[i]);}}
  else{const char* names[]={"Előző",playing?"Szünet":"Indít","Követk.","Forrás",btaudio::outputSelected()?"Scan":"Menü"};for(unsigned i=0;i<5;++i)button(i*64,196,64,44,names[i]);}
 }
 if(full||force||statusChanged||playing!=transportWasPlaying||pulse!=previousPulse){
  if(speaker){auto r=btspeakerui::toggle;transportButton(r.x,r.y,r.w,r.h,playing,pulse);}
  else transportButton(64,196,64,44,playing,pulse);
 }
 transportPulseShown=pulse;transportWasPlaying=playing;
 if(speaker){const char* state=btaudio::connected()?"Telefon csatlakoztatva":"Várakozás a telefonra";const uint32_t stateKey=hash(state);static uint32_t oldState=0;if(full||oldState!=stateKey){oldState=stateKey;lcd.fillRect(8,80,304,14,black);text(8,81,state,btaudio::connected()?green:cyan);}}
}
void render(const dual::Snapshot&s,unsigned page,unsigned selected,bool force){
 const uint32_t now=millis();if(!lightui::due(now,frameAt,lightui::frameMs)){deferred=true;return;}
 // Hotspot and player share page 0, but own different complete screens.
 // Schedule one full repaint on either transition; keep normal frame/audio pacing.
 const uint8_t surface=network.status==SOFT_AP&&page==0?1:0;
 if((page!=0&&page!=39)||surface){transportPulsePending.store(false,std::memory_order_relaxed);transportPulseShown=false;transportPulseUntil=0;}
 deferred=false;frameAt=now;const bool full=dirty||shownPage!=page||shownSurface!=surface;dirty=false;shownSurface=surface;
 if(full){lcd.fillScreen(black);oldBanner=oldMeta=oldStatus=oldClock=0;oldStyle=255;artVisible=false;}shownPage=page;
 if(network.status==SOFT_AP&&page==0){if(full||force){
  lcd.fillRect(0,0,320,48,lime);mediumText(8,15,"Wi-Fi beállítás",black,204);
  setupButton(setupui::scan(320),"Keresés",nullptr,true);
  mediumText(8,58,"Csatlakozz a beállító hálózathoz");mediumText(8,80,"Jelszó megadása a böngészőben");
  mediumText(8,108,s.ip,lime,304);mediumText(8,150,"Vagy válassz hálózat nélküli módot");
  setupButton(setupui::reboot(240),"Újra-","indítás");
  setupButton(setupui::player(320,240),"Lejátszó");setupButton(setupui::qr(320,240),"IP-cím");
 }return;}
 if(page==0||page==39){player(s,page==39,full,force);return;}
 char line[180];uint32_t key=hash(s.notice,selected*65537u+unsigned(s.volume)+unsigned(s.playing)*256u);static uint32_t oldPageKey=0;
 // Scan completion has no UI event: poll its visible data, not repaint the LCD.
 if(page==28){
  const unsigned count=network.scanCount(),rows=network.status==SOFT_AP?6:5;
  unsigned first=selected>=rows/2?selected-rows/2:0;if(count>rows&&first+rows>count)first=count-rows;
  key=selected*65537u+count*17u+network.scanStatus()+rows*257u;
  for(unsigned i=0;i<rows&&first+i<count;++i){char name[33];if(network.scanEntry(first+i,name,sizeof(name)))key=hash(name,key);}
 }
 key=hash(s.ip,key);
 if(!full&&(!force||page==28)&&oldPageKey==key&&oldSelection==selected&&page!=40)return;oldPageKey=key;oldSelection=selected;
 lcd.fillRect(0,28,320,212,black);
 static uint32_t menuIpHash=0;const uint32_t ipHash=hash(s.ip);
 if(full||page!=28||menuIpHash!=ipHash){menuHeader(s.ip);menuIpHash=ipHash;}
 if(page!=28&&page!=40&&page!=44&&page!=42&&page!=69)mediumText(8,30,content::pageTitle(dual::Language::HU,page),cyan,304);
 if(page==4||page==29){const bool favorites=page==29;unsigned total=favorites?dual::favoritesCount():config.playlistLength();firstRow=total?(favorites?selected:litelist::first(selected,total)):0;unsigned n=firstRow;for(unsigned i=0;i<litelist::rows&&i<total&&n;++i){snprintf(line,sizeof(line),"%u %s",n,config.stationByNum(n));mediumText(8,litelist::top+i*litelist::step,line,n==selected?lime:cyan,304);n=favorites?dual::nextFavorite(n,true):n<total?n+1:0;}if(!total)text(8,56,favorites?"Nincs kedvenc állomás":"A lista üres");return;}
 if(page==40||page==44){unsigned count=page==40?btaudio::deviceCount():btaudio::savedDeviceCount();unsigned rows=page==40?7:4;firstRow=page==44?0:selected>=rows/2?selected-rows/2:0;if(count>rows&&firstRow+rows>count)firstRow=count-rows;
  for(unsigned i=0;i<rows&&firstRow+i<count;++i){btaudio::Device d{};bool ok=page==40?btaudio::device(firstRow+i,d):btaudio::savedDevice(firstRow+i,d);if(ok)mediumText(8,36+i*(page==40?26:30),d.name,firstRow+i==selected?lime:cyan,304);}
  if(!count)text(8,55,page==40?"Eszközök keresése...":"Nincs mentett eszköz");if(page==44)text(8,156,"Új keresés",selected==4?lime:cyan);return;}
 if(page==28){const unsigned count=network.scanCount(),rows=network.status==SOFT_AP?6:5;firstRow=selected>=rows/2?selected-rows/2:0;if(count>rows&&firstRow+rows>count)firstRow=count-rows;const int y=network.status==SOFT_AP?39:46,h=network.status==SOFT_AP?26:28;
  for(unsigned i=0;i<rows&&firstRow+i<count;++i){char name[33];if(network.scanEntry(firstRow+i,name,sizeof(name)))mediumText(8,y+i*h,name,firstRow+i==selected?lime:cyan,304);}if(!count)mediumText(8,60,network.scanStatus()==1?"Hálózatok keresése...":"Nincs találat");mediumText(8,205,"Jelszó megadása",lime);return;}
 if(page==42||page==69){const auto& w=page==42?dual::wifiPasswordUi():dual::ntpServerUi();mediumText(8,33,w.ssid,white,304);snprintf(line,sizeof(line),"%u karakter  %s",unsigned(strlen(w.password)),w.mode==1?"Nagybetű":w.mode==2?"Jelek":"Kisbetű");mediumText(8,52,page==42&&w.error[0]?w.error:line,cyan,page==42?222:304);
  if(page==42){char preview[36];wifipasswordui::preview(preview,sizeof(preview),w.password,w.visible,24);mediumText(8,70,preview,lime,222);button(230,46,82,36,w.visible?"Elrejt":"Mutat",selected==wifipasswordui::visibilityKey);}
  else text(8,70,w.error,lime);
  // UTF-8 "Mégse" needs seven bytes including NUL; Shift needs six.
  const int width=38,height=28;for(unsigned k=0;k<40;++k){char keytext[8]={};if(k<36){keytext[0]=dual::wifiPasswordKeyChar(w.mode,k);}else snprintf(keytext,sizeof(keytext),"%s",k==36?"Shift":k==37?"Tör":k==38?"OK":"Mégse");button(8+(k%8)*width,91+(k/8)*height,width,height,keytext,k==selected||(k==36&&w.mode==1));}return;}
 if(page==41){text(8,60,dual::sourcePendingSd()?"SD-lista előkészítése...":"Internetrádióra váltás...");text(8,90,"Kérlek várj.");text(8,200,dual::sourcePendingSd()?"Vissza: megszakítás":"Vissza: menü");return;}
 const unsigned count=content::itemCount(page);if(count){firstRow=count>8&&selected>=4?selected-4:0;if(count>8&&firstRow+8>count)firstRow=count-8;const int step=count>8?22:180/int(count);
  for(unsigned i=0;i<8&&firstRow+i<count;++i){const unsigned n=firstRow+i;const bool enabled=content::menuItem(line,sizeof(line),s,page,n);if(n==selected)lcd.fillRect(4,49+i*step,312,step,lime);mediumText(8,53+i*step,line,n==selected?black:enabled?cyan:dim,304);}return;}
 if(page==20){text(8,50,"Firmware:");text(8,70,IBT_FW_VERSION);}
 else if(page==18||page==17){text(8,50,"Hálózati cím:");mediumText(8,74,s.ip,lime);text(8,111,s.connected?"Wi-Fi csatlakoztatva":"Nincs Wi-Fi kapcsolat");}
 else if(page==2||page==24||page==25||page==30){snprintf(line,sizeof(line),"Szabad RAM: %lu B",(unsigned long)s.freeHeap);text(8,60,line);text(8,90,s.sd?"SD mód":"Rádió mód");}
 else if(page==22||page==23){text(8,60,userMessage(btaudio::status()));}
 else text(8,60,s.notice[0]?userMessage(s.notice):"Visszalépés a fejlécben");
}
bool bootFrame(bool,bool,bool,unsigned){return false;}void bootRestart(uint8_t,uint8_t,uint8_t){}
void btClockTick(){}bool cassetteReady(){return false;}void skinPreview(int){}bool skinPreviewActive(){return false;}bool skinPreviewInput(dual::Action){return false;}
void service(){}void rgbBootTestTick(){}void renderBattery(const dual::Snapshot&){}
void calibrationTarget(int x,int y,unsigned){lcd.fillScreen(black);lcd.drawCircle(x,y,12,cyan);}void calibrationDone(bool){dirty=true;}
void artworkTestFast(bool){}void artworkTestRows(unsigned,unsigned,const uint16_t*){}
static int coverSourceW=0,coverSourceH=0;
static lightplayer::Size coverSize{0,0};
bool artworkBegin(int w,int h){
 if(!playerPaintOwned()||shownPage!=0||!coverMode||w<1||h<1||w>64||h>64)return false;
 coverSourceW=w;coverSourceH=h;coverSize=lightplayer::crop(w,h);artVisible=true;return true;
}
void artworkTile(int x,int y,int w,int h,int stride,const uint16_t* pixels){
 if(!playerPaintOwned()||shownPage!=0||!coverMode||!artVisible||!pixels||x!=0||w<coverSourceW||stride<coverSourceW||y<0||h<1)return;
 const int left=lightplayer::coverX+(lightplayer::coverW-coverSize.w)/2,top=lightplayer::coverY+(lightplayer::coverH-coverSize.h)/2;
 for(int sourceRow=0;sourceRow<h&&y+sourceRow<coverSourceH;++sourceRow){
  uint16_t line[lightplayer::coverW];
  const int first=std::max(lightplayer::coverY,top+(y+sourceRow)*coverSize.h/coverSourceH),end=std::min(lightplayer::coverY+lightplayer::coverH,top+(y+sourceRow+1)*coverSize.h/coverSourceH);
  for(int outRow=first;outRow<end;++outRow){
   // Refresh the base row before compositing; scaled repeats must not inherit
   // text pixels from the preceding output row.
   for(int col=0;col<lightplayer::coverW;++col){const int sx=(lightplayer::coverX+col-left)*coverSourceW/coverSize.w;line[col]=pixels[sourceRow*stride+std::min(coverSourceW-1,std::max(0,sx))];}
   coverRow(outRow,line);
  }
 }
}
void artworkEnd(){}void artworkClear(){
 if(playerPaintOwned()&&shownPage==0&&coverMode&&artVisible){uint16_t line[lightplayer::coverW];
  for(int y=lightplayer::coverY;y<lightplayer::coverY+lightplayer::coverH;++y){std::fill(line,line+lightplayer::coverW,black);coverRow(y,line);}}
 artVisible=false;
}
}
