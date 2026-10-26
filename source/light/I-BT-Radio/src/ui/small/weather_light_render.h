#pragma once
#include "weather_font.h"
#include "light_menu_font.h"
#include "../../dual/weather_light_model.h"
#include "../../dual/sensors/passive_policy.h"
#include <stdio.h>
#include <time.h>
#include <algorithm>
#include <cmath>
namespace lightweather {
// Widen channels before shifting. Every palette value is RGB565, never byte-truncated.
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b){return uint16_t(((uint32_t(r)&248)<<8)|((uint32_t(g)&252)<<3)|(uint32_t(b)>>3));}
constexpr uint16_t bg=rgb(8,15,24),fg=rgb(239,246,252),muted=rgb(145,164,184),cyan=rgb(67,214,208),line=rgb(36,55,74),card=rgb(16,28,41),sun=rgb(255,212,113),cloud=rgb(222,235,245),rain=rgb(84,189,236),sensorBg=rgb(13,37,44);
inline uint32_t utf8(const char*& p){unsigned char c=*p++;if(c<128)return c;unsigned n=c<224?1:c<240?2:3;uint32_t v=c&((1u<<(6-n))-1);while(n--&&*p)v=(v<<6)|(uint8_t(*p++)&63);return v;}
inline int width(const char* s,unsigned size){int w=0;for(unsigned i=0;s&&*s&&i<80;++i)w+=weatherfont::glyph(size,utf8(s))->advance;return w;}
inline uint16_t blend(uint16_t a,uint16_t b,unsigned alpha){unsigned r=((a>>11)*alpha+(b>>11)*(15-alpha)+7)/15,g=(((a>>5)&63)*alpha+((b>>5)&63)*(15-alpha)+7)/15,bl=((a&31)*alpha+(b&31)*(15-alpha)+7)/15;return uint16_t((r<<11)|(g<<5)|bl);}
template<class L> void text(L& lcd,int x,int y,const char* s,unsigned size=9,uint16_t ink=fg,uint16_t back=bg,int align=0,int maxWidth=320){
 if(align)x-=align==1?width(s,size)/2:width(s,size);const int end=std::min(320,x+maxWidth);lcd.startWrite();
 for(unsigned n=0;s&&*s&&n<80;++n){const auto g=weatherfont::glyph(size,utf8(s));if(x+g->w>end)break;
  for(unsigned row=0;row<g->h&&y+int(row)<240;++row){unsigned col=0;while(col<g->w){const unsigned idx=row*g->w+col,packed=weatherfont::pixels[g->offset+idx/2],a=(idx&1)?packed&15:packed>>4;const unsigned first=col++;if(!a)continue;while(col<g->w){unsigned next=row*g->w+col,v=weatherfont::pixels[g->offset+next/2],aa=(next&1)?v&15:v>>4;if(aa!=a)break;++col;}if(x+int(first)>=0&&y+int(row)>=0)lcd.writeFillRect(x+first,y+row,col-first,1,blend(ink,back,a));}}
  x+=g->advance;
 }lcd.endWrite();
}
template<class L> void icon(L& l,int x,int y,uint16_t code,int scale=100,bool day=true){
 auto X=[&](int v){return x+v*scale/100;};auto Y=[&](int v){return y+v*scale/100;};auto R=[&](int v){return std::max(1,v*scale/100);};
 const bool partly=code==1||code==2,clear=code==0;
 if(clear||partly){int sx=partly?-4:0,sy=partly?-5:0;for(int a=0;a<8;++a){double t=a*3.14159265/4;l.drawLine(X(sx+int(11*cos(t))),Y(sy+int(11*sin(t))),X(sx+int(14*cos(t))),Y(sy+int(14*sin(t))),sun);}l.fillCircle(X(sx),Y(sy),R(8),day?sun:cloud);if(!day)l.fillCircle(X(sx+4),Y(sy-3),R(7),bg);}
 if(!clear){l.fillCircle(X(-9),Y(6),R(7),cloud);l.fillCircle(X(0),Y(1),R(10),cloud);l.fillCircle(X(11),Y(6),R(7),cloud);l.fillRect(X(-10),Y(3),R(22),R(10),cloud);}
 if(code>=51&&code<=67||code>=80&&code<=82||code>=95)for(int dx=-8;dx<=10;dx+=9)l.drawLine(X(dx),Y(17),X(dx-3),Y(23),rain);
 if(code>=71&&code<=77||code==85||code==86)for(int dx=-8;dx<=10;dx+=9)l.fillCircle(X(dx),Y(20),R(1),fg);
 if(code==45||code==48)for(int yy=17;yy<=23;yy+=5)l.drawLine(X(-14),Y(yy),X(15),Y(yy),muted);
 if(code>=95){l.drawLine(X(0),Y(13),X(-3),Y(20),sun);l.drawLine(X(-3),Y(20),X(3),Y(18),sun);l.drawLine(X(3),Y(18),X(0),Y(25),sun);}
}
struct Data {weather::Forecast forecast{};char location[40]{};radiosensor::Reading sensors[2]{};bool assigned[2]{},fresh[2]{};bool connected=false,overlay=false;uint32_t now=0;time_t epoch=0;const char* status="";};
inline uint32_t hash(const void* ptr,size_t n,uint32_t h=2166136261u){auto p=(const uint8_t*)ptr;while(n--)h=(h^*p++)*16777619u;return h;}
inline tm calendar(time_t e){tm t{};
#ifdef _WIN32
 gmtime_s(&t,&e);
#else
 gmtime_r(&e,&t);
#endif
 return t;}
