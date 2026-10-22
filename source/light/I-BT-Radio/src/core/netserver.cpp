#include "../dual/station_save_control.h"
#include "../dual/resource_limiter.h"
#include <esp_heap_caps.h>
#include <atomic>
#include "options.h"
#include "../dual/bt_audio.h"
#include "Arduino.h"
#include <SPIFFS.h>
#include <Update.h>
#include "config.h"
#include "netserver.h"
#include "web_service_owner.h"
#include "webbrand.h"
#include "web_skin_assets.h"
#include "../dual/weather.h"
#include "../dual/online_artwork.h"
#include "player.h"
#include "../dual/deferred_flags.h"
static DeferredFlags deferredAudioNotifications;
#include "telnet.h"
#include "display.h"
#include "network.h"
#include "mqtt.h"
#include "controls.h"
#include "commandhandler.h"
#include "timekeeper.h"
#include "../dual/core.h"
#include "../dual/sensors/receiver.h"
#include "../ui/content.h"
#include "../displays/dspcore.h"
#include "../displays/widgets/widgetsconfig.h" //BitrateFormat

#if DSP_MODEL==DSP_DUMMY
#define DUMMYDISPLAY
#endif

#ifdef USE_SD
#include "sdmanager.h"
#endif
#ifndef MIN_MALLOC
#define MIN_MALLOC 24112
#endif
#ifndef NSQ_SEND_DELAY
  //#define NSQ_SEND_DELAY       portMAX_DELAY
  #define NSQ_SEND_DELAY       pdMS_TO_TICKS(300)
#endif
#ifndef NS_QUEUE_TICKS
  //#define NS_QUEUE_TICKS pdMS_TO_TICKS(2)
  #define NS_QUEUE_TICKS 0
#endif

#ifdef DEBUG_V
#define DBGVB( ... ) { char buf[200]; sprintf( buf, __VA_ARGS__ ) ; Serial.print("[DEBUG]\t"); Serial.println(buf); }
#else
#define DBGVB( ... )
#endif

//#define CORS_DEBUG //Enable CORS policy: 'Access-Control-Allow-Origin' (for testing)

NetServer netserver;

AsyncWebServer webserver(80);
AsyncWebSocket websocket("/ws");
static std::atomic<bool> webReady{false},webStarting{false};
static bool webBtSuspended=false;
static bool webListenerStopped=false;
static WebServiceOwner webServiceOwner;
static std::atomic<uint32_t> webQueueEpoch{0};

void NetServer::bindServiceTask() {
  const bool bound=webServiceOwner.bind(xTaskGetCurrentTaskHandle());
  Serial.printf("[WEB-OWNER] bind=%s core=%d\n",bound?"PASS":"REFUSED",xPortGetCoreID());
}

struct DataUploadState {
  size_t written;
  size_t limit;
  uint32_t saveJob;
  bool ownsTemp;
  uint8_t kind; // 1=playlist, 2=Wi-Fi
  bool good;
  bool complete;
};
struct WebboardUploadState {
  size_t written;
  size_t limit;
  uint8_t activeKind; // 0=none, 1=playlist, 2=web asset
  bool failed;
  bool complete;
  bool playlistSeen;
  bool ownsTemp;
};
static AsyncWebServerRequest* volatile dataUploadOwner = nullptr;
static AsyncWebServerRequest* volatile webboardUploadOwner = nullptr;
static volatile bool dataImportBusy = false;
static std::atomic<bool> userResetQueued{false};
// Upload completion only means the staged file arrived. Validation and
// replacement happen later in NetServer::loop().
static uint32_t dataImportJob=0;
static volatile uint8_t dataImportResult = 0; // idle, pending, pass, fail

static unsigned webSkinIndex(const dual::Settings& s) {
  if(s.vintageSkin==6)return 15;
  if(s.vintageSkin==5)return 12;
  if(s.vintageSkin==4)return 11;
  if(s.vintageSkin>=1 && s.vintageSkin<=3)return 5u+s.vintageSkin;
  if(s.playerEnabled)return s.kajonSkin?4u:s.mediaPlayerSkin?3u:2u;
  return s.cassetteEnabled?1u:0u;
}
static void webJsonString(AsyncResponseStream* response,const char* value) {
  response->print('"');
  for(const unsigned char* p=(const unsigned char*)value;*p;++p){
    if(*p=='"'||*p=='\\'){response->print('\\');response->print(char(*p));}
    else if(*p<32)response->print(' ');
    else response->print(char(*p));
  }
  response->print('"');
}

