#include "../dual/v026_probe.h"
#include "options.h"
#include "config.h"
#include "replace_file.h"
#include "playlist_parse.h"
#include "playlist_index_reader.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "sd_path.h"
#include "../dual/core.h"
#include "display.h"
#include "player.h"
#include "network.h"
#include "netserver.h"
#include "controls.h"
#include "timekeeper.h"
#include "telnet.h"
#include "rtcsupport.h"
#include "../displays/tools/l10n.h"
#ifdef USE_SD
#include "sdmanager.h"
#endif
#ifdef USE_NEXTION
#include "../displays/nextion.h"
#endif
#include <cstddef>
#include <atomic>
#include <Preferences.h>
#include <nvs.h>
#include "user_reset_policy.h"
#include "../dual/core.h"

#if DSP_MODEL==DSP_DUMMY
#define DUMMYDISPLAY
#endif

Config config;

namespace {
std::atomic_flag dataInstallBusy = ATOMIC_FLAG_INIT;
class DataInstallGuard {
public:
  DataInstallGuard(): owned(!dataInstallBusy.test_and_set(std::memory_order_acquire)) {}
  ~DataInstallGuard() { if(owned) dataInstallBusy.clear(std::memory_order_release); }
  explicit operator bool() const { return owned; }
  DataInstallGuard(const DataInstallGuard&) = delete;
  DataInstallGuard& operator=(const DataInstallGuard&) = delete;
private:
  bool owned;
};
}

#if BRIGHTNESS_PIN!=255
namespace {
constexpr uint32_t BACKLIGHT_PWM_HZ = 20000;
constexpr uint8_t BACKLIGHT_PWM_BITS = 8;
bool backlightPwmReady = false;
void backlightPwmWrite(uint8_t duty) {
  if(!backlightPwmReady) {
    backlightPwmReady = ledcAttach(BRIGHTNESS_PIN, BACKLIGHT_PWM_HZ, BACKLIGHT_PWM_BITS);
    Serial.printf("[BACKLIGHT] GPIO=%d LEDC=%s freq=%luHz bits=%u\n", BRIGHTNESS_PIN,
                  backlightPwmReady?"OK":"FAIL", (unsigned long)BACKLIGHT_PWM_HZ,
                  (unsigned)BACKLIGHT_PWM_BITS);
  }
  if(backlightPwmReady) ledcWrite(BRIGHTNESS_PIN, duty);
  else digitalWrite(BRIGHTNESS_PIN, duty?HIGH:LOW);
}
}
#endif

void u8fix(char *src){
  if(!src || !*src) return;
  char last = src[strlen(src)-1]; 
  if ((uint8_t)last >= 0xC2) src[strlen(src)-1]='\0';
}

bool Config::_isFSempty() {
  // The WebUI is embedded in firmware. Only the station playlist is needed
  // before building its index; missing legacy /www assets are not a fault.
  return !SPIFFS.exists(PLAYLIST_PATH);
}

void Config::init() {
  EEPROM.begin(EEPROM_SIZE);
  sdResumePos = 0;
  screensaverTicks = 0;
  screensaverPlayingTicks = 0;
  newConfigMode = 0;
  isScreensaver = false;
  memset(tmpBuf, 0, BUFLEN);
  //bootInfo();
#if RTCSUPPORTED
  _rtcFound = false;
  BOOTLOG("RTC begin(SDA=%d,SCL=%d)", RTC_SDA, RTC_SCL);
  if(rtc.init()){
    BOOTLOG("done");
    _rtcFound = true;
  }else{
    BOOTLOG("[ERROR] - Couldn't find RTC");
  }
#endif
  emptyFS = true;
#if IR_PIN!=255
    irindex=-1;
#endif
#if defined(USE_SD) && (defined(SD_SPIPINS) || SD_HSPI)
  #if !defined(SD_SPIPINS)
    SDSPI.begin();
  #else
    SDSPI.begin(SD_SPIPINS); // SCK, MISO, MOSI
  #endif
#endif
  eepromRead(EEPROM_START, store);
  bootInfo(); // https://github.com/e2002/yoradio/pull/149
  if (store.config_set != 4262) {
    setDefaults();
  }
  if(store.version>CONFIG_VERSION) store.version=1;
  while(store.version!=CONFIG_VERSION) _setupVersion();
  if(strncmp(store.mdnsname,"yoradio-",8)==0 ||
     strncmp(store.mdnsname,"i-bt-radio-v56-",15)==0 ||
     strncmp(store.mdnsname,"i-bt-radio-v57-",15)==0) {
    snprintf(tmpBuf, MDNS_LENGTH, "i-bt-radio-v58-%x", (unsigned int)getChipId());
    saveValue(store.mdnsname, tmpBuf, MDNS_LENGTH);
  }
  BOOTLOG("CONFIG_VERSION\t%d", store.version);
  store.play_mode = store.play_mode & 0b11;
  if(store.play_mode>1) store.play_mode=PM_WEB;
  _initHW();
  if (!SPIFFS.begin(false) /* V026: never format user settings/assets automatically */) {
    Serial.println("##[ERROR]#\tSPIFFS Mount Failed");
    return;
  }
  BOOTLOG("SPIFFS mounted");
  if(!applyPendingUserReset())Serial.println("[USER-RESET] FAIL pending retained; no automatic restart");
  // A power loss between the two rename operations leaves the old data in
  // *.prev. Restore it before loading Wi-Fi or indexing the playlist.
  ibtfile::recover(SPIFFS, PLAYLIST_PATH, PLAYLIST_PREV_PATH);
  ibtfile::recover(SPIFFS, SSIDS_PATH, WIFI_PREV_PATH);
  // A reset after playlist replacement may happen before the index is saved.
  // Rebuild it from the authoritative CSV on every boot.
  if (SPIFFS.exists(PLAYLIST_PATH)) SPIFFS.remove(INDEX_PATH);
  emptyFS = _isFSempty();
  if(emptyFS) BOOTLOG("Station playlist is missing");
  ssidsCount = 0;
  #ifdef USE_SD
  _SDplaylistFS = getMode()==PM_SDCARD?&sdman:(true?&SPIFFS:_SDplaylistFS);
  #else
  _SDplaylistFS = &SPIFFS;
  #endif
  _bootDone=false;
  setTimeConf();
}

