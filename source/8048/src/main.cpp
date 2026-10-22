#include <Arduino.h>
#include <Wire.h>
#include "display_config.h"
#include "fonts.h"
#include "menu_data.h"
#include "demo_skin_assets.h"
static constexpr char version[]="MENU8048C-R4";
MenuDisplay screen;
lgfx::LGFX_Sprite gfx(&screen);
constexpr uint16_t BG=0x0862, CARD=0x18E5, TEXT=0xF7BE, MUTED=0xAD75, ACCENT=0x4D7B;
int current=-1,top=0; uint8_t choices[pageCount][16]={}; bool ready=false,dirty=true;
uint8_t touchAddress=0; uint32_t nextPoll=0; bool pressed=false; int sx=0,sy=0,lastX=0,lastY=0;
int navHistory[32]={},navDepth=0;
void backPage();
void openPage(int page);
int demoSkin=2;bool demoClock=false;int demoWave=0,demoFreq=2;bool demoBt=false;
bool demoPlaying=false;int demoVolume=50;int brightness=96;
String command; int testPage=-1,testRow=0,testRendered=0;uint32_t nextTest=0;
uint32_t utf8(const char*& p){uint8_t a=*p++;if(a<128)return a;int n=(a&0xE0)==0xC0?1:(a&0xF0)==0xE0?2:3;uint32_t c=a&((1<<(6-n))-1);while(n--&&*p)c=(c<<6)|(*p++&63);return c;}
void text(int x,int y,int limit,const char* s,uint16_t color=TEXT){int start=x;auto& f=ibtfonts::font24;while(*s){auto& g=ibtfonts::find(f,utf8(s));if(x-start+g.w>limit)break;for(int yy=0;yy<g.h;++yy)for(int xx=0;xx<g.w;++xx){unsigned b=yy*g.w+xx;if(f.data[g.offset+b/8]&(0x80>>(b&7)))gfx.drawPixel(x+g.x+xx,y+yy,color);}x+=g.advance;}}
void button(int x,int y,int w,const char* title){gfx.fillRoundRect(x,y,w,44,9,CARD);gfx.drawRoundRect(x,y,w,44,9,ACCENT);text(x+16,y+8,w-24,title);}
void rowDraw(int slot,int row){int y=92+slot*55; gfx.fillRoundRect(18,y,744,49,8,CARD);auto&r=pages[current].rows[row];text(34,y+10,435,r.label);text(486,y+10,258,r.values[choices[current][row]],r.target>=0?ACCENT:MUTED);}
void home(){
 if(!demoClock&&demoSkin==5){gfx.fillScreen(BG);button(18,14,112,"MENÜ");text(151,12,550,"Service / jelgenerátor (demó)",ACCENT);
 gfx.fillRoundRect(18,92,764,180,12,CARD);for(int x=30;x<760;x++){int y=180+int(50*sin((x-30)*0.025));gfx.drawPixel(x,y,ACCENT);}
 static const char* waves[]={"Szinusz","Négyszög","Háromszög","Felfutás","Lefutás"};static const char* freq[]={"1 Hz","100 Hz","1 kHz","10 kHz"};
 button(18,300,236,waves[demoWave]);button(273,300,236,freq[demoFreq]);button(528,300,254,demoBt?"BT OUT":"DAC");
 button(18,370,236,demoPlaying?"Stop":"Start");button(273,370,236,"Felület");text(22,434,754,"Bemutató / nem ad ki mérőjelet",MUTED);gfx.pushSprite(0,0);dirty=false;return;}
 if(!demoClock){gfx.fillScreen(BG);auto* pixels=demoSkins[demoSkin];
 for(int y=0;y<240;y++)for(int x=0;x<320;x++)gfx.fillRect(80+x*2,y*2,2,2,pixels[y*320+x]);
 gfx.fillRoundRect(4,10,72,48,8,CARD);text(12,19,62,"Menü",TEXT);gfx.fillRoundRect(724,10,72,48,8,CARD);text(749,19,30,">",ACCENT);
 gfx.fillRect(0,400,80,80,BG);text(8,418,70,"DEMO",MUTED);gfx.pushSprite(0,0);dirty=false;return;}
 gfx.fillScreen(BG);button(18,14,112,"MENÜ");text(151,12,484,"Ébresztőóra / demó",ACCENT);button(650,14,132,"Rádió");
 gfx.fillRoundRect(18,92,764,238,16,CARD);text(44,108,600,"Minta idő / nincs hálózati időszinkron",MUTED);
 gfx.setFont(&lgfx::fonts::Font7);gfx.setTextSize(2);gfx.setTextColor(TEXT);gfx.drawCenterString("12:34",400,175);gfx.setTextSize(1);
 button(18,370,360,"Ébresztő beállításai");button(398,370,384,"Óranézet");text(22,434,756,"Felfutás / tartás / lefutás és heti napok",MUTED);gfx.pushSprite(0,0);dirty=false;
}
void draw(){if(!ready)return;if(current<0){home();return;}gfx.fillScreen(BG);button(18,14,112,"MENÜ");button(650,14,132,"Vissza");text(151,10,484,pages[current].title,ACCENT);text(151,43,484,"I-BT-Radio BETA / MENU8048C-R4",MUTED);text(22,429,550,"Menüpróba / csak RAM-beállítások",MUTED);
 for(int i=0;i<6&&top+i<pages[current].count;++i)rowDraw(i,top+i);
 int count=pages[current].count;gfx.fillRoundRect(775,92,7,324,3,CARD);int h=324*min(count,6)/count;int y=92+(count>6?(324-h)*top/(count-6):0);gfx.fillRoundRect(775,y,7,h,3,ACCENT);
 if(count>6){button(626,428,73,"Fel");button(709,428,73,"Le");}gfx.pushSprite(0,0);dirty=false;Serial.printf("[MENU] page=%d title=%s top=%d\n",current,pages[current].title,top);}