static uint32_t stationSaveJob(const String& value){
  if(!value.length()||value.length()>10)return 0;uint32_t n=0;
  for(unsigned i=0;i<value.length();++i){if(value[i]<'0'||value[i]>'9'||n>(0x7fffffffu-unsigned(value[i]-'0'))/10)return 0;n=n*10+unsigned(value[i]-'0');}
  return n;
}
static void stationDeviceId(char* out,size_t size){
  const uint64_t chip=ESP.getEfuseMac();uint32_t id=2166136261u;
  for(unsigned i=0;i<8;++i){id^=uint8_t(chip>>(i*8));id*=16777619u;}
  snprintf(out,size,"%08lx",(unsigned long)id);
}
static void stationSaveReply(AsyncWebServerRequest* request,unsigned code=200){
  auto s=stationsave::status();char body[240],device[9];stationDeviceId(device,sizeof(device));
  const bool uploadReady=s.phase==stationsave::Ready&&!dataImportBusy&&!dataUploadOwner&&!webboardUploadOwner&&ibtlimit::allowAllocation(8192);
  snprintf(body,sizeof(body),"{\"job\":%lu,\"device\":\"%s\",\"phase\":\"%s\",\"uploadReady\":%s,\"installed\":%s,\"resumeRequested\":%s,\"station\":%u}",(unsigned long)s.job,device,stationsave::phaseName(s.phase),uploadReady?"true":"false",s.installed?"true":"false",s.resume?"true":"false",unsigned(s.station));
  auto* response=request->beginResponse(code,"application/json",body);response->addHeader("Cache-Control","no-store");request->send(response);
}
static bool stationSaveAuthorized(AsyncWebServerRequest* request){
#if defined(HTTP_USER) && defined(HTTP_PASS)
  if(!request->authenticate(HTTP_USER,HTTP_PASS)){request->requestAuthentication();return false;}
#endif
  if(network.status!=CONNECTED||network.offlinePlayer()){request->send(503,"text/plain","Radio offline");return false;}
  if(request->method()==HTTP_POST&&request->header("Origin")!=String("http://")+request->header("Host")){request->send(403,"text/plain","Invalid origin");return false;}
  return true;
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleIndex(AsyncWebServerRequest * request);
void handleNotFound(AsyncWebServerRequest * request);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

bool  shouldReboot  = false;

static bool webFirmwareUpdateAllowed(AsyncWebServerRequest *request) {
#if defined(DUAL_ALLOW_WEB_FW_UPDATE) && !DUAL_ALLOW_WEB_FW_UPDATE
  // SMALL accepts only an explicitly selected SPIFFS target.
  // Missing/malformed target must fail closed instead of reaching getParam(nullptr).
  if(!request || !request->hasParam("updatetarget", true)) return false;
  return request->getParam("updatetarget", true)->value() == "spiffs";
#else
  return request != nullptr;
#endif
}
static bool safeWebboardName(const String& name) {
  if(!name.length() || name.length()>64 || name[0]=='.' || name.indexOf("..")>=0)return false;
  for(unsigned i=0;i<name.length();i++){
    char c=name[i];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;
  }
  return true;
}
#ifdef MQTT_ROOT_TOPIC
//Ticker mqttplaylistticker;
bool  mqttplaylistblock = false;
void mqttplaylistSend() {
  mqttplaylistblock = true;
//  mqttplaylistticker.detach();
  mqttPublishPlaylist();
  mqttplaylistblock = false;
}
#endif

String updateError() {
  char message[BUFLEN];
  snprintf(message,sizeof(message),"Update failed with error (%d)<br /> %s",(int)Update.getError(),Update.errorString());
  return String(message); // response-owned copy; never shared with the queue consumer
}

bool NetServer::begin(bool quiet) {
#if IBT_STREAM_ART_DIAG
  Serial.println("[DIAG-SERVICES] WEBUI=OFF HTTP_LISTENER=OFF WS=OFF queue=NONE");return false;
#endif
  if(webReady)return true; // Online/offline cycling reuses routes and the queue.
  if(network.status==SDREADY) return true;
  bool available=false;if(!webStarting.compare_exchange_strong(available,true))return false;
  struct StartOwner {~StartOwner(){webStarting.store(false);}} startOwner;
  if(webReady.load())return true;
  if(!quiet) Serial.print("##[BOOT]#\tnetserver.begin\t");
  dataUploadOwner=nullptr;
  dataImportBusy=false;
  dataImportResult=0;
  importRequest = IMDONE;
  irRecordEnable = false;
  playerBufMax = psramFound()?320*1024:(IBT_NO_PSRAM?16*1024:32*1024);
  nsQueue = xQueueCreate( 20, sizeof( nsRequestParams_t ) );
  if(nsQueue==NULL){Serial.println("[WEB] START=DEFERRED no_queue_memory=1 audio_unchanged=1");return false;}

  webserver.on("/", HTTP_ANY, handleIndex);
  webserver.on("/api/station-save/prepare",HTTP_POST,[](AsyncWebServerRequest* request){
    if(!stationSaveAuthorized(request))return;
    char device[9];stationDeviceId(device,sizeof(device));
    if(request->hasArg("device")&&request->arg("device")!=device){request->send(409,"text/plain","Different device at this address");return;}
    const uint32_t job=stationSaveJob(request->arg("job"));
    if(!job){request->send(400,"text/plain","Invalid job");return;}
    auto current=stationsave::status();
    if(userResetQueued.load()||((dataImportBusy||dataUploadOwner||webboardUploadOwner)&&current.job!=job)){request->send(409,"text/plain","Import busy");return;}
    if(!stationsave::begin(job)){request->send(503,"text/plain","Save control busy or insufficient memory");return;}
    stationSaveReply(request,202);
  });
  webserver.on("/api/station-save/status",HTTP_GET,[](AsyncWebServerRequest* request){
    if(!stationSaveAuthorized(request))return;
    if(!stationSaveJob(request->arg("job"))||stationSaveJob(request->arg("job"))!=stationsave::status().job){request->send(404,"text/plain","Unknown save job");return;}
    stationSaveReply(request);
  });
  for(const char* operation:{"/api/station-save/finish","/api/station-save/cancel"}){
    webserver.on(operation,HTTP_POST,[](AsyncWebServerRequest* request){
      if(!stationSaveAuthorized(request))return;
      if(!stationsave::finish(stationSaveJob(request->arg("job")),request->url()=="/api/station-save/cancel")){request->send(409,"text/plain","Save phase does not allow release");return;}
      stationSaveReply(request,202);
    });
  }

  webserver.on("/api/videoton.ttf", HTTP_GET, [](AsyncWebServerRequest *request) {request->send(404,"text/plain","Not available in Light");});
  webserver.on("/api/skin.png", HTTP_GET, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(request->arg("id")!="2"){request->send(404,"text/plain","No Light skin image");return;}
    auto* response=request->beginResponse_P(200,"image/png",webSkin_winamp,webSkin_winamp_len);
    response->addHeader("Cache-Control","public, max-age=86400, immutable");request->send(response);
  });
  webserver.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    auto* response=request->beginResponse(200,"application/json","{\"major\":\"166\",\"release\":\"211\",\"webui\":\"IBT_R211_1\",\"save\":\"lease3\"}");
    response->addHeader("Cache-Control","no-store");request->send(response);
  });
  webserver.on("/api/player", HTTP_GET, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(network.status!=CONNECTED){request->send(503,"text/plain","Radio offline");return;}
    dual::Snapshot s=dual::snapshot();dual::prepareTrackText(s);
    AsyncResponseStream* response=request->beginResponseStream("application/json; charset=utf-8");
    response->print("{\"skin\":");response->print(webSkinIndex(s.settings));
    // Bind persisted browser save requests to this unit, not merely its IP.
    const uint64_t chip=ESP.getEfuseMac();uint32_t device=2166136261u;
    for(unsigned i=0;i<8;++i){device^=uint8_t(chip>>(i*8));device*=16777619u;}
    char deviceText[9];snprintf(deviceText,sizeof(deviceText),"%08lx",(unsigned long)device);
    response->print(",\"device\":");webJsonString(response,deviceText);
    response->print(",\"retroColor\":");response->print(s.settings.retroColor);
    if(s.settings.vintageSkin==6){
      char clock[8]="--:--";time_t now=time(nullptr);tm local{};
      if(now>1704067200&&localtime_r(&now,&local))snprintf(clock,sizeof(clock),"%02u:%02u",unsigned(local.tm_hour),unsigned(local.tm_min));
      response->print(",\"clock\":");webJsonString(response,clock);
      response->print(",\"preset\":");response->print(config.lastStation());
    }
    if(s.settings.vintageSkin==5){
      char clock[8]="--:--",summary[100];time_t now=time(nullptr);tm local{};
      if(now>1600000000&&localtime_r(&now,&local))snprintf(clock,sizeof(clock),"%02u:%02u",unsigned(local.tm_hour),unsigned(local.tm_min));
      const auto c=weather::configuration();const auto w=weather::reading();
      if(c.enabled&&weather::fresh(w,millis()))snprintf(summary,sizeof(summary),"%s%d.%d C\n%s\n%u%%",w.temperatureTenths<0?"-":"",abs(int(w.temperatureTenths))/10,abs(int(w.temperatureTenths))%10,c.location,w.humidity);
      else snprintf(summary,sizeof(summary),"%s\n%s",c.location,c.enabled?"Adatra var":"API: ki");
      response->print(",\"clock\":");webJsonString(response,clock);response->print(",\"weather\":");webJsonString(response,summary);
    }
    response->print(",\"station\":");webJsonString(response,s.station);
    response->print(",\"artist\":");webJsonString(response,s.artist);
    response->print(",\"title\":");webJsonString(response,s.title);
    response->print(",\"playing\":");response->print(s.playing?"true":"false");
    response->print(",\"sd\":");response->print(s.sd?"true":"false");
    response->print(",\"btIn\":");response->print(btaudio::inputSelected()?"true":"false");
    response->print(",\"btOut\":");response->print(btaudio::outputSelected()?"true":"false");
    response->print(",\"elapsed\":");response->print(s.elapsed);
    response->print(",\"bitrate\":");response->print(s.bitrate);
    response->print(",\"sampleRate\":");response->print(s.sampleRate);
    response->print(",\"volume\":");response->print((unsigned(s.volume)*100u+127u)/254u);
    response->print(",\"buffer\":");response->print(s.bufferCapacity?
      unsigned((uint64_t(s.buffer)*100u)/s.bufferCapacity):0u);
    response->print('}');response->addHeader("Cache-Control","no-store");
    request->send(response);
  });
  webserver.on("/api/player-action", HTTP_POST, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(network.status!=CONNECTED){request->send(503,"text/plain","Radio offline");return;}
    if(request->header("Origin")!=String("http://")+request->header("Host")){
      request->send(403,"text/plain","Invalid origin");return;
    }
    const String action=request->arg("action");
    dual::Action command=dual::Action::None;
    if(action=="prev")command=dual::Action::Prev;
    else if(action=="next")command=dual::Action::Next;
    else if(action=="toggle")command=dual::Action::Toggle;
    else if(action=="volm")command=dual::Action::VolDown;
    else if(action=="volp")command=dual::Action::VolUp;
    if(command==dual::Action::None){request->send(400,"text/plain","Invalid action");return;}
    dual::send(command); // The UI task owns player and BT control; never run them here.
    AsyncWebServerResponse* response=request->beginResponse(202,"application/json","{\"queued\":true}");
    response->addHeader("Cache-Control","no-store");request->send(response);
  });
  // Fetch the radio names only when the browser search is opened. One
  // streamed file transfer avoids filesystem scans for every keystroke.
  webserver.on("/api/reset-user", HTTP_POST, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(request->header("Origin")!=String("http://")+request->header("Host") || request->arg("confirm")!="reset-user-keep-connections"){
      request->send(403,"text/plain","Invalid reset request");return;
    }
    if(stationsave::holdsAudio() || network.status!=CONNECTED || config.getMode()!=PM_WEB || player.isRunning() || player.status()!=STOPPED || btaudio::wanted() || dataImportBusy || dataUploadOwner || webboardUploadOwner){
      request->send(409,"text/plain","Stop playback, select internet radio and DAC, finish imports first");return;
    }
    bool expected=false;
    if(!userResetQueued.compare_exchange_strong(expected,true)){request->send(409,"text/plain","Reset already queued");return;}
    request->send(202,"application/json","{\"status\":\"queued\",\"wifi\":\"preserved\",\"bluetooth\":\"preserved\"}");
  });
  webserver.on("/api/radio-stations.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(network.status!=CONNECTED){request->send(503,"text/plain","Radio offline");return;}
    if(!SPIFFS.exists(PLAYLIST_PATH)){request->send(404,"text/plain","No radio stations");return;}
    request->send(SPIFFS,PLAYLIST_PATH,"text/plain; charset=utf-8",false);
  });
#if IBT_NO_PSRAM && FEATURE_ARTWORK
  webserver.on("/api/cover",HTTP_GET,[](AsyncWebServerRequest* request){
    if(!stationSaveAuthorized(request))return;
    const auto s=onlineart::uploadStatus();char body[340],key[17];snprintf(key,sizeof(key),"%016llx",(unsigned long long)s.station);
    snprintf(body,sizeof(body),"{\"stationKey\":\"%s\",\"generation\":%lu,\"enabled\":%s,\"qualified\":%s,\"busy\":%s,\"playing\":%s,\"hash\":%lu,\"bytes\":4096,\"albumState\":\"%s\",\"logoTyped\":%s}",key,(unsigned long)s.generation,s.enabled?"true":"false",s.qualified?"true":"false",s.busy?"true":"false",player.isRunning()?"true":"false",(unsigned long)s.hash,onlineart::albumStatus(),s.hash?"true":"false");
    auto* response=request->beginResponse(200,"application/json",body);response->addHeader("Cache-Control","no-store");request->send(response);
  });
  webserver.on("/api/cover",HTTP_POST,[](AsyncWebServerRequest* request){
    if(!stationSaveAuthorized(request))return;
    uint32_t hash=0;
    const bool owned=dataUploadOwner==request;
    const bool ok=owned&&onlineart::uploadFinish(hash);
    if(owned){onlineart::uploadAbort();dataUploadOwner=nullptr;}
    char body[96];snprintf(body,sizeof(body),"{\"saved\":%s,\"hash\":%lu,\"bytes\":4096}",ok?"true":"false",(unsigned long)hash);
    if(!ok){request->send(409,"text/plain; charset=utf-8","Borító nem menthető: 60 másodperc hallgatás, majd szünet és elegendő memória szükséges.");return;}
    request->send(200,"application/json",body);
  },nullptr,[](AsyncWebServerRequest* request,uint8_t* data,size_t len,size_t index,size_t total){
    if(!index){
#if defined(HTTP_USER) && defined(HTTP_PASS)
      if(!request->authenticate(HTTP_USER,HTTP_PASS))return;
#endif
      if(request->header("Origin")!=String("http://")+request->header("Host")||network.status!=CONNECTED||config.getMode()!=PM_WEB||btaudio::wanted()||player.isRunning()||player.status()!=STOPPED||total!=4096||dataUploadOwner||webboardUploadOwner||dataImportBusy||userResetQueued.load()||stationsave::holdsAudio())return;
      const auto status=onlineart::uploadStatus();char key[17];snprintf(key,sizeof(key),"%016llx",(unsigned long long)status.station);
      const uint32_t g=stationSaveJob(request->arg("generation"));
      if(!g||g!=status.generation||request->arg("station")!=key||!onlineart::uploadBegin(g,status.station))return;
      dataUploadOwner=request;
      request->onDisconnect([request](){if(dataUploadOwner==request){onlineart::uploadAbort();dataUploadOwner=nullptr;}});
    }
    if(dataUploadOwner!=request)return;
    if(total!=4096||config.getMode()!=PM_WEB||btaudio::wanted()||player.isRunning()||player.status()!=STOPPED||stationsave::holdsAudio()||!onlineart::uploadWrite(index,data,len)){
      onlineart::uploadAbort();dataUploadOwner=nullptr;
    }
  });