void Config::_setupVersion(){
  uint16_t currentVersion = store.version;
  switch(currentVersion){
    case 1:
      saveValue(&store.screensaverEnabled, false);
      saveValue(&store.screensaverTimeout, (uint16_t)20);
      break;
    case 2:
      snprintf(tmpBuf, MDNS_LENGTH, "i-bt-radio-v58-%x", (unsigned int)getChipId());
      saveValue(store.mdnsname, tmpBuf, MDNS_LENGTH);
      saveValue(&store.skipPlaylistUpDown, false);
      break;
    case 3:
      saveValue(&store.screensaverBlank, false);
      saveValue(&store.screensaverPlayingEnabled, false);
      saveValue(&store.screensaverPlayingTimeout, (uint16_t)5);
      saveValue(&store.screensaverPlayingBlank, false);
      break;
    case 4:
      saveValue(&store.abuff, (uint16_t)(VS1053_CS==255?7:10));
      saveValue(&store.telnet, true);
      saveValue(&store.watchdog, true);
      saveValue(&store.timeSyncInterval, (uint16_t)60);    //min
      saveValue(&store.timeSyncIntervalRTC, (uint16_t)24); //hours
      saveValue(&store.weatherSyncInterval, (uint16_t)30); // min
    default:
      break;
  }
  currentVersion++;
  saveValue(&store.version, currentVersion);
}

void Config::changeMode(int newmode){
  if(player.routeSourceChange(newmode))return;
#ifdef USE_SD
  bool pir = player.isRunning();
  if(SDC_CS==255) return;
  if(getMode()==PM_SDCARD) {
    sdResumePos = player.getFilePos();
  }
  if(network.status==SOFT_AP || display.mode()==LOST){
    saveValue(&store.play_mode, static_cast<uint8_t>(PM_SDCARD));
    delay(50);
    ESP.restart();
  }
  if(!sdman.ready && newmode!=PM_WEB) {
    if(!sdman.start()){
      Serial.println("##[ERROR]#\tSD Not Found");
      netserver.requestOnChange(GETPLAYERMODE, 0);
      sdman.stop();
      return;
    }
  }
  if(newmode<0||newmode>MAX_PLAY_MODE){
    store.play_mode++;
    if(getMode() > MAX_PLAY_MODE) store.play_mode=0;
  }else{
    store.play_mode=(playMode_e)newmode;
  }
  saveValue(&store.play_mode, store.play_mode, true, true);
  _SDplaylistFS = getMode()==PM_SDCARD?&sdman:(true?&SPIFFS:_SDplaylistFS);
  if(getMode()==PM_SDCARD){
    if(pir) player.sendCommand({PR_STOP, 0});
    display.putRequest(NEWMODE, SDCHANGE);
    #ifdef NETSERVER_LOOP1
    while(display.mode()!=SDCHANGE)
      delay(10);
    #endif
  }
  if(getMode()==PM_WEB) {
    #if IBT_NO_PSRAM
    if(network.status==SDREADY){network.enterOnlinePlayer();Serial.println("[SOURCE-RESTORE] WEB=QUEUED reboot=0");}
#else
    if(network.status==SDREADY) ESP.restart();
#endif
    sdman.stop();
  }
  if(!_bootDone) return;
  initPlaylistMode();
  if (pir) player.sendCommand({PR_PLAY, getMode()==PM_WEB?store.lastStation:store.lastSdStation});
  netserver.resetQueue();
  //netserver.requestOnChange(GETPLAYERMODE, 0);
  netserver.requestOnChange(GETINDEX, 0);
  //netserver.requestOnChange(GETMODE, 0);
 // netserver.requestOnChange(CHANGEMODE, 0);
  display.resetQueue();
  display.putRequest(NEWMODE, PLAYER);
  display.putRequest(NEWSTATION);
#endif
}

void Config::initSDPlaylist() {
#ifdef USE_SD
  // Rebuild at each mount so cards swapped while in WEB mode are never stale.
  if(!sdman.indexSDPlaylist())return;
  uint16_t count=playlistLength();
  if(count && (store.lastSdStation<1 || store.lastSdStation>count)){
    lastStation(1);
    sdResumePos=0;
  }
#endif //#ifdef USE_SD
}

bool Config::spiffsCleanup(){
  bool ret = (SPIFFS.exists(PLAYLIST_SD_PATH)) || (SPIFFS.exists(INDEX_SD_PATH)) || (SPIFFS.exists(INDEX_PATH));
  if(SPIFFS.exists(PLAYLIST_SD_PATH)) SPIFFS.remove(PLAYLIST_SD_PATH);
  if(SPIFFS.exists(INDEX_SD_PATH)) SPIFFS.remove(INDEX_SD_PATH);
  if(SPIFFS.exists(INDEX_PATH)) SPIFFS.remove(INDEX_PATH);
  return ret;
}

void Config::waitConnection(){
#if I2S_DOUT==255
  return;
#endif
  while(!player.connproc) vTaskDelay(50);
  vTaskDelay(500);
}

