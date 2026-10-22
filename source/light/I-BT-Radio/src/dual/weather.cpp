#include "weather.h"
#include "weather_decode.h"
#include "resource_limiter.h"
#include "sensors/receiver.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <atomic>
namespace weather {
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static Config config{};static Reading sample{};static uint32_t revision=0;
static std::atomic<bool> dirty{false},allowed{false},fetching{false};
static std::atomic<unsigned> result{0};
static TaskHandle_t worker=nullptr;static bool loaded=false;
Config configuration(){portENTER_CRITICAL(&mux);Config c=config;portEXIT_CRITICAL(&mux);return c;}
Reading reading(){portENTER_CRITICAL(&mux);Reading r=sample;portEXIT_CRITICAL(&mux);return r;}
bool enabled(){portENTER_CRITICAL(&mux);const bool on=config.enabled;portEXIT_CRITICAL(&mux);return on;}
bool pending(){return dirty.load();}
bool busy(){return fetching.load();}
static std::atomic<bool> audioVeto{false};
void audioRequested(){audioVeto.store(true);allowed.store(false);}
const char* status(){
 if(dirty.load())return result.load()==5?"Mentesi hiba; nyugalomban ujraprobalja":"Mentesre var / lejatszas STOP";
 if(!enabled())return "Kikapcsolva";
 static const char*const states[]={"Frissitesre var","Nyugalmi allapotra var","Mentes","Friss adat","Halozati/API hiba; kesobb ujraprobalja","Mentesi hiba","RAM/mas hattermunka miatt halasztva"};
 unsigned n=result.load();return states[n<7?n:1];
}
bool configure(const Config& c){
 if(!validConfig(c)||dirty.load())return false;
 portENTER_CRITICAL(&mux);if(dirty.load()){portEXIT_CRITICAL(&mux);return false;}
 config=c;++revision;sample={};dirty.store(true);portEXIT_CRITICAL(&mux);allowed.store(false);return true;
}
void toggle(){Config c=configuration();c.enabled=!c.enabled;configure(c);}
bool temperature(int&t){const auto r=reading();t=r.temperatureTenths;return enabled()&&fresh(r,millis());}
static bool current(uint32_t rev){portENTER_CRITICAL(&mux);const bool same=rev==revision&&config.enabled;portEXIT_CRITICAL(&mux);return same&&allowed.load()&&heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=32768;}
static bool save(const Config& c){
 Preferences p;if(!p.begin("ibt_weather",false))return false;
 bool ok=p.putBytes("profile",&c,sizeof(c))==sizeof(c);Config check{};
 ok=ok&&p.getBytes("profile",&check,sizeof(check))==sizeof(check)&&!memcmp(&c,&check,sizeof(c));p.end();return ok;
}
static bool fetch(const Config& c,uint32_t rev,Reading& value,int& statusCode){
 // Optional body and URL live only in PSRAM; no internal fallback.
 char* storage=(char*)heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!storage)return false;
 char*url=storage;char*body=storage+768;constexpr unsigned capacity=3328;unsigned n=0;bool ok=false;
 if(c.provider==Provider::OpenMeteo)snprintf(url,768,"https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f&current=temperature_2m,relative_humidity_2m,weather_code,pressure_msl,wind_speed_10m&forecast_days=1",c.latitude,c.longitude);
 else snprintf(url,768,"https://api.openweathermap.org/data/2.5/weather?lat=%.5f&lon=%.5f&units=metric&appid=%s",c.latitude,c.longitude,c.key);
 esp_http_client_config_t cfg{};cfg.url=url;cfg.crt_bundle_attach=esp_crt_bundle_attach;cfg.timeout_ms=1500;cfg.buffer_size=512;cfg.buffer_size_tx=512;cfg.disable_auto_redirect=true;
 const uint32_t start=millis();auto h=esp_http_client_init(&cfg);
 if(h){
  if(current(rev)&&esp_http_client_open(h,0)==ESP_OK&&current(rev)){
   const int64_t size=esp_http_client_fetch_headers(h);statusCode=esp_http_client_get_status_code(h);
   if(statusCode==200&&size<capacity&&current(rev)){
    esp_http_client_set_timeout_ms(h,250);
    while(current(rev)&&uint32_t(millis()-start)<6000&&n<capacity-1){
     const int got=esp_http_client_read(h,body+n,capacity-1-n);
     if(got<0)break;n+=unsigned(got);
     if(esp_http_client_is_complete_data_received(h)){ok=true;break;}
     if(!got)break;vTaskDelay(1);
    }
    body[n]=0;ok=ok&&current(rev)&&decode(body,n,c.provider,value);
   }
  }
  esp_http_client_close(h);esp_http_client_cleanup(h);
 }
 // URL may include an API key: never print it or HTTP-client error text.
 memset(storage,0,4096);free(storage);return ok;
}
static void run(void*){
 uint32_t last=0;bool attempted=false;uint32_t seen=UINT32_MAX;
 for(;;){
  if(allowed.load()){
   Config c;uint32_t rev;portENTER_CRITICAL(&mux);c=config;rev=revision;portEXIT_CRITICAL(&mux);
   if(seen!=rev){seen=rev;attempted=false;}
   if(c.enabled&&!dirty.load()&&WiFi.status()==WL_CONNECTED&&(!attempted||uint32_t(millis()-last)>=uint32_t(c.intervalMinutes)*60000u)){
    const auto free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(due(c,allowed.load(),true,free,largest,millis(),last,attempted)&&sensors::quiescent()&&ibtlimit::acquire(ibtlimit::Job::Weather,16384)){
     fetching=true;
     if(current(rev)&&sensors::quiescent()){
      last=millis();attempted=true;Reading next{};int code=0;const bool ok=fetch(c,rev,next,code);
      if(ok&&current(rev)){next.updated=millis();portENTER_CRITICAL(&mux);if(rev==revision){next.revision=sample.revision+1;sample=next;}portEXIT_CRITICAL(&mux);result=3;}
      else result=4;
      Serial.printf("[WEATHER] result=%s http=%d ms=%lu audio_veto=%u\n",ok?"OK":"DEFER_OR_FAIL",code,(unsigned long)(millis()-last),allowed.load()?0:1);
     }
     fetching=false;ibtlimit::release(ibtlimit::Job::Weather);
    }else result=6;
   }
  }
  vTaskDelay(pdMS_TO_TICKS(allowed.load()&&enabled()?1000:10000));
 }
}
void tick(bool idle){
 if(!loaded){Config restored{};bool found=false;Preferences p;if(p.begin("ibt_weather",true)){found=p.getBytesLength("profile")==sizeof(restored)&&p.getBytes("profile",&restored,sizeof(restored))==sizeof(restored)&&validConfig(restored);p.end();}
  portENTER_CRITICAL(&mux);if(found&&!dirty.load()&&revision==0)config=restored;loaded=true;portEXIT_CRITICAL(&mux);
 }
 static uint32_t idleAt=millis();
 if(!idle||audioVeto.exchange(false))idleAt=millis();
 idle=idle&&uint32_t(millis()-idleAt)>=15000;
 allowed.store(idle);
 // Small verified NVS save belongs to the existing idle UI owner, not to
 // the optional TLS task. Keep the original sensor reserve and no internal image fallback.
 static uint32_t saveAt=0,coldAt=0;static bool saveAttempted=false,coldAttempted=false;
 if(!dirty.load())saveAttempted=false;
 if(idle&&dirty.load()&&(!saveAttempted||uint32_t(millis()-saveAt)>=30000)){
  saveAt=millis();saveAttempted=true;
  if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=32768&&heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=16384&&sensors::quiescent()&&ibtlimit::acquire(ibtlimit::Job::Weather,2048)){
   fetching=true;const Config c=configuration();const bool ok=save(c);fetching=false;ibtlimit::release(ibtlimit::Job::Weather);
   if(ok){dirty.store(false);result=1;}else result=5;
  }else result=6;
 }
 if(idle&&enabled()&&!dirty.load()&&!worker&&(!coldAttempted||uint32_t(millis()-coldAt)>=30000)){
  coldAt=millis();coldAttempted=true;
  if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>75000&&heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=32768){
   if(xTaskCreatePinnedToCore(run,"weather",6144,nullptr,1,&worker,0)!=pdPASS){worker=nullptr;result=6;}
  }else result=6;
 }
}
}