#endif
  webserver.on("/api/station-play", HTTP_POST, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(network.status!=CONNECTED){request->send(503,"text/plain","Radio offline");return;}
    if(config.getMode()!=PM_WEB||btaudio::wanted()){
      request->send(409,"text/plain","Select internet radio and DAC first");return;
    }
    if(request->header("Origin")!=String("http://")+request->header("Host")){
      request->send(403,"text/plain","Invalid origin");return;
    }
    const String number=request->arg("number");
    if(number.length()<1||number.length()>3){request->send(400,"text/plain","Invalid station");return;}
    for(size_t i=0;i<number.length();++i)if(number[i]<'0'||number[i]>'9'){
      request->send(400,"text/plain","Invalid station");return;
    }
    const unsigned station=unsigned(number.toInt());
    if(station<1||station>480||!dual::webStationPlay(uint16_t(station))){
      request->send(503,"text/plain","Station busy");return;
    }
    request->send(202,"application/json","{\"queued\":true}");
  });
  // The browser reads one small menu page at a time. Labels and values come
  // from the same renderer as the device, without a second menu definition.
  webserver.on("/api/menu", HTTP_GET, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(network.status!=CONNECTED){request->send(503,"text/plain","Radio offline");return;}
    const unsigned page=unsigned(request->arg("page").toInt());
    const unsigned count=menustructure::webReadable(page)?content::itemCount(page):0;
    if(!count){request->send(400,"text/plain","Invalid menu page");return;}
    const dual::Snapshot snap=dual::snapshot();
    AsyncResponseStream *response=request->beginResponseStream("application/json; charset=utf-8");
    auto quote=[response](const char* value){
      response->print('"');
      for(const unsigned char *p=(const unsigned char*)value;*p;++p){
        if(*p=='"'||*p=='\\'){response->print('\\');response->print(char(*p));}
        else if(*p<32){response->print(' ');}
        else response->print(char(*p));
      }
      response->print('"');
    };
    response->print("{\"page\":");response->print(page);
    response->print(",\"title\":");quote(content::pageTitle(snap.settings.language,page));
    response->print(",\"items\":[");
    for(unsigned item=0;item+(page==1?0:1)<count;++item){
      char label[128];const bool enabled=content::menuItem(label,sizeof(label),snap,page,item);
      if(item)response->print(',');
      response->print("{\"index\":");response->print(item);
      response->print(",\"label\":");quote(label);
      const unsigned child=enabled?menustructure::webChild(page,item):0;
      if(child){response->print(",\"child\":");response->print(child);}
      if(page==73&&item<12&&item!=3){static const int8_t map[]={0,1,2,-1,3,4,5,6,7,8,9,10};const unsigned parameter=map[item];const auto c=monodsp::configuration();response->printf(",\"parameter\":%u,\"value\":%d,\"min\":%d,\"max\":%d,\"step\":%d",parameter,c.v[parameter],monodsp::Minimum[parameter],monodsp::Maximum[parameter],monodsp::Step[parameter]);}
      if(page==68&&item<4){const auto servers=dual::clockServers();response->print(",\"value\":");quote(servers.values[item]);}
      response->print(",\"enabled\":");response->print(enabled?"true":"false");
      response->print(",\"action\":");response->print(enabled&&dual::webMenuSupported(page,item)?"true":"false");
      response->print('}');
    }
    response->print("]}");response->addHeader("Cache-Control","no-store");request->send(response);
  });
  webserver.on("/api/mono", HTTP_POST, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(network.status!=CONNECTED){request->send(503,"text/plain","Radio offline");return;}
    if(request->header("Origin")!=String("http://")+request->header("Host")){request->send(403,"text/plain","Origin rejected");return;}
    if(!request->hasParam("parameter",true)||!request->hasParam("value",true)){request->send(400,"text/plain","Missing field");return;}
    const String parameter=request->getParam("parameter",true)->value(),value=request->getParam("value",true)->value();
    char *end=nullptr;const long key=strtol(parameter.c_str(),&end,10);const bool goodKey=parameter.length()&&!*end&&key>=0&&key<long(monodsp::Parameters);
    const long number=strtol(value.c_str(),&end,10);const bool ok=goodKey&&value.length()&&!*end&&number>=-32768&&number<=32767&&monodsp::set(unsigned(key),int(number));
    request->send(ok?202:400,"application/json",ok?"{\"accepted\":true,\"save\":\"when_stopped\"}":"{\"error\":\"Invalid MONO DSP value\"}");
  });
  webserver.on("/api/weather", HTTP_GET, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    const auto c=weather::configuration();const auto w=weather::reading();
    auto response=request->beginResponseStream("application/json; charset=utf-8");
    response->print("{\"enabled\":");response->print(c.enabled?"true":"false");
    response->print(",\"provider\":");response->print(unsigned(c.provider));
    response->print(",\"latitude\":");response->print(c.latitude,5);response->print(",\"longitude\":");response->print(c.longitude,5);
    response->print(",\"location\":");webJsonString(response,c.location);
    response->print(",\"interval\":");response->print(c.intervalMinutes);
    response->print(",\"key_set\":");response->print(c.key[0]?"true":"false");
    response->print(",\"pending\":");response->print(weather::pending()?"true":"false");
    response->print(",\"status\":");webJsonString(response,weather::status());
    response->print(",\"fresh\":");response->print(c.enabled&&weather::fresh(w,millis())?"true":"false");
    response->print(",\"temperature_tenths\":");response->print(w.temperatureTenths);
    response->print('}');response->addHeader("Cache-Control","no-store");request->send(response);
  });
  webserver.on("/api/weather", HTTP_POST, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(network.status!=CONNECTED){request->send(503,"text/plain","Radio offline");return;}
    if(request->header("Origin")!=String("http://")+request->header("Host")){request->send(403,"text/plain","Origin rejected");return;}
    const char* names[]={"enabled","provider","latitude","longitude","location","interval"};
    for(const char* name:names)if(!request->hasParam(name,true)){request->send(400,"text/plain","Missing weather field");return;}
    weather::Config c=weather::configuration();
    const String en=request->getParam("enabled",true)->value(),provider=request->getParam("provider",true)->value(),interval=request->getParam("interval",true)->value(),location=request->getParam("location",true)->value();
    if((en!="0"&&en!="1")||(provider!="0"&&provider!="1")||(interval!="15"&&interval!="30"&&interval!="60")||!location.length()||location.length()>=sizeof(c.location)||
      !weather::coordinate(request->getParam("latitude",true)->value().c_str(),c.latitude)||!weather::coordinate(request->getParam("longitude",true)->value().c_str(),c.longitude)){
      request->send(400,"text/plain","Invalid weather fields");return;
    }
    c.enabled=en=="1";c.provider=weather::Provider(provider=="1"?1:0);c.intervalMinutes=interval.toInt();strlcpy(c.location,location.c_str(),sizeof(c.location));
    if(request->hasParam("clear_key",true)&&request->getParam("clear_key",true)->value()=="1")memset(c.key,0,sizeof(c.key));
    if(request->hasParam("key",true)){
      String key=request->getParam("key",true)->value();key.trim();
      if(key.length()>=sizeof(c.key)){request->send(400,"text/plain","API key too long");return;}
      if(key.length())strlcpy(c.key,key.c_str(),sizeof(c.key));
      for(unsigned i=0;i<key.length();++i)key.setCharAt(i,'0');
    }
    const bool ok=weather::configure(c);memset(c.key,0,sizeof(c.key));
    auto response=request->beginResponse(ok?202:400,"application/json",ok?"{\"queued\":true}":"{\"error\":\"Invalid profile or save pending\"}");response->addHeader("Cache-Control","no-store");request->send(response);
  });
  webserver.on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    char address[18],body[320];bool hasKey=false,pending=false;sensors::bindingStatus(address,sizeof(address),hasKey,pending);radiosensor::Reading value;const bool ready=sensors::reading(value);
    snprintf(body,sizeof(body),"{\"mac\":\"%s\",\"key_set\":%s,\"pending\":%s,\"fresh\":%s,\"temperature_centi\":%d,\"humidity_centi\":%u}",address,hasKey?"true":"false",pending?"true":"false",ready&&radiosensor::hasFreshPair(value,millis())?"true":"false",int(value.temperatureCenti),unsigned(value.humidityCenti));
    auto response=request->beginResponse(200,"application/json",body);response->addHeader("Cache-Control","no-store");request->send(response);
  });
  webserver.on("/api/sensor", HTTP_POST, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(network.status!=CONNECTED){request->send(503,"text/plain","Radio offline");return;}
    if(request->header("Origin")!=String("http://")+request->header("Host")){request->send(403,"text/plain","Origin rejected");return;}
    if(!request->hasParam("mac",true)||!request->hasParam("key",true)){request->send(400,"text/plain","MAC and bindkey required in POST body");return;}
    String mac=request->getParam("mac",true)->value();String key=request->getParam("key",true)->value();
    mac.trim();mac.replace('-',':');key.trim();
    bool ok=mac.length()==17&&key.length()==32&&sensors::bindAddress(mac.c_str(),key.c_str());
    for(unsigned i=0;i<key.length();++i)key.setCharAt(i,'0');
    auto response=request->beginResponse(ok?202:400,"application/json",ok?"{\"queued\":true}":"{\"error\":\"Invalid MAC/key or pending request\"}");response->addHeader("Cache-Control","no-store");request->send(response);
  });
  webserver.on("/api/clock-server", HTTP_POST, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(network.status!=CONNECTED){request->send(503,"text/plain","Radio offline");return;}
    if(request->header("Origin")!=String("http://")+request->header("Host")){request->send(403,"text/plain","Origin rejected");return;}
    const String index=request->arg("index"),value=request->arg("value");
    if(index.length()!=1||index[0]<'0'||index[0]>'3'||value.length()>63||!dual::webClockServer(unsigned(index[0]-'0'),value.c_str())){
      request->send(400,"text/plain","Invalid NTP host or busy; keep one server enabled");return;
    }
    AsyncWebServerResponse *response=request->beginResponse(202,"application/json","{\"queued\":true}");
    response->addHeader("Cache-Control","no-store");request->send(response);
  });
  webserver.on("/api/menu-action", HTTP_POST, [](AsyncWebServerRequest *request) {
#if defined(HTTP_USER) && defined(HTTP_PASS)
    if(!request->authenticate(HTTP_USER,HTTP_PASS))return request->requestAuthentication();
#endif
    if(network.status!=CONNECTED){request->send(503,"text/plain","Radio offline");return;}
    const String origin=request->header("Origin");
    if(origin!=String("http://")+request->header("Host")){
      request->send(403,"text/plain","Invalid origin");return;
    }
    const unsigned page=unsigned(request->arg("page").toInt());
    const unsigned item=unsigned(request->arg("item").toInt());
    if(!dual::webMenuSupported(page,item)){request->send(400,"text/plain","Read-only menu item");return;}
    if(!dual::webMenuActivate(page,item)){request->send(503,"text/plain","Menu busy");return;}
    AsyncWebServerResponse *response=request->beginResponse(202,"application/json","{\"queued\":true}");
    response->addHeader("Cache-Control","no-store");request->send(response);
  });
  // Explicit route before static assets: always use this firmware's product identity.
  webserver.on("/ibt-branding.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript; charset=utf-8", ibt_branding_js);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });
  webserver.on("/import-status", HTTP_GET, [](AsyncWebServerRequest *request) {
    const uint8_t result=dataImportResult;
    const char* status=result==1?"pending":result==2?"pass":result==3?"fail":"idle";
    AsyncWebServerResponse* response=request->beginResponse(200,"application/json",String("{\"status\":\"")+status+"\"}");
    response->addHeader("Cache-Control","no-store");
    request->send(response);
  });
  // V15 captive portal probes. Never load the normal WebUI while setup AP owns Wi-Fi.
  webserver.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request){ if(network.status==SOFT_AP)request->redirect("http://192.168.4.1/"); else request->send(204); });
  webserver.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest *request){ if(network.status==SOFT_AP)request->redirect("http://192.168.4.1/"); else request->send(204); });
  webserver.on("/connecttest.txt", HTTP_ANY, [](AsyncWebServerRequest *request){ if(network.status==SOFT_AP)request->redirect("http://192.168.4.1/"); else request->send(200,"text/plain","Microsoft Connect Test"); });
  webserver.on("/ncsi.txt", HTTP_ANY, [](AsyncWebServerRequest *request){ if(network.status==SOFT_AP)request->redirect("http://192.168.4.1/"); else request->send(200,"text/plain","Microsoft NCSI"); });
  webserver.onNotFound(handleNotFound);
  webserver.onFileUpload(handleUpload);

  webserver.serveStatic("/", SPIFFS, "/www/").setCacheControl("no-cache, no-store, must-revalidate");