struct Paint {uint32_t header=0,current=0,roles=0,days=0,footer=0;bool wasOverlay=false;};
template<class L> void paint(L& l,Paint& old,const Data& d,bool full=false){
 const auto& f=d.forecast;const bool valid=f.current.valid,stale=valid&&!weather::fresh(f.current,d.now);char b[100];
 time_t e=d.epoch>=1600000000?d.epoch:valid?time_t(f.epoch)+uint32_t(d.now-f.current.updated)/1000:0;const auto t=calendar(e+f.utcOffset);
 uint32_t h=hash(d.location,strlen(d.location));h=hash(&d.connected,sizeof(d.connected),h);const uint32_t minute=e/60;h=hash(&minute,sizeof(minute),h);
 if(full)l.fillRect(0,0,320,240,bg);
 if(full||old.header!=h){old.header=h;l.fillRect(0,0,320,43,bg);text(l,14,10,d.location,17,fg,bg,0,205);snprintf(b,sizeof(b),e?"%02d:%02d":"--:--",t.tm_hour,t.tm_min);text(l,306,10,b,17,fg,bg,2);
  static const char* days[]={"VASÁRNAP","HÉTFŐ","KEDD","SZERDA","CSÜTÖRTÖK","PÉNTEK","SZOMBAT"};static const char* months[]={"JANUÁR","FEBRUÁR","MÁRCIUS","ÁPRILIS","MÁJUS","JÚNIUS","JÚLIUS","AUGUSZTUS","SZEPTEMBER","OKTÓBER","NOVEMBER","DECEMBER"};
  if(e)snprintf(b,sizeof(b),"%s, %s %d.",days[t.tm_wday],months[t.tm_mon],t.tm_mday);else snprintf(b,sizeof(b),"Óraszinkronra vár");text(l,14,34,b,9,muted);
  l.fillCircle(232,22,2,d.connected?cyan:muted);
 }
 h=f.current.revision*257u+unsigned(valid)+unsigned(stale)*2+unsigned(d.connected)*4;h=hash(d.status,strlen(d.status),h);
 if(full||old.current!=h){old.current=h;l.fillRect(0,43,320,80,bg);
  if(valid){snprintf(b,sizeof(b),"%d°",int(lround(f.current.temperatureTenths/10.)));text(l,13,50,b,42);text(l,111,58,weather::condition(f.current.code),12,fg,bg,0,105);if(stale)text(l,111,79,"Régi adat",9,sun);
   // Keep all icon pixels inside this dirty band, above the metrics/sensors.
   const bool wet=f.current.code>=45,partly=f.current.code==1||f.current.code==2;
   icon(l,265,wet?62:partly?80:76,f.current.code,wet?160:partly?190:210,f.daylight);
   snprintf(b,sizeof(b),"Pára %u%%",f.current.humidity);text(l,15,109,b,9,muted);snprintf(b,sizeof(b),"Szél %u km/h",unsigned((f.current.windTenths+5)/10));text(l,118,109,b,9,muted);snprintf(b,sizeof(b),"Érzet %d°",int(lround(f.apparent/10.)));text(l,305,109,b,9,muted,bg,2);
  }else{text(l,14,55,"Időjárás",17);text(l,14,83,d.connected?d.status:"Wi-Fi-kapcsolatra vár",12,muted,bg,0,205);icon(l,265,72,3,220);}
 }
 h=0;for(unsigned r=0;r<2;++r){h=hash(&d.assigned[r],1,h);h=hash(&d.fresh[r],1,h);if(d.fresh[r]){h=hash(&d.sensors[r].temperatureCenti,2,h);h=hash(&d.sensors[r].humidityCenti,2,h);}}
 if(full||old.roles!=h){old.roles=h;l.fillRect(0,123,320,42,bg);unsigned count=unsigned(d.assigned[0])+unsigned(d.assigned[1]);
  if(!count)text(l,160,139,"Kint / Bent: válassz BLE-hőmérőt",9,muted,bg,1);
  for(unsigned r=0;r<2;++r)if(d.assigned[r]){const int x=count==1?89:r?165:12;l.fillRoundRect(x,126,143,34,5,sensorBg);l.drawRoundRect(x,126,143,34,5,rgb(35,70,76));text(l,x+10,129,r?"Bent:":"Kint:",9,cyan,sensorBg);
   if(d.fresh[r]){const int v=d.sensors[r].temperatureCenti;snprintf(b,sizeof(b),"%s%d,%d°C",v<0?"-":"",abs(v)/100,(abs(v)%100)/10);text(l,x+10,142,b,12,fg,sensorBg);snprintf(b,sizeof(b),"%u%%",unsigned((d.sensors[r].humidityCenti+50)/100));text(l,x+133,143,b,12,rain,sensorBg,2);}else{text(l,x+10,142,"--,-°C",12,muted,sensorBg);text(l,x+133,144,"nincs adat",9,muted,sensorBg,2);}}
 }
 h=f.current.revision+unsigned(valid)*65537+unsigned(t.tm_yday)*7919u;
 if(full||old.days!=h||(old.wasOverlay&&!d.overlay)){old.days=h;old.footer=UINT32_MAX;l.fillRect(0,165,320,75,bg);static const char* dn[]={"VAS","HÉT","KED","SZE","CSÜ","PÉN","SZO"};
  for(unsigned i=0;i<5;++i){int x=12+60*i,center=x+28;l.fillRoundRect(x,168,56,70,6,card);l.drawRoundRect(x,168,56,70,6,i?line:cyan);
   if(valid){const auto day=calendar(time_t(f.days[i].epoch)+f.utcOffset+43200);const bool today=day.tm_year==t.tm_year&&day.tm_yday==t.tm_yday;snprintf(b,sizeof(b),"%s %d.",today?"MA":dn[day.tm_wday],day.tm_mday);text(l,center,173,b,9,i?fg:cyan,card,1);icon(l,center,198,f.days[i].code,72);snprintf(b,sizeof(b),"%d°",int(lround(f.days[i].high/10.)));text(l,center-2,224,b,9,sun,card,2);snprintf(b,sizeof(b),"/ %d°",int(lround(f.days[i].low/10.)));text(l,center,224,b,9,muted,card);}else text(l,center,195,"—",12,muted,card,1);}
 }
 if(d.overlay){h=f.current.revision;if(full||!old.wasOverlay||old.footer!=h){old.footer=h;l.fillRect(0,207,320,33,rgb(6,17,26));l.drawLine(0,207,319,207,line);const auto u=calendar(time_t(f.epoch)+f.utcOffset);snprintf(b,sizeof(b),valid?"Frissítve %02d:%02d":"Még nincs friss adat",u.tm_hour,u.tm_min);menufont::text(l,14,213,b,fg,rgb(6,17,26),206,15);l.fillRoundRect(228,212,80,24,5,rgb(22,67,75));l.drawRoundRect(228,212,80,24,5,cyan);menufont::text(l,268-menufont::width("Vissza",15)/2,213,"Vissza",cyan,rgb(22,67,75),76,15);}}
 old.wasOverlay=d.overlay;
}
}