char * Config::ipToStr(IPAddress ip){
  snprintf(ipBuf, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return ipBuf;
}
bool Config::prepareForPlaying(uint16_t stationId, bool persistSdSelection,bool persistLifecycle){
  setDspOn(1,persistLifecycle);
  vuThreshold = 0;
  screensaverTicks=SCREENSAVERSTARTUPDELAY;
  screensaverPlayingTicks=SCREENSAVERSTARTUPDELAY;
  if(getMode()!=PM_SDCARD) {
    display.putRequest(PSTOP);
  }
  
  if(stationId==65001){
    strlcpy(station.name,"V026 MP3 stream test",sizeof(station.name));
    strlcpy(station.url,V026_STREAM_MAIN,sizeof(station.url));
    station.title[0]=0;station.bitrate=128;station.ovol=0;
    Serial.printf("[STREAM-TEST] url=%s\n",station.url);
  }else if(!loadStation(stationId,persistSdSelection,persistLifecycle)) return false;
  setTitle(getMode()==PM_WEB?LANG::const_PlConnect:"[next track]");
  station.bitrate=0;
  setBitrateFormat(BF_UNKNOWN);
  display.putRequest(DBITRATE);
  display.putRequest(NEWSTATION);
  display.putRequest(NEWMODE, PLAYER);
  netserver.requestOnChange(STATION, 0);
  netserver.requestOnChange(MODE, 0);
  if(persistLifecycle && store.smartstart!=2)
    setSmartStart(0);
  return true;
}
void Config::configPostPlaying(uint16_t stationId,bool persistLifecycle,bool persistSdSelection){
  if(getMode()==PM_SDCARD) {
    sdResumePos = 0;
    if(persistLifecycle&&persistSdSelection)saveValue(&store.lastSdStation, stationId);
    else store.lastSdStation=stationId;
  }
  if(persistLifecycle && store.smartstart!=2) setSmartStart(1);
  netserver.requestOnChange(MODE, 0);
  //display.putRequest(NEWMODE, PLAYER);
  display.putRequest(PSTART);
}
void Config::initPlaylistMode(){
  uint16_t _lastStation = 0;
  // The SD index does not exist until the card has mounted and indexing ends.
  uint16_t cs = getMode()==PM_WEB?playlistLength():0;
  #ifdef USE_SD
    if(getMode()==PM_SDCARD){
      if(!sdman.start()){
        store.play_mode=PM_WEB;
        _SDplaylistFS=&SPIFFS;
        Serial.println("[SD-AUDIO] FAIL mount; returning to WEB");
        cs=playlistLength();
        _lastStation = store.lastStation;
      }else{
        if(_bootDone) Serial.println("SD Mounted"); else BOOTLOG("SD Mounted");
          if(_bootDone) Serial.println("Waiting for SD card indexing..."); else BOOTLOG("Waiting for SD card indexing...");
          if(!sdman.catalogReady.load()){
            sdman.clearIndexCancel();
            initSDPlaylist();
          }
          cs=playlistLength();
          if(_bootDone) Serial.println("done"); else BOOTLOG("done");
          if(!cs){
            Serial.println("[SD-AUDIO] FAIL empty catalog; returning to WEB");
            sdman.stop();
            store.play_mode=PM_WEB;
            _SDplaylistFS=&SPIFFS;
            cs=playlistLength();
            _lastStation=store.lastStation;
          }else{
            _lastStation = store.lastSdStation;
            if(_lastStation>cs)_lastStation=1;
            if(_lastStation==0)_lastStation=_randomStation(cs);
          }
      }
    }else{
      Serial.println("done");
      _lastStation = store.lastStation;
    }
  #else //ifdef USE_SD
    store.play_mode=PM_WEB;
    _lastStation = store.lastStation;
  #endif
  if(getMode()==PM_WEB && !emptyFS) initPlaylist();
  log_i("%d" ,_lastStation);
  if (_lastStation == 0 && cs > 0) {
    _lastStation = getMode()==PM_WEB?1:_randomStation(cs);
  }
  lastStation(_lastStation);
  saveValue(&store.play_mode, store.play_mode, true, true);
  _bootDone = true;
  loadStation(_lastStation);
}

void Config::_initHW(){
  loadTheme();
  #if IR_PIN!=255
  eepromRead(EEPROM_START_IR, ircodes);
  if(ircodes.ir_set!=4224){
    ircodes.ir_set=4224;
    memset(ircodes.irVals, 0, sizeof(ircodes.irVals));
  }
  #endif
  #if BRIGHTNESS_PIN!=255
    pinMode(BRIGHTNESS_PIN, OUTPUT);
    setBrightness(false);
  #endif
}

uint16_t Config::color565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void Config::loadTheme(){
  theme.background    = color565(COLOR_BACKGROUND);
  theme.meta          = color565(COLOR_STATION_NAME);
  theme.metabg        = color565(COLOR_STATION_BG);
  theme.metafill      = color565(COLOR_STATION_FILL);
  theme.title1        = color565(COLOR_SNG_TITLE_1);
  theme.title2        = color565(COLOR_SNG_TITLE_2);
  theme.digit         = color565(COLOR_DIGITS);
  theme.div           = color565(COLOR_DIVIDER);
  theme.weather       = color565(COLOR_WEATHER);
  theme.vumax         = color565(COLOR_VU_MAX);
  theme.vumin         = color565(COLOR_VU_MIN);
  theme.clock         = color565(COLOR_CLOCK);
  theme.clockbg       = color565(COLOR_CLOCK_BG);
  theme.seconds       = color565(COLOR_SECONDS);
  theme.dow           = color565(COLOR_DAY_OF_W);
  theme.date          = color565(COLOR_DATE);
  theme.heap          = color565(COLOR_HEAP);
  theme.buffer        = color565(COLOR_BUFFER);
  theme.ip            = color565(COLOR_IP);
  theme.vol           = color565(COLOR_VOLUME_VALUE);
  theme.rssi          = color565(COLOR_RSSI);
  theme.bitrate       = color565(COLOR_BITRATE);
  theme.volbarout     = color565(COLOR_VOLBAR_OUT);
  theme.volbarin      = color565(COLOR_VOLBAR_IN);
  theme.plcurrent     = color565(COLOR_PL_CURRENT);
  theme.plcurrentbg   = color565(COLOR_PL_CURRENT_BG);
  theme.plcurrentfill = color565(COLOR_PL_CURRENT_FILL);
  theme.playlist[0]   = color565(COLOR_PLAYLIST_0);
  theme.playlist[1]   = color565(COLOR_PLAYLIST_1);
  theme.playlist[2]   = color565(COLOR_PLAYLIST_2);
  theme.playlist[3]   = color565(COLOR_PLAYLIST_3);
  theme.playlist[4]   = color565(COLOR_PLAYLIST_4);
  #include "../displays/tools/tftinverttitle.h"
}

template <class T> int Config::eepromWrite(int ee, const T& value) {
  const uint8_t* p = (const uint8_t*)(const void*)&value;
  int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
  EEPROM.commit();
  return i;
}

template <class T> int Config::eepromRead(int ee, T& value) {
  uint8_t* p = (uint8_t*)(void*)&value;
  int i;;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
  return i;
}

// Called only by the existing web service owner, with audio already stopped.
namespace {
struct ResetConnections {
  uint8_t btquick=1,dnsmanual=0,dnsFields=0;
  uint8_t primary[4]{},secondary[4]{};
  char mdns[MDNS_LENGTH]{};
};
}
bool Config::queueUserReset(){
  ResetConnections keep;
  strlcpy(keep.mdns,store.mdnsname,sizeof(keep.mdns));
  Preferences original;
  if(original.begin("yo-dual-v1",true)){
    keep.btquick=original.getUChar("btquick",1);keep.dnsmanual=original.getBool("dnsmanual",false)?1:0;
    if(original.getBytesLength("dnsprimary")==4&&original.getBytes("dnsprimary",keep.primary,4)==4)keep.dnsFields|=1;
    if(original.getBytesLength("dnssecond")==4&&original.getBytes("dnssecond",keep.secondary,4)==4)keep.dnsFields|=2;
    original.end();
  }
  Preferences job;if(!job.begin("ibt-maint",false))return false;
  ResetConnections verify;
  bool ok=job.putUChar("ssid",store.lastSSID)==1 && job.putBytes("connections",&keep,sizeof(keep))==sizeof(keep)
    &&job.getBytes("connections",&verify,sizeof(verify))==sizeof(verify)&&memcmp(&verify,&keep,sizeof(keep))==0;
  if(ok)ok=job.putBool("reset",true)==1 && job.getBool("reset",false);
  job.end();return ok;
}
bool Config::applyPendingUserReset(){
  Preferences job;if(!job.begin("ibt-maint",true))return true;
  bool pending=job.getBool("reset",false);uint8_t ssid=job.getUChar("ssid",store.lastSSID);
  ResetConnections keep;bool saved=job.getBytesLength("connections")==sizeof(keep)&&job.getBytes("connections",&keep,sizeof(keep))==sizeof(keep);
  job.end();if(!pending)return true;if(!saved)return false;
  // Boot-time only, before dual/sensor/weather workers load or can re-save state.
  struct Storage {
    ResetConnections keep;uint8_t ssid;
    bool (*defaults)(uint8_t,const char*);
    bool clearNamespace(const char* name){
      nvs_handle_t h;esp_err_t r=nvs_open(name,NVS_READWRITE,&h);if(r!=ESP_OK)return false;
      r=nvs_erase_all(h);if(r==ESP_OK)r=nvs_commit(h);nvs_close(h);return r==ESP_OK;
    }
    bool restoreConnectionOptions(){Preferences p;if(!p.begin("yo-dual-v1",false))return false;
      bool ok=p.putUChar("btquick",keep.btquick)==1&&p.putBool("dnsmanual",keep.dnsmanual!=0)==1;
      if(keep.dnsFields&1)ok=p.putBytes("dnsprimary",keep.primary,4)==4&&ok;
      if(keep.dnsFields&2)ok=p.putBytes("dnssecond",keep.secondary,4)==4&&ok;
      p.end();return ok;}
    bool removeFile(const char* path){return !SPIFFS.exists(path)||SPIFFS.remove(path);}
    bool resetSettings(){return defaults(ssid,keep.mdns);}
  } storage{keep,ssid,[](uint8_t keepSsid,const char* mdns){
    config.setDefaults();config.store.lastSSID=keepSsid;strlcpy(config.store.mdnsname,mdns,MDNS_LENGTH);
    for(unsigned i=0;i<EEPROM_START;i++)EEPROM.write(i,0); // learned IR, never NVS BT bonds
    EEPROM.put(EEPROM_START,config.store);return EEPROM.commit();
  }};
  if(!ibtreset::apply(storage))return false;
  if(!job.begin("ibt-maint",false))return false;
  bool ok=job.remove("reset");job.end();
  Serial.printf("[USER-RESET] %s wifi=preserved classic_bt=preserved SD_media=preserved\n",ok?"PASS":"FAIL");return ok;
}

void Config::reset(){
  setDefaults();
  delay(500);
  ESP.restart();
}
void Config::enableScreensaver(bool val){
  saveValue(&store.screensaverEnabled, val);
#ifndef DSP_LCD
  display.putRequest(NEWMODE, PLAYER);
#endif
}
void Config::setScreensaverTimeout(uint16_t val){
  val=constrain(val,5,65520);
  saveValue(&store.screensaverTimeout, val);
#ifndef DSP_LCD
  display.putRequest(NEWMODE, PLAYER);
#endif
}
void Config::setScreensaverBlank(bool val){
  saveValue(&store.screensaverBlank, val);
#ifndef DSP_LCD
  display.putRequest(NEWMODE, PLAYER);
#endif
}
void Config::setScreensaverPlayingEnabled(bool val){
  saveValue(&store.screensaverPlayingEnabled, val);
#ifndef DSP_LCD
  display.putRequest(NEWMODE, PLAYER);
#endif
}
void Config::setScreensaverPlayingTimeout(uint16_t val){
  val=constrain(val,1,1080);
  config.saveValue(&config.store.screensaverPlayingTimeout, val);
#ifndef DSP_LCD
  display.putRequest(NEWMODE, PLAYER);
#endif
}
void Config::setScreensaverPlayingBlank(bool val){
  saveValue(&store.screensaverPlayingBlank, val);
#ifndef DSP_LCD
  display.putRequest(NEWMODE, PLAYER);
#endif
}
void Config::setSntpOne(const char *val){
  bool tzdone = false;
  if (strlen(val) > 0 && strlen(store.sntp2) > 0) {
    dual::clockConfigure();
    tzdone = true;
  } else if (strlen(val) > 0) {
    dual::clockConfigure();
    tzdone = true;
  }
  if (tzdone) {
    timekeeper.forceTimeSync = true;
    saveValue(config.store.sntp1, val, 35);
  }
}
void Config::setShowweather(bool val){
  config.saveValue(&config.store.showweather, val);
  timekeeper.forceWeather = true;
  display.putRequest(SHOWWEATHER);
}
void Config::setWeatherKey(const char *val){
  saveValue(store.weatherkey, val, WEATHERKEY_LENGTH);
  display.putRequest(NEWMODE, CLEAR);
  display.putRequest(NEWMODE, PLAYER);
}
void Config::setSDpos(uint32_t val){
  if (getMode()==PM_SDCARD){
    sdResumePos = 0;
    if(!player.isRunning()){
      player.setResumeFilePos(val-player.sd_min);
      player.sendCommand({PR_PLAY, config.store.lastSdStation});
    }else{
      player.setFilePos(val-player.sd_min);
    }
  }
}
#if IR_PIN!=255
void Config::setIrBtn(int val){
  irindex = val;
  netserver.irRecordEnable = (irindex >= 0);
  irchck = 0;
  netserver.irValsToWs();
  if (irindex < 0) saveIR();
}
#endif
void Config::resetSystem(const char *val, uint8_t clientId){
  if (strcmp(val, "system") == 0) {
    saveValue(&store.smartstart, (uint8_t)2, false);
    saveValue(&store.audioinfo, false, false);
    saveValue(&store.vumeter, false, false);
    saveValue(&store.softapdelay, (uint8_t)0, false);
    saveValue(&store.abuff, (uint16_t)(VS1053_CS==255?7:10), false);
    saveValue(&store.telnet, true);
    saveValue(&store.watchdog, true);
    snprintf(store.mdnsname, MDNS_LENGTH, "i-bt-radio-v58-%x", (unsigned int)getChipId());
    saveValue(store.mdnsname, store.mdnsname, MDNS_LENGTH, true, true);
    display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
    netserver.requestOnChange(GETSYSTEM, clientId);
    return;
  }
  if (strcmp(val, "screen") == 0) {
    saveValue(&store.flipscreen, false, false);
    display.flip();
    saveValue(&store.invertdisplay, false, false);
    display.invert();
    saveValue(&store.dspon, true, false);
    store.brightness = 100;
    setBrightness(false);
    saveValue(&store.contrast, (uint8_t)55, false);
    display.setContrast();
    saveValue(&store.numplaylist, false);
    saveValue(&store.screensaverEnabled, false);
    saveValue(&store.screensaverTimeout, (uint16_t)20);
    saveValue(&store.screensaverBlank, false);
    saveValue(&store.screensaverPlayingEnabled, false);
    saveValue(&store.screensaverPlayingTimeout, (uint16_t)5);
    saveValue(&store.screensaverPlayingBlank, false);
    display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
    netserver.requestOnChange(GETSCREEN, clientId);
    return;
  }
  if (strcmp(val, "timezone") == 0) {
    saveValue(&store.tzHour, (int8_t)3, false);
    saveValue(&store.tzMin, (int8_t)0, false);
    saveValue(store.sntp1, "pool.ntp.org", 35, false);
    saveValue(store.sntp2, "0.ru.pool.ntp.org", 35);
    saveValue(&store.timeSyncInterval, (uint16_t)60);
    saveValue(&store.timeSyncIntervalRTC, (uint16_t)24);
    dual::clockConfigure();
    timekeeper.forceTimeSync = true;
    netserver.requestOnChange(GETTIMEZONE, clientId);
    return;
  }
  if (strcmp(val, "weather") == 0) {
    saveValue(&store.showweather, false, false);
    saveValue(store.weatherlat, "55.7512", 10, false);
    saveValue(store.weatherlon, "37.6184", 10, false);
    saveValue(store.weatherkey, "", WEATHERKEY_LENGTH);
    saveValue(&store.weatherSyncInterval, (uint16_t)30);
    //network.trueWeather=false;
    display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
    netserver.requestOnChange(GETWEATHER, clientId);
    return;
  }
  if (strcmp(val, "controls") == 0) {
    saveValue(&store.volsteps, (uint8_t)1, false);
    saveValue(&store.fliptouch, false, false);
    saveValue(&store.dbgtouch, false, false);
    saveValue(&store.skipPlaylistUpDown, false);
    setEncAcceleration(200);
    setIRTolerance(40);
    netserver.requestOnChange(GETCONTROLS, clientId);
    return;
  }
  if (strcmp(val, "1") == 0) {
    config.reset();
    return;
  }
}



void Config::setDefaults() {
  store.config_set = 4262;
  store.version = CONFIG_VERSION;
  store.volume = 12;
  store.balance = 0;
  store.trebble = 0;
  store.middle = 0;
  store.bass = 0;
  store.lastStation = 0;
  store.countStation = 0;
  store.lastSSID = 0;
  store.audioinfo = false;
  store.smartstart = 2;
  store.tzHour = 3;
  store.tzMin = 0;
  store.timezoneOffset = 0;

  store.vumeter=false;
  store.softapdelay=0;
  store.flipscreen=false;
  store.invertdisplay=false;
  store.numplaylist=false;
  store.fliptouch=false;
  store.dbgtouch=false;
  store.dspon=true;
  store.brightness=100;
  store.contrast=55;
  strlcpy(store.sntp1,"pool.ntp.org", 35);
  strlcpy(store.sntp2,"1.ru.pool.ntp.org", 35);
  store.showweather=false;
  strlcpy(store.weatherlat,"55.7512", 10);
  strlcpy(store.weatherlon,"37.6184", 10);
  strlcpy(store.weatherkey,"", WEATHERKEY_LENGTH);
  store._reserved = 0;
  store.lastSdStation = 0;
  store.sdsnuffle = false;
  store.volsteps = 1;
  store.encacc = 200;
  store.play_mode = 0;
  store.irtlp = 35;
  store.btnpullup = true;
  store.btnlongpress = 200;
  store.btnclickticks = 300;
  store.btnpressticks = 500;
  store.encpullup = false;
  store.enchalf = false;
  store.enc2pullup = false;
  store.enc2half = false;
  store.forcemono = false;
  store.i2sinternal = false;
  store.rotate90 = false;
  store.screensaverEnabled = false;
  store.screensaverTimeout = 20;
  store.screensaverBlank = false;
  snprintf(store.mdnsname, MDNS_LENGTH, "i-bt-radio-v58-%x", (unsigned int)getChipId());
  store.skipPlaylistUpDown = false;
  store.screensaverPlayingEnabled = false;
  store.screensaverPlayingTimeout = 5;
  store.screensaverPlayingBlank = false;
  store.abuff = VS1053_CS==255?7:10;
  store.telnet = true;
  store.watchdog = true;
  store.timeSyncInterval = 60;    //min
  store.timeSyncIntervalRTC = 24; //hour
  store.weatherSyncInterval = 30; //min
  eepromWrite(EEPROM_START, store);
}

void Config::setTimezone(int8_t tzh, int8_t tzm) {
  saveValue(&store.tzHour, tzh, false);
  saveValue(&store.tzMin, tzm);
}

void Config::setTimezoneOffset(uint16_t tzo) {
  saveValue(&store.timezoneOffset, tzo);
}

uint16_t Config::getTimezoneOffset() {
  return 0; // TODO
}

void Config::setSnuffle(bool sn){
  saveValue(&store.sdsnuffle, sn);
  if(store.sdsnuffle) player.next();
}

#if IR_PIN!=255
void Config::saveIR(){
  eepromWrite(EEPROM_START_IR, ircodes);
}
#endif

void Config::saveVolume(){
  saveValue(&store.volume, store.volume, true, true);
}

uint8_t Config::setVolume(uint8_t val) {
  store.volume = val;
  display.putRequest(DRAWVOL);
  netserver.requestOnChange(VOLUME, 0);
  return store.volume;
}

void Config::setTone(int8_t bass, int8_t middle, int8_t trebble) {
  saveValue(&store.bass, bass, false);
  saveValue(&store.middle, middle, false);
  saveValue(&store.trebble, trebble);
  player.setTone(store.bass, store.middle, store.trebble);
  netserver.requestOnChange(EQUALIZER, 0);
}

void Config::setSmartStart(uint8_t ss) {
  saveValue(&store.smartstart, ss);
}

void Config::setBalance(int8_t balance) {
  saveValue(&store.balance, balance);
  player.setBalance(store.balance);
  netserver.requestOnChange(BALANCE, 0);
}

uint8_t Config::setLastStation(uint16_t val) {
  lastStation(val);
  return store.lastStation;
}

uint8_t Config::setCountStation(uint16_t val) {
  saveValue(&store.countStation, val);
  return store.countStation;
}

uint8_t Config::setLastSSID(uint8_t val) {
  saveValue(&store.lastSSID, val);
  return store.lastSSID;
}

void Config::setTitle(const char* title) {
  vuThreshold = 0;
  memset(config.station.title, 0, BUFLEN);
  strlcpy(config.station.title, title, BUFLEN);
  u8fix(config.station.title);
  netserver.requestOnChange(TITLE, 0);
  display.putRequest(NEWTITLE);
}

void Config::setStation(const char* station) {
  memset(config.station.name, 0, BUFLEN);
  strlcpy(config.station.name, station, BUFLEN);
  u8fix(config.station.name);
}

bool Config::indexPlaylist() {
  dual::invalidateStationSearchCache();
  File playlist = SPIFFS.open(PLAYLIST_PATH, "r");
  if (!playlist) return false;
  SPIFFS.remove(INDEX_STAGED_PATH);
  File index = SPIFFS.open(INDEX_STAGED_PATH, "w");
  if (!index) {playlist.close();return false;}
  char line[BUFLEN*3];
  int sOvol;
  uint16_t rows=0;
  bool good=true;
  const uint32_t started=millis();
  // A real blocked tick lets IDLE0 run; yield() alone can keep loopTask ready.
  auto pause=[](){vTaskDelay(1);};
  ibtplaylist::IndexReader<File,decltype(pause)> reader(playlist,pause);
  Serial.println("[PLAYLIST-INDEX] phase=BEGIN chunk_bytes=128");
  while (good) {
    uint32_t pos = reader.position();
    size_t used=0;
    bool overflow=false;
    const auto result=reader.readLine(line,sizeof(line),used,overflow);
    if(result==decltype(reader)::End)break;
    if(result==decltype(reader)::Error){good=false;break;}
    if (overflow || !used || rows>=480 || !parseCSV(line,tmpBuf,tmpBuf2,sOvol) ||
        index.write((const uint8_t*)&pos,sizeof(pos))!=sizeof(pos)) good=false;
    else ++rows;
  }
  index.flush();
  good=good && rows>0 && index.size()==size_t(rows)*sizeof(uint32_t);
  index.close();
  playlist.close();
  Serial.printf("[PLAYLIST-INDEX] phase=VALIDATED good=%u rows=%u bytes=%lu blocks=%lu blocked_ticks=%lu elapsed_ms=%lu\n",
      unsigned(good),unsigned(rows),(unsigned long)reader.position(),
      (unsigned long)reader.readBlocks(),(unsigned long)reader.readBlocks(),
      (unsigned long)(uint32_t(millis()-started)));
  if (!good) {SPIFFS.remove(INDEX_STAGED_PATH);return false;}
  good=ibtfile::replace(SPIFFS, INDEX_STAGED_PATH, INDEX_PATH, INDEX_PREV_PATH);
  if (!good) SPIFFS.remove(INDEX_STAGED_PATH);
  dual::invalidateStationSearchCache();
  return good;
}

void Config::initPlaylist() {
  //store.countStation = 0;
  if (!SPIFFS.exists(INDEX_PATH) && !indexPlaylist()) Serial.println("[PLAYLIST] Index validation failed");

  /*if (SPIFFS.exists(INDEX_PATH)) {
    File index = SPIFFS.open(INDEX_PATH, "r");
    store.countStation = index.size() / 4;
    index.close();
    saveValue(&store.countStation, store.countStation, true, true);
  }*/
}
uint16_t Config::playlistLength(){
  uint16_t out = 0;
  if (SDPLFS()->exists(REAL_INDEX)) {
    File index = SDPLFS()->open(REAL_INDEX, "r");
    out = index.size() / 4;
    index.close();
  }
  return out;
}
bool Config::loadStation(uint16_t ls, bool persistSdSelection,bool persistLifecycle) {
  int sOvol;
  uint16_t cs = playlistLength();
  if (cs == 0 || ls == 0) {
    memset(station.url, 0, BUFLEN);
    memset(station.name, 0, BUFLEN);
    strncpy(station.name, "I-BT-Radio V029", BUFLEN);
    station.ovol = 0;
    return false;
  }
  if (ls > cs) {
    ls = 1;
  }
  File playlist = SDPLFS()->open(REAL_PLAYL, "r");
  File index = SDPLFS()->open(REAL_INDEX, "r");
  if(!playlist || !index || !index.seek((ls - 1) * 4, SeekSet)){
    playlist.close();index.close();return false;
  }
  uint32_t pos;
  if(index.readBytes((char *) &pos, 4)!=4 || pos>=playlist.size()){
    index.close();playlist.close();return false;
  }
  index.close();
  playlist.seek(pos, SeekSet);
  String ibtStationLine=playlist.readStringUntil('\n');
  artworkUrl[0]=0;
  if(getMode()==PM_SDCARD){
    const char *name=nullptr,*path=nullptr;size_t nn=0,pn=0;
    bool valid=ibtsd::record(ibtStationLine.c_str(),name,nn,path,pn)&&sdman.selectPath(path,pn);
    if(!valid){sdman.selectPath(nullptr,0);playlist.close();return false;}
    ibtsd::displayCopy(station.name,sizeof(station.name),name,nn);
    ibtsd::displayCopy(station.url,sizeof(station.url),path,pn);
    station.ovol=0;
    if(persistSdSelection)setLastStation(ls);else store.lastSdStation=ls;
    playlist.close();return true;
  }
  if (parseCSV(ibtStationLine.c_str(), tmpBuf, tmpBuf2, sOvol)) {
    memset(station.url, 0, BUFLEN);
    memset(station.name, 0, BUFLEN);
    strncpy(station.name, tmpBuf, BUFLEN);
    strncpy(station.url, tmpBuf2, BUFLEN);
    station.ovol = sOvol;
    if(getMode()==PM_WEB){
      const char *a=ibtStationLine.c_str();
      const char *t1=strchr(a,'\t');
      const char *t2=t1?strchr(t1+1,'\t'):nullptr;
      const char *t3=t2?strchr(t2+1,'\t'):nullptr;
      if(t3 && t3[1]){
        const char *v=t3+1;
        size_t n=strcspn(v,"\r\n\t");
        if(n>=sizeof(artworkUrl))n=sizeof(artworkUrl)-1;
        memcpy(artworkUrl,v,n);artworkUrl[n]=0;
      }
    }
    if(persistLifecycle)setLastStation(ls);else store.lastStation=ls;
  } else {playlist.close();return false;}
  playlist.close();
  return true;
}

char * Config::stationByNum(uint16_t num){
  memset(_stationBuf,0,sizeof(_stationBuf));
  if(num<1 || num>playlistLength())return _stationBuf;
  File playlist = SDPLFS()->open(REAL_PLAYL, "r");
  File index = SDPLFS()->open(REAL_INDEX, "r");
  if(!playlist || !index || !index.seek((num - 1) * 4, SeekSet)){
    playlist.close();index.close();return _stationBuf;
  }
  uint32_t pos;
  if(index.readBytes((char *) &pos, 4)!=4 || pos>=playlist.size()){
    index.close();playlist.close();return _stationBuf;
  }
  index.close();
  playlist.seek(pos, SeekSet);
  String label=playlist.readStringUntil('\t');
  ibtsd::displayCopy(_stationBuf,sizeof(_stationBuf),label.c_str(),label.length());
  playlist.close();
  return _stationBuf;
}

void Config::escapeQuotes(const char* input, char* output, size_t maxLen) {
  size_t j = 0;
  for (size_t i = 0; input[i] != '\0' && j < maxLen - 1; ++i) {
    if (input[i] == '"' && j < maxLen - 2) {
      output[j++] = '\\';
      output[j++] = '"';
    } else {
      output[j++] = input[i];
    }
  }
  output[j] = '\0';
}

bool Config::parseCSV(const char* line, char* name, char* url, int &ovol) {
  return ibtplaylist::csv(line,name,url,BUFLEN,ovol);
}

bool Config::parseJSON(const char* line, char* name, char* url, int &ovol) {
  return ibtplaylist::json(line,name,url,BUFLEN,ovol);
}

bool Config::parseWsCommand(const char* line, char* cmd, char* val, uint8_t cSize) {
  char *tmpe;
  tmpe = strstr(line, "=");
  if (tmpe == NULL) return false;
  memset(cmd, 0, cSize);
  strlcpy(cmd, line, tmpe - line + 1);
  //if (strlen(tmpe + 1) == 0) return false;
  memset(val, 0, cSize);
  strlcpy(val, tmpe + 1, strlen(line) - strlen(cmd) + 1);
  return true;
}

bool Config::parseSsid(const char* line, char* ssid, char* pass) {
  if (!line || !ssid || !pass) return false;
  const char *tmpe = strchr(line, '\t');
  if (!tmpe) return false;
  size_t pos = size_t(tmpe - line);
  size_t total = strlen(line);
  while (total && (line[total-1]=='\r' || line[total-1]=='\n')) total--;
  if (pos == 0 || pos >= WIFI_SSID_CAP || total <= pos || total - pos - 1 >= WIFI_PASS_CAP) return false;
  memset(ssid, 0, WIFI_SSID_CAP);
  memcpy(ssid, line, pos);
  memset(pass, 0, WIFI_PASS_CAP);
  memcpy(pass, line + pos + 1, total - pos - 1);
  return true;
}

bool Config::saveWifiFromNextion(const char* post, bool reboot){
  if (!post || player.isRunning() || player.status()!=STOPPED || getMode()!=PM_WEB) return false;
  constexpr size_t limit=5*(WIFI_SSID_CAP+WIFI_PASS_CAP+2);
  size_t length=0;
  while(length<=limit && post[length]) ++length;
  if(!length || length>limit) return false;
  SPIFFS.remove(WIFI_LEGACY_STAGE_PATH);
  File file=SPIFFS.open(WIFI_LEGACY_STAGE_PATH,"w");
  bool good=file && file.write((const uint8_t*)post,length)==length;
  if(file){file.flush();good=good && file.size()==length;file.close();}
  if(!good){SPIFFS.remove(WIFI_LEGACY_STAGE_PATH);return false;}
  good=saveWifi(WIFI_LEGACY_STAGE_PATH,reboot);
  if(!good)SPIFFS.remove(WIFI_LEGACY_STAGE_PATH);
  return good;
}

bool Config::replaceDataFile(const char* staged, const char* target, const char* previous) {
  return ibtfile::replace(SPIFFS, staged, target, previous);
}

bool Config::installPlaylist(const char* staged) {
  DataInstallGuard guard;
  if (!guard) return false; // Concurrent WebUI and SD restores must not interleave replacements.
  const bool hadPlaylist=SPIFFS.exists(PLAYLIST_PATH);
  if (!ibtfile::replaceKeepPrevious(SPIFFS, staged, PLAYLIST_PATH, PLAYLIST_PREV_PATH)) return false;
  if (indexPlaylist()) {
    if (SPIFFS.exists(PLAYLIST_PREV_PATH)) SPIFFS.remove(PLAYLIST_PREV_PATH);
    return true;
  }
  // Restore the old playlist; indexPlaylist left its existing index intact.
  bool restored=SPIFFS.remove(PLAYLIST_PATH);
  if (SPIFFS.exists(PLAYLIST_PREV_PATH))
    restored=restored && SPIFFS.rename(PLAYLIST_PREV_PATH, PLAYLIST_PATH);
  if (!restored || !hadPlaylist) {
    SPIFFS.remove(INDEX_PATH);
    if (!restored) Serial.println("[PLAYLIST] Rollback failed; index disabled");
  }
  return false;
}

bool Config::saveWifi(const char* staged, bool reboot) {
  if (player.isRunning() || player.status()!=STOPPED || getMode()!=PM_WEB) return false;
  if (!staged || !*staged) return false;
  DataInstallGuard guard;
  if (!guard) return false;
  File file = SPIFFS.open(staged, "r");
  if (!file || !file.size() || file.size() > 5 * (WIFI_SSID_CAP + WIFI_PASS_CAP + 2)) return false;
  char line[WIFI_SSID_CAP + WIFI_PASS_CAP + 3];
  char ssid[WIFI_SSID_CAP], password[WIFI_PASS_CAP];
  unsigned count = 0;
  while (file.available()) {
    size_t n = 0; bool tooLong = false;
    while (file.available()) {
      int ch = file.read();
      if (ch == '\n') break;
      if (n < sizeof(line) - 1) line[n++] = char(ch); else tooLong = true;
    }
    line[n] = 0;
    if (tooLong || !parseSsid(line, ssid, password) || ++count > 5) {file.close();return false;}
  }
  file.close();
  if (!count || !replaceDataFile(staged, SSIDS_PATH, WIFI_PREV_PATH)) return false;
  if(reboot)ESP.restart();
  return true;
}

bool Config::rememberVerifiedWifi(const char* ssid,const char* password,uint8_t* savedSlot){
  if(!ssid||!password)return false;
  const size_t sl=strlen(ssid),pl=strlen(password);
  if(!sl||sl>=WIFI_SSID_CAP||pl>=WIFI_PASS_CAP)return false;
  if(strpbrk(ssid,"\t\r\n")||strpbrk(password,"\t\r\n"))return false;
  DataInstallGuard guard;
  if(!guard)return false;
  unsigned index=ssidsCount;
  for(unsigned i=0;i<ssidsCount;i++)if(strcmp(ssids[i].ssid,ssid)==0){index=i;break;}
  if(index==ssidsCount&&ssidsCount>=5)return false; // Never silently evict another network.
  if(index<ssidsCount&&strcmp(ssids[index].password,password)==0){
    if(savedSlot)*savedSlot=index+1;
    return true; // Do not rewrite flash for an unchanged saved network.
  }
  constexpr const char* stage="/data/wifi.setup";
  SPIFFS.remove(stage);
  File file=SPIFFS.open(stage,"w");
  if(!file)return false;
  size_t expected=0,written=0;
  const unsigned count=ssidsCount+(index==ssidsCount?1:0);
  for(unsigned i=0;i<count;i++){
    const char* name=i==index?ssid:ssids[i].ssid;
    const char* pass=i==index?password:ssids[i].password;
    expected+=strlen(name)+strlen(pass)+2;
    written+=file.print(name);written+=file.print('\t');
    written+=file.print(pass);written+=file.print('\n');
  }
  file.flush();
  const bool complete=written==expected&&file.size()==expected;
  file.close();
  if(!complete){SPIFFS.remove(stage);return false;}
  const bool hadPrevious=SPIFFS.exists(SSIDS_PATH);
  if(!ibtfile::replaceKeepPrevious(SPIFFS,stage,SSIDS_PATH,WIFI_PREV_PATH)){
    SPIFFS.remove(stage);return false;
  }
  if(!initNetwork()||ssidsCount!=count||strcmp(ssids[index].ssid,ssid)!=0){
    SPIFFS.remove(SSIDS_PATH);
    if(hadPrevious)SPIFFS.rename(WIFI_PREV_PATH,SSIDS_PATH);
    initNetwork();
    return false;
  }
  if(hadPrevious)SPIFFS.remove(WIFI_PREV_PATH);
  if(savedSlot)*savedSlot=index+1;
  return true;
}

void Config::setTimeConf(){
#if IBT_STREAM_ART_DIAG
  return;
#endif
  dual::clockConfigure();
}

bool Config::initNetwork() {
  File file = SPIFFS.open(SSIDS_PATH, "r");
  if (!file || file.isDirectory()) {
    return false;
  }
  char ssidval[WIFI_SSID_CAP], passval[WIFI_PASS_CAP];
  uint8_t c = 0;
  ssidsCount = 0;
  while (file.available() && c < 5) {
    if (parseSsid(file.readStringUntil('\n').c_str(), ssidval, passval)) {
      strlcpy(ssids[c].ssid, ssidval, WIFI_SSID_CAP);
      strlcpy(ssids[c].password, passval, WIFI_PASS_CAP);
      ssidsCount++;
      c++;
    }
  }
  file.close();
  return true;
}

void Config::setBrightness(bool dosave){
#if BRIGHTNESS_PIN!=255
  if(!store.dspon && dosave) {
    display.wakeup();
  }
  backlightPwmWrite((uint8_t)map(store.brightness, 0, 100, 0, 255));
  if(!store.dspon) store.dspon = true;
  if(dosave){
    saveValue(&store.brightness, store.brightness, false, true);
    saveValue(&store.dspon, store.dspon, true, true);
  }
#endif
#ifdef USE_NEXTION
  nextion.wake();
  char cmd[15];
  snprintf(cmd, 15, "dims=%d", store.brightness);
  nextion.putcmd(cmd);
  if(!store.dspon) store.dspon = true;
  if(dosave){
    saveValue(&store.brightness, store.brightness, false, true);
    saveValue(&store.dspon, store.dspon, true, true);
  }
#endif
}

void Config::setDspOn(bool dspon, bool saveval){
  if(saveval){
    store.dspon = dspon;
    saveValue(&store.dspon, store.dspon, true, true);
  }
#ifdef USE_NEXTION
  if(!dspon) nextion.sleep();
  else nextion.wake();
#endif
  if(!dspon){
#if BRIGHTNESS_PIN!=255
  backlightPwmWrite(0);
#endif
    display.deepsleep();
  }else{
    display.wakeup();
#if BRIGHTNESS_PIN!=255
  backlightPwmWrite((uint8_t)map(store.brightness, 0, 100, 0, 255));
#endif
  }
}

void Config::doSleep(){
  if(BRIGHTNESS_PIN!=255) backlightPwmWrite(0);
  display.deepsleep();
#ifdef USE_NEXTION
  nextion.sleep();
#endif
#if !defined(ARDUINO_ESP32C3_DEV)
  if(WAKE_PIN!=255) esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, LOW);
  esp_sleep_enable_timer_wakeup(config.sleepfor * 60 * 1000000ULL);
  esp_deep_sleep_start();
#endif
}