#ifdef CORS_DEBUG
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), F("*"));
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Headers"), F("content-type"));
#endif
  webBtSuspended=btaudio::webBlocked();
  websocket.enable(!webBtSuspended);
  webListenerStopped=network.offlinePlayer();
  if(!webListenerStopped)webserver.begin();
  webReady=true;
  //if(strlen(config.store.mdnsname)>0)
  //  MDNS.begin(config.store.mdnsname);
  websocket.onEvent(onWsEvent);
  webserver.addHandler(&websocket);
  if(network.status==SOFT_AP){
    Serial.printf("[WIFI-PORTAL] listener=%u bt_blocked=%u free=%u largest=%u\n",unsigned(!webBtSuspended),unsigned(webBtSuspended),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
  }
  if(!quiet) Serial.println("done");
  return true;
}

size_t NetServer::chunkedHtmlPageCallback(uint8_t* buffer, size_t maxLen, size_t index){
  File requiredfile;
  bool sdpl = strcmp(netserver.chunkedPathBuffer, PLAYLIST_SD_PATH) == 0;
  if(sdpl){
    requiredfile = config.SDPLFS()->open(netserver.chunkedPathBuffer, "r");
  }else{
    requiredfile = SPIFFS.open(netserver.chunkedPathBuffer, "r");
  }
  if (!requiredfile) return 0;
  size_t filesize = requiredfile.size();
  size_t needread = filesize - index;
  if (!needread) {
    requiredfile.close();
    display.unlock();
    return 0;
  }
  #ifdef MAX_PL_READ_BYTES
    if(maxLen>MAX_PL_READ_BYTES) maxLen=MAX_PL_READ_BYTES;
  #endif
  size_t canread = (needread > maxLen) ? maxLen : needread;
  DBGVB("[%s] seek to %d in %s and read %d bytes with maxLen=%d", __func__, index, netserver.chunkedPathBuffer, canread, maxLen);
  //netserver.loop();
  requiredfile.seek(index, SeekSet);
  requiredfile.read(buffer, canread);
  index += canread;
  if (requiredfile) requiredfile.close();
  return canread;
}

void NetServer::chunkedHtmlPage(const String& contentType, AsyncWebServerRequest *request, const char * path) {
  const bool sdpl = strcmp(path, PLAYLIST_SD_PATH) == 0;
  if (sdpl ? !config.SDPLFS()->exists(path) : !SPIFFS.exists(path)) {
    request->send(404, "text/plain", "File not found");
    return;
  }
  memset(chunkedPathBuffer, 0, sizeof(chunkedPathBuffer));
  strlcpy(chunkedPathBuffer, path, sizeof(chunkedPathBuffer)-1);
  AsyncWebServerResponse *response;
  #ifndef NETSERVER_LOOP1
  display.lock();
  #endif
  response = request->beginChunkedResponse(contentType, chunkedHtmlPageCallback);
  response->addHeader("Cache-Control","max-age=31536000");
  request->send(response);
}

#ifndef DSP_NOT_FLIPPED
  #define DSP_CAN_FLIPPED true
#else
  #define DSP_CAN_FLIPPED false
#endif
#if !defined(HIDE_WEATHER) && (!defined(DUMMYDISPLAY) && !defined(USE_NEXTION))
  #define SHOW_WEATHER  true
#else
  #define SHOW_WEATHER  false
#endif

const char *getFormat(BitrateFormat _format) {
  switch (_format) {
    case BF_MP3:  return "MP3";
    case BF_AAC:  return "AAC";
    case BF_FLAC: return "FLC";
    case BF_OGG:  return "OGG";
    case BF_WAV:  return "WAV";
    default:      return "bitrate";
  }
}

