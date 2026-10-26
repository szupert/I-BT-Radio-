#include "options.h"
#include <atomic>
#include <esp_heap_caps.h>
static std::atomic<bool> localPlayerOnly{false},localPlayerRequest{false},localPlayerReady{false};
static std::atomic<bool> wifiAttemptOwned{false},savedSearchRunning{false},onlineRequest{false};
static std::atomic<bool> setupModeRequest{false};
static void launchSavedSearch();
#if IBT_NO_PSRAM
static std::atomic<bool> btNetworkRequested{false},btNetworkLease{false},btNetworkRestore{false};
#endif
#include <DNSServer.h>
#include "time.h"
#include "rtcsupport.h"
#include "network.h"
#include "display.h"
#include "config.h"
#include "telnet.h"
#include "netserver.h"
#include "player.h"
#include "mqtt.h"
#include "timekeeper.h"
#include "../pluginsManager/pluginsManager.h"
#include <esp_wifi.h>
#include "../dual/bt_audio.h"
#include "../dual/wifi_bt_policy.h"
#include "../dual/clock_sync_policy.h"
#include "../dual/dns_auto.h"
#include "../dual/core.h"

#ifndef WIFI_ATTEMPTS
  #define WIFI_ATTEMPTS  16
#endif

#ifndef SEARCH_WIFI_CORE_ID
  #define SEARCH_WIFI_CORE_ID  0
#endif
MyNetwork network;
static bool btRadioReserved(){return btaudio::wanted()||!btaudio::idle();}
static void serviceBtWifiPolicy(){
  static ibtcoex::Policy policy;
  const bool staReady=WiFi.getMode()==WIFI_STA&&WiFi.status()==WL_CONNECTED;
  policy.tick(millis(),staReady,btRadioReserved(),[](bool bt){
    wifi_ps_type_t actual=WIFI_PS_NONE;
    const wifi_ps_type_t target=bt?WIFI_PS_MIN_MODEM:WIFI_PS_NONE;
    const esp_err_t read=esp_wifi_get_ps(&actual);
    if(read!=ESP_OK)return false;
    if(actual==target)return true;
    const esp_err_t result=esp_wifi_set_ps(target);
    Serial.printf("[WIFI-BT] ps=%s result=%d\n",bt?"MIN_MODEM":"NONE",int(result));
    return result==ESP_OK;
  });
}


void MyNetwork::WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  if(network.offlinePlayer()||network.status==SOFT_AP)return; // setup service owns stable STA-to-AP transition
  network.beginReconnect = false;
  player.lockOutput = false;
  delay(100);
  display.putRequest(NEWMODE, PLAYER);
  if(config.getMode()==PM_SDCARD) {
    network.status=CONNECTED;
    display.putRequest(NEWIP, 0);
  }else{
    display.putRequest(NEWMODE, PLAYER);
    if (network.lostPlaying) player.sendCommand({PR_PLAY, config.lastStation()});
  }
  #ifdef MQTT_ROOT_TOPIC
    connectToMqtt();
  #endif
}

void MyNetwork::WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info){
  if(network.offlinePlayer()||network.status==SOFT_AP)return; // setup portal owns Wi-Fi until valid STA credentials exist
  if(!network.beginReconnect){
    uint8_t idx=(config.store.lastSSID>0 && config.store.lastSSID<=config.ssidsCount)?config.store.lastSSID-1:0;
    Serial.printf("Lost connection, reconnecting to %s...\n", config.ssidsCount?config.ssids[idx].ssid:"known Wi-Fi");
    if(config.getMode()==PM_SDCARD) {
      network.status=SDREADY;
      display.putRequest(NEWIP, 0);
    }else{
      network.lostPlaying = player.isRunning();
      if (network.lostPlaying) { player.lockOutput = true; player.sendCommand({PR_STOP, 0}); }
      display.putRequest(NEWMODE, LOST);
    }
  }
  network.beginReconnect = true;
  WiFi.reconnect();
}

