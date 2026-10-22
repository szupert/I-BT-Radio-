#include "options.h"
#include "Arduino.h"
#include "timekeeper.h"
#include "../dual/resource_limiter.h"
#include <WiFiClient.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "network.h"
#include "display.h"
#include "player.h"
#include "netserver.h"
#include "rtcsupport.h"
#include "../displays/tools/l10n.h"
#include "../pluginsManager/pluginsManager.h"
#ifdef USE_NEXTION
#include "../displays/nextion.h"
#endif
#if DSP_MODEL==DSP_DUMMY
#define DUMMYDISPLAY
#endif

#if RTCSUPPORTED
  //#define TIME_SYNC_INTERVAL  24*60*60*1000
  #define TIME_SYNC_INTERVAL  config.store.timeSyncIntervalRTC*60*60*1000
#else
  #define TIME_SYNC_INTERVAL  config.store.timeSyncInterval*60*1000
#endif
#define WEATHER_SYNC_INTERVAL config.store.weatherSyncInterval*60*1000

#define SYNC_STACK_SIZE       1024 * 4
#define SYNC_TASK_CORE        0
#define SYNC_TASK_PRIORITY    3
#define WEATHER_STRING_L      254

#ifdef HEAP_DBG
  void printHeapFragmentationInfo(const char* title){
    size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    float fragmentation = 100.0 * (1.0 - ((float)largestBlock / (float)freeHeap));
    Serial.printf("\n****** %s ******\n", title);
    Serial.printf("* Free heap: %u bytes\n", freeHeap);
    Serial.printf("* Largest free block: %u bytes\n", largestBlock);
    Serial.printf("* Fragmentation: %.2f%%\n", fragmentation);
    Serial.printf("*************************************\n\n");
  }
  #define HEAP_INFO() printHeapFragmentationInfo(__PRETTY_FUNCTION__)
#else
  #define HEAP_INFO()
#endif

TimeKeeper timekeeper;

void _syncTask(void *pvParameters) {
  auto job=static_cast<ibtlimit::Job>(reinterpret_cast<uintptr_t>(pvParameters));
  if(job==ibtlimit::Job::TimeSync)timekeeper.timeTask();
  else if(job==ibtlimit::Job::Weather)timekeeper.weatherTask();
  ibtlimit::release(job);
  timekeeper.busy=false;
  vTaskDelete(NULL);
}

TimeKeeper::TimeKeeper(){
  busy          = false;
  forceWeather  = true;
  forceTimeSync = true;
  _returnPlayerTime = _doAfterTime = 0;
  weatherBuf=NULL;
  #if (DSP_MODEL!=DSP_DUMMY || defined(USE_NEXTION)) && !defined(HIDE_WEATHER)
    weatherBuf = (char *) malloc(sizeof(char) * WEATHER_STRING_L);
    if(weatherBuf)memset(weatherBuf, 0, WEATHER_STRING_L);
  #endif
}

bool TimeKeeper::loop0(){ // core0 (display)
  if (network.status != CONNECTED) return true;
  uint32_t currentTime = millis();
  static uint32_t _last1s = 0;
  static uint32_t _last2s = 0;
  static uint32_t _last5s = 0;
  if (currentTime - _last1s >= 1000) { // 1sec
    _last1s = currentTime;
//#ifndef DUMMYDISPLAY
#if !defined(DUMMYDISPLAY) || defined(USE_NEXTION)
  #ifndef UPCLOCK_CORE1
    _upClock();
  #endif
#endif
  }
  if (currentTime - _last2s >= 2000) { // 2sec
    _last2s = currentTime;
    _upRSSI();
  }
  if (currentTime - _last5s >= 5000) { // 5sec
    _last5s = currentTime;
    //HEAP_INFO();
  }

  return true; // just in case
}