#define APPEND_GROUP(name) do { \
  if(strlcat(wsBuf,"\"" name "\",",sizeof(wsBuf))>=sizeof(wsBuf))return; \
} while(0)
void NetServer::processQueue(){
  if(nsQueue==NULL) return;
  static uint32_t seenEpoch=0;
  const uint32_t epoch=webQueueEpoch.load(std::memory_order_acquire);
  if(epoch!=seenEpoch){pendingReplies=0;seenEpoch=epoch;}
  uint32_t bit=deferredAudioNotifications.take();
  if(bit){
    nsRequestParams_t pending{static_cast<requestType_e>(__builtin_ctz(bit)),0};
    if(xQueueSend(nsQueue,&pending,0)!=pdPASS)deferredAudioNotifications.add(bit);
  }
  nsRequestParams_t request;
  // One reply per service iteration. Never enqueue GETINDEX/STATION children
  // into the queue this same task must drain (a full queue would self-wait).
  if(btaudio::webBlocked())pendingReplies=0;
  bool received=false;
  if(pendingReplies){
    const uint32_t bit=pendingReplies&(~pendingReplies+1u);
    pendingReplies&=~bit;
    request={static_cast<requestType_e>(__builtin_ctz(bit)),pendingClient};
    received=true;
  }else received=xQueueReceive(nsQueue,&request,NS_QUEUE_TICKS)==pdPASS;
  if(received){
    if(btaudio::webBlocked() && request.type!=PLAYLISTSAVED && request.type!=CHANGEMODE)return;
    uint8_t clientId = request.clientId;
    wsBuf[0]='\0';
    switch (request.type) {
      case PLAYLIST:        getPlaylist(clientId); break;
      case PLAYLISTSAVED:   {
        #ifdef USE_SD
        if(config.getMode()==PM_SDCARD) {
        //  config.indexSDPlaylist();
          config.initSDPlaylist();
        }
        #endif
        if(config.getMode()==PM_WEB){
          config.indexPlaylist(); 
          config.initPlaylist(); 
          dual::preloadStationSearch();
        }
        getPlaylist(clientId); break;
      }
      case GETACTIVE: {
          bool dbgact = false, nxtn=false;
          //String act = F("\"group_wifi\",");
          strcpy(wsBuf,"{\"act\":[");
          APPEND_GROUP("group_wifi");
          if (network.status == CONNECTED) {
                                                                //act += F("\"group_system\",");
                                                                APPEND_GROUP("group_system");
            if (BRIGHTNESS_PIN != 255 || DSP_CAN_FLIPPED || DSP_MODEL == DSP_NOKIA5110 || dbgact)    APPEND_GROUP("group_display");
          #ifdef USE_NEXTION
                                                                APPEND_GROUP("group_nextion");
            if (!SHOW_WEATHER || dbgact)                        APPEND_GROUP("group_weather");
            nxtn=true;
          #endif
                                                              #if defined(LCD_I2C) || defined(DSP_OLED)
                                                                APPEND_GROUP("group_oled");
                                                              #endif
                                                              #if !defined(HIDE_VU) && !defined(DUMMYDISPLAY)
                                                                APPEND_GROUP("group_vu");
                                                              #endif
            if (BRIGHTNESS_PIN != 255 || nxtn || dbgact)        APPEND_GROUP("group_brightness");
            if (DSP_CAN_FLIPPED || dbgact)                      APPEND_GROUP("group_tft");
            if (TS_MODEL != TS_MODEL_UNDEFINED || dbgact)       APPEND_GROUP("group_touch");
            if (DSP_MODEL == DSP_NOKIA5110)                     APPEND_GROUP("group_nokia");
                                                                APPEND_GROUP("group_timezone");
            if (SHOW_WEATHER || dbgact)                         APPEND_GROUP("group_weather");
                                                                APPEND_GROUP("group_controls");
            if (ENC_BTNL != 255 || ENC2_BTNL != 255 || dbgact)  APPEND_GROUP("group_encoder");
            if (IR_PIN != 255 || dbgact)                        APPEND_GROUP("group_ir");
            if (!psramInit())                                   APPEND_GROUP("group_buffer");
                                                              #if RTCSUPPORTED
                                                                APPEND_GROUP("group_rtc");
                                                              #else
                                                                APPEND_GROUP("group_wortc");
                                                              #endif
          }
          size_t len = strlen(wsBuf);
          if (len > 0 && wsBuf[len - 1] == ',') wsBuf[len - 1] = '\0';
          if(strlcat(wsBuf,"]}",sizeof(wsBuf))>=sizeof(wsBuf))return;
          break;
        }
      case GETINDEX: {
          pendingClient=clientId;
          pendingReplies=(1u<<STATIONNAME)|(1u<<ITEM)|(1u<<TITLE)|(1u<<VOLUME)|
            (1u<<EQUALIZER)|(1u<<BALANCE)|(1u<<BITRATE)|(1u<<MODE)|
            (1u<<SDINIT)|(1u<<GETPLAYERMODE);
          if(config.getMode()==PM_SDCARD)
            pendingReplies|=(1u<<SDPOS)|(1u<<SDLEN)|(1u<<SDSNUFFLE);
          return;
        }
      case GETSYSTEM:     sprintf (wsBuf, "{\"sst\":%d,\"aif\":%d,\"vu\":%d,\"softr\":%d,\"vut\":%d,\"mdns\":\"%s\",\"ipaddr\":\"%s\", \"abuff\": %d, \"telnet\": %d, \"watchdog\": %d }", 
                                  config.store.smartstart != 2, 
                                  config.store.audioinfo, 
                                  config.store.vumeter, 
                                  config.store.softapdelay,
                                  config.vuThreshold,
                                  config.store.mdnsname,
                                  config.ipToStr(WiFi.localIP()),
                                  config.store.abuff,
                                  config.store.telnet,
                                  config.store.watchdog); 
                                  break;
      case GETSCREEN:     sprintf (wsBuf, "{\"flip\":%d,\"inv\":%d,\"nump\":%d,\"tsf\":%d,\"tsd\":%d,\"dspon\":%d,\"br\":%d,\"con\":%d,\"scre\":%d,\"scrt\":%d,\"scrb\":%d,\"scrpe\":%d,\"scrpt\":%d,\"scrpb\":%d}", 
                                  config.store.flipscreen, 
                                  config.store.invertdisplay, 
                                  config.store.numplaylist, 
                                  config.store.fliptouch, 
                                  config.store.dbgtouch, 
                                  config.store.dspon, 
                                  config.store.brightness, 
                                  config.store.contrast,
                                  config.store.screensaverEnabled,
                                  config.store.screensaverTimeout,
                                  config.store.screensaverBlank,
                                  config.store.screensaverPlayingEnabled,
                                  config.store.screensaverPlayingTimeout,
                                  config.store.screensaverPlayingBlank);
                                  break;
      case GETTIMEZONE:   sprintf (wsBuf, "{\"tzh\":%d,\"tzm\":%d,\"sntp1\":\"%s\",\"sntp2\":\"%s\", \"timeint\":%d,\"timeintrtc\":%d}", 
                                  config.store.tzHour, 
                                  config.store.tzMin, 
                                  config.store.sntp1, 
                                  config.store.sntp2,
                                  config.store.timeSyncInterval,
                                  config.store.timeSyncIntervalRTC); 
                                  break;
      case GETWEATHER:    sprintf (wsBuf, "{\"wen\":%d,\"wlat\":\"%s\",\"wlon\":\"%s\",\"wkey\":\"%s\",\"wint\":%d}", 
                                  config.store.showweather, 
                                  config.store.weatherlat, 
                                  config.store.weatherlon, 
                                  config.store.weatherkey,
                                  config.store.weatherSyncInterval); 
                                  break;
      case GETCONTROLS:   sprintf (wsBuf, "{\"vols\":%d,\"enca\":%d,\"irtl\":%d,\"skipup\":%d}", 
                                  config.store.volsteps, 
                                  config.store.encacc, 
                                  config.store.irtlp,
                                  config.store.skipPlaylistUpDown); 
                                  break;
      case DSPON:         sprintf (wsBuf, "{\"dspontrue\":%d}", 1); break;
      case STATION:       pendingClient=clientId; pendingReplies=(1u<<STATIONNAME)|(1u<<ITEM); break;
      case STATIONNAME:   sprintf (wsBuf, "{\"payload\":[{\"id\":\"nameset\", \"value\": \"%s\"}]}", config.station.name); break;
      case ITEM:          sprintf (wsBuf, "{\"current\": %d}", config.lastStation()); break;
      case TITLE:         sprintf (wsBuf, "{\"payload\":[{\"id\":\"meta\", \"value\": \"%s\"}]}", config.station.title); telnet.printf("##CLI.META#: %s\n> ", config.station.title); break;
      case VOLUME:        sprintf (wsBuf, "{\"payload\":[{\"id\":\"volume\", \"value\": %d}]}", config.store.volume); telnet.printf("##CLI.VOL#: %d\n", config.store.volume); break;
      case NRSSI:         sprintf (wsBuf, "{\"payload\":[{\"id\":\"rssi\", \"value\": %d}, {\"id\":\"heap\", \"value\": %d}]}", rssi, (player.isRunning() && config.store.audioinfo)?(int)(100*player.inBufferFilled()/playerBufMax):0); /*rssi = 255;*/ break;
      case SDPOS: {
        const auto progress=player.progressSnapshot();
        snprintf(wsBuf,sizeof(wsBuf),"{\"sdpos\": %lu,\"sdend\": %lu,\"sdtpos\": %lu,\"sdtend\": %lu}",
                 (unsigned long)progress.position,(unsigned long)progress.size,
                 (unsigned long)progress.elapsed,(unsigned long)progress.duration);
        break;
      }
      case SDLEN:         sprintf (wsBuf, "{\"sdmin\": %lu,\"sdmax\": %lu}", player.sd_min, player.sd_max); break;
      case SDSNUFFLE:     sprintf (wsBuf, "{\"snuffle\": %d}", config.store.sdsnuffle); break;
      case BITRATE:       sprintf (wsBuf, "{\"payload\":[{\"id\":\"bitrate\", \"value\": %d}, {\"id\":\"fmt\", \"value\": \"%s\"}]}", config.station.bitrate, getFormat(config.configFmt)); break;
      case MODE:          sprintf (wsBuf, "{\"payload\":[{\"id\":\"playerwrap\", \"value\": \"%s\"}]}", player.status() == PLAYING ? "playing" : "stopped"); telnet.info(); break;
      case EQUALIZER:     sprintf (wsBuf, "{\"payload\":[{\"id\":\"bass\", \"value\": %d}, {\"id\": \"middle\", \"value\": %d}, {\"id\": \"trebble\", \"value\": %d}]}", config.store.bass, config.store.middle, config.store.trebble); break;
      case BALANCE:       sprintf (wsBuf, "{\"payload\":[{\"id\": \"balance\", \"value\": %d}]}", config.store.balance); break;
      case SDINIT:        sprintf (wsBuf, "{\"sdinit\": %d}", SDC_CS!=255); break;
      case GETPLAYERMODE: sprintf (wsBuf, "{\"playermode\": \"%s\"}", config.getMode()==PM_SDCARD?"modesd":"modeweb"); break;
      #ifdef USE_SD
        case CHANGEMODE:    config.changeMode(config.newConfigMode); return; break;
      #endif
      default:          break;
    }
    if (strlen(wsBuf) > 0) {
      if (clientId == 0) { websocket.textAll(wsBuf); }else{ websocket.text(clientId, wsBuf); }
  #ifdef MQTT_ROOT_TOPIC
      if (clientId == 0 && (request.type == STATION || request.type == ITEM || request.type == TITLE || request.type == MODE)) mqttPublishStatus();
      if (clientId == 0 && request.type == VOLUME) mqttPublishVolume();
  #endif
    }
  }
}