static volatile uint16_t v027DisconnectReason=0;
static bool v027EventsRegistered=false;
static bool v027ReconnectCallbacks=false;
static uint8_t v027RecoveryIndex=0;
static char v027SetupSsid[32]="I-BT-Radio-Setup";
static DNSServer v15CaptiveDns;
static bool v15CaptiveDnsStarted=false;
static uint32_t v15StaConnectedAt=0;
static uint32_t v16StaAttemptAt=0;
static bool v16StaAttemptActive=false;
static bool v16SetupFailed=false;
static uint32_t recoveryStarted=0;
static bool recoveryWaiting=false;
static portMUX_TYPE wifiStateMux=portMUX_INITIALIZER_UNLOCKED;
static char connectingSsid[WIFI_SSID_CAP]={0};
static uint8_t connectingAttempt=0;
static char selectedSetupSsidValue[WIFI_SSID_CAP]={0};
static char pendingSsid[WIFI_SSID_CAP]={0},pendingPass[WIFI_PASS_CAP]={0};
static char activeSsid[WIFI_SSID_CAP]={0},activePass[WIFI_PASS_CAP]={0};
static bool setupRequest=false,setupBusy=false;
static uint32_t setupQueuedAt=0;
struct ScanEntry {char ssid[WIFI_SSID_CAP];int16_t rssi;};
static ScanEntry scanEntries[12]={};
static uint8_t scanEntryCount=0,scanPhase=0; // idle, searching, ready, failed
static bool scanRequested=false,scanCancelRequested=false;
static uint32_t scanStarted=0;
static void showConnecting(const char* ssid,uint8_t attempt){
  portENTER_CRITICAL(&wifiStateMux);
  strlcpy(connectingSsid,ssid?ssid:"",sizeof(connectingSsid));
  connectingAttempt=attempt;
  portEXIT_CRITICAL(&wifiStateMux);
}
bool MyNetwork::connectingInfo(char* ssid,size_t capacity,uint8_t* attempt) const{
  if(!ssid||!capacity)return false;
  portENTER_CRITICAL(&wifiStateMux);
  strlcpy(ssid,connectingSsid,capacity);
  if(attempt)*attempt=connectingAttempt;
  portEXIT_CRITICAL(&wifiStateMux);
  return ssid[0]!=0;
}
bool MyNetwork::selectedSetupSsid(char* ssid,size_t capacity) const{
  if(!ssid||!capacity)return false;
  portENTER_CRITICAL(&wifiStateMux);
  strlcpy(ssid,selectedSetupSsidValue,capacity);
  portEXIT_CRITICAL(&wifiStateMux);
  return ssid[0]!=0;
}
bool MyNetwork::requestScan(){
  if(wifiAttemptOwned.load()||savedSearchRunning.load()||onlineRequest.load()||localPlayerRequest.load())return false;
  if(btRadioReserved())return false;
  if(status!=SOFT_AP && (player.isRunning()||player.status()!=STOPPED))return false;
  if(offlinePlayer())startSetup();
  portENTER_CRITICAL(&wifiStateMux);
  const bool accepted=!setupBusy;
  if(accepted){scanRequested=true;scanCancelRequested=false;scanPhase=1;scanEntryCount=0;}
  portEXIT_CRITICAL(&wifiStateMux);
  return accepted;
}
void MyNetwork::cancelScan(){
  portENTER_CRITICAL(&wifiStateMux);
  scanCancelRequested=true;scanRequested=false;
  portEXIT_CRITICAL(&wifiStateMux);
}
uint8_t MyNetwork::scanStatus() const{
  portENTER_CRITICAL(&wifiStateMux);
  const uint8_t result=scanPhase;
  portEXIT_CRITICAL(&wifiStateMux);
  return result;
}
unsigned MyNetwork::scanCount() const{
  portENTER_CRITICAL(&wifiStateMux);
  const unsigned result=scanEntryCount;
  portEXIT_CRITICAL(&wifiStateMux);
  return result;
}
bool MyNetwork::scanEntry(unsigned index,char* ssid,size_t capacity,int* rssi) const{
  if(!ssid||!capacity)return false;
  portENTER_CRITICAL(&wifiStateMux);
  const bool valid=scanPhase==2&&index<scanEntryCount;
  if(valid){strlcpy(ssid,scanEntries[index].ssid,capacity);if(rssi)*rssi=scanEntries[index].rssi;}
  else ssid[0]=0;
  portEXIT_CRITICAL(&wifiStateMux);
  return valid;
}
int MyNetwork::selectSetupNetwork(unsigned index){
  char ssid[WIFI_SSID_CAP];
  if(!scanEntry(index,ssid,sizeof(ssid)))return 0;
  portENTER_CRITICAL(&wifiStateMux);
  strlcpy(selectedSetupSsidValue,ssid,sizeof(selectedSetupSsidValue));
  portEXIT_CRITICAL(&wifiStateMux);
  for(unsigned i=0;i<config.ssidsCount;i++)if(strcmp(config.ssids[i].ssid,ssid)==0){
    if(v16SetupFailed)return 2; // stale saved password can be replaced locally
    return connectScannedNetwork(ssid,config.ssids[i].password)?1:0;
  }
  return 2;
}
static const char* reasonName(uint16_t r){
  switch(r){
    case 201:return "NO_AP_FOUND"; case 202:return "AUTH_FAIL";
    case 203:return "ASSOC_FAIL"; case 204:return "HANDSHAKE_TIMEOUT";
    case 205:return "CONNECTION_FAIL"; case 200:return "BEACON_TIMEOUT";
    case 15:return "4WAY_HANDSHAKE_TIMEOUT"; case 2:return "AUTH_EXPIRE";
    default:return "SEE_ESP_REASON_CODE";
  }
}
void MyNetwork::diagnosticSnapshot(uint8_t clientId) const {
  telnet.printf(clientId,"[WIFI-SETUP-STATE] mode=%u status=%u offline=%u busy=%u ap_clients=%u free=%u largest=%u\n",
    unsigned(WiFi.getMode()),unsigned(status),unsigned(offlinePlayer()),unsigned(setupInProgress()),
    unsigned(WiFi.softAPgetStationNum()),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
    unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
  const auto ip=WiFi.localIP(), gateway=WiFi.gatewayIP();
  const auto dns0=WiFi.dnsIP(0), dns1=WiFi.dnsIP(1);
  const bool sta=WiFi.getMode()==WIFI_STA||WiFi.getMode()==WIFI_AP_STA;
  const bool ready=sta&&WiFi.status()==WL_CONNECTED&&uint32_t(ip)!=0;
  const bool dnsPresent=uint32_t(dns0)!=0||uint32_t(dns1)!=0;
  // Each complete line fits the existing 220-byte USB formatter.
  telnet.printf(clientId,"[WIFI-NET] connected=%u ip_valid=%u gateway_valid=%u dns_valid=%u sta=%u readiness=%s\n",
    unsigned(WiFi.status()==WL_CONNECTED),unsigned(uint32_t(ip)!=0),
    unsigned(uint32_t(gateway)!=0),unsigned(dnsPresent),unsigned(sta),
    !ready?"WAIT_WIFI":!dnsPresent?"NO_DNS":radiodnsauto::status());
  telnet.printf(clientId,"[WIFI-DNS] ip=%s gateway=%s dns0=%s dns1=%s\n",
    ip.toString().c_str(),gateway.toString().c_str(),dns0.toString().c_str(),dns1.toString().c_str());
}
static void observeWiFi(WiFiEvent_t event,WiFiEventInfo_t info){
  if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
    v027DisconnectReason=info.wifi_sta_disconnected.reason;
    Serial.printf("[WIFI] DISCONNECTED reason=%u name=%s\n",unsigned(v027DisconnectReason),reasonName(v027DisconnectReason));
  } else if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP){
    Serial.printf("[WIFI] GOT_IP ip=%s rssi=%d\n",WiFi.localIP().toString().c_str(),WiFi.RSSI());
    network.diagnosticSnapshot();
  }
}
bool MyNetwork::wifiBegin(bool silent){
  if(offlinePlayer())return false;
  bool unowned=false;if(!wifiAttemptOwned.compare_exchange_strong(unowned,true))return false;
  struct Owner {~Owner(){wifiAttemptOwned.store(false);}} owner;
  if(config.ssidsCount==0){Serial.println("[WIFI] NO_SAVED_CREDENTIALS");return false;}
  if(!v027EventsRegistered){WiFi.onEvent(observeWiFi);v027EventsRegistered=true;}
  WiFi.persistent(false); // Do not rewrite the SDK's credential storage on each retry.
  WiFi.setAutoReconnect(false); // Only one bounded boot attempt may own the STA at a time.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  const unsigned first=(config.store.lastSSID>0 && config.store.lastSSID<=config.ssidsCount)?config.store.lastSSID-1:0;
  showConnecting(config.ssids[first].ssid,0);
  // One initial bounded scan: report ONLY matches to saved networks, not passwords
  // or neighbouring SSIDs. No auth/security threshold is silently weakened.
  int networks=WiFi.scanNetworks(true,true,false,200);
  const uint32_t scanAt=millis();
  while(networks==WIFI_SCAN_RUNNING&&uint32_t(millis()-scanAt)<10000){
    if(offlinePlayer()){esp_wifi_scan_stop();WiFi.scanDelete();showConnecting(nullptr,0);Serial.println("[WIFI] BOOT_SCAN=CANCELLED offline=1");return false;}
    delay(25);networks=WiFi.scanComplete();
  }
  if(offlinePlayer()){esp_wifi_scan_stop();WiFi.scanDelete();showConnecting(nullptr,0);return false;}
  if(networks==WIFI_SCAN_RUNNING){esp_wifi_scan_stop();WiFi.scanDelete();showConnecting(nullptr,0);return false;}
  for(unsigned n=0;n<config.ssidsCount;n++){
    bool found=false;
    for(int i=0;i<networks;i++)if(WiFi.SSID(i)==config.ssids[n].ssid){
      found=true;Serial.printf("[WIFI] SAVED_AP slot=%u visible=1 channel=%d rssi=%d auth=%d\n",n+1,WiFi.channel(i),WiFi.RSSI(i),int(WiFi.encryptionType(i)));
    }
    if(!found)Serial.printf("[WIFI] SAVED_AP slot=%u visible=0 scan_result=%d\n",n+1,networks);
  }
  WiFi.scanDelete();
  if(networks==0){showConnecting(nullptr,0);Serial.println("[WIFI] NO_AP_VISIBLE SETUP_REQUIRED");return false;}
  for(unsigned slot=0;slot<config.ssidsCount;slot++){
    const unsigned index=(first+slot)%config.ssidsCount;
    for(unsigned attempt=1;attempt<=3;attempt++){
      if(offlinePlayer()){showConnecting(nullptr,0);return false;}
      WiFi.disconnect(false,false); // Never erase saved credentials.
      delay(100);v027DisconnectReason=0;
      showConnecting(config.ssids[index].ssid,attempt);
      Serial.printf("[WIFI] CONNECT slot=%u attempt=%u/3 budget_ms=5000 credentials=UNCHANGED\n",index+1,attempt);
      if(!silent)display.putRequest(BOOTSTRING,index);
      v027RecoveryIndex=index;
      WiFi.begin(config.ssids[index].ssid,config.ssids[index].password);
      const uint32_t started=millis();
      while(WiFi.status()!=WL_CONNECTED && uint32_t(millis()-started)<5000&&!offlinePlayer()){delay(100);}
      if(offlinePlayer()){showConnecting(nullptr,0);Serial.println("[WIFI] CONNECT=CANCELLED offline=1");return false;}
      if(WiFi.status()==WL_CONNECTED){
        config.setLastSSID(index+1);
        WiFi.setAutoReconnect(true);
        showConnecting(nullptr,0);
        Serial.printf("[WIFI] CONNECT=PASS slot=%u attempt=%u ip=%s rssi=%d\n",index+1,attempt,WiFi.localIP().toString().c_str(),WiFi.RSSI());return true;
      }
      Serial.printf("[WIFI] CONNECT=FAIL slot=%u attempt=%u/3 reason=%u name=%s\n",index+1,attempt,unsigned(v027DisconnectReason),reasonName(v027DisconnectReason));
    }
    Serial.printf("[WIFI] NEXT_SAVED_AP after=3 slot=%u\n",index+1);
  }
  showConnecting(nullptr,0);
  return false;
}