void navigate(int page){current=page;top=0;dirty=true;if(page<1)navDepth=0;}
void openPage(int page){if(page<0||page>=pageCount)return;if(current>=0&&navDepth<32)navHistory[navDepth++]=current;current=page;top=0;dirty=true;}
void backPage(){if(navDepth){current=navHistory[--navDepth];top=0;dirty=true;}else navigate(-1);}
void activate(int row){if(row<0||row>=pages[current].count)return;auto&r=pages[current].rows[row];if(sourceIds[current]==47&&row<9){demoSkin=row;demoClock=false;navigate(-1);return;}
 if(sourceIds[current]==1&&row==4){demoClock=!demoClock;navigate(-1);return;}
 if(r.target>=0)openPage(r.target);else if(r.target==-3)backPage();else if(r.target==-2){testPage=0;testRow=0;testRendered=0;nextTest=millis();}else{choices[current][row]=(choices[current][row]+1)%r.count;
 if(sourceIds[current]==10&&row==2){static const uint8_t levels[]={96,26,64,128,192,255};brightness=levels[choices[current][row]%6];screen.setBrightness(brightness);}
 rowDraw(row-top,row);int y=92+(row-top)*55;screen.setClipRect(0,y,800,49);gfx.pushSprite(0,0);screen.clearClipRect();}}
void tap(int x,int y){if(current<0){
 if(demoClock){if(y<66&&x<140)navigate(0);else if(y<66&&x>640){demoClock=false;dirty=true;}else if(y>=370&&y<414)openPage(pageIndex(x<398?59:64));return;}
 if(demoSkin==5){if(y<66&&x<140)navigate(0);else if(y>=300&&y<344){if(x<254)demoWave=(demoWave+1)%5;else if(x<509)demoFreq=(demoFreq+1)%4;else demoBt=!demoBt;dirty=true;}else if(y>=370&&y<414){if(x<254){demoPlaying=!demoPlaying;dirty=true;}else openPage(pageIndex(47));}return;}

 if(!demoClock){if(x<80&&y<80){navigate(0);return;}if(x>=720&&y<80){demoSkin=(demoSkin+1)%9;dirty=true;return;}
 if(demoSkin==2&&y>=300&&y<425){openPage(pageIndex(57));return;}
 if(y>=400){if(x>550){navigate(0);}else if(x<220){openPage(pageIndex(4));}else{demoPlaying=!demoPlaying;dirty=true;}}return;}

 if(y<66&&x<140){navigate(0);return;}
 if(y>=312&&y<356){if(x<238)demoPlaying=!demoPlaying;else if(x>=255&&x<375)demoVolume=max(0,demoVolume-5);else if(x>=391&&x<511)demoVolume=min(100,demoVolume+5);dirty=true;return;}
 if(y>=375&&y<419){if(x<238)openPage(pageIndex(4));else if(x<475)openPage(pageIndex(30));else openPage(pageIndex(10));}return;}
 if(y<66){if(x<140)navigate(0);else if(x>640)backPage();return;}int maxTop=max(0,pages[current].count-6);if(x>764&&y>=92&&y<=416){top=constrain((y-92)*maxTop/324,0,maxTop);dirty=true;return;}if(y>=428){if(x>=709)top=min(top+1,maxTop);else if(x>=626)top=max(0,top-1);dirty=true;return;}if(y>=92&&y<422&&x<764){int i=(y-92)/55;if(i<6&&(y-92)%55<49)activate(top+i);}}
