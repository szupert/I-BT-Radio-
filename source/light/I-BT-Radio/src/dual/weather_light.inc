#include "weather.h"
#include "weather_light_model.h"
#include "resource_limiter.h"
#include "sensors/receiver.h"
#include "bt_audio.h"
#include "../core/player.h"
#include "../core/network.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <esp_heap_caps.h>
#include <atomic>
namespace weather {
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static Config config{};static Forecast cached{};static uint32_t revision=0;
static std::atomic<bool> shown{false},allowed{false},fetching{false},want{false},dirty{false};
static std::atomic<unsigned> result{0};static bool loaded=false,attempted=false;static uint32_t last=0;
static std::atomic<uint32_t> overlayUntil{0};
bool active(){return shown.load();}
void setActive(bool on){shown=on;allowed=false;want=on;overlayUntil=0;if(on){sensors::enableRoles();portENTER_CRITICAL(&mux);config.enabled=true;portEXIT_CRITICAL(&mux);}}
bool controlsVisible(uint32_t now){const uint32_t end=overlayUntil.load();return end&&int32_t(end-now)>0;}
void wake(uint32_t now){overlayUntil=now+8000;}
Config configuration(){portENTER_CRITICAL(&mux);Config c=config;portEXIT_CRITICAL(&mux);return c;}
Forecast forecast(){portENTER_CRITICAL(&mux);Forecast f=cached;portEXIT_CRITICAL(&mux);return f;}
Reading reading(){return forecast().current;}
bool enabled(){return configuration().enabled;}
bool pending(){return dirty.load();}
bool busy(){return fetching.load()||(shown.load()&&want.load()&&allowed.load());}
void audioRequested(){allowed=false;want=false;}
void refresh(){if(active())want=true;}
const char* status(){static const char* states[]={"Adatokra vár","Frissítés...","Friss adat","API-hiba: a korábbi adat marad","Mentésre vár","Memóriafoglalás sikertelen"};return states[std::min(result.load(),5u)];}
bool configure(const Config& c){if(!validConfig(c)||c.provider!=Provider::OpenMeteo||dirty.load())return false;portENTER_CRITICAL(&mux);config=c;++revision;cached={};dirty=true;portEXIT_CRITICAL(&mux);want=true;return true;}
void toggle(){Config c=configuration();c.enabled=!c.enabled;configure(c);}
bool temperature(int& t){auto r=reading();t=r.temperatureTenths;return enabled()&&fresh(r,millis());}
void tick(bool idle){allowed=idle&&active();}
static bool current(uint32_t rev){portENTER_CRITICAL(&mux);const bool same=revision==rev&&config.enabled;portEXIT_CRITICAL(&mux);return same&&active()&&allowed.load();}
static bool save(const Config& c){Preferences p;if(!p.begin("ibt_weather",false))return false;Config check{};bool ok=p.putBytes("profile",&c,sizeof(c))==sizeof(c)&&p.getBytes("profile",&check,sizeof(check))==sizeof(check)&&!memcmp(&c,&check,sizeof(c));p.end();return ok;}
static void run(void*){
 const Config c=configuration();uint32_t rev;portENTER_CRITICAL(&mux);rev=revision;portEXIT_CRITICAL(&mux);
 const uint32_t start=millis();char url[448];
 snprintf(url,sizeof(url),"https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m&daily=weather_code,temperature_2m_max,temperature_2m_min&forecast_days=5&timezone=auto&timeformat=unixtime",c.latitude,c.longitude);
 char* body=(char*)malloc(4096);bool ok=false;int code=0;Forecast next{};
 if(body){esp_http_client_config_t cfg{};cfg.url=url;cfg.crt_bundle_attach=esp_crt_bundle_attach;cfg.timeout_ms=2000;cfg.buffer_size=512;cfg.buffer_size_tx=512;cfg.disable_auto_redirect=true;
  auto h=esp_http_client_init(&cfg);if(h){
   if(current(rev)&&esp_http_client_open(h,0)==ESP_OK&&current(rev)){
    const int64_t bytes=esp_http_client_fetch_headers(h);code=esp_http_client_get_status_code(h);unsigned n=0;
    if(code==200&&bytes<4096&&current(rev)){esp_http_client_set_timeout_ms(h,250);const uint32_t readAt=millis();
     while(current(rev)&&uint32_t(millis()-readAt)<2000&&n<4095){int got=esp_http_client_read(h,body+n,4095-n);if(got<0)break;n+=unsigned(got);if(esp_http_client_is_complete_data_received(h)){ok=true;break;}if(!got)break;vTaskDelay(1);}
     body[n]=0;ok=ok&&current(rev)&&decodeForecast(body,n,next);
    }
   }
   esp_http_client_close(h);esp_http_client_cleanup(h);
  }free(body);
 }
 if(ok&&current(rev)){next.current.updated=millis();portENTER_CRITICAL(&mux);if(revision==rev){next.current.revision=cached.current.revision+1;cached=next;}portEXIT_CRITICAL(&mux);result=2;}else result=body?3:5;
 Serial.printf("[LIGHT-WEATHER] result=%s http=%d ms=%lu free=%lu largest=%lu task_hwm=%lu playback=OFF\n",ok?"OK":"FAIL_OR_CANCEL",code,(unsigned long)(millis()-start),(unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned long)uxTaskGetStackHighWaterMark(nullptr));
 ibtlimit::release(ibtlimit::Job::Weather);fetching=false;vTaskDelete(nullptr);
}
void service(){
 if(!loaded){Preferences p;Config c{};bool found=false;if(p.begin("ibt_weather",true)){found=p.getBytesLength("profile")==sizeof(c)&&p.getBytes("profile",&c,sizeof(c))==sizeof(c)&&validConfig(c)&&c.provider==Provider::OpenMeteo;p.end();}portENTER_CRITICAL(&mux);if(found&&!dirty.load()&&!revision)config=c;loaded=true;portEXIT_CRITICAL(&mux);}
 if(fetching.load()||player.isRunning()||player.status()!=STOPPED||btaudio::wanted()||!btaudio::idle())return;
 static uint32_t saveAt=0;static bool saveTried=false;
 if(dirty.load()&&(!saveTried||uint32_t(millis()-saveAt)>=30000)&&sensors::quiescent()&&!ibtlimit::backgroundBusy()){
  saveAt=millis();saveTried=true;const auto c=configuration();if(save(c))dirty=false;else result=4;
 }
 if(!active()||!allowed.load()||!enabled()||dirty.load()||WiFi.status()!=WL_CONNECTED||network.connectionBusy())return;
 const uint32_t retry=result.load()==2?uint32_t(configuration().intervalMinutes)*60000u:60000u;
 if(!want.load()&&attempted&&uint32_t(millis()-last)<retry)return;
 // First veto passive BLE, then wait for its normal scan/GATT + SDK teardown ACK.
 want=true;
 if(!sensors::quiescent()||esp_bluedroid_get_status()!=ESP_BLUEDROID_STATUS_UNINITIALIZED||esp_bt_controller_get_status()!=ESP_BT_CONTROLLER_STATUS_IDLE)return;
 if(!ibtlimit::acquire(ibtlimit::Job::Weather,0))return;
 fetching=true;want=false;attempted=true;last=millis();result=1;
 if(xTaskCreatePinnedToCore(run,"weather",6144,nullptr,tskIDLE_PRIORITY,nullptr,0)!=pdPASS){fetching=false;ibtlimit::release(ibtlimit::Job::Weather);result=5;}
}
}