bool TimeKeeper::loop1(){ // core1 (player)
  uint32_t currentTime = millis();
  static uint32_t _last1s = 0;
  static uint32_t _last2s = 0;
  if (currentTime - _last1s >= 1000) { // 1sec
    pm.on_ticker();
    _last1s = currentTime;
//#ifndef DUMMYDISPLAY
#if !defined(DUMMYDISPLAY) || defined(USE_NEXTION)
  #ifdef UPCLOCK_CORE1
    _upClock();
  #endif
#endif
    _upScreensaver();
    _upSDPos();
    _returnPlayer();
    _doAfterWait();
  }
  if (currentTime - _last2s >= 2000) { // 2sec
    _last2s = currentTime;
  }

  //#ifdef DUMMYDISPLAY
  #if defined(DUMMYDISPLAY) && !defined(USE_NEXTION)
  return true;
  #endif
  // Sync weather & time
  static uint32_t lastWeatherTime = 0;
  if (currentTime - lastWeatherTime >= WEATHER_SYNC_INTERVAL) {
    lastWeatherTime = currentTime;
    forceWeather = true;
  }
  static uint32_t lastTimeTime = 0;
  if (currentTime - lastTimeTime >= TIME_SYNC_INTERVAL) {
    lastTimeTime = currentTime;
    forceTimeSync = true;
  }
  static uint32_t nextSync=0;
  if (!busy && (forceWeather || forceTimeSync) && network.status==CONNECTED && int32_t(currentTime-nextSync)>=0) {
    auto job=forceTimeSync?ibtlimit::Job::TimeSync:ibtlimit::Job::Weather;
    if(ibtlimit::acquire(job,8192)){
      busy=true;nextSync=currentTime+30000;
      if(xTaskCreatePinnedToCore(_syncTask,"syncTask",SYNC_STACK_SIZE,reinterpret_cast<void*>(static_cast<uintptr_t>(job)),1,NULL,SYNC_TASK_CORE)!=pdPASS){
        busy=false;nextSync=currentTime+1000;ibtlimit::release(job);
      }
    }
  }
  
  return true; // just in case
}

void TimeKeeper::waitAndReturnPlayer(uint8_t time_s){
  _returnPlayerTime = millis()+time_s*1000;
}
void TimeKeeper::_returnPlayer(){
  if(_returnPlayerTime>0 && millis()>=_returnPlayerTime){
    _returnPlayerTime = 0;
    display.putRequest(NEWMODE, PLAYER);
  }
}

void TimeKeeper::waitAndDo(uint8_t time_s, void (*callback)()){
  _doAfterTime = millis()+time_s*1000;
  _aftercallback = callback;
}
void TimeKeeper::_doAfterWait(){
  if(_doAfterTime>0 && millis()>=_doAfterTime){
    _doAfterTime = 0;
    _aftercallback();
  }
}

void TimeKeeper::_upClock(){
#if RTCSUPPORTED
  if(config.isRTCFound()) rtc.getTime(&network.timeinfo);
#else
  if(network.timeinfo.tm_year>100 || network.status == SDREADY) {
    network.timeinfo.tm_sec++;
    mktime(&network.timeinfo);
  }
#endif
  if(display.ready()&&ibtlimit::allowRedraw()) display.putRequest(CLOCK);
}

void TimeKeeper::_upScreensaver(){
#ifndef DSP_LCD
  if(!display.ready()) return;
  if(config.store.screensaverEnabled && display.mode()==PLAYER && !player.isRunning()){
    config.screensaverTicks++;
    if(config.screensaverTicks > config.store.screensaverTimeout+SCREENSAVERSTARTUPDELAY){
      if(config.store.screensaverBlank){
        display.putRequest(NEWMODE, SCREENBLANK);
      }else{
        display.putRequest(NEWMODE, SCREENSAVER);
      }
      config.screensaverTicks=SCREENSAVERSTARTUPDELAY;
    }
  }
  if(config.store.screensaverPlayingEnabled && display.mode()==PLAYER && player.isRunning()){
    config.screensaverPlayingTicks++;
    if(config.screensaverPlayingTicks > config.store.screensaverPlayingTimeout*60+SCREENSAVERSTARTUPDELAY){
      if(config.store.screensaverPlayingBlank){
        display.putRequest(NEWMODE, SCREENBLANK);
      }else{
        display.putRequest(NEWMODE, SCREENSAVER);
      }
      config.screensaverPlayingTicks=SCREENSAVERSTARTUPDELAY;
    }
  }
#endif
}

void TimeKeeper::_upRSSI(){
  if(network.status == CONNECTED){
    netserver.setRSSI(WiFi.RSSI());
    netserver.requestOnChange(NRSSI, 0);
    if(display.ready()) display.putRequest(DSPRSSI, netserver.getRSSI());
  }
#ifdef USE_SD
  if(display.mode()!=SDCHANGE) player.sendCommand({PR_CHECKSD, 0});
#endif
  player.sendCommand({PR_VUTONUS, 0});
}

void TimeKeeper::_upSDPos(){
  if(player.isRunning() && config.getMode()==PM_SDCARD) netserver.requestOnChange(SDPOS, 0);
}