bool readReg(uint16_t reg,uint8_t* dst,size_t n){Wire.beginTransmission(touchAddress);Wire.write(reg>>8);Wire.write(reg&255);if(Wire.endTransmission(false))return false;if(Wire.requestFrom(touchAddress,(uint8_t)n)!=n)return false;for(size_t i=0;i<n;i++)dst[i]=Wire.read();return true;}
void clearTouch(){Wire.beginTransmission(touchAddress);Wire.write(0x81);Wire.write(0x4E);Wire.write(0);Wire.endTransmission();}
void touch(){if(!touchAddress)return;uint8_t status=0;if(!readReg(0x814E,&status,1)||!(status&128))return;int count=status&15;if(count>5){clearTouch();return;}uint8_t point[8];if(count&& !readReg(0x814F,point,8))return;clearTouch();if(count){int x=point[1]|point[2]<<8,y=point[3]|point[4]<<8;if(x>=800||y>=480)return;lastX=x;lastY=y;if(!pressed){sx=x;sy=y;pressed=true;}}else if(pressed){pressed=false;int dy=lastY-sy;if(current<0&&abs(lastX-sx)>80){demoSkin=(demoSkin+(lastX<sx?1:8))%9;dirty=true;return;}if(abs(dy)>45){if(current>=0){int m=max(0,pages[current].count-6);top=constrain(top+(dy<0?3:-3),0,m);dirty=true;}}else if(abs(lastX-sx)<35)tap(sx,sy);}}
void status(){Serial.printf("[STATUS] firmware=%s display=%d touch=0x%02X psram=%u heap=%u page=%d audio=DISABLED wifi=DISABLED\n",version,ready,touchAddress,ESP.getPsramSize(),ESP.getFreeHeap(),current);}
void setup(){Serial.begin(115200);Serial.setTimeout(10);pinMode(2,OUTPUT);digitalWrite(2,LOW);delay(150);Serial.printf("[BOOT] %s target=ESP32-8048S070C\n",version);if(!psramFound()){Serial.println("[FAIL] PSRAM missing");return;}if(!screen.begin()){Serial.println("[FAIL] RGB begin");return;}screen.setBrightness(brightness);
 gfx.setColorDepth(16);gfx.setPsram(true);ready=gfx.createSprite(800,480)!=nullptr;buildMenu();if(!ready){Serial.println("[FAIL] RGB begin");return;}Wire.begin(19,20,400000);Wire.setTimeOut(20);pinMode(38,OUTPUT);digitalWrite(38,LOW);delay(15);digitalWrite(38,HIGH);delay(80);for(uint8_t a:{uint8_t(0x5D),uint8_t(0x14)}){Wire.beginTransmission(a);if(!Wire.endTransmission()){touchAddress=a;break;}}draw();screen.setBrightness(brightness);status();Serial.println("[BOOT-READY] MENU8048C-R4");}
void loop(){if(ready){uint32_t now=millis();if(int32_t(now-nextPoll)>=0){nextPoll=now+25;touch();}if(dirty)draw();if(testPage>=0&&int32_t(now-nextTest)>=0){nextTest=now+100;if(testPage>=pageCount){Serial.printf("[SELFTEST] RENDER_PASS pages=%d rows=%d touch=NOT_RUN functions=DEMO\n",pageCount,testRendered);testPage=-1;navigate(0);}else{current=testPage;top=constrain(testRow-5,0,max(0,pages[current].count-6));draw();++testRendered;if(++testRow>=pages[current].count){testRow=0;++testPage;}}}}
 while(Serial.available()){char c=Serial.read();if(c=='\n'){command.trim();if(command=="status")status();else if(command=="selftest"){testPage=0;testRow=0;testRendered=0;nextTest=millis();}else if(command=="stop"){testPage=-1;navigate(0);}else if(command=="home")navigate(-1);command="";}else if(c!='\r'&&command.length()<80)command+=c;}delay(5);}