#undef APPEND_GROUP

void NetServer::loop() {
#if IBT_STREAM_ART_DIAG
  return;
#endif
  WebServiceOwner::Scope service(webServiceOwner,xTaskGetCurrentTaskHandle());
  if(!service)return; // before queue, cleanup, imports, reboot or server transitions
  static uint32_t lastReport=0,lastRefused=0,lastRecursive=0;
  const uint32_t refused=webServiceOwner.refused(),recursive=webServiceOwner.recursive();
  const uint32_t now=millis();
  if((refused!=lastRefused||recursive!=lastRecursive)&&uint32_t(now-lastReport)>=10000){
    lastReport=now;lastRefused=refused;lastRecursive=recursive;
    Serial.printf("[WEB-OWNER] refused=%lu recursive=%lu core=%d\n",
      (unsigned long)refused,(unsigned long)recursive,xPortGetCoreID());
  }
  if(!webReady)return;
  stationsave::poll();
  if(userResetQueued.load()){
    if(player.isRunning()||player.status()!=STOPPED||btaudio::wanted()||dataImportBusy||dataUploadOwner||webboardUploadOwner){
      userResetQueued=false;Serial.println("[USER-RESET] refused state changed");
    }else if(config.queueUserReset()){
      Serial.println("[USER-RESET] queued for next boot; connections retained");
      delay(300);ESP.restart();return;
    }else{userResetQueued=false;Serial.println("[USER-RESET] FAIL could not persist request");}
  }
  if(btaudio::webBlocked()||network.offlinePlayer()){
    if(!webBtSuspended){
      Serial.printf("[WEB-BT] suspend list_head=%p\n", websocket.clientListHeadAddress());
      // The AsyncTCP callbacks close established clients when webBlocked() becomes true.
      // closeAll() traverses the client list during this transition and can re-enter
      // its disconnect removal path. Stop accepting clients without walking that list.
      websocket.enable(false);if(network.offlinePlayer()){webserver.end();webListenerStopped=true;}webBtSuspended=true;
      Serial.printf("[WEB-BT] websocket=OFF listener=%s\n",webListenerStopped?"OFF":"SAVE_CONTROL_ONLY");
    }
    if(!network.offlinePlayer()&&webListenerStopped&&network.status==CONNECTED){webserver.begin();webListenerStopped=false;}
    // Preserve already queued local source/playlist changes, drop only notifications.
    processQueue();
    return;
  }
  if(webBtSuspended){
    if(network.status!=CONNECTED&&network.status!=SOFT_AP)return;
    Serial.printf("[WEB-BT] resume list_head=%p\n", websocket.clientListHeadAddress());
    if(webListenerStopped){webserver.begin();webListenerStopped=false;}websocket.enable(true);webBtSuspended=false;
    Serial.println("[WEB-BT] ON");
  }
  if(network.status==SDREADY) return;
  if (shouldReboot) {
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }
  processQueue();
  websocket.cleanupClients();
  switch (importRequest) {
    case IMPL: { const bool good=importPlaylist();dataImportResult=good?2:3;stationsave::imported(dataImportJob,good);dataImportJob=0;importRequest=IMDONE;dataImportBusy=false;break; }
    case IMWIFI:  dataImportResult=config.saveWifi()?2:3; importRequest = IMDONE; dataImportBusy=false; break;
    default:      break;
  }
  //processQueue();
}

#if IR_PIN!=255
void NetServer::irToWs(const char* protocol, uint64_t irvalue) {
  char message[160];
  snprintf (message,sizeof(message), "{\"ircode\": %llu, \"protocol\": \"%s\"}", irvalue, protocol);
  websocket.textAll(message);
}
void NetServer::irValsToWs() {
  if (!irRecordEnable) return;
  char message[160];
  snprintf (message,sizeof(message), "{\"irvals\": [%llu, %llu, %llu]}", config.ircodes.irVals[config.irindex][0], config.ircodes.irVals[config.irindex][1], config.ircodes.irVals[config.irindex][2]);
  websocket.textAll(message);
}
#endif

void NetServer::onWsMessage(void *arg, uint8_t *data, size_t len, uint8_t clientId) {
  if(btaudio::webBlocked())return;
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (config.parseWsCommand((const char*)data, _wscmd, _wsval, 65)) {
      if (strcmp(_wscmd, "ping") == 0) {
        websocket.text(clientId, "{\"pong\": 1}");
        return;
      }
      if (strcmp(_wscmd, "trebble") == 0) {
        int8_t valb = atoi(_wsval);
        config.setTone(config.store.bass, config.store.middle, valb);
        return;
      }
      if (strcmp(_wscmd, "middle") == 0) {
        int8_t valb = atoi(_wsval);
        config.setTone(config.store.bass, valb, config.store.trebble);
        return;
      }
      if (strcmp(_wscmd, "bass") == 0) {
        int8_t valb = atoi(_wsval);
        config.setTone(valb, config.store.middle, config.store.trebble);
        return;
      }
      if (strcmp(_wscmd, "submitplaylistdone") == 0) {
#ifdef MQTT_ROOT_TOPIC
        //mqttplaylistticker.attach(5, mqttplaylistSend);
        timekeeper.waitAndDo(5, mqttplaylistSend);
#endif
        if (player.isRunning()) player.sendCommand({PR_PLAY, -config.lastStation()});
        return;
      }
      
      if(cmd.exec(_wscmd, _wsval, clientId)){
        return;
      }
    }
  }
}

void NetServer::getPlaylist(uint8_t clientId) {
  if(btaudio::webBlocked())return;
  char message[BUFLEN];
  snprintf(message,sizeof(message), "{\"file\": \"http://%s%s\"}", config.ipToStr(WiFi.localIP()), PLAYLIST_PATH);
  if (clientId == 0) { websocket.textAll(message); } else { websocket.text(clientId, message); }
}

int NetServer::_readPlaylistLine(File &file, char * line, size_t size){
  if(!line||size<2)return -1;
  size_t used=0;bool overflow=false;
  while(file.available()){
    int ch=file.read();if(ch<0)return -1;
    if(ch=='\n')break;
    if(ch==0){overflow=true;continue;}
    if(used<size-1)line[used++]=char(ch);else overflow=true;
  }
  if(overflow)return -1;
  if(used&&line[used-1]=='\r')used--;
  line[used]=0;
  return int(used);
}

bool NetServer::importPlaylist() {
  if(config.getMode()==PM_SDCARD || player.isRunning() || player.status()!=STOPPED) return false;
  //player.sendCommand({PR_STOP, 0});
  File tempfile = SPIFFS.open(TMP_PATH, "r");
  if (!tempfile) {
    return false;
  }
  char linePl[BUFLEN*3];
  char nsBuf[BUFLEN],nsBuf2[BUFLEN]; // importer-local, also safe in /webboard callback
  int sOvol;
  int lineSize=_readPlaylistLine(tempfile, linePl, sizeof(linePl));
  if(lineSize<=0){tempfile.close();SPIFFS.remove(TMP_PATH);return false;}
  if (config.parseCSV(linePl, nsBuf, nsBuf2, sOvol)) {
    unsigned rows=1;
    while(tempfile.available()){
      lineSize=_readPlaylistLine(tempfile,linePl,sizeof(linePl));
      if(lineSize<=0||++rows>480||!config.parseCSV(linePl,nsBuf,nsBuf2,sOvol)){
        tempfile.close();SPIFFS.remove(TMP_PATH);return false;
      }
    }
    tempfile.close();
    if(player.isRunning()||player.status()!=STOPPED)return false;
    if(!config.installPlaylist(TMP_PATH))return false;
    dual::invalidateStationSearchCache();
    dual::invalidateFavorites();
    dual::preloadStationSearch();
    requestOnChange(PLAYLIST, 0);
    return true;
  }
  if (config.parseJSON(linePl, nsBuf, nsBuf2, sOvol)) {
    SPIFFS.remove(PLAYLIST_IMPORT_PATH);
    File playlistfile = SPIFFS.open(PLAYLIST_IMPORT_PATH, "w");
    if (!playlistfile) {tempfile.close();return false;}
    unsigned rows=1;
    int written=snprintf(linePl,sizeof(linePl),"%s\t%s\t%d",nsBuf,nsBuf2,sOvol);
    bool good=written>0&&size_t(written)<sizeof(linePl)&&playlistfile.println(linePl)>size_t(written);
    while (tempfile.available()) {
      lineSize=_readPlaylistLine(tempfile,linePl,sizeof(linePl));
      if(lineSize<=0||++rows>480||!config.parseJSON(linePl,nsBuf,nsBuf2,sOvol)){good=false;break;}
      written=snprintf(linePl,sizeof(linePl),"%s\t%s\t%d",nsBuf,nsBuf2,sOvol);
      if(written<=0||size_t(written)>=sizeof(linePl)||playlistfile.println(linePl)<=size_t(written)){good=false;break;}
    }
    playlistfile.flush();
    playlistfile.close();
    tempfile.close();
    SPIFFS.remove(TMP_PATH);
    if(!good){SPIFFS.remove(PLAYLIST_IMPORT_PATH);return false;}
    if(player.isRunning()||player.status()!=STOPPED)return false;
    if (!config.installPlaylist(PLAYLIST_IMPORT_PATH)) return false;
    dual::invalidateStationSearchCache();
    dual::invalidateFavorites();
    dual::preloadStationSearch();
    requestOnChange(PLAYLIST, 0);
    return true;
  }
  tempfile.close();
  SPIFFS.remove(TMP_PATH);
  return false;
}