void TimeKeeper::timeTask(){
  static uint8_t tsFailCnt = 0;
  if(getLocalTime(&network.timeinfo,1000)){
    tsFailCnt = 0;
    forceTimeSync = false;
    mktime(&network.timeinfo);
    display.putRequest(CLOCK, 1);
    network.requestTimeSync(true);
    #if RTCSUPPORTED
      if (config.isRTCFound()) rtc.setTime(&network.timeinfo);
    #endif
  }else{
    if(tsFailCnt<4){
      forceTimeSync = true;
      tsFailCnt++;
    }else{
      forceTimeSync = false;
      tsFailCnt=0;
    }
  }
}
void TimeKeeper::weatherTask(){
  forceWeather = false;
  if(!weatherBuf || strlen(config.store.weatherkey)==0 || !config.store.showweather) return;
  if(!_getWeather())forceWeather=true;
}

// One bounded response, read by the low-priority sync worker. The permit is
// held through client.stop(), buffer cleanup and task completion.
bool _getWeather() {
#if (DSP_MODEL!=DSP_DUMMY || defined(USE_NEXTION)) && !defined(HIDE_WEATHER)
  const char* host="api.openweathermap.org";
  WiFiClient client;
  if(!client.connect(host,80,3000))return false;
  char request[320];
  int n=snprintf(request,sizeof(request),"GET /data/2.5/weather?lat=%s&lon=%s&units=%s&lang=%s&appid=%s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",config.store.weatherlat,config.store.weatherlon,LANG::weatherUnits,LANG::weatherLang,config.store.weatherkey,host);
  if(n<=0||n>=int(sizeof(request))||client.write((const uint8_t*)request,n)!=size_t(n)){client.stop();return false;}
  uint32_t caps=ESP.getPsramSize()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  char* response=(char*)heap_caps_malloc(4097,caps);
  if(!response){client.stop();return false;}
  size_t used=0;uint32_t start=millis();
  while(used<4096&&uint32_t(millis()-start)<10000){
    if(client.available()){int r=client.read((uint8_t*)response+used,min(size_t(client.available()),4096-used));if(r>0)used+=r;}
    else if(!client.connected())break;
    else vTaskDelay(pdMS_TO_TICKS(5));
  }
  bool complete=!client.connected()&&!client.available()&&used<4096;
  client.stop();response[used]=0;
  char* line=strstr(response,"\r\n\r\n");
  if(!complete||!strstr(response," 200 ")||!line){free(response);return false;}
  line+=4;
  char desc[120]={},icon[5]={};float tempf=0,tempfl=0,wind_speed=0;int hum=0,press=0,wind_deg=0;bool result=true;char* cursor;
  cursor=strstr(line,"\"description\":\"");if(!cursor||sscanf(cursor,"\"description\":\"%119[^\"]",desc)!=1)result=false;
  cursor=strstr(line,"\"icon\":\"");if(!cursor||sscanf(cursor,"\"icon\":\"%4[^\"]",icon)!=1)result=false;
  cursor=strstr(line,"\"temp\":");if(!cursor||sscanf(cursor,"\"temp\":%f",&tempf)!=1)result=false;
  cursor=strstr(line,"\"feels_like\":");if(!cursor||sscanf(cursor,"\"feels_like\":%f",&tempfl)!=1)result=false;
  cursor=strstr(line,"\"pressure\":");if(!cursor||sscanf(cursor,"\"pressure\":%d",&press)!=1)result=false;
  cursor=strstr(line,"\"humidity\":");if(!cursor||sscanf(cursor,"\"humidity\":%d",&hum)!=1)result=false;
  cursor=strstr(line,"\"speed\":");if(!cursor||sscanf(cursor,"\"speed\":%f",&wind_speed)!=1)result=false;
  cursor=strstr(line,"\"deg\":");if(cursor)sscanf(cursor,"\"deg\":%d",&wind_deg);
  free(response);if(!result)return false;
  press=int(press/1.333);int direction=((wind_deg%360+360)%360)/22.5;
  #ifdef WEATHER_FMT_SHORT
  snprintf(timekeeper.weatherBuf,WEATHER_STRING_L,weatherFmt,tempf,press,hum);
  #elif EXT_WEATHER
  snprintf(timekeeper.weatherBuf,WEATHER_STRING_L,LANG::weatherFmt,desc,tempf,tempfl,press,hum,wind_speed,LANG::wind[direction]);
  #else
  snprintf(timekeeper.weatherBuf,WEATHER_STRING_L,LANG::weatherFmt,desc,tempf,press,hum);
  #endif
  display.putRequest(NEWWEATHER);return true;
#else
  return false;
#endif
}

//******************