static void serviceScan(){
  static bool active=false;
  bool requested=false,cancel=false,busy=false;
  portENTER_CRITICAL(&wifiStateMux);
  requested=scanRequested;scanRequested=false;
  cancel=scanCancelRequested;scanCancelRequested=false;
  busy=setupBusy;
  portEXIT_CRITICAL(&wifiStateMux);
  if(cancel||busy||btRadioReserved()||(active&&(player.isRunning()||player.status()!=STOPPED))){
    if(active)esp_wifi_scan_stop();
    if(active||cancel)WiFi.scanDelete();
    active=false;
    portENTER_CRITICAL(&wifiStateMux);
    scanPhase=0;scanEntryCount=0;
    portEXIT_CRITICAL(&wifiStateMux);
    if(!requested||busy||btRadioReserved())return;
  }
  if(requested){
    if(active){esp_wifi_scan_stop();WiFi.scanDelete();active=false;}
    if(player.isRunning()||player.status()!=STOPPED){
      portENTER_CRITICAL(&wifiStateMux);scanPhase=3;portEXIT_CRITICAL(&wifiStateMux);
      return;
    }
    const int result=WiFi.scanNetworks(true,false,false,120);
    active=result==-1||result>=0;
    scanStarted=millis();
    if(result<-1){portENTER_CRITICAL(&wifiStateMux);scanPhase=3;portEXIT_CRITICAL(&wifiStateMux);return;}
  }
  if(!active)return;
  const int count=WiFi.scanComplete();
  if(count==-1&&uint32_t(millis()-scanStarted)<10000)return;
  if(count<0){
    esp_wifi_scan_stop();WiFi.scanDelete();active=false;
    portENTER_CRITICAL(&wifiStateMux);scanPhase=3;scanEntryCount=0;portEXIT_CRITICAL(&wifiStateMux);
    return;
  }
  ScanEntry found[12]={};unsigned kept=0;
  for(int i=0;i<count&&kept<12;i++){
    String name=WiFi.SSID(i);
    if(!name.length()||name.length()>=WIFI_SSID_CAP)continue;
    bool duplicate=false;
    for(unsigned j=0;j<kept;j++)if(name==found[j].ssid){duplicate=true;break;}
    if(duplicate)continue;
    strlcpy(found[kept].ssid,name.c_str(),sizeof(found[kept].ssid));
    found[kept].rssi=WiFi.RSSI(i);kept++;
  }
  WiFi.scanDelete();active=false;
  portENTER_CRITICAL(&wifiStateMux);
  memcpy(scanEntries,found,kept*sizeof(ScanEntry));
  scanEntryCount=kept;scanPhase=2;
  portEXIT_CRITICAL(&wifiStateMux);
  Serial.printf("[WIFI] SCAN=PASS visible=%u shown=%u\n",unsigned(count),kept);
}