void NetServer::requestOnChange(requestType_e request, uint8_t clientId) {
#if IBT_STREAM_ART_DIAG
  return;
#endif
  if(btaudio::webBlocked() && request!=PLAYLISTSAVED && request!=CHANGEMODE)return;
  if(nsQueue==NULL) return;
  nsRequestParams_t nsrequest;
  nsrequest.type = request;
  nsrequest.clientId = clientId;
  // Audio-originated state changes must never wait 300 ms for a slow WebUI.
  // Coalesce only state notifications, never playlist/source mutation commands.
  constexpr uint32_t stateMask=(1u<<STATION)|(1u<<STATIONNAME)|(1u<<TITLE)|(1u<<VOLUME)|
    (1u<<NRSSI)|(1u<<BITRATE)|(1u<<MODE)|(1u<<EQUALIZER)|(1u<<BALANCE)|(1u<<SDPOS)|(1u<<SDLEN);
  if(clientId==0 && player.onAudioTask() && (stateMask&(1u<<request))){
    if(xQueueSend(nsQueue,&nsrequest,0)!=pdPASS)deferredAudioNotifications.add(1u<<request);
    return;
  }
  xQueueSend(nsQueue, &nsrequest, NSQ_SEND_DELAY);
}

void NetServer::resetQueue(){
  if(nsQueue!=NULL) xQueueReset(nsQueue);
  webQueueEpoch.fetch_add(1,std::memory_order_release);
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if(request->url()=="/upload"){
    if (!index) {
      if(userResetQueued.load() || dataUploadOwner || webboardUploadOwner || dataImportBusy) return;
      const uint32_t saveJob=stationSaveJob(request->header("X-Station-Save"));
      if(stationsave::holdsAudio()&&(!stationsave::acceptsUpload(saveJob)||filename!="playlist.csv"))return;
      dataUploadOwner=request;
      auto* state=(DataUploadState*)calloc(1,sizeof(DataUploadState));
      request->_tempObject=state;
      request->onDisconnect([request](){
        if(dataUploadOwner==request){
          auto* state=(DataUploadState*)request->_tempObject;if(state)stationsave::uploadAborted(state->saveJob);
          dataUploadOwner=nullptr;if(state&&state->ownsTemp)SPIFFS.remove(TMP_PATH);
        }
      });
      if(!state)return;
      state->saveJob=saveJob;
      state->kind=filename=="playlist.csv"?1:(filename=="wifi.csv"||filename=="tempwifi.csv"?2:0);
      state->limit=state->kind==1?256*1024:5*(WIFI_SSID_CAP+WIFI_PASS_CAP+2);
      state->good=state->kind && !player.isRunning() && player.status()==STOPPED && config.getMode()==PM_WEB;
      // Stage imports first. Wi-Fi uploads must never erase the playlist.
      if(state->good&&saveJob)state->good=stationsave::uploadStarted(saveJob);
      if(state->good){state->ownsTemp=true;SPIFFS.remove(TMP_PATH);request->_tempFile=SPIFFS.open(TMP_PATH,"w");}
      state->good=state->good && bool(request->_tempFile);
    }
    auto* state=(DataUploadState*)request->_tempObject;
    if(dataUploadOwner!=request || !state)return;
    if (len) {
      if(!state->good || index!=state->written || len>state->limit-state->written || request->_tempFile.write(data,len)!=len)state->good=false;
      else state->written+=len;
    }
    if (final) {
      state->complete=true;
      // File::write uses buffered stdio. size()/stat cannot see its last
      // partial buffer until flush; this used to reject complete CSV uploads.
      request->_tempFile.flush();
      state->good=state->good && state->written && request->_tempFile.size()==state->written;
      Serial.printf("[DATA-UPLOAD] kind=%u bytes=%u persisted=%u complete=%u accepted=%u\n",unsigned(state->kind),unsigned(state->written),unsigned(request->_tempFile.size()),unsigned(state->complete),unsigned(state->good));
      request->_tempFile.close();
      if(!state->good&&state->ownsTemp)SPIFFS.remove(TMP_PATH);
    }
  }else if(request->url()=="/update"){
    if(!webFirmwareUpdateAllowed(request)) {
      if(!index) Serial.println("Firmware web update disabled for SMALL single-app layout; use USB flashing.");
      return;
    }
    if (!index) {
      int target = (request->getParam("updatetarget", true)->value() == "spiffs") ? U_SPIFFS : U_FLASH;
      Serial.printf("Update Start: %s\n", filename.c_str());
      player.sendCommand({PR_STOP, 0});
      display.putRequest(NEWMODE, UPDATING);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, target)) {
        Update.printError(Serial);
        request->send(200, "text/html", updateError());
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
        request->send(200, "text/html", updateError());
      }
    }
    if (final) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %uB\n", index + len);
      } else {
        Update.printError(Serial);
        request->send(200, "text/html", updateError());
      }
    }
  }else if(request->url()=="/webboard"){
    DBGVB("File: %s, size:%u bytes, index: %u, final: %s\n", filename.c_str(), len, index, final?"true":"false");
    if (!index) {
      auto* state=(WebboardUploadState*)request->_tempObject;
      if(!state){
        state=(WebboardUploadState*)calloc(1,sizeof(WebboardUploadState));
        request->_tempObject=state;
        if(state)request->onDisconnect([request](){
          if(webboardUploadOwner==request){
            webboardUploadOwner=nullptr;
            auto* upload=(WebboardUploadState*)request->_tempObject;
            if(upload && upload->ownsTemp)SPIFFS.remove(TMP_PATH);
          }
        });
      }
      if(!state)return;
      if(state->activeKind){request->_tempFile.close();state->failed=true;}
      state->activeKind=0;state->written=0;state->complete=false;
      if(webboardUploadOwner && webboardUploadOwner!=request)state->failed=true;
      else webboardUploadOwner=request;
      if(!safeWebboardName(filename) || filename=="wifi.csv" || filename=="tempwifi.csv" ||
         dataUploadOwner || dataImportBusy || stationsave::holdsAudio() || player.isRunning() || player.status()!=STOPPED ||
         config.getMode()!=PM_WEB)state->failed=true;
      if(state->failed)return;
      state->activeKind=filename=="playlist.csv"?1:2;
      state->limit=state->activeKind==1?256*1024:1024*1024;
      if(state->activeKind==1){
        if(state->playlistSeen){state->failed=true;return;}
        state->ownsTemp=true;
        SPIFFS.remove(TMP_PATH);
        request->_tempFile=SPIFFS.open(TMP_PATH,"w");
      }else request->_tempFile=SPIFFS.open(String("/www/")+filename,"w");
      if(!request->_tempFile)state->failed=true;
    }
    auto* state=(WebboardUploadState*)request->_tempObject;
    if(!state || state->failed || webboardUploadOwner!=request || !state->activeKind)return;
    if (len) {
      if(index!=state->written || len>state->limit-state->written || request->_tempFile.write(data,len)!=len)state->failed=true;
      else state->written+=len;
    }
    if (final) {
      request->_tempFile.flush();
      if(!state->written || request->_tempFile.size()!=state->written)state->failed=true;
      request->_tempFile.close();
      if(state->activeKind==1){
        if(state->failed)SPIFFS.remove(TMP_PATH);
        else state->playlistSeen=true;
      }
      state->activeKind=0;state->complete=!state->failed;
    }
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: /*netserver.requestOnChange(STARTUP, client->id()); */if (config.store.audioinfo) Serial.printf("[WEBSOCKET] client #%lu connected from %s\n", client->id(), config.ipToStr(client->remoteIP())); break;
    case WS_EVT_DISCONNECT: if (config.store.audioinfo) Serial.printf("[WEBSOCKET] client #%lu disconnected\n", client->id()); break;
    case WS_EVT_DATA: netserver.onWsMessage(arg, data, len, client->id()); break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}
