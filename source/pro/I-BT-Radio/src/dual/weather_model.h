#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ctime>
namespace weather {
enum class Provider : uint8_t { OpenMeteo, OpenWeather };
struct Config {
 uint32_t format=119;
 double latitude=47.4979,longitude=19.0402;
 char location[40]="Budapest",key[65]{};
 uint16_t intervalMinutes=15;
 Provider provider=Provider::OpenMeteo;
 bool enabled=false;
};
struct Reading {
 int16_t temperatureTenths=0;
 uint16_t humidity=0,windTenths=0,pressure=0,code=0;
 uint32_t updated=0,revision=0;
 bool valid=false;
};
inline bool validConfig(const Config& c){
 if(c.format!=119||!std::isfinite(c.latitude)||!std::isfinite(c.longitude)||c.latitude< -90||c.latitude>90||c.longitude< -180||c.longitude>180)return false;
 if(c.intervalMinutes!=15&&c.intervalMinutes!=30&&c.intervalMinutes!=60)return false;
 if(unsigned(c.provider)>1||!memchr(c.location,0,sizeof(c.location))||!c.location[0]||!memchr(c.key,0,sizeof(c.key)))return false;
 for(const unsigned char*p=(const unsigned char*)c.location;*p;++p)if(*p<32||*p==127)return false;
 for(const char*p=c.key;*p;++p)if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||*p=='_'||*p=='-'))return false;
 return !c.enabled||c.provider!=Provider::OpenWeather||strlen(c.key)>=16;
}
inline bool coordinate(const char* s,double& value){
 if(!s||!*s)return false;char*end=nullptr;double n=strtod(s,&end);
 if(end==s||*end||!std::isfinite(n))return false;value=n;return true;
}
inline bool fresh(const Reading&r,uint32_t now){return r.valid&&uint32_t(now-r.updated)<3600000u;}
inline bool due(const Config&c,bool idle,bool wifi,unsigned free,unsigned largest,uint32_t now,uint32_t last,bool attempted){
 return c.enabled&&idle&&wifi&&free>65000&&largest>=32768&&(!attempted||uint32_t(now-last)>=uint32_t(c.intervalMinutes)*60000u);
}
struct Solar {uint8_t phase=3;int x=160,y=130;bool visible=false,valid=false;double elevation=-90;};
// NOAA fractional-year approximation, UTC input; independent of local DST.
// https://gml.noaa.gov/grad/solcalc/solareqns.PDF
inline Solar solar(time_t epoch,double latitude,double longitude){
 Solar s;if(epoch<1600000000||!std::isfinite(latitude)||!std::isfinite(longitude)||fabs(latitude)>90||fabs(longitude)>180)return s;
 tm utc{};
#ifdef _WIN32
 gmtime_s(&utc,&epoch);
#else
 gmtime_r(&epoch,&utc);
#endif
 const int year=utc.tm_year+1900;
 const bool leap=year%4==0&&(year%100!=0||year%400==0);
 constexpr double pi=3.14159265358979323846;
 const double hour=utc.tm_hour+utc.tm_min/60.0+utc.tm_sec/3600.0;
 const double g=2*pi/(leap?366:365)*(utc.tm_yday+(hour-12)/24);
 const double eq=229.18*(.000075+.001868*cos(g)-.032077*sin(g)-.014615*cos(2*g)-.040849*sin(2*g));
 const double dec=.006918-.399912*cos(g)+.070257*sin(g)-.006758*cos(2*g)+.000907*sin(2*g)-.002697*cos(3*g)+.00148*sin(3*g);
 double solarMinutes=fmod(hour*60+eq+4*longitude,1440.0);if(solarMinutes<0)solarMinutes+=1440;
 const double ha=solarMinutes/4-180,lat=latitude*pi/180;
 double cosine=sin(lat)*sin(dec)+cos(lat)*cos(dec)*cos(ha*pi/180);if(cosine>1)cosine=1;if(cosine< -1)cosine=-1;
 s.elevation=90-acos(cosine)*180/pi;s.valid=true;s.visible=s.elevation>-.833;
 s.phase=s.elevation< -6?3:s.elevation<8?(ha<0?0:2):1;
 s.x=int(160+ha*1.35);if(s.x<24)s.x=24;if(s.x>296)s.x=296;
 s.y=int(127-std::fmax(0.0,s.elevation)*1.05);if(s.y<28)s.y=28;
 return s;
}
}