// Setup AP is intentionally quiet: no repeated background reconnect storm.
// Captive DNS + HTTP stay alive while credentials are entered and while STA connects.
// AP is stopped only after STA has been continuously connected for 3 seconds.
static clocksync::Retry clockRetry;
bool MyNetwork::connectionBusy() const {return wifiAttemptOwned.load()||savedSearchRunning.load()||onlineRequest.load()||setupModeRequest.load();}
bool MyNetwork::offlinePlayer() const {return localPlayerOnly.load();}
bool MyNetwork::offlineReady() const{return localPlayerOnly.load()&&localPlayerReady.load();}
bool MyNetwork::prepareLocalBluetooth(){
#if IBT_NO_PSRAM
  if(offlineReady()&&WiFi.getMode()==WIFI_OFF)return true;
  btNetworkRequested.store(true);return false;
#else
  return true;
#endif
}
void MyNetwork::requestSetup(){
  // Cancel the association cooperatively; the network owner starts the AP.
  onlineRequest.store(false);localPlayerRequest.store(false);
  localPlayerOnly.store(true);localPlayerReady.store(false);setupModeRequest.store(true);
}
void MyNetwork::enterOnlinePlayer(){
  setupModeRequest.store(false);
#if IBT_NO_PSRAM
  if(btNetworkLease.load())btNetworkRestore.store(true);
#endif
  localPlayerReady.store(false);localPlayerOnly.store(true); // Cancel an older association before reusing its owner.
  localPlayerRequest.store(false);onlineRequest.store(true);
}
void MyNetwork::enterOfflinePlayer(unsigned target,bool temporaryBluetooth){
  setupModeRequest.store(false);
#if IBT_NO_PSRAM
  if(!temporaryBluetooth)btNetworkRestore.store(false); // Explicit Offline always wins.
#else
  (void)temporaryBluetooth;
#endif
  (void)target;onlineRequest.store(false);localPlayerReady.store(false);
  localPlayerOnly.store(true);
  status=SDREADY; // Exit onboarding immediately; shutdown remains network-owned.
  localPlayerRequest.store(true);
}
void MyNetwork::service(){
  radiodnsauto::tick();
  // Association owns the driver until cancellation/success is acknowledged.
  // Recovery and setup scans must not race the background saved-network search.
  if(wifiAttemptOwned.load()||savedSearchRunning.load())return;
  if(setupModeRequest.load()){
    if(btRadioReserved())return; // Wait for the existing audio-owner teardown ACK.
    setupModeRequest.store(false);
#if IBT_NO_PSRAM
    btNetworkRequested.store(false);btNetworkLease.store(false);btNetworkRestore.store(false);
#endif
    cancelScan();serviceScan();
    config.initNetwork();startSetup();netserver.begin(true);
    Serial.println("[WIFI-SETUP] request=STARTED owner=NETWORK credentials=RETAINED");
    return;
  }
#if IBT_NO_PSRAM
  if(btNetworkRequested.load()&&btRadioReserved()){
    btNetworkRequested.store(false);
    if(!offlinePlayer()){
      btNetworkLease.store(true);btNetworkRestore.store(true);
      enterOfflinePlayer(1,true);
      Serial.println("[BT-NETWORK] suspend=QUEUED owner=NETWORK credentials=RETAINED");
    }
  }
  if(!btRadioReserved()){
    btNetworkRequested.store(false);
    if(btNetworkLease.exchange(false)&&btNetworkRestore.exchange(false)){
      enterOnlinePlayer();Serial.println("[BT-NETWORK] restore=QUEUED");
    }
  }
#endif
  if(onlineRequest.load()){
    if(btRadioReserved())return; // Existing BT resource limits are never bypassed.
    onlineRequest.store(false);localPlayerOnly.store(false);localPlayerReady.store(false);
    config.initNetwork();status=SDREADY;beginReconnect=false;lostPlaying=false;
    if(config.ssidsCount==0){startSetup();netserver.begin(true);}else launchSavedSearch();
    Serial.println("[ONLINE-PLAYER] connect=QUEUED credentials=RETAINED");
  }
  if(localPlayerRequest.exchange(false)){
    // This owner alone cancels scan/association and shuts down the setup AP.
    cancelScan();serviceScan();
    portENTER_CRITICAL(&wifiStateMux);
    setupRequest=false;setupBusy=false;
    memset(pendingPass,0,sizeof(pendingPass));memset(activePass,0,sizeof(activePass));
    portEXIT_CRITICAL(&wifiStateMux);
    recoveryWaiting=false;v16StaAttemptActive=false;v16SetupFailed=false;
    beginReconnect=false;lostPlaying=false;
    showConnecting(nullptr,0);
    if(v15CaptiveDnsStarted){v15CaptiveDns.stop();v15CaptiveDnsStarted=false;}
    WiFi.setAutoReconnect(false);
    // Arduino AP.clear() calls AP.begin(): softAPdisconnect(false) would start
    // an unused AP/DHCP server even when we only had STA. mode(OFF) already
    // stops/deinitializes both interfaces without clearing saved credentials.
#if !IBT_NO_PSRAM
    WiFi.softAPdisconnect(false);WiFi.disconnect(false,false);
#endif
    const bool wifiOff=WiFi.mode(WIFI_OFF);
    status=SDREADY;player.lockOutput=false;localPlayerReady.store(wifiOff);
    Serial.printf("[LOCAL-NETWORK] shutdown=%s free=%u largest=%u\n",wifiOff?"PASS":"FAIL",unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
    bool stopWeb=config.getMode()==PM_WEB;
#if IBT_NO_PSRAM
    stopWeb=stopWeb&&!btNetworkLease.load(); // BT's audio-owner command already stopped WEB; PR_STOP would cancel BT-IN.
#endif
    if(stopWeb)player.sendCommand({PR_STOP,0});
    display.putRequest(NEWMODE,PLAYER);
    Serial.println("[OFFLINE-PLAYER] wifi=OFF credentials=RETAINED SD_BT_DAC=AVAILABLE");
  }
  if(offlinePlayer())return;
  serviceBtWifiPolicy();
  // Boot starts SNTP before association. Retry only while no valid UTC exists,
  // from the network owner, never from the display/audio task or Wi-Fi callback.
  static uint32_t clockCheck=0;
  static int previousClockValid=-1;
  const uint32_t clockNow=millis();
  if(uint32_t(clockNow-clockCheck)>=1000){
    clockCheck=clockNow;
    const bool ready=WiFi.status()==WL_CONNECTED&&uint32_t(WiFi.localIP())!=0;
    const bool clockValid=time(nullptr)>=clocksync::ValidUtc;
    if(previousClockValid!=int(clockValid)){
      Serial.printf("[CLOCK-SYNC] valid=%u wifi=%u ntp_events=%lu source=SYSTEM_UTC\n",unsigned(clockValid),unsigned(ready),(unsigned long)dual::clockSyncCount());
      previousClockValid=int(clockValid);
    }
    const auto clockSettings=dual::clockServers();
    if(clockRetry.due(clockNow,ready,clockValid,clockSettings.values)){
      const char* server=clockSettings.values[clockRetry.index()];
      dual::clockConfigure(server);
      Serial.printf("[CLOCK-NTP] attempt=%lu slot=0 server=%s max_servers=%u waiting=1\n",
                    (unsigned long)clockRetry.count(),server,unsigned(CONFIG_LWIP_SNTP_MAX_SERVERS));
    }
  }
  bool startSetupAttempt=false;
  if(status==SOFT_AP){
    portENTER_CRITICAL(&wifiStateMux);
    // Let the HTTP acknowledgement reach the phone before Wi-Fi retunes to
    // the home AP's channel. The setup AP remains available throughout.
    if(setupRequest && uint32_t(millis()-setupQueuedAt)>=1200){
      strlcpy(activeSsid,pendingSsid,sizeof(activeSsid));
      strlcpy(activePass,pendingPass,sizeof(activePass));
      setupRequest=false;startSetupAttempt=true;
    }
    portEXIT_CRITICAL(&wifiStateMux);
  }
  if(startSetupAttempt){
    cancelScan();serviceScan(); // Scan and connect never own the STA concurrently.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false,false);
    v027DisconnectReason=0;v15StaConnectedAt=0;v16SetupFailed=false;
    v16StaAttemptAt=millis();v16StaAttemptActive=true;
    showConnecting(activeSsid,1);
    Serial.printf("[WIFI-SETUP] STA_CONNECT=START ssid=%s AP_RETAINED=YES\n",activeSsid);
    WiFi.begin(activeSsid,activePass);
  }
  serviceScan();
  if(status!=SOFT_AP){
    if(WiFi.status()==WL_CONNECTED){recoveryWaiting=false;return;}
    if(status!=CONNECTED&&status!=SDREADY)return;
    if(!recoveryWaiting){recoveryStarted=millis();recoveryWaiting=true;return;}
    if(uint32_t(millis()-recoveryStarted)<30000)return;
    recoveryWaiting=false;
    Serial.println("[WIFI] RECOVERY_TIMEOUT SETUP_AP=START credentials=UNCHANGED");
    startSetup();return;
  }
  if(v15CaptiveDnsStarted)v15CaptiveDns.processNextRequest();
  if(!v16StaAttemptActive)return; // AP remains available until a queued credential is tested.
  if(WiFi.status()!=WL_CONNECTED){
    v15StaConnectedAt=0;
    if(v16StaAttemptActive && uint32_t(millis()-v16StaAttemptAt)>=20000){
      WiFi.disconnect(false,false);
      v16StaAttemptActive=false;v16SetupFailed=true;
      showConnecting(nullptr,0);
      portENTER_CRITICAL(&wifiStateMux);setupBusy=false;portEXIT_CRITICAL(&wifiStateMux);
      Serial.printf("[WIFI-SETUP] STA_CONNECT=FAIL reason=%u name=%s AP_RETAINED=YES\n",
                    unsigned(v027DisconnectReason),reasonName(v027DisconnectReason));
    }
    return;
  }
  if(v15StaConnectedAt==0){
    v15StaConnectedAt=millis();
    Serial.printf("[WIFI-SETUP] STA_CONNECTED_PENDING ip=%s keep_ap_ms=3000\n",WiFi.localIP().toString().c_str());
    return;
  }
  if(uint32_t(millis()-v15StaConnectedAt)<5000)return;
  if(v16StaAttemptActive){
    uint8_t slot=0;
    if(!config.rememberVerifiedWifi(activeSsid,activePass,&slot)){
      Serial.println("[WIFI-SETUP] SAVE=FAIL AP_RETAINED=YES credentials=UNCHANGED");
      WiFi.disconnect(false,false);v15StaConnectedAt=0;
      v16StaAttemptActive=false;v16SetupFailed=true;
      showConnecting(nullptr,0);
      portENTER_CRITICAL(&wifiStateMux);setupBusy=false;portEXIT_CRITICAL(&wifiStateMux);
      return;
    }
    config.setLastSSID(slot);
  }
  v16StaAttemptActive=false;v16SetupFailed=false;
  showConnecting(nullptr,0);
  portENTER_CRITICAL(&wifiStateMux);setupBusy=false;portEXIT_CRITICAL(&wifiStateMux);
  if(v15CaptiveDnsStarted){v15CaptiveDns.stop();v15CaptiveDnsStarted=false;}
  status=CONNECTED;player.lockOutput=false;
  WiFi.softAPdisconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  setWifiParams();
  config.initPlaylistMode();
  telnet.begin(true);
  display.putRequest(NEWIP,0);display.putRequest(NEWMODE,PLAYER);
  Serial.printf("[WIFI-SETUP] STA_CONNECT=PASS ip=%s AP_STOPPED=YES\n",WiFi.localIP().toString().c_str());
  player.sendCommand({PR_PLAY,-65001});
}

bool MyNetwork::applySetupCredentials(const char* ssid,const char* pass){
  if(status!=SOFT_AP || ssid==nullptr || pass==nullptr)return false;
  const size_t sl=strlen(ssid),pl=strlen(pass);
  if(sl==0 || sl>=WIFI_SSID_CAP || pl>=WIFI_PASS_CAP ||
     strpbrk(ssid,"\t\r\n") || strpbrk(pass,"\t\r\n"))return false;
  portENTER_CRITICAL(&wifiStateMux);
  const bool accepted=!setupBusy;
  if(accepted){
    strlcpy(pendingSsid,ssid,sizeof(pendingSsid));
    strlcpy(pendingPass,pass,sizeof(pendingPass));
    strlcpy(selectedSetupSsidValue,ssid,sizeof(selectedSetupSsidValue));
    setupRequest=true;setupBusy=true;
    setupQueuedAt=millis();
  }
  portEXIT_CRITICAL(&wifiStateMux);
  if(!accepted)return false;
  v16SetupFailed=false;
  if(!v027EventsRegistered){WiFi.onEvent(observeWiFi);v027EventsRegistered=true;}
  Serial.printf("[WIFI-SETUP] STA_CONNECT=QUEUED ssid_len=%u password_len=%u AP_RETAINED=YES\n",unsigned(sl),unsigned(pl));
  return true;
}

bool MyNetwork::connectScannedNetwork(const char* ssid,const char* pass){
  if(btRadioReserved())return false;
  if(!ssid||!pass)return false;
  const size_t sl=strlen(ssid),pl=strlen(pass);
  if(!sl||sl>=WIFI_SSID_CAP||pl>=WIFI_PASS_CAP||(pl>0&&pl<8)||
     strpbrk(ssid,"\t\r\n")||strpbrk(pass,"\t\r\n"))return false;
  if(status!=SOFT_AP){
    // The normal Wi-Fi scanner is available only with playback stopped.
    if(player.isRunning()||player.status()!=STOPPED)return false;
    startSetup();
  }
  return applySetupCredentials(ssid,pass);
}

void searchWiFi(void * pvParameters){
  (void)pvParameters;
  const bool connected=network.wifiBegin(true);
  if(!network.offlinePlayer()){
    if(!connected){network.startSetup();netserver.begin(true);}
    else{
      network.status=CONNECTED;netserver.begin(true);telnet.begin(true);network.setWifiParams();
      display.putRequest(NEWIP,0);
#ifdef MQTT_ROOT_TOPIC
      mqttInit();
#endif
    }
  }
  savedSearchRunning.store(false);vTaskDelete(NULL);
}
static void launchSavedSearch(){
  bool absent=false;if(!savedSearchRunning.compare_exchange_strong(absent,true))return;
  if(xTaskCreatePinnedToCore(searchWiFi,"searchWiFi",1024*4,NULL,0,NULL,SEARCH_WIFI_CORE_ID)!=pdPASS){
    savedSearchRunning.store(false);network.startSetup();netserver.begin(true);
    Serial.println("[WIFI] SAVED_SEARCH=NO_TASK setup=1");
  }
}

#define DBGAP false

void MyNetwork::begin() {
  BOOTLOG("network.begin");
#if defined(BOARD_CYD28)
  const unsigned mode=dual::snapshot().settings.networkMode;
  if(mode||offlinePlayer()){if(mode)enterOfflinePlayer(mode);service();return;}
#endif
  config.initNetwork();
  if (config.ssidsCount == 0 || DBGAP) {
    raiseSoftAP();
    return;
  }
  if(config.getMode()!=PM_SDCARD){
    if(!wifiBegin()){
      if(offlinePlayer()){status=SDREADY;return;}
      raiseSoftAP();
      Serial.println("##[BOOT]#\tdone");
      return;
    }
    Serial.println(".");
    status = CONNECTED;
    setWifiParams();
    #ifdef MQTT_ROOT_TOPIC
      mqttInit();
    #endif
  }else{
    status = SDREADY;
    launchSavedSearch();
  }
  
  Serial.println("##[BOOT]#\tdone");
  if(REAL_LEDBUILTIN!=255) digitalWrite(REAL_LEDBUILTIN, LOW);
  
#if RTCSUPPORTED
  if(config.isRTCFound()){
    rtc.getTime(&network.timeinfo);
    mktime(&network.timeinfo);
    display.putRequest(CLOCK);
  }
#endif
  if (network_on_connect) network_on_connect();
  pm.on_connect();
}

void MyNetwork::setWifiParams(){
  WiFi.setSleep(false);
  if(!v027ReconnectCallbacks){
    WiFi.onEvent(WiFiReconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(WiFiLostConnection, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    v027ReconnectCallbacks=true;
  }
  //config.setTimeConf(); //??
  // RADIO_RESCUE_V53_20260911: mDNS disabled temporarily to return heap to radio audio.
  // Re-enable after PSRAM/coexistence validation.
}

void MyNetwork::requestTimeSync(bool withTelnetOutput, uint8_t clientId) {
  if(!withTelnetOutput)return;
  const time_t stamp=time(nullptr);tm local{};
  const bool valid=stamp>=1704067200&&localtime_r(&stamp,&local);
  char value[48]={};
  if(valid)strftime(value,sizeof(value),"%Y-%m-%dT%H:%M:%S%z",&local);
  telnet.printf(clientId,"##SYS.DATE#: %s\n",valid?value:"UNSYNCED");
  telnet.printf(clientId,"[CLOCK-STATE] valid=%u epoch=%lld wifi=%u ntp_events=%lu source=SYSTEM_UTC\n> ",unsigned(valid),(long long)stamp,unsigned(WiFi.status()==WL_CONNECTED),(unsigned long)dual::clockSyncCount());
}

void rebootTime() {
  ESP.restart();
}

bool MyNetwork::setupFailed() const {return v16SetupFailed;}
bool MyNetwork::setupInProgress() const {
  portENTER_CRITICAL(&wifiStateMux);
  const bool busy=setupBusy;
  portEXIT_CRITICAL(&wifiStateMux);
  return status==SOFT_AP && busy;
}
const char* MyNetwork::setupSsid() const {return v027SetupSsid;}
void MyNetwork::startSetup(){
  localPlayerRequest.store(false);onlineRequest.store(false);localPlayerReady.store(false);localPlayerOnly.store(false);
  beginReconnect=true;
  status=SOFT_AP;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false,false); // preserve stored credentials
  delay(100);
  raiseSoftAP();
}
void MyNetwork::raiseSoftAP() {
  snprintf(v027SetupSsid,sizeof(v027SetupSsid),"I-BT-Radio-Setup");
  recoveryWaiting=false;status = SOFT_AP;v15StaConnectedAt=0;
  v16StaAttemptAt=0;v16StaAttemptActive=false;v16SetupFailed=false;
  showConnecting(nullptr,0);
  portENTER_CRITICAL(&wifiStateMux);
  setupRequest=false;setupBusy=false;scanRequested=false;scanCancelRequested=true;
  portEXIT_CRITICAL(&wifiStateMux);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_AP_STA); // V15: portal must survive a subsequent WiFi.begin()
  IPAddress apIP(192,168,4,1),mask(255,255,255,0);
  WiFi.softAPConfig(apIP,apIP,mask);
  const bool apOK=WiFi.softAP(v027SetupSsid, apPassword);
  if(v15CaptiveDnsStarted)v15CaptiveDns.stop();
  v15CaptiveDnsStarted=v15CaptiveDns.start(53,"*",WiFi.softAPIP());
  Serial.println("##[BOOT]#");
  BOOTLOG("************************************************");
  BOOTLOG("Running in AP setup mode");
  BOOTLOG("Connect to AP %s with password %s", v027SetupSsid, apPassword);
  BOOTLOG("Open http://192.168.4.1/ and enter home Wi-Fi SSID/password");
  BOOTLOG("************************************************");
  Serial.printf("[WIFI-SETUP] AP=%s password=%s ip=192.168.4.1 result=%s\n",v027SetupSsid,apPassword,apOK?"PASS":"FAIL");
}

void MyNetwork::requestWeatherSync(){
  display.putRequest(NEWWEATHER);
}