void handleNotFound(AsyncWebServerRequest * request) {
  if(network.status==SOFT_AP){
    if(request->method()==HTTP_GET){request->redirect("http://192.168.4.1/");return;}
    request->send(404,"text/plain","Wi-Fi setup: open http://192.168.4.1/");
    return;
  }
#if defined(HTTP_USER) && defined(HTTP_PASS)
  if(network.status == CONNECTED)
    if (request->url() == "/logout") {
      request->send(401);
      return;
    }
    if (!request->authenticate(HTTP_USER, HTTP_PASS)) {
      return request->requestAuthentication();
    }
#endif
  if(request->url()=="/emergency") { request->send_P(200, "text/html", emergency_form); return; }
  if (request->method() == HTTP_GET) {
    DBGVB("[%s] client ip=%s request of %s", __func__, config.ipToStr(request->client()->remoteIP()), request->url().c_str());
    if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 || 
        strcmp(request->url().c_str(), SSIDS_PATH) == 0 || 
        strcmp(request->url().c_str(), INDEX_PATH) == 0 || 
        strcmp(request->url().c_str(), TMP_PATH) == 0 || 
        strcmp(request->url().c_str(), PLAYLIST_SD_PATH) == 0 || 
        strcmp(request->url().c_str(), INDEX_SD_PATH) == 0) {
#ifdef MQTT_ROOT_TOPIC
      if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0) while (mqttplaylistblock) vTaskDelay(5);
#endif
      if(strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 && config.getMode()==PM_SDCARD){
        netserver.chunkedHtmlPage("application/octet-stream", request, PLAYLIST_SD_PATH);
      }else{
        netserver.chunkedHtmlPage("application/octet-stream", request, request->url().c_str());
      }
      return;
    }// if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 || 
  }// if (request->method() == HTTP_GET)
  
  if (request->method() == HTTP_POST) {
    if(request->url()=="/webboard"){
      auto* state=(WebboardUploadState*)request->_tempObject;
      bool ok=webboardUploadOwner==request && state && state->complete && !state->failed && !state->activeKind;
      if(ok && state->playlistSeen)ok=netserver.importPlaylist();
      if(webboardUploadOwner==request){webboardUploadOwner=nullptr;if(!ok && state && state->ownsTemp)SPIFFS.remove(TMP_PATH);}
      if(!ok){request->send(409,"text/plain","WebUI upload or playlist validation failed");return;}
      request->redirect("/");
      if(config.emptyFS)ESP.restart();
      return;
    }
    if(request->url()=="/upload"){ // <--upload playlist.csv or wifi.csv
      auto* state=(DataUploadState*)request->_tempObject;
      bool ours=dataUploadOwner==request;
      bool valid=ours && state && state->complete && state->good &&
                 SPIFFS.exists(TMP_PATH);
      if(!valid){if(ours){if(state)stationsave::uploadAborted(state->saveJob);dataUploadOwner=nullptr;if(state&&state->ownsTemp)SPIFFS.remove(TMP_PATH);}request->send(409,"text/plain","Upload incomplete or busy");return;}
      if (state->kind==1 && request->hasParam("plfile", true, true)) {
        dataImportBusy=true;dataUploadOwner=nullptr;dataImportJob=state->saveJob;
        dataImportResult=1;
        netserver.importRequest = IMPL;
        request->send(202,"application/json","{\"status\":\"pending\",\"check\":\"/import-status\"}");
      } else if (state->kind==2 && request->hasParam("wifile", true, true)) {
        dataImportBusy=true;dataUploadOwner=nullptr;
        dataImportResult=1;
        netserver.importRequest = IMWIFI;
        request->send(202,"application/json","{\"status\":\"pending\",\"check\":\"/import-status\"}");
      } else {
        stationsave::uploadAborted(state->saveJob);
        dataUploadOwner=nullptr;
        if(state->ownsTemp)SPIFFS.remove(TMP_PATH);
        request->send(400,"text/plain","Invalid upload field");
      }
      return;
    }
    if(request->url()=="/update"){ // <--upload firmware/filesystem
      if(!webFirmwareUpdateAllowed(request)) {
        shouldReboot = false;
        request->send(409, "text/plain", "Firmware web update disabled on SMALL single-app layout. Flash firmware over USB.");
        return;
      }
      shouldReboot = !Update.hasError();
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : updateError());
      response->addHeader("Connection", "close");
      request->send(response);
      return;
    }
  }// if (request->method() == HTTP_POST)
  
  if (request->url() == "/favicon.ico") {
    request->send(200, "image/x-icon", "data:,");
    return;
  }
  if (request->url() == "/variables.js") {
    char variables[BUFLEN];
    snprintf(variables,sizeof(variables),"var yoVersion='%s';\nvar formAction='%s';\nvar playMode='%s';\n",YOVERSION,(network.status==CONNECTED&&!config.emptyFS)?"webboard":"",network.status==CONNECTED?"player":"ap");
    request->send(200,"text/html",variables);
    return;
  }
  if (strcmp(request->url().c_str(), "/settings.html") == 0 || strcmp(request->url().c_str(), "/update.html") == 0 || strcmp(request->url().c_str(), "/ir.html") == 0){
    // Main HTML is served from the exact-length gzip flash asset.
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html; charset=utf-8", index_html_gz, sizeof(index_html_gz));
    response->addHeader("Content-Encoding","gzip");
    response->addHeader("Cache-Control","no-cache");
    response->addHeader("ETag", String("\"")+IBT_FW_VERSION+"-ui171\"");
    request->send(response);
    return;
  }
  if (request->method() == HTTP_GET && request->url() == "/webboard") {
    request->send_P(200, "text/html", emptyfs_html);
    return;
  }
  Serial.print("Not Found: ");
  Serial.println(request->url());
  request->send(404, "text/plain", "Not found");
}

static String wifiEscapeAttribute(const char* raw){
  String result;
  for(const char* p=raw;p&&*p;p++){
    switch(*p){
      case '&':result+="&amp;";break;
      case '"':result+="&quot;";break;
      case '<':result+="&lt;";break;
      case '>':result+="&gt;";break;
      case '\'':result+="&#39;";break;
      default:result+=*p;break;
    }
  }
  return result;
}

void handleIndex(AsyncWebServerRequest * request) {
  if(network.status==SOFT_AP){
    if(request->method()==HTTP_GET){
      if(network.setupInProgress()){
        AsyncWebServerResponse *response=request->beginResponse_P(200,"text/html; charset=utf-8",wifi_setup_saved_html);
        response->addHeader("Cache-Control","no-store, no-cache, must-revalidate");
        response->addHeader("Connection","close");
        request->send(response);
        return;
      }
      char selected[WIFI_SSID_CAP];
      network.selectedSetupSsid(selected,sizeof(selected));
      String html(wifi_setup_html);
      html.replace("__SELECTED__",wifiEscapeAttribute(selected));
      html.replace("__ERROR__",network.setupFailed()?"<p>Nem sikerült csatlakozni. Ellenőrizd a nevet és a jelszót.</p>":"");
      AsyncWebServerResponse *response=request->beginResponse(200,"text/html; charset=utf-8",html);
      response->addHeader("Cache-Control","no-store, no-cache, must-revalidate");
      response->addHeader("Connection","close");
      request->send(response);
      return;
    }
    if(request->method()==HTTP_POST){
      const String ssid=request->arg("ssid");
      const String pass=request->arg("pass");
      if(ssid.length()==0 || ssid.length()>32 || pass.length()>63){
        request->send(400,"text/plain","Hibas SSID vagy jelszo hossz.");
        return;
      }
      if(!network.applySetupCredentials(ssid.c_str(),pass.c_str())){
        request->send(409,"text/plain","A Wi-Fi adatok nem fogadhatók el, vagy másik csatlakozás folyamatban van.");
        return;
      }
      AsyncWebServerResponse *response=request->beginResponse_P(200,"text/html; charset=utf-8",wifi_setup_saved_html);
      response->addHeader("Cache-Control","no-store, no-cache, must-revalidate");
      response->addHeader("Connection","close");
      request->send(response);
      return;
    }
    request->send(405,"text/plain","Method not allowed");
    return;
  }
#if defined(HTTP_USER) && defined(HTTP_PASS)
  if(network.status == CONNECTED)
    if (!request->authenticate(HTTP_USER, HTTP_PASS)) {
      return request->requestAuthentication();
    }
#endif
  if (request->url()=="/" && request->method()==HTTP_GET && (request->params()==0 || (request->params()==1&&request->hasParam("ibt-ui")&&request->arg("ibt-ui").length()<=96))) {
    if(network.status == CONNECTED) {
      const String etag = String("\"") + IBT_FW_VERSION + "-ui171\"";
      if(request->header("If-None-Match") == etag) {
        AsyncWebServerResponse *response = request->beginResponse(304);
        response->addHeader("ETag", etag);
        response->addHeader("Cache-Control","private, no-cache");
        request->send(response);
        return;
      }
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html; charset=utf-8", index_html_gz, sizeof(index_html_gz));
    response->addHeader("Content-Encoding","gzip");
      response->addHeader("ETag", etag);
      response->addHeader("Cache-Control","private, no-cache");
      request->send(response);
      // Main HTML is served from the exact-length gzip flash asset. 
    } else request->redirect("/settings.html");
    return;
  }
  if(network.status == CONNECTED){
    int paramsNr = request->params();
    if(paramsNr==1){
      AsyncWebParameter* p = request->getParam(0);
      if(cmd.exec(p->name().c_str(),p->value().c_str())) {
        if(p->name()=="reset" || p->name()=="clearspiffs") request->redirect("/");
        if(p->name()=="clearspiffs") { delay(100); ESP.restart(); }
        request->send(200, "text/plain", "");
        return;
      }
    }
    if (request->hasArg("trebble") && request->hasArg("middle") && request->hasArg("bass")) {
      config.setTone(request->getParam("bass")->value().toInt(), request->getParam("middle")->value().toInt(), request->getParam("trebble")->value().toInt());
      request->send(200, "text/plain", "");
      return;
    }
    if (request->hasArg("sleep")) {
      int sford = request->getParam("sleep")->value().toInt();
      int safterd = request->hasArg("after")?request->getParam("after")->value().toInt():0;
      if(sford > 0 && safterd >= 0){ request->send(200, "text/plain", ""); config.sleepForAfter(sford, safterd); return; }
    }
    request->send(404, "text/plain", "Not found");
    
  }else{
    request->send(404, "text/plain", "Not found");
  }
}