void Config::doSleepW(){
  if(BRIGHTNESS_PIN!=255) backlightPwmWrite(0);
  display.deepsleep();
#ifdef USE_NEXTION
  nextion.sleep();
#endif
#if !defined(ARDUINO_ESP32C3_DEV)
  if(WAKE_PIN!=255) esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, LOW);
  esp_deep_sleep_start();
#endif
}

void Config::sleepForAfter(uint16_t sf, uint16_t sa){
  sleepfor = sf;
  if(sa > 0) timekeeper.waitAndDo(sa * 60, doSleep);
  else doSleep();
}

void Config::bootInfo() {
  BOOTLOG("************************************************");
  BOOTLOG("*              I-BT-Radio V029              *");
  BOOTLOG("************************************************");
  BOOTLOG("------------------------------------------------");
  BOOTLOG("arduino:\t%d", ARDUINO);
  BOOTLOG("compiler:\t%s", __VERSION__);
  BOOTLOG("esp32core:\t%d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
  uint32_t chipId = 0;
  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  BOOTLOG("chip:\t\tmodel: %s | rev: %d | id: %lu | cores: %d | psram: %lu", ESP.getChipModel(), ESP.getChipRevision(), chipId, ESP.getChipCores(), ESP.getPsramSize());
  BOOTLOG("display:\t%d", DSP_MODEL);
  if(VS1053_CS==255) {
    BOOTLOG("audio:\t\t%s (%d, %d, %d)", "I2S", I2S_DOUT, I2S_BCLK, I2S_LRC);
  }else{
    BOOTLOG("audio:\t\t%s (%d, %d, %d, %d, %s)", "VS1053", VS1053_CS, VS1053_DCS, VS1053_DREQ, VS1053_RST, VS_HSPI?"true":"false");
  }
  BOOTLOG("audioinfo:\t%s", store.audioinfo?"true":"false");
  BOOTLOG("smartstart:\t%d", store.smartstart);
  BOOTLOG("vumeter:\t%s", store.vumeter?"true":"false");
  BOOTLOG("softapdelay:\t%d", store.softapdelay);
  BOOTLOG("flipscreen:\t%s", store.flipscreen?"true":"false");
  BOOTLOG("invertdisplay:\t%s", store.invertdisplay?"true":"false");
  BOOTLOG("showweather:\t%s", store.showweather?"true":"false");
  BOOTLOG("buttons:\tleft=%d, center=%d, right=%d, up=%d, down=%d, mode=%d, pullup=%s", 
          BTN_LEFT, BTN_CENTER, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_MODE, BTN_INTERNALPULLUP?"true":"false");
  BOOTLOG("encoders:\tl1=%d, b1=%d, r1=%d, pullup=%s, l2=%d, b2=%d, r2=%d, pullup=%s", 
          ENC_BTNL, ENC_BTNB, ENC_BTNR, ENC_INTERNALPULLUP?"true":"false", ENC2_BTNL, ENC2_BTNB, ENC2_BTNR, ENC2_INTERNALPULLUP?"true":"false");
  BOOTLOG("ir:\t\t%d", IR_PIN);
  if(SDC_CS!=255) BOOTLOG("SD:\t\t%d", SDC_CS);
  BOOTLOG("------------------------------------------------");
}
