#if IBT_COMPACT_UI
#include "frontend_compact.inc"
#elif IBT_NO_PSRAM
#include "frontend_light.inc"
#else
#include "../../core/options.h"
#include "../../dual/lite_memory.h"
#include "../bt_speaker_layout.h"
#include "../../dual/bt_audio.h"
#include "../frontend.h"
#include "../content.h"
#include "../user_message_hu.h"
#include "../../core/config.h"
#include "../../core/network.h"
#include <Adafruit_ST7789.h>
#include <Arduino.h>
namespace front {
static SPIClass bus(HSPI);static Adafruit_ST7789 lcd(&bus,TFT_CS,TFT_DC,TFT_RST);
static bool ready=false,dirty=true;static uint32_t at=0;static unsigned firstRow=0,lastPage=~0u;static uint32_t oldKey=0;
static void text(int x,int y,const char* s,uint16_t color=0xffff,unsigned size=1){lcd.setTextColor(color,0x0841);lcd.setTextSize(size);lcd.setCursor(x,y);for(unsigned i=0;s&&*s&&i<64;i++){uint32_t c=dual::nextUtf8(s);switch(c){case 225:case 193:c='a';break;case 233:case 201:c='e';break;case 237:case 205:c='i';break;case 243:case 246:case 337:case 211:case 214:case 336:c='o';break;case 250:case 252:case 369:case 218:case 220:case 368:c='u';break;default:break;}lcd.write(c<128?c:'?');}}
bool earlyBoot(){if(ready)return true;bus.begin(DUAL_TFT_SCK,DUAL_TFT_MISO,DUAL_TFT_MOSI,TFT_CS);lcd.init(240,320);lcd.setRotation(1);lcd.invertDisplay(false);lcd.setTextWrap(false);pinMode(BRIGHTNESS_PIN,OUTPUT);digitalWrite(BRIGHTNESS_PIN,HIGH);lcd.fillScreen(0x0841);text(16,96,"I-BT-Radio Lite",0x07ff,2);text(16,128,IBT_NO_PSRAM?"PSRAM nelkul / 5 kep/mp":"PSRAM / 20 kep/mp");ready=true;return true;}
bool begin(){return earlyBoot();}dual::Size nativeSize(){return {320,240};}
void invalidate(){dirty=true;}bool renderPending(){return false;}bool menuViewport(unsigned,unsigned& first){first=firstRow;return true;}
unsigned stationListFirst(){return firstRow;}void setRotation(uint8_t r){lcd.setRotation((r+1)&3);dirty=true;}
void render(const dual::Snapshot&s,unsigned page,unsigned selected,bool force){
 uint32_t now=millis();if(uint32_t(now-at)<litememory::frameInterval)return;
 uint32_t key=2166136261u;auto hash=[&](const char*p){while(*p){key^=(uint8_t)*p++;key*=16777619u;}};hash(s.station);hash(s.artist);hash(s.title);key^=selected*16777619u+s.volume*65537u+s.playing*13u+s.sd*23u+(s.freeHeap/4096)*31u;
 if(!dirty&&!force&&lastPage==page&&key==oldKey)return;at=now;oldKey=key;lastPage=page;dirty=false;
 lcd.fillScreen(0x0841);char line[192];
 if(network.status==SOFT_AP){text(8,8,"Wi-Fi beallitas",0x07ff,2);text(8,44,s.ip);text(8,65,"Csatlakozz a beallito halozathoz");text(8,82,"Nyisd meg az IP-cimet bongeszoben");text(8,114,"Halozat es jelszo: webes felulet");return;}
 if(page==39){text(8,7,"BT bemenet -> DAC",0x07ff,2);text(8,38,btaudio::status());text(8,68,s.artist);text(8,92,s.title);const btspeakerui::Rect rects[]={btspeakerui::previous,btspeakerui::toggle,btspeakerui::next,btspeakerui::menu};const char* labels[]={"ELOZO","INDIT","KOVETK.","MENU"};for(unsigned i=0;i<4;i++){auto r=rects[i];lcd.drawRect(r.x,r.y,r.w,r.h,0x07ff);text(r.x+6,r.y+12,labels[i]);}lcd.drawRect(60,198,244,22,0x07ff);snprintf(line,sizeof(line),"Hangero %u",s.volume);text(70,204,line);return;}
 if(page==40||page==44){text(8,8,page==40?"BT kereses / VISSZA":"Mentett BT / VISSZA",0x07ff,2);unsigned count=page==40?btaudio::deviceCount():btaudio::savedDeviceCount();unsigned visible=page==40?7:4;firstRow=selected>=visible/2?selected-visible/2:0;if(count>visible&&firstRow+visible>count)firstRow=count-visible;if(!count)text(8,50,btaudio::status());for(unsigned i=0;i<visible&&firstRow+i<count;i++){btaudio::Device d{};bool ok=page==40?btaudio::device(firstRow+i,d):btaudio::savedDevice(firstRow+i,d);if(ok){snprintf(line,sizeof(line),"%c %s",firstRow+i==selected?'>':' ',d.name);text(8,36+i*(page==40?26:30),line,firstRow+i==selected?0xffe0:0xffff);}}if(page==44)text(8,156,"Uj kereses",0x07ff);return;}
 if(page==0){text(8,5,btaudio::inputSelected()?"BT be -> DAC":btaudio::outputSelected()?"SD -> BT ki":s.sd?"SD -> DAC":"Wi-Fi -> DAC",0x07ff,2);text(8,35,s.station);text(8,62,s.artist);text(8,82,s.title);snprintf(line,sizeof(line),"HANG %u  %s",s.volume,s.playing?"LEJATSZAS":"ALL");text(8,110,line,0xffe0,2);snprintf(line,sizeof(line),"RAM %lu  Puffer %lu",(unsigned long)s.freeHeap,(unsigned long)s.buffer);text(8,144,line);text(8,170,s.notice[0]?userMessage(s.notice):"MP3 / WAV   Bluetooth: menu");const char* labels[]={"ELOZO","INDIT","KOVETK.","FORRAS","MENU"};for(int i=0;i<5;i++){lcd.drawRect(i*64,196,63,43,0x07ff);text(i*64+7,212,labels[i]);}return;}
 if(page==4){text(8,7,"VISSZA / Lejatszasi lista",0x07ff,2);unsigned total=config.playlistLength();firstRow=selected>3?selected-3:1;for(unsigned i=0;i<7;i++){unsigned n=firstRow+i;if(!total)break;while(n>total)n-=total;snprintf(line,sizeof(line),"%c %u %s",n==selected?'>':' ',n,config.stationByNum(n));text(8,56+i*23,line,n==selected?0xffe0:0xffff);}return;}
 text(8,8,content::pageTitle(s.settings.language,page),0x07ff,2);text(250,8,"VISSZA");unsigned count=content::itemCount(page);firstRow=count>8&&selected>=4?selected-4:0;if(count>8&&firstRow+8>count)firstRow=count-8;int step=count>8?22:count?(240-60)/count:22;
 for(unsigned i=0;i<8&&firstRow+i<count;i++){unsigned n=firstRow+i;content::menuItem(line,sizeof(line),s,page,n);text(8,53+i*step,line,n==selected?0xffe0:0xffff);if(n==selected)lcd.drawRect(2,49+i*step,316,step,0xffe0);}
}
bool bootFrame(bool,bool,bool,unsigned){return false;}void bootRestart(uint8_t,uint8_t,uint8_t){}void btClockTick(){}bool cassetteReady(){return false;}void skinPreview(int){}bool skinPreviewActive(){return false;}bool skinPreviewInput(dual::Action){return false;}void service(){}void rgbBootTestTick(){}
void calibrationTarget(int x,int y,unsigned){lcd.fillScreen(0x0841);lcd.drawCircle(x,y,12,0x07ff);}void calibrationDone(bool){dirty=true;}
void renderBattery(const dual::Snapshot&){}void artworkTestFast(bool){}void artworkTestRows(unsigned,unsigned,const uint16_t*){}
bool artworkBegin(int,int){return false;}void artworkTile(int,int,int,int,int,const uint16_t*){}void artworkEnd(){}void artworkClear(){}
}

#endif
