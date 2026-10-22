#include "memory_checkpoint.h"
#if IBT_COMPACT_UI
#include "ir_receiver.h"
#endif
#include "lite_memory.h"
#include "mono_dsp.h"
#include "../ui/alarm_layout.h"
#include "../ui/setup_layout.h"
#include "../ui/wifi_password_layout.h"
#include "../ui/lite_list.h"
#include "../ui/interaction_policy.h"

#include "../core/options.h"
#include "core.h"
#include "surface_policy.h"
#include "artwork_ab.h"
#include "load_probe.h"
#include "audio_path_probe.h"
#include "sd_bt_ui_policy.h"
#include "menu_render_policy.h"
#include "bt_audio.h"
#include "bt_media.h"
#include "track_text.h"
#include "resource_limiter.h"
#include "input_low_probe.h"
#include "ui_governor.h"
#include "frame_cadence.h"
#include "light_policy.h"
#include "artwork.h"
#include "online_artwork.h"
#include "v027_himem.h"
#include "../boards/hal.h"
#include "../ui/frontend.h"
#include "../ui/videoton_layout.h"
#include "boot_progress.h"
#include "connectivity_policy.h"
#include "../ui/content.h"
#if defined(BOARD_CYD28)
#include "../ui/cassette_layout.h"
#endif
#include "../core/config.h"
#include "../core/player.h"
#include "../core/sdmanager.h"
#include "alarm_sd_policy.h"
#include "../core/network.h"
#include "../core/display.h"
#include "../audioI2S/direct_stream_policy.h"
#include "backup_sd.h"
#include <Preferences.h>
#include "vu_probe.h"
#include "bt_input_vu.h"
#include "../ui/bt_speaker_layout.h"
#include <EEPROM.h>
#include <SPIFFS.h>
#include <esp_heap_caps.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <time.h>
#include <ctype.h>
#include <atomic>
#include "weather.h"
#include "clock_gesture.h"
#include "menu_scan_policy.h"
#include "sensors/receiver.h"
#include <new>
#include "clock_sync_policy.h"
namespace dual {
static const char playerSettingKey[] = {119,105,110,97,109,112,0}; // Keeps the existing saved-setting format.
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static Snapshot state;
static int pendingConnectivity=-1;static uint8_t connectivityStep=0;static uint32_t connectivityAt=0;
static alarmui::TimeDraft alarmTimeDraft;
static bool alarmTimeError=false;
bool alarmDraftError(){return alarmTimeError;}
uint8_t alarmDraftHour(){return alarmTimeDraft.hour;}
uint8_t alarmDraftMinute(){return alarmTimeDraft.minute;}
bool alarmDraftEnding(){return alarmTimeDraft.ending;}
static uint8_t dnsDraft[4]={};static unsigned dnsTarget=0,dnsOctet=0;
uint8_t dnsDraftOctet(unsigned index){return dnsDraft[index%4];}
unsigned dnsEditOctet(){return dnsOctet;}
static QueueHandle_t commands=nullptr;
struct WebMenuCommand { uint8_t page; uint16_t item; char value[64]={}; };
static QueueHandle_t webMenuCommands=nullptr;
static UiGovernor uiGovernor;
static uint32_t settingsVersion=0,savedVersion=0,lastSnapshot=0,lastDiag=0;
static bool initialized=false, frontendOK=false, bitrateRejected=false, low=false;
static bool audioSettingPending=false;
static uint32_t nextAudioCommit=0;
static uint32_t nextSettingsCommit=0;
static uint8_t favoriteBits[60]={}; // 480 playlist rows
static uint32_t favoriteVersion=0,favoriteSaved=0,nextFavoriteCommit=0;
static uint32_t favoritePlaylistHash=0;
static char searchQuery[24]={};
static uint16_t* searchResults=nullptr; // Optional result cache, PSRAM only.
static uint16_t searchFound=0;
static uint8_t searchState=0; // 0=ready, 1=stop playback, 2=playlist error, 3=no match, 4=radio mode required
static SemaphoreHandle_t searchCacheLock=nullptr;
static char* searchNames=nullptr; // 480 x BUFLEN in PSRAM, one allocation for device lifetime
static uint16_t searchRows=0;
static uint8_t stressState=0; // 0=ready, 1=pass, 2=fail/not-run, 3=limiter denied, 4=stop playback
static unsigned page=0, selected=0;
static uint32_t menuActivityAt=0;
static std::atomic<uint32_t> clockActivityAt{0};
static std::atomic<unsigned> clockFocus{0};
unsigned clockHomeFocus(){return clockFocus.load(std::memory_order_relaxed);}
void clockWake(){clockActivityAt.store(millis(),std::memory_order_relaxed);}
bool clockControlsVisible(){return uint32_t(millis()-clockActivityAt.load(std::memory_order_relaxed))<5000;}
struct ClockStorage {
    WifiPasswordUi wifi,entry;
    clocksync::Servers servers;
    char active[clocksync::ServerCapacity]={};
    uint32_t version=0,saved=0;
    unsigned editIndex=0;
    SemaphoreHandle_t lock=nullptr;
};
static ClockStorage* clockStore=nullptr;
static bool ensureClockStore(){
    if(clockStore)return true;
    void* storage=heap_caps_malloc(sizeof(ClockStorage),litememory::payloadCaps);
    if(!storage){Serial.println("[CLOCK] PSRAM settings allocation failed; no internal fallback");return false;}
    clockStore=new(storage) ClockStorage;
    return true;
}
clocksync::Servers clockServers(){clocksync::Servers result;if(!clockStore)return result;portENTER_CRITICAL(&mux);result=clockStore->servers;portEXIT_CRITICAL(&mux);return result;}
const WifiPasswordUi& wifiPasswordUi(){static const WifiPasswordUi empty{};return clockStore?clockStore->wifi:empty;}
const WifiPasswordUi& ntpServerUi(){static const WifiPasswordUi empty{};return clockStore?clockStore->entry:empty;}
char wifiPasswordKeyChar(unsigned mode,unsigned key){
    static const char* const keys[]={
        "abcdefghijklmnopqrstuvwxyz0123456789",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
        " !@#$%^&*()-_=+[]{};:'\",.<>/?\\|`~"
    };
    if(mode>=3||key>=36||key>=strlen(keys[mode]))return 0;
    return keys[mode][key];
}
static unsigned memoryParent=13, networkParent=9;
static unsigned bluetoothParent=1,bluetoothParentRow=2;
static unsigned stationListParent=0;
static playMode_e pendingSourceTarget=PM_WEB;
static uint32_t pendingSourceAt=0;
static bool pendingSourceCancelSent=false;
static volatile bool redraw=true;
static bool shortPending=false, secondDown=false, stationPreview=false;
static bool clockMenuPending=false;
static bool clockPressWakeOnly=false;
static void input(Action a);
enum class BtRatePhase : uint8_t { Idle, Stopping, Connecting };
static BtRatePhase btRatePhase=BtRatePhase::Idle;
static uint32_t btRateSince=0,btRateTarget=0;
static uint16_t btRateStation=0;
static bool btRateSawWanted=false;
static void btRateNotice(const char* message){
    portENTER_CRITICAL(&mux);
    strlcpy(state.notice,message,sizeof(state.notice));
    portEXIT_CRITICAL(&mux);
    redraw=true;
}
static void btRateFail(const char* message){
    Serial.printf("[BT-RATE-AB] FAIL reason=%s target=%lu\n",message,(unsigned long)btRateTarget);
    btRatePhase=BtRatePhase::Idle;
    btRateNotice(message);
    player.sendCommand({PR_BT,-1});
}
static void btRateTouch(){
    if(btRatePhase!=BtRatePhase::Idle||!btaudio::transmitting())return;
    if(!btaudio::hasSavedDevice()){
        btRateNotice("BT: nincs mentett eszkoz");return;
    }
    const unsigned rates[]={16000,32000,44100};
    const unsigned current=btaudio::outputRate();
    unsigned next=0;
    while(next<3&&rates[next]!=current)++next;
    btRateTarget=rates[next==3?0:(next+1)%3];
    btRateStation=config.lastStation();
    if(!player.sendCommand({PR_STOP,0})){
        btRateNotice("BT: stop parancs sikertelen");return;
    }
    btRateSawWanted=false;btRateSince=millis();btRatePhase=BtRatePhase::Stopping;
    Serial.printf("[BT-RATE-AB] stop target=%lu\n",(unsigned long)btRateTarget);
}
static void btRateTick(uint32_t now){
    if(btRatePhase==BtRatePhase::Idle)return;
    if(btRatePhase==BtRatePhase::Stopping){
        if(!btaudio::wanted()&&btaudio::idle()&&player.status()==STOPPED){
            if(!btaudio::setOutputRate(btRateTarget)){
                btRateFail("BT: frekvencia mentes hiba");return;
            }
            if(!btaudio::reconnectSaved()||!player.sendCommand({PR_BT,0})){
                btRateFail("BT: ujracsatlakozas hiba");return;
            }
            btRateSawWanted=false;btRateSince=now;btRatePhase=BtRatePhase::Connecting;
            Serial.printf("[BT-RATE-AB] reconnect target=%lu\n",(unsigned long)btRateTarget);
        }else if(uint32_t(now-btRateSince)>15000){
            btRateFail("BT: leallitas idotullepes");
        }
        return;
    }
    if(btaudio::wanted())btRateSawWanted=true;
    if(btRateSawWanted&&!btaudio::wanted()){
        btRatePhase=BtRatePhase::Idle;
        btRateNotice("BT: kapcsolat hiba, DAC");return;
    }
    if(btaudio::transmitting()){
        if(player.sendCommand({PR_PLAY,btRateStation})){
            Serial.printf("[BT-RATE-AB] resume target=%lu\n",(unsigned long)btRateTarget);
            btRatePhase=BtRatePhase::Idle;
        }
    }else if(uint32_t(now-btRateSince)>95000||
             (btaudio::wanted()&&btaudio::linkFailed())){
        btRateFail("BT: kapcsolat hiba, DAC");
    }
}
static bool uiStressActive=false,uiStressInjecting=false,uiStressDraining=false;
static uint16_t uiStressActionMs=500,uiStressDrawMs=500;
static uint32_t uiStressActions=0,uiStressFrames=0,uiStressRenderCalls=0,uiStressRenderedActions=0;
static uint32_t uiStressStarted=0,uiStressLastAction=0,uiStressLastCheck=0;
static uint32_t uiStressBaseLow=0,uiStressBaseDrops=0;
static uint32_t uiStressBaseGaps=0;
static char uiStressReason[24]="idle"; // longest defined reason has 21 characters
static bool uiStressTour=false,uiStressTourRestored=false;
static bool uiStressFullScan=false;
static uint8_t uiStressScanPage=0,uiStressScanItem=0;
static uint32_t uiStressScanItems=0,uiStressScanDisabled=0,uiStressScanRendered=0;
static constexpr unsigned UiScanSlots=36+(IBT_COMPACT_UI?1:0);
static uint8_t (*uiStressScanSeen)[2]=nullptr; // Allocate diagnostic coverage in PSRAM only when needed.
static uint8_t uiStressAlarmSeen[4]{}; // Only the weekly alarm has more than 16 rows.
static Settings uiStressOriginalSettings;
static uint8_t uiStressTourStep=0;
static uint32_t uiStressTourVisits=0,uiStressTourEdits=0,uiStressTourCycles=0;
enum : unsigned {
    PAGE_HOME=0, PAGE_MAIN=1, PAGE_MEMORY=2, PAGE_BLUETOOTH=3, PAGE_STATIONS_LIST=4,
    PAGE_ROTARY_DEBUG=5, PAGE_SOUND=6, PAGE_SETTINGS=7, PAGE_STATIONS=8, PAGE_WIFI=9,
    PAGE_DISPLAY=10, PAGE_CONTROLS=11, PAGE_STORAGE=12, PAGE_DIAGNOSTICS=13,
    PAGE_VOLUME_STREAM=14, PAGE_EQ=15, PAGE_OUTPUTS=16, PAGE_WIFI_QR=17, PAGE_NETWORK_INFO=18,
    PAGE_SYSTEM=19, PAGE_FIRMWARE_INFO=20, PAGE_SAVED_WIFI=21, PAGE_BT_INFO=22, PAGE_DAC_INFO=23, PAGE_FS_INFO=24, PAGE_ART_CACHE=25, PAGE_WEB_UPDATE=26, PAGE_BUTTON_MAP=27, PAGE_WIFI_SCAN=28, PAGE_FAVORITES_LIST=29, PAGE_SD_INFO=30, PAGE_STATION_SEARCH=31, PAGE_SEARCH_RESULTS=32, PAGE_PLAYLIST_EDITOR=33, PAGE_MEMORY_STRESS=34, PAGE_BACKUP_MENU=37, PAGE_BACKUP_RESULT=38, PAGE_BT_PLAYER=39, PAGE_BT_SCAN=40, PAGE_SOURCE_PENDING=41, PAGE_WIFI_PASSWORD=42, PAGE_BT_RATE=43, PAGE_BT_SAVED=44, PAGE_IDLE=45, PAGE_VU_TIMER=46, PAGE_SURFACE=47, PAGE_SERVICE=48, PAGE_HARDWARE=49, PAGE_BAT_CAL=50, PAGE_BAT_VALUE=51, PAGE_BAT_DISPLAY=52, PAGE_DNS=53, PAGE_DNS_ADDRESS=54, PAGE_DNS_VALUE=55, PAGE_RETRO_COLOR=56, PAGE_EQ10=57, PAGE_EQ_PRESETS=58, PAGE_ALARM=59
};
// Network setup is an explicit user workflow, not an idle menu preview.
// In particular, slow password entry must not discard the draft.
static bool manualWifiPage(unsigned p){
    return p==PAGE_WIFI||p==PAGE_WIFI_SCAN||p==PAGE_WIFI_PASSWORD||
           p==PAGE_WIFI_QR||p==PAGE_NETWORK_INFO||p==PAGE_SAVED_WIFI||
           p==PAGE_DNS||p==PAGE_DNS_ADDRESS||p==PAGE_DNS_VALUE;
}
static std::atomic<int> surfaceRequest{-1};
static unsigned settingsSkin(const Settings& s){return s.alarm.clockHome?13:playerSkinId(s);}
static void publishSurface(const Settings& s){surfacepolicy::publish(page==PAGE_SERVICE?5:page==PAGE_BT_PLAYER&&!s.alarm.clockHome?14:settingsSkin(s));}
bool requestSurfaceSkin(unsigned skin){
#if IBT_COMPACT_UI
 if(skin!=3&&skin!=12&&skin!=13&&skin!=14)return false;
#endif
if(skin==4||!surfacepolicy::validSkin(skin))return false;int empty=-1;return surfaceRequest.compare_exchange_strong(empty,int(skin));}
void surfaceDiagnostics(){const auto skin=surfacepolicy::skin();Serial.printf("[SURFACE] policy=1 skin=%u surface=%s pending=%d bt_remote=%u ble_skin=%u\n",skin,surfacepolicy::name(surfacepolicy::forSkin(skin)),surfaceRequest.load(),unsigned(surfacepolicy::remoteTransport(surfacepolicy::forSkin(skin))),unsigned(surfacepolicy::ble(surfacepolicy::forSkin(skin))));}
enum class AlarmPhase : uint8_t { Idle, Stop, Source, Volume, Play, Starting, Active, Fallback, Ending };
static std::atomic<AlarmPhase> alarmPhase{AlarmPhase::Idle};
static std::atomic<uint32_t> alarmLastDay{0};
static uint32_t alarmStarted=0,alarmRetryAt=0;
uint32_t alarmLastOccurrence(){return alarmLastDay;}
static uint32_t alarmPlayQueuedAt=0;
static bool alarmToneRequested=false;
static time_t alarmStartEpoch=0,alarmEndEpoch=0;
time_t alarmEndTime(){return alarmPhase==AlarmPhase::Idle?0:alarmEndEpoch;}
static uint8_t alarmLastVolume=255,alarmLastLight=255;
static constexpr uint8_t AlarmManualLight=254;
static uint8_t alarmPreviousVolume=0;
static uint8_t alarmManualOverride=0; // 1=manual takeover, 2=explicit stop/restore
bool alarmFallbackValid(){
    portENTER_CRITICAL(&mux);const bool manual=alarmManualOverride!=0;portEXIT_CRITICAL(&mux);
    return alarmPhase.load()==AlarmPhase::Fallback&&!manual;
}
static bool alarmMarkerCommitted=false;
static bool alarmSdSourceRequested=false;
static uint8_t alarmSource=0;
static std::atomic<uint32_t> alarmRequestEpoch{0};
bool alarmRequestValid(uint32_t token){return alarmclock::requestAllowed(token,alarmRequestEpoch.load(std::memory_order_acquire));}
bool alarmCleanupValid(uint32_t token){
    portENTER_CRITICAL(&mux);const bool manual=alarmManualOverride!=0;portEXIT_CRITICAL(&mux);
    return alarmclock::cleanupAllowed(token,alarmLastDay.load(std::memory_order_acquire),
        (alarmPhase.load(std::memory_order_acquire)==AlarmPhase::Ending||alarmPhase.load()==AlarmPhase::Fallback),manual);
}
void alarmStopRequested(){
    alarmRequestEpoch.exchange(0,std::memory_order_acq_rel);
    if(alarmPhase.load()==AlarmPhase::Idle)return;
    portENTER_CRITICAL(&mux);alarmManualOverride=2;portEXIT_CRITICAL(&mux);
}
void alarmTakeoverRequested(){
    alarmRequestEpoch.exchange(0,std::memory_order_acq_rel);
    // Ending has already revoked the token, but still owns cleanup. A new
    // manual source/play/volume command must also take ownership from Ending.
    if(alarmPhase.load(std::memory_order_acquire)==AlarmPhase::Idle)return;
    portENTER_CRITICAL(&mux);alarmManualOverride=1;portEXIT_CRITICAL(&mux);
}
static bool alarmCommand(playerRequestType_e type,int payload){
    const uint32_t token=alarmRequestEpoch.load(std::memory_order_acquire);
    return token&&player.sendCommand({type,payload,token});
}
static std::atomic<bool> alarmSdOwned{false};
static std::atomic<alarmsd::EndEvent> alarmSdEndPending{alarmsd::EndEvent::None};
bool alarmSdEof(bool failed){
    if(!alarmSdOwned.load(std::memory_order_acquire)||config.getMode()!=PM_SDCARD)return false;
    // Keep EOF claimed while stopping; it must not fall through to the
    // ordinary SD playlist after this alarm request has been revoked.
    if(alarmRequestEpoch.load(std::memory_order_acquire))
        alarmSdEndPending.store(alarmsd::endEvent(!failed),std::memory_order_release);
    return true;
}
static void releaseAlarmSd(){
    alarmSdOwned.store(false,std::memory_order_release);
    alarmSdEndPending.store(alarmsd::EndEvent::None,std::memory_order_release);
    sdman.freeAlarmFolderScan();
}
static void restoreAlarmLight() {
    const Settings s=snapshot().settings;
    const bool clockDim=s.alarm.nightClock&&s.alarm.clockHome&&page==PAGE_HOME&&
        !player.isRunning();
    board::brightness(clockDim?s.alarm.lightStart:(s.brightness?s.brightness:100));
}
static void failAlarm(const char* reason,const char* notice){
    alarmRequestEpoch.store(0,std::memory_order_release);
    const bool toneFailed=alarmPhase==AlarmPhase::Fallback;
    alarmPhase=toneFailed?AlarmPhase::Ending:AlarmPhase::Fallback;alarmRetryAt=0;alarmPlayQueuedAt=millis();
    if(!toneFailed)alarmToneRequested=false;
    portENTER_CRITICAL(&mux);
    strlcpy(state.notice,toneFailed?notice:"Ebreszto: tititi tartalekhang",sizeof(state.notice));state.alarmActive=!toneFailed;
    portEXIT_CRITICAL(&mux);
    Serial.printf("[ALARM] result=FAIL reason=%s\n",reason);redraw=true;
}
static bool alarmSdCatalogValid(const AlarmFolderScan* scan){
    return scan&&scan->status.load(std::memory_order_acquire)==1&&scan->tracks&&scan->count&&
        scan->cursor<scan->count&&sdman.catalogReady.load()&&sdman.indexedTracks.load()==scan->total;
}
// Only the UI task changes alarmPhase. Other tasks can request cancellation
// after their manual command has actually entered the action queue.
static void cancelAlarmForUser(bool restoreVolume=false) {
    if(alarmPhase==AlarmPhase::Idle)return;
    if(alarmToneRequested||alarmPhase==AlarmPhase::Fallback)player.serviceStop();
    alarmToneRequested=false;
    alarmRequestEpoch.store(0,std::memory_order_release);
    releaseAlarmSd();
    alarmPhase=AlarmPhase::Idle;
    if(restoreVolume)player.setVol(alarmPreviousVolume);
    portENTER_CRITICAL(&mux);state.alarmActive=false;portEXIT_CRITICAL(&mux);
    restoreAlarmLight();
    Serial.println("[ALARM] result=USER_OVERRIDE");
    redraw=true;
}
static uint8_t calibrationPoint=0;
static unsigned eqPresetParent=PAGE_EQ10;
unsigned sensorCalibrationPoint(){return calibrationPoint;}
static serviceui::State serviceUiSettings;
static bool serviceInfo=false,serviceRoutePending=false;
serviceui::State serviceSettings(){return serviceUiSettings;}
bool serviceDiagnostics(){return serviceInfo;}
static void serviceControl(serviceui::Control control){
  using serviceui::Control;
  if(control==Control::Stop){player.serviceStop();return;}
  if(control==Control::Back){player.serviceStop();serviceRoutePending=false;page=PAGE_SURFACE;selected=5;redraw=true;return;}
  if(control==Control::Diagnostics||control==Control::Generator){serviceInfo=control==Control::Diagnostics;redraw=true;return;}
  if(control==Control::Start){
    if(!serviceRoutePending&&!player.serviceActive())player.serviceStart(serviceUiSettings);
    return;
  }
  if(serviceInfo||control==Control::None)return;
  // Every parameter edit stops the old signal. A new START is always explicit.
  player.serviceStop();
  if(control==Control::Dac||control==Control::Bt){
    serviceui::route(serviceUiSettings,control==Control::Dac?serviceui::Route::Dac:serviceui::Route::Bt);
    serviceRoutePending=true;
  }else if(control==Control::Wave)serviceui::nextWave(serviceUiSettings);
  else if(control==Control::Minus||control==Control::Plus)serviceui::frequency(serviceUiSettings,control==Control::Plus);
  else if(control==Control::Level)serviceui::nextLevel(serviceUiSettings);
  else if(control==Control::Channel)serviceui::nextChannel(serviceUiSettings);
  else if(control==Control::Step)serviceUiSettings.step=serviceUiSettings.step>=1000?1:serviceUiSettings.step*10;
  else if(control==Control::Rate){
    constexpr uint32_t rates[]={32000,44100,48000,96000,192000};unsigned i=0;
    while(i<5&&rates[i]!=serviceUiSettings.rate)++i;
    serviceui::sampleRate(serviceUiSettings,rates[(i+1)%5]);
  }
  redraw=true;
}
static void serviceRouteTick(){
  if(!serviceRoutePending||player.serviceActive())return;
  const bool bt=serviceUiSettings.route==serviceui::Route::Bt;
  if(bt&&config.getMode()==PM_WEB){serviceRoutePending=false;return;}
  if(bt&&!btaudio::hasSavedDevice()){serviceRoutePending=false;return;}
  if((bt&&btaudio::outputSelected())||(!bt&&!btaudio::wanted()&&btaudio::idle())){
    serviceRoutePending=false;return;
  }
  if(player.sendCommand({PR_BT,bt?0:-1})){
    cancelAlarmForUser();
    if(bt)btaudio::reconnectSaved();
    serviceRoutePending=false;
  }
}
static void openBluetoothMenu(unsigned parent,unsigned row,unsigned selection){
    bluetoothParent=parent;bluetoothParentRow=row;
    page=PAGE_BLUETOOTH;selected=selection;redraw=true;
}
static int localRoutePending=-1;static uint32_t localRouteAt=0;
static bool speakerSearchPending=false;static uint8_t speakerSearchStep=0;static uint32_t speakerSearchAt=0;
void bluetoothMode(int mode){
    localRoutePending=-1;
    pendingConnectivity=-1; // An explicit BT choice supersedes deferred offline phone selection.
    if(mode!=0)speakerSearchPending=false;
    if(mode==0&&config.getMode()==PM_WEB){
        if(!player.sendCommand({PR_BT,-1})){btRateNotice("BT foglalt, probald ujra");return;}
        speakerSearchPending=true;speakerSearchStep=0;speakerSearchAt=millis();
        page=PAGE_SOURCE_PENDING;pendingSourceTarget=PM_SDCARD;pendingSourceAt=0;selected=0;redraw=true;return;
    }
    if(!player.sendCommand({PR_BT,mode})){btRateNotice("BT parancs var, probald ujra");redraw=true;return;}
    if(page!=PAGE_BLUETOOTH&&page!=PAGE_BT_SCAN&&page!=PAGE_BT_SAVED){bluetoothParent=page;bluetoothParentRow=selected;}
    cancelAlarmForUser();
    const auto bs=snapshot().settings;
    page=mode==1?((bs.vintageSkin==5||bs.vintageSkin==6)?PAGE_HOME:PAGE_BT_PLAYER):mode==0?PAGE_BT_SCAN:PAGE_HOME;
    selected=0;redraw=true;
}
static void reconnectDevice(unsigned index){
    if(config.getMode()==PM_WEB){
        btRateNotice("BT OUT csak SD forrasbol");return;
    }
    btaudio::Device device{};
    if(!btaudio::savedDevice(index,device)||!player.sendCommand({PR_BT,0})||
       !btaudio::connectSavedDevice(index)){redraw=true;return;}
    cancelAlarmForUser();
    page=PAGE_BT_SAVED;selected=index;redraw=true;
}
unsigned memoryStressStatus(){return stressState;}
static void runMemoryStress(){
    if(player.isRunning()||player.status()!=STOPPED){stressState=4;redraw=true;return;}
    if(!ibtlimit::acquire(ibtlimit::Job::Diagnostic,4096)){stressState=3;redraw=true;return;}
    v027::testHighMemory();
    ibtlimit::release(ibtlimit::Job::Diagnostic);
    stressState=!strcmp(v027::memoryReport().highResult,"PASS")?1:2;
    redraw=true;
}
const char* stationSearchQuery(){return searchQuery;}
unsigned stationSearchCount(){return searchFound;}
uint16_t stationSearchResult(unsigned index){return searchResults&&index<searchFound?searchResults[index]:0;}
bool stationSearchResultName(unsigned index,char* out,size_t cap){
    if(!out||!cap)return false;
    out[0]=0;
    uint16_t station=stationSearchResult(index);
    if(!station)return false;
    if(searchCacheLock&&xSemaphoreTake(searchCacheLock,pdMS_TO_TICKS(2))==pdTRUE){
        if(searchNames&&station<=searchRows){
            strlcpy(out,searchNames+size_t(station-1)*BUFLEN,cap);
            xSemaphoreGive(searchCacheLock);
            return out[0]!=0;
        }
        xSemaphoreGive(searchCacheLock);
    }
    // The filesystem fallback is safe only after playback has stopped.
    if(player.isRunning()||player.status()!=STOPPED)return false;
    const char* name=config.stationByNum(station);
    if(!name)return false;
    strlcpy(out,name,cap);
    return out[0]!=0;
}
unsigned stationSearchStatus(){return searchState;}
static char searchFold(const unsigned char*& p){
    unsigned char c=*p++;
    if(c<128)return char(toupper(c));
    if(c==0xC3&&*p){
        switch(*p++){
          case 0x81:case 0xA1:return 'A';
          case 0x89:case 0xA9:return 'E';
          case 0x8D:case 0xAD:return 'I';
          case 0x93:case 0xB3:case 0x96:case 0xB6:return 'O';
          case 0x9A:case 0xBA:case 0x9C:case 0xBC:return 'U';
          default:return 0;
        }
    }
    if(c==0xC5&&*p){
        switch(*p++){
          case 0x90:case 0x91:return 'O';
          case 0xB0:case 0xB1:return 'U';
          default:return 0;
        }
    }
    return 0;
}
static bool searchName(const char* name){
    char folded[170];size_t n=0;const unsigned char* p=(const unsigned char*)name;
    while(*p&&n<sizeof(folded)-1){char c=searchFold(p);if(c)folded[n++]=c;}
    folded[n]=0;return strstr(folded,searchQuery)!=nullptr;
}
void invalidateStationSearchCache(){
    if(searchCacheLock)xSemaphoreTake(searchCacheLock,portMAX_DELAY);
    searchRows=0;searchFound=0;
    if(searchCacheLock)xSemaphoreGive(searchCacheLock);
}
bool preloadStationSearch(){
    if(config.getMode()!=PM_WEB||player.isRunning()||player.status()!=STOPPED)return false;
    if(!searchCacheLock||xSemaphoreTake(searchCacheLock,pdMS_TO_TICKS(500))!=pdTRUE)return false;
    if(searchRows){xSemaphoreGive(searchCacheLock);return true;}
    if(!searchNames)searchNames=(char*)heap_caps_malloc(480u*BUFLEN,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!searchNames){xSemaphoreGive(searchCacheLock);return false;}
    File index=SPIFFS.open(INDEX_PATH,"r"),playlist=SPIFFS.open(PLAYLIST_PATH,"r");
    bool good=index&&playlist&&index.size()>=4&&index.size()%4==0&&index.size()/4<=480;
    uint16_t count=good?uint16_t(index.size()/4):0;
    for(uint16_t i=0;good&&i<count;i++){
        uint32_t pos=0;
        if(index.readBytes((char*)&pos,4)!=4||pos>=playlist.size()||!playlist.seek(pos)){good=false;break;}
        char* dest=searchNames+size_t(i)*BUFLEN;size_t used=0;int c;
        while((c=playlist.read())>=0&&c!='\t'&&c!='\r'&&c!='\n'){
            if(c==0||used>=BUFLEN-1){good=false;break;}
            dest[used++]=char(c);
        }
        if(c!='\t'||!used)good=false;
        dest[used]=0;
    }
    index.close();playlist.close();
    searchRows=good?count:0;
    xSemaphoreGive(searchCacheLock);
    return good;
}
static bool searchCached(){
    if(!searchCacheLock||xSemaphoreTake(searchCacheLock,pdMS_TO_TICKS(2))!=pdTRUE)return false;
    bool ready=searchRows>0;
    if(ready){
        for(uint16_t i=0;i<searchRows;i++)
            if(searchName(searchNames+size_t(i)*BUFLEN))searchResults[searchFound++]=uint16_t(i+1);
    }
    xSemaphoreGive(searchCacheLock);
    return ready;
}
static void executeStationSearch(){
    searchFound=0;searchState=0;
    if(!searchResults)searchResults=(uint16_t*)heap_caps_calloc(480,sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!searchResults){searchState=2;redraw=true;return;}
    if(config.getMode()!=PM_WEB){searchState=4;redraw=true;return;}
    if(!searchQuery[0]){searchState=3;redraw=true;return;}
    if(searchCached()){
        searchState=searchFound?0:3;
        if(searchFound){page=PAGE_SEARCH_RESULTS;selected=0;}
        redraw=true;return;
    }
    if(player.isRunning()||player.status()!=STOPPED){searchState=1;redraw=true;return;}
    if(preloadStationSearch()&&searchCached()){
        searchState=searchFound?0:3;
        if(searchFound){page=PAGE_SEARCH_RESULTS;selected=0;}
        redraw=true;return;
    }
    File index=SPIFFS.open(INDEX_PATH,"r");
    File playlist=SPIFFS.open(PLAYLIST_PATH,"r");
    if(!index||!playlist||index.size()<4||index.size()%4){searchState=2;index.close();playlist.close();redraw=true;return;}
    size_t count=index.size()/4;if(count>480)count=480;
    for(size_t i=0;i<count;i++){
        if(player.isRunning()||player.status()!=STOPPED){searchState=1;searchFound=0;break;}
        uint32_t pos=0;
        if(index.readBytes((char*)&pos,4)!=4||pos>=playlist.size()||!playlist.seek(pos)){searchState=2;searchFound=0;break;}
        char name[170];size_t used=0;int c;
        while((c=playlist.read())>=0&&c!='\t'&&c!='\n'&&c!='\r'){
            if(used<sizeof(name)-1)name[used++]=char(c);
        }
        name[used]=0;
        if(searchName(name))searchResults[searchFound++]=uint16_t(i+1);
        if((i&15)==15)vTaskDelay(1);
    }
    index.close();playlist.close();
    if(searchState==0)searchState=searchFound?0:3;
    if(searchFound){page=PAGE_SEARCH_RESULTS;selected=0;}
    redraw=true;
}
static uint32_t playlistHash(){
    File file=SPIFFS.open(PLAYLIST_PATH,"r");if(!file)return 0;
    uint32_t hash=2166136261u;uint8_t buffer[256];
    while(file.available()){
        size_t n=file.read(buffer,sizeof(buffer));if(!n)break;
        for(size_t i=0;i<n;i++){hash^=buffer[i];hash*=16777619u;}
    }
    file.close();return hash?hash:1;
}
bool favorite(uint16_t n){
    if(n<1||n>480)return false;
    portENTER_CRITICAL(&mux);bool yes=(favoriteBits[(n-1)/8]>>(uint8_t((n-1)%8)))&1;portEXIT_CRITICAL(&mux);
    return yes;
}
unsigned favoritesCount(){
    unsigned count=0;portENTER_CRITICAL(&mux);
    for(uint8_t b:favoriteBits){while(b){count+=b&1;b>>=1;}}
    portEXIT_CRITICAL(&mux);return count;
}
uint16_t nextFavorite(uint16_t from,bool forward){
    uint16_t n=from;
    for(unsigned i=0;i<480;i++){
        n=forward?(n>=480?1:n+1):(n<=1?480:n-1);
        if(favorite(n))return n;
    }
    return 0;
}
static void toggleFavorite(uint16_t n){
    if(n<1||n>480)return;
    portENTER_CRITICAL(&mux);favoriteBits[(n-1)/8]^=uint8_t(1u<<((n-1)%8));
    favoriteVersion++;state.pendingSave=true;portEXIT_CRITICAL(&mux);redraw=true;
}
void invalidateFavorites(){
    portENTER_CRITICAL(&mux);memset(favoriteBits,0,sizeof(favoriteBits));
    favoritePlaylistHash=0;favoriteVersion++;state.pendingSave=true;portEXIT_CRITICAL(&mux);redraw=true;
}
static std::atomic<uint32_t> clockSyncEvents{0};
uint32_t clockSyncCount(){return clockSyncEvents.load(std::memory_order_relaxed);}
void clockConfigure(const char* server) {
    // NVS/config is loaded before STA association. Keep the local timezone
    // available for the clock face, but start SNTP only after an IP exists.
    setenv("TZ",DUAL_TZ,1);
    tzset();
    if(!ensureClockStore())return;
    if(!clockStore->lock)clockStore->lock=xSemaphoreCreateMutex(); // first call is single-threaded boot
    if(WiFi.status()!=WL_CONNECTED || uint32_t(WiFi.localIP())==0)return;
    if(!clockStore->lock||xSemaphoreTake(clockStore->lock,pdMS_TO_TICKS(100))!=pdTRUE)return;
    // The lwIP API retains the hostname pointer. Keep an owned buffer alive,
    // stopping the old request before changing it.
    esp_sntp_stop();
    strlcpy(clockStore->active,server?server:clocksync::server(0),sizeof(clockStore->active));
    // This build has CONFIG_LWIP_SNTP_MAX_SERVERS=1: all fallbacks must
    // explicitly replace slot zero instead of being silently ignored.
    sntp_set_time_sync_notification_cb([](timeval*){
        clockSyncEvents.fetch_add(1,std::memory_order_relaxed);
    });
    configTzTime(DUAL_TZ,clockStore->active);
    sntp_set_sync_interval(6UL*60UL*60UL*1000UL);
    xSemaphoreGive(clockStore->lock);
}
Snapshot snapshot() {
    Snapshot s;portENTER_CRITICAL(&mux);s=state;portEXIT_CRITICAL(&mux);return s;
}
bool debugEnabled(){portENTER_CRITICAL(&mux);bool v=state.settings.debug;portEXIT_CRITICAL(&mux);return v;}
bool uiOverloadActive(){return uiGovernor.inputOverloadActive(millis());}
bool sourcePendingSd(){return pendingSourceTarget==PM_SDCARD;}
static bool calibrateTouch(bool force=false){
#if defined(BOARD_CYD28)
    if(!frontendOK||(!force&&!board::touchCalibrationNeeded()))return true;const int sx[4]={24,295,295,24},sy[4]={24,24,215,215};Point r[4];Serial.println("[TOUCH] calibration start");
    for(unsigned i=0;i<4;i++){front::calibrationTarget(sx[i],sy[i],i+1);uint32_t deadline=millis()+30000;Point q;while(board::touchRaw(q)&&int32_t(deadline-millis())>0)delay(15);while(!board::touchRaw(q)&&int32_t(deadline-millis())>0)delay(15);if(int32_t(deadline-millis())<=0){front::calibrationDone(false);return false;}int32_t ax=0,ay=0;int n=0;while(board::touchRaw(q)&&n<16){ax+=q.x;ay+=q.y;n++;delay(12);}if(n<3){front::calibrationDone(false);return false;}r[i]={(int)(ax/n),(int)(ay/n)};while(board::touchRaw(q)&&int32_t(deadline-millis())>0)delay(15);}
    int hdx=((r[1].x+r[2].x)-(r[0].x+r[3].x))/2,hdy=((r[1].y+r[2].y)-(r[0].y+r[3].y))/2;bool swap=abs(hdy)>abs(hdx);auto rx=[&](int i){return swap?r[i].y:r[i].x;};auto ry=[&](int i){return swap?r[i].x:r[i].y;};float left=(rx(0)+rx(3))*.5f,right=(rx(1)+rx(2))*.5f,top=(ry(0)+ry(1))*.5f,bottom=(ry(2)+ry(3))*.5f;float x0=left+(left-right)*24.0f/271.0f,x1=right+(right-left)*24.0f/271.0f,y0=top+(top-bottom)*24.0f/191.0f,y1=bottom+(bottom-top)*24.0f/191.0f;board::TouchCalibration c{(int)min(x0,x1),(int)max(x0,x1),(int)min(y0,y1),(int)max(y0,y1),swap,x0>x1,y0>y1};bool ok=board::touchSaveCalibration(c);front::calibrationDone(ok);Serial.printf("[TOUCH] save=%s xmin=%d xmax=%d ymin=%d ymax=%d swap=%d invx=%d invy=%d\n",ok?"PASS":"FAIL",c.xMin,c.xMax,c.yMin,c.yMax,c.swapXY,c.invertX,c.invertY);return ok;
#else
    return true;
#endif
}
void begin() {
    monodsp::begin();
    if(!ensureClockStore())return;
    commands=xQueueCreate(12,sizeof(Action));
    webMenuCommands=xQueueCreate(4,sizeof(WebMenuCommand));
    if(!searchCacheLock)searchCacheLock=xSemaphoreCreateMutex();
    favoritePlaylistHash=playlistHash();
    Preferences p; if(p.begin("yo-dual-v1",true)) {
        state.settings.menuIdleSeconds=p.getUShort("menuidle",10);
        state.settings.dnsManual=p.getBool("dnsmanual",false);
        if(p.getBytesLength("dnsprimary")==4)p.getBytes("dnsprimary",state.settings.dnsPrimary,4);
        if(p.getBytesLength("dnssecond")==4)p.getBytes("dnssecond",state.settings.dnsSecondary,4);
        vuprobe::setRefresh(p.getUShort("vutimer",500));
#if IBT_NO_PSRAM
        lightui::style.store(lightui::validStyle(p.getUChar("yovu",1)));
        if(!p.getBool("yovuv1",false))vuprobe::setRefresh(100);
#endif
        state.settings.language=Language(p.getUChar("lang",0));
        state.settings.theme=Theme(p.getUChar("theme",0));
        state.settings.rotation=p.getUChar("rotate",0);
        state.settings.brightness=p.getUChar("bright",100);
        for(unsigned i=0;i<4;++i){char key[8];snprintf(key,sizeof(key),"ntp%u",i);
            String value=p.getString(key,clocksync::server(i));
            if(value.length()<64&&clocksync::validServer(value.c_str()))strlcpy(clockStore->servers.values[i],value.c_str(),64);
        }
        if(!clocksync::anyServer(clockStore->servers.values))strlcpy(clockStore->servers.values[0],clocksync::server(0),64);
        state.settings.alarm.mode=p.getUChar("alm_mode",0);
        state.settings.alarm.enabledProfile=p.getUChar("alm_prof",state.settings.alarm.mode==1?1:2);
        state.settings.alarm.onHour=p.getUChar("alm_h0",7);
        state.settings.alarm.onMinute=p.getUChar("alm_m0",0);
        state.settings.alarm.offHour=p.getUChar("alm_h1",7);
        state.settings.alarm.offMinute=p.getUChar("alm_m1",30);
        state.settings.alarm.station=p.getUShort("alm_station",1);
        state.settings.alarm.volumeStart=p.getUChar("alm_v0",10);
        state.settings.alarm.volumePeak=p.getUChar("alm_v1",70);
        state.settings.alarm.volumeEnd=p.getUChar("alm_v2",10);
        state.settings.alarm.volumeRise=p.getUChar("alm_vup",5);
        state.settings.alarm.volumeFall=p.getUChar("alm_vdown",5);
        state.settings.alarm.lightStart=p.getUChar("alm_l0",100);
        state.settings.alarm.lightPeak=p.getUChar("alm_l1",100);
        state.settings.alarm.lightEnd=p.getUChar("alm_l2",100);
        state.settings.alarm.lightRise=p.getUChar("alm_lup",5);
        state.settings.alarm.lightFall=p.getUChar("alm_ldown",5);
        state.settings.alarm.nightClock=p.getUChar("alm_clock",0)!=0;
        state.settings.alarm.clockHome=p.getUChar("alm_home",0)!=0;
        state.settings.alarm.daysMask=p.getUChar("alm_days",0x7f);
        state.settings.alarm.clockDisplay=p.getUChar("alm_view",0);
        state.settings.alarm.blinkColon=p.getBool("alm_blink",true);
        state.settings.alarm.clockStyle=p.getUChar("alm_font",0);state.settings.alarm.clockColor=p.getUChar("alm_ink",0);state.settings.alarm.textColor=p.getUChar("alm_text",0);
        state.settings.alarm.source=p.getUChar("alm_src",0);
        state.settings.alarm.sdPlayMode=p.getUChar("alm_sdplay",0);
        alarmLastDay=p.getUInt("alarmday",0);
        if(p.getBytesLength("eq10")==sizeof(state.settings.graphicEq))
            p.getBytes("eq10",&state.settings.graphicEq,sizeof(state.settings.graphicEq));
        state.settings.sensorMode=p.getUChar("sensor",0);
        state.settings.batteryIndicator=p.getUChar("batshow",0)!=0;
        state.settings.batteryStep=p.getUChar("batstep",1);
        state.settings.batteryAppearance=p.getUChar("batstyle",0);
        const char* keys[]={"bat0","bat25","bat50","bat75","bat100"};
        for(unsigned i=0;i<5;i++)state.settings.batteryMv[i]=p.getUShort(keys[i],state.settings.batteryMv[i]);
        state.settings.maxKbps=p.getUShort("rate",0);
        state.settings.rotaryMode=p.getUChar("rotmode",1);
        state.settings.volumeStep=p.getUChar("volstep",1);
        state.settings.debug=p.getUChar("debug",0)!=0;
        state.settings.artworkEnabled=p.getUChar("artwork",IBT_NO_PSRAM?0:1)!=0;
        state.settings.cassetteEnabled=p.getUChar("cassette",1)!=0;
        state.settings.bufferIndicator=p.getUChar("bufbar",1)!=0;
        state.settings.reelAnimation=p.getUChar("reels",0)!=0;
        state.settings.wifiIndicator=p.getUChar("wifibar",1)!=0;
        state.settings.btQuickScan=p.getUChar("btquick",1)!=0;
        state.settings.playerEnabled=p.getUChar(playerSettingKey,1)!=0;
        state.settings.mediaPlayerSkin=p.getUChar("mediaskin",0)!=0;
        state.settings.kajonSkin=false; // Retired SMALL skin; saved ID falls back to Winamp.
        state.settings.vintageSkin=p.getUChar("vinskin",0);
        state.settings.bootScreen=p.getUChar("bootart",0);
        state.settings.bootSound=p.getUChar("bootsound",0);
        state.settings.networkMode=p.getUChar("netmode",0);
        state.settings.retroColor=p.getUChar("retrocol",0);
        // R74 shipped this disabled, which hid the gesture even on touch panels.
        // Enable it once for R75; subsequent user choices remain persistent.
        state.settings.swipeSkins=p.getUChar("swipev2",0)?p.getUChar("swipeskin",1)!=0:true;
        state.settings.hlsEnabled=p.getUChar("hls",0)!=0;
        if(favoritePlaylistHash && p.getUInt("favhash",0)==favoritePlaylistHash && p.getBytesLength("favorites")==sizeof(favoriteBits))p.getBytes("favorites",favoriteBits,sizeof(favoriteBits));
        if(!p.getBool("light100v1",false)){
            state.settings.brightness=100;state.settings.alarm.lightStart=100;state.settings.alarm.lightPeak=100;state.settings.alarm.lightEnd=100;
            state.pendingSave=true;++settingsVersion;
        }
        p.end();
    }
#if IBT_STREAM_ART_DIAG
    state.settings.networkMode=0;state.settings.alarm.clockHome=false;state.settings.alarm.mode=0;
    state.settings.artworkEnabled=true;state.settings.playerEnabled=true;state.settings.mediaPlayerSkin=false;
    state.settings.vintageSkin=0;state.settings.cassetteEnabled=false;state.settings.bootSound=0;
    state.settings.sensorMode=0;state.settings.swipeSkins=false;state.settings.maxKbps=0;
    lightui::style.store(0);vuprobe::dacEnabled.store(false);vuprobe::localBtEnabled.store(false);btinputvu::enabled.store(false);
#endif
    sanitize(state.settings);
#if !FEATURE_ARTWORK
    state.settings.artworkEnabled=false;
#endif
    publishSurface(state.settings);
    ibtdirect::setHlsEnabled(state.settings.hlsEnabled);
    config.store.volsteps=(uint8_t)max(1,(int(state.settings.volumeStep)*254+50)/100);
    config.store.audioinfo=state.settings.debug;
#if defined(BOARD_CYD28)
    const char *model="SMALL";
#else
    const char *model="BIG";
#endif
    if(strlen(DUAL_DEVICE_NAME))textCopy(state.device,sizeof(state.device),DUAL_DEVICE_NAME);
    else snprintf(state.device,sizeof(state.device),"I-BT-Radio %s %s-%06lX",IBT_FW_VERSION,model,(unsigned long)(ESP.getEfuseMac()&0xffffff));
    // Avoid stale settings from an older firmware enabling expensive legacy features.
    config.store.vumeter=false;config.store.showweather=false;
    config.store.watchdog=false; // use the client's bounded connect; do not kill a task inside TCP/TLS
    config.store.screensaverEnabled=false;config.store.screensaverPlayingEnabled=false;
    artwork::begin();
#if !IBT_STREAM_ART_DIAG
    btaudio::loadSavedDevice();
#endif
    onlineart::begin();
    onlineart::setEnabled(surfacepolicy::artwork(surfacepolicy::current())&&state.settings.artworkEnabled&&state.settings.vintageSkin!=5&&state.settings.vintageSkin!=6&&(IBT_NO_PSRAM||!state.settings.playerEnabled||state.settings.vintageSkin||state.settings.mediaPlayerSkin),state.generation);
    board::begin();board::brightness(state.settings.brightness?state.settings.brightness:100);
    frontendOK=front::begin();
    if(frontendOK){front::setRotation(0);
#if !IBT_STREAM_ART_DIAG
    calibrateTouch();
#endif
    front::setRotation(state.settings.rotation);}
    initialized=true;
    if(state.settings.networkMode&&!state.settings.alarm.clockHome){pendingConnectivity=state.settings.networkMode;connectivityStep=0;connectivityAt=0;}
    if(debugEnabled())Serial.printf("[DUAL] %s version=%s UI=%s commands=%s\n",state.device,DUAL_VERSION,frontendOK?"OK":"failed",commands?"OK":"failed");
}
void send(Action a) {
    if(btaudio::inputSelected()){
        const int key=a==Action::Prev?2:a==Action::Next?3:a==Action::Toggle?(btaudio::inputPlaying()?1:0):a==Action::PlayerPlay?0:a==Action::PlayerPause?1:-1;
        if(key>=0){
#if IBT_NO_PSRAM
            if(a==Action::Toggle||a==Action::PlayerPlay||a==Action::PlayerPause)front::transportAck();
#endif
            btaudio::control(unsigned(key));return;} // Never toggle a local source in BT-IN.
    }
    const bool expendable=a==Action::RotCW||a==Action::RotCCW||
      a==Action::VolDown||a==Action::VolUp;
    const bool queued=commands&&(!expendable||uxQueueMessagesWaiting(commands)<9)&&
        xQueueSend(commands,&a,0)==pdTRUE;
#if IBT_NO_PSRAM
    if(queued&&(a==Action::Toggle||a==Action::PlayerPlay||a==Action::PlayerPause))front::transportAck();
#endif
    if(!queued){
        portENTER_CRITICAL(&mux);state.queueDrops++;portEXIT_CRITICAL(&mux);
    }else if(a==Action::Prev||a==Action::Next||a==Action::Toggle||
             a==Action::PlayerPlay||a==Action::PlayerPause||
             a==Action::VolDown||a==Action::VolUp||
             a==Action::Source||a==Action::SourceWeb||a==Action::SourceSd){
        portENTER_CRITICAL(&mux);alarmManualOverride=a==Action::PlayerPause?2:1;portEXIT_CRITICAL(&mux);
        alarmRequestEpoch.store(0,std::memory_order_release);
    }
}
// R10 diagnostic UI path: RAM-only by construction; persistent mutation paths are excluded.
bool r10DiagUi(R10DiagUiAction a){
    if(!initialized)return false;
    switch(a){
      case R10DiagUiAction::Menu: page=PAGE_MAIN;selected=0;break;
      case R10DiagUiAction::RotCW:
        if(page!=PAGE_MAIN){page=PAGE_MAIN;selected=0;} else selected=(selected+1)%4;break;
      case R10DiagUiAction::RotCCW:
        if(page!=PAGE_MAIN){page=PAGE_MAIN;selected=3;} else selected=(selected+3)%4;break;
      case R10DiagUiAction::Short:
        if(page==PAGE_MAIN){
          page=selected==0?PAGE_STATIONS:selected==1?PAGE_SOUND:selected==2?PAGE_BLUETOOTH:PAGE_SETTINGS;
          selected=0;
        } else {page=PAGE_MAIN;selected=0;}
        break;
      case R10DiagUiAction::Long:
#if defined(BOARD_CYD28)
        front::skinPreview(-1);
#endif
        page=PAGE_HOME;selected=0;break;
      case R10DiagUiAction::Double: page=(page==PAGE_BLUETOOTH)?PAGE_MEMORY:PAGE_BLUETOOTH;selected=0;break;
    }
    stationPreview=false;shortPending=false;secondDown=false;redraw=true;artwork::redraw(snapshot().generation);return true;
}
void r10DiagRecoverHome(){page=PAGE_HOME;selected=0;stationPreview=false;shortPending=false;secondDown=false;redraw=true;if(!btaudio::transmitting())artwork::redraw(snapshot().generation);}
unsigned r10DiagPage(){return page;}
unsigned r10DiagSelected(){return selected;}
bool r10UiSelfcheck(){
    if(!initialized||network.status==SOFT_AP)return false;
    r10DiagRecoverHome();
    input(Action::Menu);
    const bool opened=page==PAGE_MAIN;
    input(Action::EncoderLong);
    const bool longHome=page==PAGE_HOME;
    input(Action::EncoderShort);
    input(Action::EncoderDown);
    input(Action::EncoderLong);
    const bool comboMenu=page==PAGE_MAIN;
    input(Action::EncoderLong);
    const bool finalHome=page==PAGE_HOME;
    const bool ok=opened&&longHome&&comboMenu&&finalHome;
    Serial.printf("[UI-SELFTEST] result=%s menu=%u long_home=%u short_long_menu=%u final_home=%u\n",
      ok?"PASS":"FAIL",opened?1:0,longHome?1:0,comboMenu?1:0,finalHome?1:0);
    return ok;
}
bool r10UiStressStart(unsigned actionMs,unsigned drawMs,unsigned tour){
    Snapshot s=snapshot();
    const bool fullScan=tour==2;
    if(tour>2||uiStressActive||!initialized||!frontendOK||network.status==SOFT_AP||
       (!fullScan&&(config.getMode()!=PM_WEB||btaudio::wanted()))||
       !s.playing||(!fullScan&&s.buffer<(IBT_NO_PSRAM?8192u:32768u))||s.freeHeap<ibtlimit::Policy::reserve||
       actionMs<10||actionMs>500||drawMs<10||drawMs>500)return false;
    if(tour==1&&(actionMs<250||s.pendingSave||settingsVersion!=savedVersion))return false;
#if defined(BOARD_CYD28)
    front::skinPreview(-1);
#endif
    uiStressActive=true;uiStressInjecting=false;uiStressDraining=false;
    uiStressTour=tour==1;uiStressFullScan=fullScan;uiStressTourRestored=false;
    uiStressScanPage=0;uiStressScanItem=0;uiStressScanItems=0;uiStressScanDisabled=0;
    if(!uiStressScanSeen)uiStressScanSeen=static_cast<uint8_t (*)[2]>(heap_caps_calloc(UiScanSlots,2,litememory::payloadCaps));
    if(!uiStressScanSeen){uiStressActive=false;return false;}
    uiStressScanRendered=0;memset(uiStressScanSeen,0,UiScanSlots*2);
    memset(uiStressAlarmSeen,0,sizeof(uiStressAlarmSeen));
    uiStressOriginalSettings=s.settings;uiStressTourStep=0;
    uiStressTourVisits=0;uiStressTourEdits=0;uiStressTourCycles=0;
    uiStressActionMs=(uint16_t)actionMs;uiStressDrawMs=(uint16_t)drawMs;
    uiStressActions=0;uiStressFrames=0;uiStressRenderCalls=0;uiStressRenderedActions=0;
    uiStressStarted=millis();
    uiStressLastAction=uiStressStarted;uiStressLastCheck=uiStressStarted;
    uiStressBaseLow=s.lowBufferEvents;uiStressBaseDrops=s.queueDrops;
    uiStressBaseGaps=player.audioGapEvents();
    strlcpy(uiStressReason,"running",sizeof(uiStressReason));
    page=PAGE_MAIN;selected=0;redraw=true;
    return true;
}
void r10UiStressDrain(){uiStressDraining=true;}
void r10UiStressStop(const char* reason){
    if(!uiStressActive)return;
    uiStressActive=false;uiStressInjecting=false;
    if(uiStressTour){
        portENTER_CRITICAL(&mux);state.settings=uiStressOriginalSettings;portEXIT_CRITICAL(&mux);
        uiStressTour=false;uiStressTourRestored=true;redraw=true;
    }
    if(uiStressFullScan){uiStressFullScan=false;uiStressTourRestored=true;redraw=true;}
    strlcpy(uiStressReason,reason?reason:"stopped",sizeof(uiStressReason));
    r10DiagRecoverHome();
    char notice[sizeof(state.notice)]{};
    snprintf(notice,sizeof(notice),"TEST STOP %s",uiStressReason);
    portENTER_CRITICAL(&mux);
    memcpy(state.notice,notice,sizeof(notice));
    portEXIT_CRITICAL(&mux);
    Serial.printf("[UI-STRESS] stop=%s actions=%lu frames=%lu\n",uiStressReason,
      (unsigned long)uiStressActions,(unsigned long)uiStressFrames);
}
bool r10UiStressInfo(uint16_t& actionMs,uint16_t& drawMs,uint32_t& actions,uint32_t& frames){
    actionMs=uiStressActionMs;drawMs=uiStressDrawMs;actions=uiStressActions;frames=uiStressFrames;
    return uiStressActive;
}
void r10UiStressStatus(){
    Snapshot s=snapshot();
    Serial.printf("[UI-STRESS] active=%u reason=%s action_ms=%u draw_ms=%u actions=%lu frames=%lu render_calls=%lu elapsed_ms=%lu buffer=%lu low_events=%lu heap=%lu queue_drops=%lu gap_events=%lu tour=%u visits=%lu edits=%lu cycles=%lu restored=%u page=%u selected=%u scan_items=%lu scan_disabled=%lu scan_rendered=%lu\n",
      uiStressActive?1:0,uiStressReason,unsigned(uiStressActionMs),unsigned(uiStressDrawMs),
      (unsigned long)uiStressActions,(unsigned long)uiStressFrames,(unsigned long)uiStressRenderCalls,
      (unsigned long)(millis()-uiStressStarted),(unsigned long)s.buffer,
      (unsigned long)s.lowBufferEvents,(unsigned long)s.freeHeap,(unsigned long)s.queueDrops,
      (unsigned long)player.audioGapEvents(),
      uiStressFullScan?2:(uiStressTour?1:0),(unsigned long)uiStressTourVisits,(unsigned long)uiStressTourEdits,
      (unsigned long)uiStressTourCycles,uiStressTourRestored?1:0,page,selected,
      (unsigned long)uiStressScanItems,(unsigned long)uiStressScanDisabled,
      (unsigned long)uiStressScanRendered);
}
struct UiTourStep {Action action;uint8_t page,sel,nextPage,nextSel,edit;};
static constexpr uint8_t NO_EDIT=255;
static const char* uiTourPageName(unsigned p){
    switch(p){
      case PAGE_HOME:return "HOME";case PAGE_MAIN:return "MAIN";case PAGE_STATIONS:return "STATIONS";
      case PAGE_SOUND:return "SOUND";case PAGE_BLUETOOTH:return "BLUETOOTH";case PAGE_BT_INFO:return "BT_INFO";
      case PAGE_SETTINGS:return "SETTINGS";case PAGE_DISPLAY:return "DISPLAY";default:return "UNKNOWN";
    }
}
static const char* uiTourActionName(Action a){
    switch(a){case Action::Select:return "select";case Action::RotCW:return "rotary_next";
      case Action::RotCCW:return "rotary_prev";case Action::EncoderLong:return "long_press";
      case Action::Menu:return "menu";default:return "other";}
}
static constexpr UiTourStep uiTourSteps[]={
#if IBT_NO_PSRAM
    {Action::Select,PAGE_MAIN,0,PAGE_STATIONS,0,NO_EDIT},
    {Action::RotCW,PAGE_STATIONS,0,PAGE_STATIONS,1,NO_EDIT},
    {Action::EncoderLong,PAGE_STATIONS,1,PAGE_HOME,0,NO_EDIT},
    {Action::Menu,PAGE_HOME,0,PAGE_MAIN,0,NO_EDIT},
    {Action::RotCW,PAGE_MAIN,0,PAGE_MAIN,1,NO_EDIT},
    {Action::Select,PAGE_MAIN,1,PAGE_SOUND,0,NO_EDIT},
    {Action::RotCW,PAGE_SOUND,0,PAGE_SOUND,2,NO_EDIT},
    {Action::EncoderLong,PAGE_SOUND,2,PAGE_HOME,0,NO_EDIT},
    {Action::Menu,PAGE_HOME,0,PAGE_MAIN,0,NO_EDIT},
    {Action::RotCW,PAGE_MAIN,0,PAGE_MAIN,1,NO_EDIT},
    {Action::RotCW,PAGE_MAIN,1,PAGE_MAIN,2,NO_EDIT},
    {Action::RotCW,PAGE_MAIN,2,PAGE_MAIN,3,NO_EDIT},
    {Action::Select,PAGE_MAIN,3,PAGE_SETTINGS,0,NO_EDIT},
    {Action::RotCW,PAGE_SETTINGS,0,PAGE_SETTINGS,1,NO_EDIT},
    {Action::Select,PAGE_SETTINGS,1,PAGE_DISPLAY,0,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,0,PAGE_DISPLAY,1,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,1,PAGE_DISPLAY,2,NO_EDIT},
    {Action::Select,PAGE_DISPLAY,2,PAGE_VU_TIMER,0,NO_EDIT},
    {Action::RotCCW,PAGE_VU_TIMER,0,PAGE_VU_TIMER,7,NO_EDIT},
    {Action::Select,PAGE_VU_TIMER,7,PAGE_DISPLAY,2,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,2,PAGE_DISPLAY,3,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,3,PAGE_DISPLAY,4,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,4,PAGE_DISPLAY,5,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,5,PAGE_DISPLAY,6,NO_EDIT},
    {Action::Select,PAGE_DISPLAY,6,PAGE_SETTINGS,1,NO_EDIT},
    {Action::EncoderLong,PAGE_SETTINGS,1,PAGE_HOME,0,NO_EDIT},
    {Action::Menu,PAGE_HOME,0,PAGE_MAIN,0,NO_EDIT}
#else
    {Action::Select,PAGE_MAIN,0,PAGE_STATIONS,0,NO_EDIT},
    {Action::RotCW,PAGE_STATIONS,0,PAGE_STATIONS,1,NO_EDIT},
    {Action::RotCW,PAGE_STATIONS,1,PAGE_STATIONS,2,NO_EDIT},
    {Action::EncoderLong,PAGE_STATIONS,2,PAGE_HOME,0,NO_EDIT},
    {Action::Menu,PAGE_HOME,0,PAGE_MAIN,0,NO_EDIT},
    {Action::RotCW,PAGE_MAIN,0,PAGE_MAIN,1,NO_EDIT},
    {Action::Select,PAGE_MAIN,1,PAGE_SOUND,0,NO_EDIT},
    {Action::RotCW,PAGE_SOUND,0,PAGE_SOUND,1,NO_EDIT},
    {Action::RotCW,PAGE_SOUND,1,PAGE_SOUND,2,NO_EDIT},
    {Action::EncoderLong,PAGE_SOUND,2,PAGE_HOME,0,NO_EDIT},
    {Action::Menu,PAGE_HOME,0,PAGE_MAIN,0,NO_EDIT},
    {Action::RotCW,PAGE_MAIN,0,PAGE_MAIN,1,NO_EDIT},
    {Action::RotCW,PAGE_MAIN,1,PAGE_MAIN,2,NO_EDIT},
    {Action::Select,PAGE_MAIN,2,PAGE_BLUETOOTH,0,NO_EDIT},
    {Action::RotCW,PAGE_BLUETOOTH,0,PAGE_BLUETOOTH,1,NO_EDIT},
    {Action::RotCW,PAGE_BLUETOOTH,1,PAGE_BLUETOOTH,2,NO_EDIT},
    {Action::RotCW,PAGE_BLUETOOTH,2,PAGE_BLUETOOTH,3,NO_EDIT},
    {Action::Select,PAGE_BLUETOOTH,3,PAGE_BT_INFO,0,NO_EDIT},
    {Action::EncoderLong,PAGE_BT_INFO,0,PAGE_HOME,0,NO_EDIT},
    {Action::Menu,PAGE_HOME,0,PAGE_MAIN,0,NO_EDIT},
    {Action::RotCW,PAGE_MAIN,0,PAGE_MAIN,1,NO_EDIT},
    {Action::RotCW,PAGE_MAIN,1,PAGE_MAIN,2,NO_EDIT},
    {Action::RotCW,PAGE_MAIN,2,PAGE_MAIN,3,NO_EDIT},
    {Action::Select,PAGE_MAIN,3,PAGE_SETTINGS,0,NO_EDIT},
    {Action::RotCW,PAGE_SETTINGS,0,PAGE_SETTINGS,1,NO_EDIT},
    {Action::Select,PAGE_SETTINGS,1,PAGE_DISPLAY,0,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,0,PAGE_DISPLAY,1,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,1,PAGE_DISPLAY,2,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,2,PAGE_DISPLAY,3,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,3,PAGE_DISPLAY,4,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,4,PAGE_DISPLAY,5,NO_EDIT},
    {Action::Select,PAGE_DISPLAY,5,PAGE_DISPLAY,5,5},
    {Action::RotCW,PAGE_DISPLAY,5,PAGE_DISPLAY,6,NO_EDIT},
    {Action::Select,PAGE_DISPLAY,6,PAGE_DISPLAY,6,6},
    {Action::RotCW,PAGE_DISPLAY,6,PAGE_DISPLAY,7,NO_EDIT},
    {Action::Select,PAGE_DISPLAY,7,PAGE_DISPLAY,7,7},
    {Action::RotCW,PAGE_DISPLAY,7,PAGE_DISPLAY,8,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,8,PAGE_DISPLAY,9,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,9,PAGE_DISPLAY,10,NO_EDIT},
    {Action::RotCW,PAGE_DISPLAY,10,PAGE_DISPLAY,11,NO_EDIT},
    {Action::Select,PAGE_DISPLAY,11,PAGE_SETTINGS,1,NO_EDIT},
    {Action::EncoderLong,PAGE_SETTINGS,1,PAGE_HOME,0,NO_EDIT},
    {Action::Menu,PAGE_HOME,0,PAGE_MAIN,0,NO_EDIT}
#endif
};
static bool uiTourFlag(const Settings& s,uint8_t item){
    return item==5?s.bufferIndicator:item==6?s.reelAnimation:
           item==7?s.wifiIndicator:item==8?s.btQuickScan:s.playerEnabled;
}
static void r10UiTourStep(){
    const auto& step=uiTourSteps[uiStressTourStep];
    if(page!=step.page||selected!=step.sel){
        Serial.printf("[UI-TOUR] route=FAIL step=%u page=%u selected=%u\n",uiStressTourStep,page,selected);
        r10UiStressStop("menu_route_fail");return;
    }
    Settings before;
    if(step.edit!=NO_EDIT)before=snapshot().settings;
    uiStressInjecting=true;input(step.action);uiStressInjecting=false;
    if(!uiStressActive||page!=step.nextPage||selected!=step.nextSel){
        Serial.printf("[UI-TOUR] route=FAIL step=%u page=%u selected=%u\n",uiStressTourStep,page,selected);
        r10UiStressStop("menu_route_fail");return;
    }
    if(step.edit!=NO_EDIT){
        Snapshot after=snapshot();
        if(uiTourFlag(before,step.edit)==uiTourFlag(after.settings,step.edit)||after.pendingSave){
            Serial.printf("[UI-TOUR] edit=FAIL item=%u pending_save=%u\n",step.edit,after.pendingSave?1:0);
            r10UiStressStop("menu_edit_fail");return;
        }
        ++uiStressTourEdits;
        Serial.printf("[UI-TOUR] edit=PASS item=%u value=%u ram_only=1\n",
                      step.edit,uiTourFlag(after.settings,step.edit)?1:0);
    }
    Serial.printf("[UI-TOUR] step=%u result=PASS from=%s:%u action=%s to=%s:%u\n",
      unsigned(uiStressTourStep+1),uiTourPageName(step.page),step.sel,uiTourActionName(step.action),
      uiTourPageName(step.nextPage),step.nextSel);
    if(step.page!=step.nextPage)++uiStressTourVisits;
    ++uiStressActions;
    if(++uiStressTourStep>=sizeof(uiTourSteps)/sizeof(uiTourSteps[0])){
        uiStressTourStep=0;++uiStressTourCycles;
        Serial.printf("[UI-TOUR] cycle=PASS count=%lu visits=%lu edits=%lu\n",
                      (unsigned long)uiStressTourCycles,(unsigned long)uiStressTourVisits,
                      (unsigned long)uiStressTourEdits);
    }
}
static void r10UiFullScanStep();
static void r10UiStressTick(uint32_t now){
    static uint32_t sdStoppedAt=0;
    if(!uiStressActive){sdStoppedAt=0;return;}
    if(uint32_t(now-uiStressLastCheck)>=500){
        uiStressLastCheck=now;Snapshot s=snapshot();
        if(uiStressFullScan){
            if(s.lowBufferEvents>uiStressBaseLow||s.queueDrops>uiStressBaseDrops||
               player.audioGapEvents()>uiStressBaseGaps){
                Serial.printf("[UI-MENU-SCAN] audio_event low_delta=%lu drop_delta=%lu gap_delta=%lu\n",
                  (unsigned long)(s.lowBufferEvents-uiStressBaseLow),
                  (unsigned long)(s.queueDrops-uiStressBaseDrops),
                  (unsigned long)(player.audioGapEvents()-uiStressBaseGaps));
                uiStressBaseLow=s.lowBufferEvents;uiStressBaseDrops=s.queueDrops;
                uiStressBaseGaps=player.audioGapEvents();
            }
            if(s.freeHeap<ibtlimit::Policy::reserve){sdStoppedAt=0;r10UiStressStop("low_heap_guard");return;}
            if(!s.playing){
                // SD EOF/reopen briefly reports stopped. Pause the menu, never
                // disable heap protection or tolerate a persistent stopped player.
                if(s.sd){
                    if(!sdStoppedAt)sdStoppedAt=now;
                    if(uint32_t(now-sdStoppedAt)<1000)return;
                }
                sdStoppedAt=0;r10UiStressStop("playback_stopped_guard");return;
            }
            sdStoppedAt=0;
        }else if(!s.playing||s.buffer<16384||s.lowBufferEvents>uiStressBaseLow||
           s.queueDrops>uiStressBaseDrops||s.freeHeap<ibtlimit::Policy::reserve){
            r10UiStressStop("audio_or_memory_guard");return;
        }
        if(!uiStressFullScan&&player.audioGapEvents()>uiStressBaseGaps){
            r10UiStressStop("audio_gap_guard");return;
        }
    }
    if(uiStressDraining)return; // Finish the pending guarded render without requesting another row.
    if(sdStoppedAt)return; // Keep the scan paused between the 500 ms guard checks.
    // Never advance a requested menu row before its guarded render completes.
    if(uiStressFullScan&&uiStressActions!=uiStressRenderedActions)return;
    if(uint32_t(now-uiStressLastAction)>=uiStressActionMs){
        uiStressLastAction=now;
        if(uiStressFullScan)r10UiFullScanStep();
        else if(uiStressTour)r10UiTourStep();
        else {uiStressInjecting=true;input(Action::RotCW);uiStressInjecting=false;++uiStressActions;}
    }
}
void track(const char *path) {
    portENTER_CRITICAL(&mux);
    state.generation++;state.artist[0]=state.album[0]=state.title[0]=state.notice[0]=0;
    if(path&&*path&&config.getMode()==PM_SDCARD) {
        const char *base=strrchr(path,'/');
        textCopy(state.title,sizeof(state.title),base?base+1:path);
    }
    uint32_t generation=state.generation;
    portEXIT_CRITICAL(&mux);
    bitrateRejected=false;player.eofHeader=false;
    artwork::reset(config.getMode()==PM_SDCARD?path:"",generation);
    onlineart::reset(generation);
    if(config.getMode()==PM_WEB && config.stationArtworkUrl()[0]) onlineart::requestUrl(config.stationArtworkUrl(),generation);
}
static void metadata(char *dest,const char *s) {portENTER_CRITICAL(&mux);textCopy(dest,170,s);portEXIT_CRITICAL(&mux);}
void metadataArtist(const char *s){metadata(state.artist,s);}
void metadataAlbum(const char *s){metadata(state.album,s);}
void metadataTitle(const char *s){metadata(state.title,s);if(config.getMode()==PM_WEB)onlineart::requestTrack(s,snapshot().generation);}
static __attribute__((noinline)) void metadataTail(const char* field,const char* text){
    uint32_t tail[8]{};unsigned count=0;const char* p=text;
    while(*p){tail[count%8]=tracktext::next(p);++count;}
    Serial.printf("[TRACK-TEXT] field=%s bytes=%u codepoints=%u tail=",field,unsigned(strlen(text)),count);
    for(unsigned i=count>8?count-8:0;i<count;++i)Serial.printf("U+%04lX%s",(unsigned long)tail[i%8],i+1<count?",":"");
    Serial.println();
}
__attribute__((noinline)) void trackTextDiagnostics(){
    char artist[170]{},title[170]{};bool sd=false;const bool bt=btaudio::inputSelected();
    if(bt){const auto media=btmedia::view();tracktext::copy(artist,sizeof(artist),media.artist);tracktext::copy(title,sizeof(title),media.title);}
    else {portENTER_CRITICAL(&mux);memcpy(artist,state.artist,sizeof(artist));memcpy(title,state.title,sizeof(title));sd=state.sd;portEXIT_CRITICAL(&mux);}
    artist[sizeof(artist)-1]=title[sizeof(title)-1]=0;
    Serial.printf("[TRACK-TEXT] source=%s read_only=1\n",bt?"BT_IN":sd?"SD":"WIFI");
    metadataTail("raw_artist",artist);metadataTail("raw_title",title);
    tracktext::normalize(artist,sizeof(artist),title,sizeof(title),!bt&&!sd,sd&&!bt);
    metadataTail("clean_artist",artist);metadataTail("clean_title",title);
}
// Keep this work outside both render frames and the audio snapshot publisher.
__attribute__((noinline)) void prepareTrackText(Snapshot& s) {
    const bool btIn=btaudio::inputSelected();
    if(btIn){
        s.playing=btaudio::inputPlaying(); // Presentation follows phone pause/play, not merely BT selected.
        tracktext::copy(s.station,sizeof(s.station),"BT IN");
        if(btaudio::connected()){
            const auto media=btmedia::view();
            tracktext::copy(s.artist,sizeof(s.artist),media.artist);
            tracktext::copy(s.title,sizeof(s.title),media.title);
        }else s.artist[0]=s.title[0]=0;
    }
    tracktext::normalize(s.artist,sizeof(s.artist),s.title,sizeof(s.title),!btIn&&!s.sd,s.sd&&!btIn);
    s.album[0]=0;
}
void imageHint(size_t pos,size_t size){artwork::hint(pos,size);}
void displayEvent(int type,int payload) {
    (void)payload;
    if(type==DSP_START){portENTER_CRITICAL(&mux);state.ready=true;portEXIT_CRITICAL(&mux);}
}
    static constexpr uint8_t uiStressScanPages[]={PAGE_MAIN,PAGE_STATIONS,PAGE_SOUND,
      PAGE_VOLUME_STREAM,73,PAGE_EQ,PAGE_EQ10,PAGE_EQ_PRESETS,PAGE_OUTPUTS,PAGE_BLUETOOTH,PAGE_BT_RATE,
      PAGE_SETTINGS,PAGE_WIFI,PAGE_DISPLAY,PAGE_SURFACE,PAGE_RETRO_COLOR,PAGE_CONTROLS,
      PAGE_STORAGE,PAGE_SYSTEM,PAGE_BACKUP_MENU,PAGE_DIAGNOSTICS,PAGE_IDLE,PAGE_VU_TIMER,PAGE_HARDWARE,PAGE_BAT_CAL,PAGE_BAT_DISPLAY,PAGE_DNS,PAGE_DNS_ADDRESS,60,61,62,63,64,70,71,72,
#if IBT_COMPACT_UI
      74,
#endif
      PAGE_ALARM};
static_assert(sizeof(uiStressScanPages)==UiScanSlots+1);
static unsigned scanSlot(unsigned p){for(unsigned i=0;i<sizeof(uiStressScanPages);i++)if(uiStressScanPages[i]==p)return i;return 255;}
static unsigned menuCount(unsigned p){return content::itemCount(p);}
static bool menuEnabled(unsigned p,unsigned item){
#if IBT_COMPACT_UI
    if(p==74)return item<17&&item!=15;
#endif
#if IBT_COMPACT_UI
    if(p==PAGE_SOUND&&(item==1||item==4))return false;
    if(p==PAGE_DISPLAY&&(item==3||item==9))return false;
    if(p==PAGE_SURFACE)return item<4;
    if(p==PAGE_EQ||p==PAGE_EQ10||p==73)return false;
#else
    if(p==PAGE_MAIN&&item==4)return false;
    if(p==PAGE_SOUND&&(item==1||item==4))return false;
    if(p==PAGE_SETTINGS&&(item==3||item==4))return false;
    if(p==PAGE_DISPLAY)return item<7;
    if(p==PAGE_SURFACE||p==PAGE_ALARM||p==PAGE_EQ||p==PAGE_EQ10||p==73)return false;
    if(p==PAGE_BT_RATE&&IBT_NO_PSRAM&&item<4&&item!=1)return false;

#endif

    switch(p){
      case 75:return item<5;
      case PAGE_MAIN:return item<5;
      case PAGE_STATIONS:return item<=2||(FEATURE_SD&&item==3)||(item>=4&&item<8);
      case PAGE_SOUND:return item<=5;
      case 73:return item<13&&item!=3;
      case PAGE_VOLUME_STREAM:return item<5;
      case PAGE_EQ:return item<5;
      case PAGE_EQ10:return item<13;
      case PAGE_EQ_PRESETS:return item<=eq10::PresetCount;
      case PAGE_OUTPUTS:return item==0||item==2||item==3||item==4;
      case PAGE_BLUETOOTH:return item<content::itemCount(p);
      case PAGE_BT_RATE:return item<5;
      case PAGE_SETTINGS:return item<content::itemCount(p);
      case 72:return item<9;
      case PAGE_ALARM:return item<7;
      case 64:return item<content::itemCount(p)&&item!=4;
      case 70:return item==4||(item<3&&sensors::available());
      case 71:return item==8||item<sensors::deviceCount();
      case 60:case 61:case 62:case 63:case 68:return item<content::itemCount(p);
      case PAGE_HARDWARE:return item==1?sensors::available():item<content::itemCount(p);
      case PAGE_BAT_CAL:return item<6;
      case PAGE_BAT_DISPLAY:return item<4;
      case PAGE_VU_TIMER:return item<content::itemCount(p);
      case PAGE_IDLE:return item<8;
      case PAGE_WIFI:return item<9;
      case PAGE_DNS:return item<7;
      case PAGE_DNS_ADDRESS:return item<6;
      case PAGE_DISPLAY:return item<content::itemCount(p);
      case PAGE_SURFACE:return item<15&&item!=4;
      case PAGE_RETRO_COLOR:return item<10;
      case PAGE_CONTROLS:return item<=4||item==6||(IBT_COMPACT_UI&&item==5);
      case PAGE_STORAGE:return item==0||(FEATURE_SD&&item==1)||item==2||item==3||item==4;
      case PAGE_SYSTEM:return item<=3||(FEATURE_SD&&item==4)||item==5;
      case PAGE_BACKUP_MENU:return item<=4;
      case PAGE_DIAGNOSTICS:return item<content::itemCount(p);
      default:return false;
    }
}
static void r10UiFullScanStep(){
    // Every visible row on every menu page is rendered; no action is activated.
    // This keeps setup, pairing, restart and backup operations untouched.
    const auto& pages=uiStressScanPages;
    const uint8_t next=pages[uiStressScanPage];
    if(page!=next)++uiStressTourVisits;
    page=next;selected=uiStressScanItem;redraw=true;
    const bool enabled=menuEnabled(page,selected);
    if(!enabled)++uiStressScanDisabled;
    ++uiStressScanItems;++uiStressActions;
    Serial.printf("[UI-MENU-SCAN] page=%u item=%u enabled=%u result=RENDER_REQUESTED action=NOT_EXECUTED\n",
      unsigned(page),unsigned(selected),enabled?1:0);
    if(++uiStressScanItem>=menuCount(page)){
        uiStressScanItem=0;
        if(++uiStressScanPage>=sizeof(pages)/sizeof(pages[0])){
            uiStressScanPage=0;++uiStressTourCycles;
            Serial.printf("[UI-MENU-SCAN] cycle=REQUESTED pages=%u items=%lu disabled=%lu rendered=%lu render_calls=%lu frames=%lu\n",
              unsigned(sizeof(pages)/sizeof(pages[0])),
              (unsigned long)uiStressScanItems,(unsigned long)uiStressScanDisabled,
              (unsigned long)uiStressScanRendered,
              (unsigned long)uiStressRenderCalls,(unsigned long)uiStressFrames);
        }
    }
}
static void persistSetting(const Settings &s){
    publishSurface(s);
    if(uiStressActive&&uiStressTour&&uiStressInjecting){
        // Exercise the real menu editor in RAM; do not wear flash during a sweep.
        portENTER_CRITICAL(&mux);state.settings=s;portEXIT_CRITICAL(&mux);return;
    }
    const auto previous=snapshot().settings;
    if(alarmPhase!=AlarmPhase::Idle&&(previous.brightness!=s.brightness||previous.sensorMode!=s.sensorMode)){
        // Keep the alarm's audio schedule, but the user's light selection wins
        // for this occurrence, including a switch back to the ambient sensor.
        alarmLastLight=AlarmManualLight;
    }
    if(alarmPhase!=AlarmPhase::Idle&&!alarmclock::samePlayback(previous.alarm,s.alarm)){
        // A new schedule must not change the displayed deadline while the
        // previous schedule continues underneath it. Stop the old occurrence.
        alarmRequestEpoch.store(0,std::memory_order_release);
        alarmPhase=AlarmPhase::Ending;alarmRetryAt=0;
        Serial.println("[ALARM] result=SETTINGS_CHANGED_STOP");
    }
    portENTER_CRITICAL(&mux);state.settings=s;state.pendingSave=true;settingsVersion++;portEXIT_CRITICAL(&mux);
}
static void editDisplay(unsigned item){
    Settings s=snapshot().settings;
    switch(item){
      case 0:s.theme=Theme((unsigned(s.theme)+1)%3);break;
      case 1:s.rotation=(s.rotation+(IBT_NO_PSRAM?2:1))&3;break;
      case 2:s.brightness=light::next(s.brightness,s.sensorMode==2);if(!s.brightness)s.sensorMode=1;break;
      case 3:s.artworkEnabled=!s.artworkEnabled;break;
      case 4:s.cassetteEnabled=!s.cassetteEnabled;if(!s.cassetteEnabled&&!s.playerEnabled)s.playerEnabled=true;break;
      case 5:s.bufferIndicator=!s.bufferIndicator;break;
      case 6:s.reelAnimation=!s.reelAnimation;break;
      case 7:s.wifiIndicator=!s.wifiIndicator;break;
      case 8:s.btQuickScan=!s.btQuickScan;break;
      case 9:s.batteryIndicator=!s.batteryIndicator;break;
      case 10:s.mediaPlayerSkin=!s.mediaPlayerSkin;if(s.mediaPlayerSkin)s.playerEnabled=true;break;
      default:return;
    }
    persistSetting(s);
    if(item==1&&frontendOK)front::setRotation(s.rotation);
    if(item==2)board::brightness(s.brightness?s.brightness:100);
    if(item==9&&frontendOK)front::invalidate();
    if(item==3||item==9||item==10){
        Snapshot current=snapshot();onlineart::setEnabled(surfacepolicy::artwork(surfacepolicy::current())&&s.artworkEnabled&&s.vintageSkin!=5&&s.vintageSkin!=6&&(IBT_NO_PSRAM||!s.playerEnabled||s.vintageSkin||s.mediaPlayerSkin),current.generation);
        if(s.artworkEnabled&&s.vintageSkin!=5&&s.vintageSkin!=6&&(IBT_NO_PSRAM||!s.playerEnabled||s.vintageSkin||s.mediaPlayerSkin)){
            if(config.stationArtworkUrl()[0])onlineart::requestUrl(config.stationArtworkUrl(),current.generation);
            if(current.title[0])onlineart::requestTrack(current.title,current.generation);
        }
    }
    redraw=true;
}
static void editSystem(unsigned item){
    if(item!=0)return;
    Settings s=snapshot().settings;
    s.language=Language((unsigned(s.language)+1)%3);
    persistSetting(s);redraw=true;
}
static void editControls(unsigned item){
    if(item!=0)return;Settings s=snapshot().settings;s.rotaryMode=s.rotaryMode==1?2:1;persistSetting(s);redraw=true;
}
static void editVolumeStream(unsigned item){
    Settings s=snapshot().settings;
    if(item==0){s.volumeStep=s.volumeStep>=10?1:s.volumeStep+1;persistSetting(s);config.store.volsteps=(uint8_t)max(1,(int(s.volumeStep)*254+50)/100);}
    else if(item==1){const uint16_t values[]={64,96,128,192,320,0};unsigned i=0;while(i<5&&values[i]!=s.maxKbps)i++;s.maxKbps=values[(i+1)%6];persistSetting(s);}
    else if(item==2){s.hlsEnabled=!s.hlsEnabled;persistSetting(s);ibtdirect::setHlsEnabled(s.hlsEnabled);}
    else return;
    redraw=true;
}
static void editDiagnostics(unsigned item){
    if(item!=0)return;Settings s=snapshot().settings;s.debug=!s.debug;persistSetting(s);config.store.audioinfo=s.debug;redraw=true;
}
static uint32_t shortAt=0;
static uint32_t restartArmedAt=0;
static uint32_t backupArmedAt=0;
static unsigned backupArmedItem=0;
bool backupRestoreArmed(){return page==PAGE_BACKUP_MENU && selected==backupArmedItem && backupArmedAt && uint32_t(millis()-backupArmedAt)<=5000;}
static uint16_t channelCursor=1;

static uint16_t wrapStation(int value) {
    int n=(int)config.playlistLength();
    if(n<1)return 1;
    while(value<1)value+=n;
    while(value>n)value-=n;
    return (uint16_t)value;
}
static void showStationPreview(uint16_t station) {
    const char *name=config.stationByNum(station);
    // UI task owns this scratch buffer. Do filesystem/stdio work before entering
    // the spinlock; a static buffer avoids increasing the small display stack.
    static char preview[sizeof(state.notice)];
    const unsigned count=config.playlistLength();
    snprintf(preview,sizeof(preview),"[%u/%u] %s",station,count,name?name:"");
    portENTER_CRITICAL(&mux);
    memcpy(state.notice,preview,sizeof(state.notice));
    portEXIT_CRITICAL(&mux);
    redraw=true;
}
static void commitStation(uint16_t station) {
    if(!config.playlistLength())return;
    station=wrapStation(station);
    if(!player.sendCommand({PR_PLAY,station})){redraw=true;return;}
    cancelAlarmForUser();
    stationPreview=false;
    portENTER_CRITICAL(&mux);state.notice[0]=0;portEXIT_CRITICAL(&mux);
    page=PAGE_HOME;redraw=true;artwork::redraw(snapshot().generation);
}
// The UI task owns the alarm and only queues audio commands. Never start a
// decoder, reconnect Bluetooth, or write the playlist in an NTP callback.
static void alarmTick(uint32_t now) {
    static uint32_t checkedAt=0;
    if(uint32_t(now-checkedAt)<500)return;
    checkedAt=now;
    uint8_t manual=0;
    portENTER_CRITICAL(&mux);manual=alarmManualOverride;alarmManualOverride=0;portEXIT_CRITICAL(&mux);
    if(manual)cancelAlarmForUser(manual==2);
    const auto setting=snapshot().settings.alarm;
    auto* alarmSdScan=sdman.alarmScan();
    const time_t stamp=time(nullptr);tm local{};
    const bool clockValid=stamp>0&&localtime_r(&stamp,&local)&&local.tm_year+1900>=2024;
    if(alarmPhase==AlarmPhase::Idle){
        // Most checks below touch storage or another subsystem. Do them only
        // in an enabled alarm minute, never twice per second all day.
        if(!clockValid||!alarmclock::due(setting,local.tm_year+1900,local.tm_yday,
            local.tm_wday,local.tm_hour,local.tm_min,alarmLastDay))return;
        if(uiStressActive||player.serviceActive()||
           (setting.source==0&&config.getMode()==PM_SDCARD&&player.isRunning())||
           btaudio::wanted()||btaudio::inputSelected()||btaudio::outputSelected()||
           (setting.source==1&&!FEATURE_SD))return;
        tm end=local;end.tm_hour=setting.offHour;end.tm_min=setting.offMinute;end.tm_sec=0;end.tm_isdst=-1;
        if(setting.offHour*60u+setting.offMinute<=setting.onHour*60u+setting.onMinute)++end.tm_mday;
        alarmEndEpoch=mktime(&end);
        alarmStartEpoch=stamp;
        if(alarmEndEpoch<=stamp)return;
        alarmLastDay=alarmclock::dayKey(local.tm_year+1900,local.tm_yday);
        alarmRequestEpoch.store(alarmLastDay,std::memory_order_release);
        releaseAlarmSd();
        alarmSdScan=nullptr;
        alarmPhase=AlarmPhase::Stop;alarmStarted=now;alarmRetryAt=0;
        alarmMarkerCommitted=false;
        alarmToneRequested=false;
        alarmSdSourceRequested=false;
        alarmSource=setting.source;
        alarmPreviousVolume=snapshot().volume;
        portENTER_CRITICAL(&mux);state.alarmActive=true;portEXIT_CRITICAL(&mux);
        alarmLastVolume=alarmLastLight=255;
        Serial.printf("[ALARM] start=%02u:%02u stop=%02u:%02u station=%u mode=%u source=%s sd_mode=%u route=DAC\n",
            unsigned(setting.onHour),unsigned(setting.onMinute),unsigned(setting.offHour),
            unsigned(setting.offMinute),unsigned(setting.station),unsigned(setting.mode),
            setting.source?"SD":"WEB",unsigned(setting.sdPlayMode));
    }
    const uint32_t durationMs=uint32_t(alarmEndEpoch-alarmStartEpoch)*1000u;
    if(setting.mode==0||setting.source!=alarmSource||!clockValid||stamp>=alarmEndEpoch||
       uint32_t(now-alarmStarted)>=durationMs)alarmPhase=AlarmPhase::Ending;
    if(alarmPhase!=AlarmPhase::Active&&alarmPhase!=AlarmPhase::Starting&&alarmPhase!=AlarmPhase::Fallback&&alarmPhase!=AlarmPhase::Ending&&
       uint32_t(now-alarmStarted)>120000){
        failAlarm("TIMEOUT","Ebreszto: forras vagy audio nem kesz");
    }
    if(alarmPhase==AlarmPhase::Ending)alarmRequestEpoch.store(0,std::memory_order_release);
    if(alarmPhase==AlarmPhase::Fallback){
        const auto tone=player.serviceStatus();
        if(tone.denied&&tone.stage==serviceui::Stage::Idle)alarmToneRequested=false;
        if(tone.fault!=serviceui::Failure::None||uint32_t(now-alarmPlayQueuedAt)>45000&&tone.stage!=serviceui::Stage::Running){
            failAlarm("FALLBACK_OUTPUT_FAILED","Ebreszto: DAC tartalekhang hiba");return;
        }
        const uint32_t total=uint32_t(alarmEndEpoch-alarmStartEpoch);
        const uint32_t elapsed=alarmclock::elapsedSeconds(now,alarmStarted,total);
        player.alarmToneLevel(alarmclock::level(setting.volumeStart,setting.volumePeak,setting.volumeEnd,elapsed,total,setting.volumeRise,setting.volumeFall,setting.mode==2));
        if(!alarmToneRequested){
            if(player.isRunning()||player.status()!=STOPPED){player.sendCommand({PR_ALARM_STOP,0,alarmLastDay.load()});return;}
            serviceui::State toneSettings;toneSettings.alarmPattern=true;toneSettings.hz=1000;toneSettings.rate=48000;toneSettings.db=-10;toneSettings.route=serviceui::Route::Dac;
            alarmToneRequested=player.serviceStart(toneSettings);
        }
        const uint8_t light=alarmclock::level(setting.lightStart,setting.lightPeak,setting.lightEnd,elapsed,total,setting.lightRise,setting.lightFall,setting.mode==2);
        if(alarmLastLight!=AlarmManualLight&&light!=alarmLastLight){board::brightness(light);alarmLastLight=light;}
        return;
    }
    if((alarmPhase==AlarmPhase::Starting||alarmPhase==AlarmPhase::Active)&&
       (config.getMode()!=(setting.source?PM_SDCARD:PM_WEB)||btaudio::wanted())){
        cancelAlarmForUser();return;
    }
    if(alarmPhase==AlarmPhase::Starting||alarmPhase==AlarmPhase::Active){
        if(setting.source&&!alarmSdCatalogValid(alarmSdScan)){
            failAlarm("SD_CATALOG_INVALID","Ebreszto: SD lista ervenytelen");return;
        }
        const auto endEvent=setting.source?alarmSdEndPending.exchange(alarmsd::EndEvent::None,std::memory_order_acq_rel):alarmsd::EndEvent::None;
        if(endEvent==alarmsd::EndEvent::Failed){
            failAlarm("SD_FILE_FAILED","Ebreszto: SD fajl nem jatszhato");return;
        }
        if(endEvent==alarmsd::EndEvent::Complete){
            if(alarmSdScan->pendingNext==UINT16_MAX){
                alarmPlayQueuedAt=now; // EOF queue-full deadline starts here, not at the old file's start
                alarmSdScan->pendingNext=uint16_t(alarmsd::next(alarmSdScan->cursor,alarmSdScan->count,
                    setting.sdPlayMode,uint32_t(esp_random())));
            }
            const size_t next=alarmSdScan->pendingNext;
            if(next==alarmSdScan->count){
                alarmPhase=AlarmPhase::Ending;
                alarmRequestEpoch.store(0,std::memory_order_release);
                Serial.println("[ALARM-SD] result=ONCE_COMPLETE");
            }else if(alarmCommand(PR_PLAY,-int(alarmSdScan->tracks[next]))){
                alarmSdScan->cursor=uint16_t(next);alarmPlayQueuedAt=now;
                alarmPhase=AlarmPhase::Starting;
                alarmSdScan->pendingNext=UINT16_MAX;
                Serial.printf("[ALARM-SD] next=%u/%u track=%u\n",unsigned(next+1),
                    unsigned(alarmSdScan->count),unsigned(alarmSdScan->tracks[next]));
            }else alarmSdEndPending.store(alarmsd::EndEvent::Complete,std::memory_order_release);
        }
        if(alarmPhase==AlarmPhase::Ending)return;
        const bool running=player.isRunning();
        if(alarmclock::playbackTimedOut(running,now,alarmPlayQueuedAt)){
            failAlarm("AUDIO_PLAYBACK_TIMEOUT","Ebreszto: lejatszas nem indult");return;
        }
        // An accepted queue request is not proof of playback. Do not run the
        // light/volume envelope while the connection/file is still opening.
        if(!running)return;
        // Renew the recovery budget while playback runs. Otherwise a dropout
        // after 30 seconds is mistaken for an already-expired initial start.
        alarmPlayQueuedAt=now;
        if(alarmPhase==AlarmPhase::Starting){
            alarmPhase=AlarmPhase::Active;
            portENTER_CRITICAL(&mux);strlcpy(state.notice,setting.source?"Ebreszto: SD szol":"Ebreszto: radio szol",sizeof(state.notice));portEXIT_CRITICAL(&mux);
            Serial.println("[ALARM] result=PLAYING");redraw=true;
        }
        const uint32_t total=uint32_t(alarmEndEpoch-alarmStartEpoch);
        const uint32_t elapsed=alarmclock::elapsedSeconds(now,alarmStarted,total);
        const bool ramp=setting.mode==2;
        const uint8_t volume=alarmclock::level(setting.volumeStart,setting.volumePeak,
            setting.volumeEnd,elapsed,total,setting.volumeRise,setting.volumeFall,ramp);
        const uint8_t light=alarmclock::level(setting.lightStart,setting.lightPeak,
            setting.lightEnd,elapsed,total,setting.lightRise,setting.lightFall,ramp);
        if(volume!=alarmLastVolume&&alarmCommand(PR_VOL,alarmclock::playerVolume(volume)))alarmLastVolume=volume;
        if(alarmLastLight!=AlarmManualLight&&light!=alarmLastLight){board::brightness(light);alarmLastLight=light;}
        return;
    }
    if(uint32_t(now-alarmRetryAt)<1000)return;
    alarmRetryAt=now;
    if(alarmPhase==AlarmPhase::Ending){
        alarmRequestEpoch.store(0,std::memory_order_release);
        if(alarmToneRequested){player.serviceStop();alarmToneRequested=false;return;}
        if(!player.isRunning()&&player.status()==STOPPED&&!player.serviceActive()){
            player.setVol(alarmPreviousVolume);
            alarmPhase=AlarmPhase::Idle;
            releaseAlarmSd();
            portENTER_CRITICAL(&mux);
            if(state.alarmActive)strlcpy(state.notice,"Ebreszto: befejezve",sizeof(state.notice));
            state.alarmActive=false;portEXIT_CRITICAL(&mux);
            restoreAlarmLight();
            Serial.println("[ALARM] result=ENDED");redraw=true;
        }else player.sendCommand({PR_ALARM_STOP,0,alarmLastDay.load()});
        return;
    }
    if(alarmPhase==AlarmPhase::Stop){
        if(!setting.source&&network.status!=CONNECTED){failAlarm("NO_NETWORK","Ebreszto: nincs Wi-Fi");return;}
        if(!player.isRunning()&&player.status()==STOPPED&&!btaudio::wanted()&&btaudio::idle()&&
           (setting.source||network.status==CONNECTED)){
            alarmPhase=AlarmPhase::Source;
        }
        else alarmCommand(PR_STOP,0);
    }
    if(alarmPhase==AlarmPhase::Source){
        const auto target=setting.source?PM_SDCARD:PM_WEB;
        if(setting.source&&alarmSdSourceRequested&&
           (player.sdPrepareState()==SdPrepareState::Failed||
           player.sdPrepareState()==SdPrepareState::Cancelled)){
            failAlarm("INDEX_FAIL","Ebreszto: SD indexeles sikertelen");return;
        }
        if(config.getMode()!=target){
            if(!alarmSdSourceRequested&&alarmCommand(PR_SOURCE,int(target)))
                alarmSdSourceRequested=true;
            return;
        }
        if(setting.source){
            if(!sdman.catalogReady.load())return;
            if(!alarmSdScan&&!sdman.beginAlarmFolderScan()){
                failAlarm("FOLDER_OPEN_FAIL","Ebreszto: SD lista nem olvashato");
                return;
            }
            alarmSdScan=sdman.alarmScan();
            if(alarmSdScan->status==0)return;
            if(alarmSdScan->status!=1){
                failAlarm("FOLDER_SCAN_FAIL","Ebreszto: ures vagy hibas SD mappa");return;
            }
            alarmSdScan->cursor=setting.sdPlayMode==1?uint16_t(esp_random()%alarmSdScan->count):0;
        }else{
            if(network.status!=CONNECTED)return;
            if(setting.station>config.playlistLength()){
                failAlarm("INVALID_STATION","Ebreszto: ervenytelen radioado");return;
            }
        }
        // Persist only after the source and selected folder/station are valid.
        if(!alarmMarkerCommitted){
            Preferences p;bool saved=p.begin("yo-dual-v1",false);
            if(saved){saved=p.putUInt("alarmday",alarmLastDay)==4;p.end();}
            if(!saved){failAlarm("MARKER_SAVE_FAIL","Ebreszto: napjelolo nem mentheto");return;}
            alarmMarkerCommitted=true;
        }
        alarmPhase=AlarmPhase::Volume;
    }
    if(alarmPhase==AlarmPhase::Volume){
        const uint8_t initial=setting.mode==2?setting.volumeStart:setting.volumePeak;
        if(alarmCommand(PR_VOL,alarmclock::playerVolume(initial))){
            alarmLastVolume=initial;alarmPhase=AlarmPhase::Play;
            const uint8_t light=setting.mode==2?setting.lightStart:setting.lightPeak;
            if(alarmLastLight!=AlarmManualLight){board::brightness(light);alarmLastLight=light;}
        }
    }
    if(alarmPhase==AlarmPhase::Play&&
       (setting.source||network.status==CONNECTED)){
        if(setting.source&&!alarmSdCatalogValid(alarmSdScan)){
            failAlarm("SD_CATALOG_INVALID","Ebreszto: SD lista ervenytelen");return;
        }
        const uint16_t track=setting.source?alarmSdScan->tracks[alarmSdScan->cursor]:setting.station;
        // Claim EOF before enqueue: a very short file can end on the audio
        // core before xQueueSend returns to this UI task.
        alarmSdOwned.store(setting.source!=0,std::memory_order_release);
        if((setting.source||track<=config.playlistLength())&&
           alarmCommand(PR_PLAY,setting.source?-int(track):int(track))){
            alarmPhase=AlarmPhase::Starting;
            alarmPlayQueuedAt=now;
            portENTER_CRITICAL(&mux);strlcpy(state.notice,setting.source?"Ebreszto: SD indul...":"Ebreszto: radio indul...",sizeof(state.notice));portEXIT_CRITICAL(&mux);
            Serial.printf("[ALARM] result=QUEUED source=%s track=%u\n",setting.source?"SD":"WEB",unsigned(track));redraw=true;
        }
    }
}
static void openChannels() {
    stationListParent=page==PAGE_STATIONS?PAGE_STATIONS:PAGE_HOME;
    channelCursor=wrapStation(config.lastStation()?config.lastStation():1);
    selected=channelCursor;page=PAGE_STATIONS_LIST;stationPreview=false;redraw=true;
}
static void openMainMenu() {
    page=PAGE_MAIN;selected=0;stationPreview=false;redraw=true;artwork::redraw(snapshot().generation);
}
static void openFavorites(){
    uint16_t last=config.lastStation();
    selected=favorite(last)?last:nextFavorite(last,true);
    page=PAGE_FAVORITES_LIST;redraw=true;
}
static void goBack(unsigned from);
static bool saveClockServer(unsigned index,const char* value){
    if(index>=4||!clocksync::validServer(value))return false;
    auto s=clockServers();strlcpy(s.values[index],value,64);
    if(!clocksync::anyServer(s.values))return false;
    portENTER_CRITICAL(&mux);clockStore->servers=s;++clockStore->version;state.pendingSave=true;portEXIT_CRITICAL(&mux);
    redraw=true;return true;
}
static void openNtpServer(unsigned index){
    clockStore->editIndex=index;clockStore->entry=WifiPasswordUi{};
    snprintf(clockStore->entry.ssid,sizeof(clockStore->entry.ssid),"NTP %u",index+1);
    const auto s=clockServers();
    strlcpy(clockStore->entry.password,s.values[index],sizeof(clockStore->entry.password));
    page=69;selected=0;redraw=true;
}
static void ntpServerKeyPress(unsigned key){
    if(key<36){
        const char c=wifiPasswordKeyChar(clockStore->entry.mode,key);const size_t n=strlen(clockStore->entry.password);
        if(c&&n<63){clockStore->entry.password[n]=c;clockStore->entry.password[n+1]=0;clockStore->entry.error[0]=0;}
        else if(c)strlcpy(clockStore->entry.error,"Max. 63 karakter",sizeof(clockStore->entry.error));
    }else if(key==36)clockStore->entry.mode=(clockStore->entry.mode+1)%3;
    else if(key==37){const size_t n=strlen(clockStore->entry.password);if(n)clockStore->entry.password[n-1]=0;clockStore->entry.error[0]=0;}
    else if(key==38){
        if(saveClockServer(clockStore->editIndex,clockStore->entry.password)){goBack(69);return;}
        strlcpy(clockStore->entry.error,"Hoszt/IP kell; legalabb egy szerver",sizeof(clockStore->entry.error));
    }else if(key==39){goBack(69);return;}
    redraw=true;
}
static void clearWifiPassword(){
    volatile char* p=clockStore->wifi.password;
    for(size_t i=0;i<sizeof(clockStore->wifi.password);i++)p[i]=0;
    clockStore->wifi.ssid[0]=clockStore->wifi.error[0]=0;
    clockStore->wifi.mode=0;clockStore->wifi.submitted=false;clockStore->wifi.visible=false;
}
static void openWifiPassword(const char* ssid){
    clearWifiPassword();
    strlcpy(clockStore->wifi.ssid,ssid,sizeof(clockStore->wifi.ssid));
    page=PAGE_WIFI_PASSWORD;selected=0;redraw=true;
}
static void openSelectedWifiPassword(unsigned index){
    char ssid[WIFI_SSID_CAP]={};
    if(network.scanEntry(index,ssid,sizeof(ssid)))openWifiPassword(ssid);
}
static void chooseScannedWifi(unsigned index){
    char ssid[WIFI_SSID_CAP]={};
    if(!network.scanEntry(index,ssid,sizeof(ssid)))return;
    const int result=network.selectSetupNetwork(index);
    if(result==1){page=PAGE_HOME;selected=0;network.cancelScan();redraw=true;}
    else if(result==2)openWifiPassword(ssid);
}
static void wifiPasswordKeyPress(unsigned key){
    if(key==wifipasswordui::visibilityKey){clockStore->wifi.visible=!clockStore->wifi.visible;redraw=true;return;}
    if(clockStore->wifi.submitted&&!network.setupFailed())return;
    if(clockStore->wifi.submitted){clockStore->wifi.submitted=false;strlcpy(clockStore->wifi.error,"Kapcsolat sikertelen",sizeof(clockStore->wifi.error));}
    if(key<36){
        const char c=wifiPasswordKeyChar(clockStore->wifi.mode,key);
        const size_t n=strlen(clockStore->wifi.password);
        if(c&&n<sizeof(clockStore->wifi.password)-1){clockStore->wifi.password[n]=c;clockStore->wifi.password[n+1]=0;clockStore->wifi.error[0]=0;}
        else if(c)strlcpy(clockStore->wifi.error,"Legfeljebb 63 karakter",sizeof(clockStore->wifi.error));
    }else if(key==36){clockStore->wifi.mode=(clockStore->wifi.mode+1)%3;}
    else if(key==37){size_t n=strlen(clockStore->wifi.password);if(n)clockStore->wifi.password[n-1]=0;clockStore->wifi.error[0]=0;}
    else if(key==38){
        const size_t n=strlen(clockStore->wifi.password);
        if(n&&n<8)strlcpy(clockStore->wifi.error,"Jelszo: 8-63 karakter",sizeof(clockStore->wifi.error));
        else if(network.connectScannedNetwork(clockStore->wifi.ssid,clockStore->wifi.password)){
            clockStore->wifi.submitted=true;clockStore->wifi.error[0]=0;
        }else strlcpy(clockStore->wifi.error,"Kapcsolodas nem indult",sizeof(clockStore->wifi.error));
    }else if(key==39){goBack(PAGE_WIFI_PASSWORD);return;}
    redraw=true;
}
static void searchKey(unsigned key){
    if(key<26){
        size_t n=strlen(searchQuery);
        if(n<sizeof(searchQuery)-1){searchQuery[n]=char('A'+key);searchQuery[n+1]=0;}
        searchState=0;
    }else if(key==26){
        size_t n=strlen(searchQuery);
        if(n&&n<sizeof(searchQuery)-1){searchQuery[n]=' ';searchQuery[n+1]=0;}
        searchState=0;
    }else if(key==27){
        size_t n=strlen(searchQuery);if(n)searchQuery[n-1]=0;searchState=0;
    }else if(key==28)executeStationSearch();
    else if(key==29)goBack(PAGE_STATION_SEARCH);
    redraw=true;
}
static uint16_t stationAtTouch(Point raw,unsigned listPage){
    Snapshot s=snapshot();Size native=front::nativeSize();Point q=toLogical(raw,native,s.settings.rotation);
#if IBT_NO_PSRAM
    {
    unsigned first=0;if(!front::menuViewport(listPage,first))return 0;
    const int row=litelist::row(q.x,q.y);
    if(row<0)return 0;
    if(listPage==PAGE_STATIONS_LIST)return litelist::station(first,config.playlistLength(),row);
    if(row>=int(favoritesCount())||!first)return 0;
    uint16_t n=uint16_t(first);
    for(int i=0;i<row;++i)n=nextFavorite(n,true);
    return n;
    }
#else
    if(listPage==PAGE_STATIONS_LIST&&s.settings.playerEnabled&&s.settings.vintageSkin){
        if(q.x<14||q.x>=302||q.y<33||q.y>=33+8*21)return 0;
        const uint16_t total=config.playlistLength();if(!total)return 0;
        int n=int(front::stationListFirst())+(q.y-33)/21;
        while(n>total)n-=total;return uint16_t(n);
    }
    if(listPage==PAGE_STATIONS_LIST){if(q.y<56||q.y>=217)return 0;unsigned total=config.playlistLength();if(!total)return 0;unsigned n=front::stationListFirst()+unsigned(q.y-56)/23;while(n>total)n-=total;return n;}
    // Match every row edge to the same offline crop transform as the renderer.
    if(listPage==PAGE_STATIONS_LIST&&s.settings.playerEnabled&&q.x>=10&&q.x<300){
        const auto edge=[](int row){return ((24+row*22-2)*240+116)/233;};
        if(q.y>=edge(0)&&q.y<edge(8)){
            uint16_t total=config.playlistLength();if(!total)return 0;
            int row=0;while(row<7&&q.y>=edge(row+1))++row;
            int n=int(front::stationListFirst())+row;while(n<1)n+=total;while(n>total)n-=total;return uint16_t(n);
        }
        return 0;
    }
    if(q.y<56||q.y>=56+7*23)return 0;
    int row=(q.y-56)/23;
    if(listPage==PAGE_STATIONS_LIST){
        uint16_t total=config.playlistLength();if(!total)return 0;
        int n=int(selected)-3+row;while(n<1)n+=total;while(n>total)n-=total;return uint16_t(n);
    }
    if(row>=int(favoritesCount())||!selected)return 0;
    uint16_t n=uint16_t(selected);
    while(row-->0)n=nextFavorite(n,true);
    return n;
#endif
}
static unsigned bleMenuParent=64;
static bool soundFromSettings=false;
static bool monoEdit=false;
static void goBack(unsigned from){
#if IBT_COMPACT_UI
    if(from==74){irreceiver::cancel();page=PAGE_CONTROLS;selected=5;redraw=true;return;}
#endif
    backupArmedAt=0;
    restartArmedAt=0;
    if(from==PAGE_SOUND&&!soundFromSettings){page=PAGE_MAIN;selected=1;redraw=true;return;}
    const auto parent=menustructure::parent(from);
    if(parent.page!=255){page=parent.page;selected=parent.row;redraw=true;return;}
    switch(from){
      case PAGE_STATIONS:page=PAGE_MAIN;selected=0;break;
      case 73:monoEdit=false;page=PAGE_SOUND;selected=4;break;
      case PAGE_SOUND:page=PAGE_MAIN;selected=1;break;
      case PAGE_BLUETOOTH:page=bluetoothParent;selected=bluetoothParentRow;break;
      case PAGE_SETTINGS:page=PAGE_MAIN;selected=3;break;
      case 60:case 61:case 62:case 63:case 64:page=PAGE_ALARM;selected=from==63?3:from==62?4:from-59;break;
      case 67:page=60;selected=alarmTimeDraft.ending?1:0;break;
      case 68:page=64;selected=3;break;
      case 70:page=bleMenuParent;selected=bleMenuParent==49?1:6;break;
      case 71:page=70;selected=2;break;
      case 69:page=68;selected=clockStore->editIndex;clockStore->entry=WifiPasswordUi{};break;
      case PAGE_DNS:page=PAGE_WIFI;selected=4;break;
      case PAGE_DNS_ADDRESS:page=PAGE_DNS;selected=dnsTarget+1;break;
      case PAGE_DNS_VALUE:page=PAGE_DNS_ADDRESS;selected=dnsOctet;break;
      case PAGE_SURFACE:page=PAGE_DISPLAY;selected=4;break;
      case PAGE_RETRO_COLOR:page=PAGE_SURFACE;selected=9;break;
      case PAGE_BAT_VALUE:page=PAGE_BAT_CAL;selected=calibrationPoint;break;
      case PAGE_BAT_DISPLAY:page=PAGE_DISPLAY;selected=9;break;
      case PAGE_VOLUME_STREAM:page=PAGE_SOUND;selected=0;break;
      case PAGE_EQ:page=PAGE_SOUND;selected=1;break;
      case PAGE_EQ10:page=PAGE_EQ;selected=3;break;
      case PAGE_EQ_PRESETS:page=eqPresetParent;selected=page==PAGE_EQ10?10:0;break;
      case PAGE_OUTPUTS:page=PAGE_SOUND;selected=2;break;
      case PAGE_FIRMWARE_INFO:page=PAGE_SYSTEM;selected=1;break;
      case PAGE_MEMORY:page=memoryParent;selected=memoryParent==PAGE_STORAGE?0:1;break;
      case PAGE_ROTARY_DEBUG:page=PAGE_DIAGNOSTICS;selected=3;break;
      case PAGE_NETWORK_INFO:page=networkParent;selected=networkParent==PAGE_WIFI?0:2;break;
      case PAGE_SAVED_WIFI:page=PAGE_WIFI;selected=2;break;
      case PAGE_WIFI_SCAN:page=network.status==SOFT_AP?PAGE_HOME:PAGE_WIFI;selected=network.status==SOFT_AP?0:3;network.cancelScan();break;
      case PAGE_WIFI_PASSWORD:clearWifiPassword();page=PAGE_WIFI_SCAN;selected=0;break;
      case PAGE_FAVORITES_LIST:page=PAGE_STATIONS;selected=1;break;
      case PAGE_STATIONS_LIST:page=stationListParent;selected=stationListParent==PAGE_STATIONS&&config.getMode()==PM_SDCARD?3:0;break;
      case PAGE_SOURCE_PENDING:
        if(pendingSourceTarget==PM_SDCARD &&
           player.sdPrepareState()!=SdPrepareState::Failed &&
           player.sdPrepareState()!=SdPrepareState::Cancelled &&
           !player.sendCommand({PR_CANCEL_SOURCE,0})){btRateNotice("Megszakítás vár, próbáld újra");redraw=true;return;}
        pendingConnectivity=-1;speakerSearchPending=false;pendingSourceAt=0;
        page=PAGE_STATIONS;selected=pendingSourceTarget==PM_SDCARD?3:0;break;
      case PAGE_SD_INFO:page=PAGE_STORAGE;selected=1;break;
      case PAGE_STATION_SEARCH:page=PAGE_STATIONS;selected=2;break;
      case PAGE_SEARCH_RESULTS:page=PAGE_STATION_SEARCH;selected=28;break;
      case PAGE_PLAYLIST_EDITOR:page=PAGE_STATIONS;selected=4;break;
      case PAGE_MEMORY_STRESS:page=PAGE_DIAGNOSTICS;selected=4;break;
      case PAGE_BACKUP_MENU:page=PAGE_SYSTEM;selected=4;break;
      case PAGE_BACKUP_RESULT:page=PAGE_BACKUP_MENU;selected=0;break;
      case PAGE_BT_INFO:page=PAGE_BLUETOOTH;selected=3;break;
      case PAGE_BT_RATE:page=PAGE_BLUETOOTH;selected=4;break;
      case PAGE_BT_PLAYER:page=PAGE_BLUETOOTH;selected=0;break;
      case PAGE_BT_SCAN:
        if(!player.sendCommand({PR_BT,-1})){btRateNotice("BT foglalt, próbáld újra");redraw=true;return;}
        page=PAGE_BLUETOOTH;selected=1;break;
      case 75:page=PAGE_HOME;selected=0;localRoutePending=-1;speakerSearchPending=false;break;
      case PAGE_BT_SAVED:page=PAGE_BLUETOOTH;selected=6;break;
      case PAGE_DAC_INFO:page=PAGE_OUTPUTS;selected=0;break;
      case PAGE_FS_INFO:page=PAGE_STORAGE;selected=3;break;
      case PAGE_ART_CACHE:page=PAGE_STORAGE;selected=2;break;
      case PAGE_WEB_UPDATE:page=PAGE_SYSTEM;selected=2;break;
      case PAGE_BUTTON_MAP:page=PAGE_CONTROLS;selected=2;break;
      default:page=PAGE_HOME;selected=0;break;
    }
    redraw=true;
}
static void selectConnectivity(unsigned mode){
    if(!connectivity::valid(mode))return;
    Settings s=snapshot().settings;
    if(mode==0&&s.networkMode==0&&!network.offlinePlayer())return;
    s.networkMode=uint8_t(mode);
    if(mode){s.alarm.clockHome=false;pendingConnectivity=int(mode);connectivityStep=0;connectivityAt=millis();}
    else {pendingConnectivity=-1;if(btaudio::wanted()||!btaudio::idle()){if(!player.sendCommand({PR_BT,-1})){btRateNotice("BT foglalt, probald ujra");return;}}}
    cancelAlarmForUser();persistSetting(s);
    if(mode)network.enterOfflinePlayer(mode);else network.enterOnlinePlayer();
    page=PAGE_HOME;selected=0;redraw=true;
#if defined(BOARD_CYD28)
    if(mode==0&&bootprogress::hold(s.bootScreen,playerSkinId(s)))front::bootRestart(s.bootScreen,s.vintageSkin,playerSkinId(s));
#endif
}
static void serviceConnectivity(){
    if(localRoutePending>=0){
        if(uint32_t(millis()-localRouteAt)>30000){localRoutePending=-1;btRateNotice("Hangútváltás nem fejeződött be");return;}
        if(!btaudio::idle()||player.queueDepth()!=0)return;
        const auto target=playMode_e(localRoutePending);
        if(target==PM_WEB&&network.offlinePlayer()){network.enterOnlinePlayer();return;}
        if(config.getMode()!=target){
            if(player.sendCommand({PR_SOURCE,target})){pendingSourceTarget=target;pendingSourceAt=millis();pendingSourceCancelSent=false;localRoutePending=-1;page=PAGE_SOURCE_PENDING;redraw=true;}
        }else{localRoutePending=-1;page=PAGE_HOME;selected=0;redraw=true;}
        return;
    }
    if(speakerSearchPending){
        if(uint32_t(millis()-speakerSearchAt)>30000){speakerSearchPending=false;page=PAGE_BLUETOOTH;selected=1;btRateNotice("SD/BT keresés nem indult el");return;}
        if(!btaudio::idle()||player.queueDepth()!=0)return;
        if(config.getMode()!=PM_SDCARD||!sdman.catalogReady.load()){
            if(!speakerSearchStep&&player.sendCommand({PR_SOURCE,PM_SDCARD}))speakerSearchStep=1;
            if(speakerSearchStep&&(player.sdPrepareState()==SdPrepareState::Failed||player.sdPrepareState()==SdPrepareState::Cancelled)){
                speakerSearchPending=false;page=PAGE_BLUETOOTH;selected=1;btRateNotice("SD-kártya nem érhető el");
            }
            return;
        }
        if(player.sendCommand({PR_BT,0})){speakerSearchPending=false;cancelAlarmForUser();page=PAGE_BT_SCAN;selected=0;redraw=true;}
        return;
    }
    if(pendingConnectivity<1||!player.audioTaskHandle()||!network.offlineReady())return;
    const unsigned target=unsigned(pendingConnectivity);
    if(!connectivityAt)connectivityAt=millis();
    if(uint32_t(millis()-connectivityAt)>30000){pendingConnectivity=-1;btRateNotice("Offline forras nem erheto el");return;}
    if(target==2){if(!btaudio::inputSelected()){if(player.queueDepth()!=0||!player.sendCommand({PR_BT,1}))return;cancelAlarmForUser();}page=snapshot().settings.vintageSkin>=5?PAGE_HOME:PAGE_BT_PLAYER;selected=0;redraw=true;pendingConnectivity=-1;return;}
    if(btaudio::inputSelected()){if(connectivityStep==0&&player.sendCommand({PR_BT,-1})){cancelAlarmForUser();connectivityStep=1;}return;}
    if(connectivityStep==1&&!btaudio::idle())return;
    if(connectivityStep>=2&&(player.sdPrepareState()==SdPrepareState::Failed||player.sdPrepareState()==SdPrepareState::Cancelled)){pendingConnectivity=-1;page=PAGE_HOME;btRateNotice("SD forras nem erheto el");return;}
    if(config.getMode()!=PM_SDCARD||!sdman.catalogReady.load()){
        if(connectivityStep<2&&player.sendCommand({PR_SOURCE,PM_SDCARD})){connectivityStep=2;page=PAGE_SOURCE_PENDING;pendingSourceTarget=PM_SDCARD;pendingSourceAt=millis();pendingSourceCancelSent=false;redraw=true;}
        return;
    }
    if(config.playlistLength()==0){pendingConnectivity=-1;openChannels();return;}
    if(!player.isRunning()){
        if(player.queueDepth()!=0||player.status()!=STOPPED)return;
        const unsigned track=config.lastStation()>=1&&config.lastStation()<=config.playlistLength()?config.lastStation():1;
        if(!player.sendCommand({PR_PLAY,int(track)}))return;
    }
    pendingConnectivity=-1;page=PAGE_HOME;selected=0;front::invalidate();redraw=true;
}
static void activateMenu(unsigned p,unsigned item){
#if IBT_COMPACT_UI
    if(p==74){if(item==16)goBack(p);else irreceiver::menu(item);redraw=true;return;}
#endif

    if(p!=PAGE_SYSTEM||item!=3)restartArmedAt=0;
    if(p!=PAGE_BACKUP_MENU || (item!=1 && item!=3))backupArmedAt=0;
    if(!menuEnabled(p,item)){redraw=true;return;}
    if(p==PAGE_MAIN){
        if(item==4){Settings s=snapshot().settings;s.alarm.clockHome=!s.alarm.clockHome;persistSetting(s);page=PAGE_HOME;front::invalidate();}
        else {if(item==2){openBluetoothMenu(PAGE_MAIN,2,0);return;}if(item==1)soundFromSettings=false;page=item==0?PAGE_STATIONS:item==1?PAGE_SOUND:PAGE_SETTINGS;}
        selected=0;redraw=true;return;
    }
    if(p==PAGE_STATIONS){
        if(item==5){bluetoothMode(1);return;}
        if(item==6){openBluetoothMenu(PAGE_STATIONS,6,2);return;}
        if((item==0||item==3)&&btaudio::inputSelected()){
            btRateNotice("Valassz DAC kimenetet a helyi forrashoz");openBluetoothMenu(PAGE_STATIONS,item,2);return;
        }
        if(item==0){
            if(network.offlinePlayer()){selectConnectivity(0);return;}
            if(config.getMode()==PM_SDCARD){
                if(btaudio::outputSelected()){
                    btRateNotice("Internet radiohoz valassz DAC-ot");return;
                }
                if(player.sendCommand({PR_SOURCE,PM_WEB})){
                    cancelAlarmForUser();
                    pendingSourceTarget=PM_WEB;pendingSourceAt=millis();pendingSourceCancelSent=false;
                    page=PAGE_SOURCE_PENDING;selected=0;redraw=true;
                }
            }else openChannels();
        }
        else if(item==1)openFavorites();
        else if(item==2){searchQuery[0]=0;searchFound=0;searchState=0;page=PAGE_STATION_SEARCH;selected=0;redraw=true;}
        else if(item==3&&FEATURE_SD){
            if(config.getMode()==PM_SDCARD&&sdman.catalogReady.load())openChannels();
            else if(player.sendCommand({PR_SOURCE,PM_SDCARD})){
                cancelAlarmForUser();
                pendingSourceTarget=PM_SDCARD;pendingSourceAt=millis();pendingSourceCancelSent=false;
                page=PAGE_SOURCE_PENDING;selected=0;redraw=true;
            }
        }
        else if(item==4){page=PAGE_PLAYLIST_EDITOR;selected=0;redraw=true;}
        else goBack(p);return;
    }
    if(p==73){if(item==12){goBack(p);return;}static const int8_t map[]={0,1,2,-1,3,4,5,6,7,8,9,10};if(item<12&&map[item]>=0)monodsp::change(unsigned(map[item]),1);redraw=true;return;}
    if(p==PAGE_SOUND){if(item==4){page=73;selected=0;redraw=true;return;}if(item==0)page=PAGE_VOLUME_STREAM;else if(item==1)page=PAGE_EQ;else if(item==2)page=PAGE_OUTPUTS;else if(item==3){openBluetoothMenu(PAGE_SOUND,3,0);return;}else {goBack(p);return;}selected=0;redraw=true;return;}
    if(p==75){
        if(item>=4){localRoutePending=-1;speakerSearchPending=false;page=PAGE_HOME;selected=0;redraw=true;return;}
        pendingConnectivity=-1;speakerSearchPending=false;localRoutePending=-1;
        if(item==3){bluetoothMode(1);return;}
        if(item==2){bluetoothMode(0);return;}
        if(!player.sendCommand({PR_BT,-1})){btRateNotice("BT foglalt, próbáld újra");return;}
        localRoutePending=item==0?PM_WEB:PM_SDCARD;localRouteAt=millis();cancelAlarmForUser();redraw=true;return;
    }
    if(p==PAGE_BLUETOOTH){
        if(item==0){bluetoothMode(1);return;}
        if(item==1){bluetoothMode(0);return;}
        if(item==6){page=PAGE_BT_SAVED;selected=btaudio::savedDeviceCount()?0:4;redraw=true;return;}
        if(item==2){bluetoothMode(-1);return;}
        if(item==3){page=PAGE_BT_INFO;selected=0;redraw=true;return;}
        if(item==4){page=PAGE_BT_RATE;selected=0;redraw=true;return;}
        if(item==5){Settings s=snapshot().settings;s.btQuickScan=!s.btQuickScan;persistSetting(s);redraw=true;return;}
        goBack(p);return;}
    if(p==PAGE_BT_RATE){
        if(item<4){static const unsigned rates[]={16000,32000,44100,48000};
            if(player.isRunning()||player.status()!=STOPPED||!btaudio::setOutputRate(rates[item])){
                portENTER_CRITICAL(&mux);
                strlcpy(state.notice,"Allitsd le a lejatszast a BT beallitashoz",sizeof(state.notice));
                portEXIT_CRITICAL(&mux);
            }
            redraw=true;
        }else goBack(p);
        return;
    }
    if(p==PAGE_VU_TIMER){
#if IBT_NO_PSRAM
        if(item<4)lightui::style.store(uint8_t(item));
        else if(item<7)vuprobe::setRefresh(item==4?100:item==5?200:500);
        else {goBack(p);redraw=true;return;}
        persistSetting(snapshot().settings);front::invalidate();redraw=true;return;
#else
        if(item<5){vuprobe::setRefresh(vuprobe::refreshOptions[item]);persistSetting(snapshot().settings);}
        goBack(p);redraw=true;return;
#endif
    }
    if(p==PAGE_BAT_DISPLAY){
        Settings s=snapshot().settings;
        if(item==0)s.batteryIndicator=!s.batteryIndicator;
        else if(item==1)s.batteryStep=s.batteryStep==1?5:s.batteryStep==5?10:s.batteryStep==10?25:1;
        else if(item==2)s.batteryAppearance=(s.batteryAppearance+1)%3;
        else {goBack(p);return;}
        persistSetting(s);if(frontendOK)front::invalidate();redraw=true;return;
    }
    if(p==PAGE_HARDWARE){
        if(item==0){Settings s=snapshot().settings;s.sensorMode=(s.sensorMode+1)%3;
            if(s.sensorMode!=1&&s.brightness==0){s.brightness=100;board::brightness(100);}
            persistSetting(s);}
        else if(item==1){bleMenuParent=PAGE_HARDWARE;page=70;selected=0;}
        else if(item==2){page=PAGE_BAT_CAL;selected=0;}else goBack(p);
        redraw=true;return;
    }
    if(p==PAGE_BAT_CAL){
        if(item<5){calibrationPoint=item;page=PAGE_BAT_VALUE;selected=0;}else goBack(p);
        redraw=true;return;
    }
    if(p==PAGE_IDLE){
        if(item<7){
            static const uint16_t seconds[]={10,20,30,60,120,300,0};
            Settings s=snapshot().settings;s.menuIdleSeconds=seconds[item];persistSetting(s);
            goBack(p);
        }else goBack(p);
        redraw=true;return;
    }
    if(p==PAGE_SETTINGS){
        if(item>=menustructure::settingsCount){goBack(p);return;}
        page=menustructure::settings[item].page;if(page==PAGE_SOUND)soundFromSettings=true;selected=0;redraw=true;return;
    }
    if(p==PAGE_BACKUP_MENU){
        if(item<4){
            if(item==1 || item==3){
                uint32_t now=millis();
                if(!backupArmedAt || backupArmedItem!=item || uint32_t(now-backupArmedAt)>5000){
                    backupArmedAt=now;backupArmedItem=item;redraw=true;return;
                }
                if(uint32_t(now-backupArmedAt)<300)return;
            }
            backupArmedAt=0;
            backupsd::run(backupsd::Operation(item));
            page=PAGE_BACKUP_RESULT;selected=0;redraw=true;
        }else goBack(p);
        return;
    }
    if(p==PAGE_DNS){
        Settings s=snapshot().settings;
        if(item==0){s.dnsManual=!s.dnsManual;persistSetting(s);}
        else if(item==1||item==2){dnsTarget=item-1;memcpy(dnsDraft,dnsTarget?s.dnsSecondary:s.dnsPrimary,4);page=PAGE_DNS_ADDRESS;selected=0;}
        else if(item>=3&&item<=5){const uint8_t cloud[8]={1,1,1,1,1,0,0,1},google[8]={8,8,8,8,8,8,4,4},quad9[8]={9,9,9,9,149,112,112,112};const auto a=item==3?cloud:item==4?google:quad9;memcpy(s.dnsPrimary,a,4);memcpy(s.dnsSecondary,a+4,4);s.dnsManual=true;persistSetting(s);}
        else goBack(p);
        redraw=true;return;
    }
    if(p==PAGE_DNS_ADDRESS){
        if(item<4){dnsOctet=item;page=PAGE_DNS_VALUE;selected=0;}
        else if(item==4){
            if(radiodns::validServer(dnsDraft)){Settings s=snapshot().settings;memcpy(dnsTarget?s.dnsSecondary:s.dnsPrimary,dnsDraft,4);persistSetting(s);goBack(p);}
            else Serial.println("[DNS-CONFIG] invalid address; not saved");
        }else goBack(p);
        redraw=true;return;
    }
    if(p==PAGE_WIFI){
        if(item>=5&&item<=7){selectConnectivity(item-5);return;}
        if(item==0){networkParent=PAGE_WIFI;page=PAGE_NETWORK_INFO;selected=0;redraw=true;return;}
        if(item==1){network.startSetup();page=PAGE_HOME;selected=0;redraw=true;return;}
        if(item==2){page=PAGE_SAVED_WIFI;selected=0;redraw=true;return;}
        if(item==3){page=PAGE_WIFI_SCAN;selected=0;network.requestScan();redraw=true;return;}
        if(item==4){page=PAGE_DNS;selected=0;redraw=true;return;}
        goBack(p);return;
    }
    if(p==72){if(item<8){Settings s=snapshot().settings;if(item<5)s.bootScreen=uint8_t(item);else s.bootSound=uint8_t(item-5);persistSetting(s);}else goBack(p);redraw=true;return;}
    if(p==PAGE_SURFACE){
#if IBT_COMPACT_UI
        {
        if(item>=3){goBack(p);return;}
        Settings s=snapshot().settings;s.alarm.clockHome=item==0;
        s.vintageSkin=item==2?5:0;s.mediaPlayerSkin=true;s.playerEnabled=true;
        persistSetting(s);page=PAGE_HOME;selected=0;front::invalidate();redraw=true;return;
        }
#endif
        if(item==4)return; // Reserved retired skin ID; never renumber stored IDs.
        if(item==0){Settings bt;portENTER_CRITICAL(&mux);bt=state.settings;portEXIT_CRITICAL(&mux);bt.alarm.clockHome=false;persistSetting(bt);bluetoothMode(1);page=PAGE_BT_PLAYER;selected=0;front::invalidate();redraw=true;return;}
        if(item==5){page=PAGE_SERVICE;selected=0;serviceInfo=false;serviceUiSettings.route=btaudio::outputSelected()?serviceui::Route::Bt:serviceui::Route::Dac;redraw=true;return;}
        if(item==9){page=PAGE_RETRO_COLOR;selected=snapshot().settings.retroColor;redraw=true;return;}
        if(item==10){Settings s=snapshot().settings;s.swipeSkins=!s.swipeSkins;persistSetting(s);redraw=true;return;}
        if(item>=14){goBack(p);return;}
        Settings s=snapshot().settings;
        s.alarm.clockHome=false;s.playerEnabled=item>=2;s.mediaPlayerSkin=item==3;s.kajonSkin=false;s.cassetteEnabled=item==1;
        s.vintageSkin=vintageForSkin(item==13?15:item);
        persistSetting(s);
        Snapshot current=snapshot();onlineart::setEnabled(surfacepolicy::artwork(surfacepolicy::current())&&s.artworkEnabled&&s.vintageSkin!=5&&s.vintageSkin!=6&&(IBT_NO_PSRAM||!s.playerEnabled||s.vintageSkin||s.mediaPlayerSkin),current.generation);
            if(s.artworkEnabled&&s.vintageSkin!=5&&s.vintageSkin!=6&&(IBT_NO_PSRAM||!s.playerEnabled||s.vintageSkin||s.mediaPlayerSkin)&&config.getMode()==PM_WEB){
                if(config.stationArtworkUrl()[0])onlineart::requestUrl(config.stationArtworkUrl(),current.generation);
                if(current.title[0])onlineart::requestTrack(current.title,current.generation);
            }
        redraw=true;return;
    }
    if(p==70){if(item==0)sensors::toggle();else if(item==1){Serial.println("[BLE-MENU] page=70 item=1 action=SEARCH");sensors::discover();Settings s=snapshot().settings;s.alarm.clockHome=true;persistSetting(s);page=PAGE_HOME;selected=0;front::invalidate();}else if(item==2){page=71;selected=0;}else if(item==4)goBack(p);redraw=true;return;}
    if(p==71){if(item<8)sensors::select(item);else goBack(p);redraw=true;return;}
    if(p==64&&item>=7&&item<=9){Settings s=snapshot().settings;if(item==7)s.alarm.clockStyle=(s.alarm.clockStyle+1)%4;else if(item==8)s.alarm.clockColor=(s.alarm.clockColor+1)%9;else s.alarm.textColor=(s.alarm.textColor+1)%9;persistSetting(s);redraw=true;return;}
    if(p==64&&item==6){bleMenuParent=64;page=70;selected=0;redraw=true;return;}
    if(p==68){if(item<4)openNtpServer(item);else goBack(p);return;}
    if(p==64&&item==3){page=68;selected=0;redraw=true;return;}
    if(p==64&&item==4){redraw=true;return;} // Fixed Hungary/CET-CEST timezone information.
    if(p==PAGE_ALARM&&item==0){Settings s=snapshot().settings;alarmui::toggle(s.alarm);persistSetting(s);redraw=true;return;}
    if(p==62&&item==0){Settings s=snapshot().settings;alarmui::profile(s.alarm);persistSetting(s);redraw=true;return;}
    if(p==PAGE_ALARM&&item>=1){
        if(item>=6){goBack(p);return;}
        page=item==3?63:item==4?62:59+item;selected=0;redraw=true;return;
    }
    if(p==60&&item<2){
        const auto a=snapshot().settings.alarm;alarmTimeDraft.ending=item==1;
        alarmTimeDraft.hour=item==1?a.offHour:a.onHour;alarmTimeDraft.minute=item==1?a.offMinute:a.onMinute;
        alarmTimeError=false;page=67;selected=0;redraw=true;return;
    }
    if(p>=60&&p<=64){
        const unsigned field=content::alarmField(p,item);
        if(field==27){page=PAGE_ALARM;selected=p==63?3:p==62?4:p-59;redraw=true;return;}
        item=field;p=PAGE_ALARM;
    }
    if(p==PAGE_ALARM){
        if(item==32){weather::toggle();redraw=true;return;}
        if(item>=27&&item!=28){goBack(p);return;}
        Settings s=snapshot().settings;auto &a=s.alarm;
        if(item>=17&&item<=23){a.daysMask^=uint8_t(1u<<(item-17));persistSetting(s);redraw=true;return;}
        auto stepPercent=[](uint8_t v){return alarmclock::nextLevel(v,0);};
        auto stepLight=[](uint8_t v){return alarmclock::nextLevel(v,5);};
        auto stepMinutes=[](uint8_t v){return alarmclock::nextMinutes(v);};
        switch(item){
          case 0:a.mode=(a.mode+1)%3;break;
          case 1:a.onHour=(a.onHour+1)%24;break;
          case 2:a.onMinute=(a.onMinute+1)%60;break;
          case 3:a.offHour=(a.offHour+1)%24;break;
          case 4:a.offMinute=(a.offMinute+1)%60;break;
          case 5:
            if(a.source||config.getMode()!=PM_WEB){
                portENTER_CRITICAL(&mux);strlcpy(state.notice,"Elobb valassz radioadot",sizeof(state.notice));portEXIT_CRITICAL(&mux);
                redraw=true;return;
            }
            a.station=config.lastStation()?config.lastStation():1;break;
          case 6:a.volumeStart=stepPercent(a.volumeStart);break;
          case 7:a.volumePeak=stepPercent(a.volumePeak);break;
          case 8:a.volumeEnd=stepPercent(a.volumeEnd);break;
          case 9:a.volumeRise=stepMinutes(a.volumeRise);break;
          case 10:a.volumeFall=stepMinutes(a.volumeFall);break;
          case 11:a.lightStart=stepLight(a.lightStart);break;
          case 12:a.lightPeak=stepLight(a.lightPeak);break;
          case 13:a.lightEnd=stepLight(a.lightEnd);break;
          case 14:a.lightRise=stepMinutes(a.lightRise);break;
          case 15:a.lightFall=stepMinutes(a.lightFall);break;
          case 16:a.nightClock=!a.nightClock;break;
          case 28:a.blinkColon=!a.blinkColon;break;
          case 24:a.clockDisplay=(a.clockDisplay+1)%3;break;
          case 25:if(FEATURE_SD)a.source=(a.source+1)%2;break;
          case 26:a.sdPlayMode=(a.sdPlayMode+1)%3;break;
        }
        persistSetting(s);redraw=true;return;
    }
    if(p==PAGE_RETRO_COLOR){
        if(item>=9){goBack(p);return;}
        Settings s=snapshot().settings;s.retroColor=uint8_t(item);persistSetting(s);
        redraw=true;return;
    }
    if(p==PAGE_DISPLAY){
#if IBT_NO_PSRAM
        if(item==2){page=PAGE_VU_TIMER;selected=0;redraw=true;}
        else if(item<6){static const unsigned map[]={2,1,0,5,7,3};editDisplay(map[item]);front::invalidate();redraw=true;}
        else goBack(p);
#else
        if(item==4){page=PAGE_SURFACE;selected=0;redraw=true;}else if(item==8){page=72;selected=0;redraw=true;}else if(item==9){page=PAGE_BAT_DISPLAY;selected=0;redraw=true;}else if(item==10){page=PAGE_VU_TIMER;selected=0;redraw=true;}else if(item<8)editDisplay(item);else goBack(p);
#endif
        return;}
    if(p==PAGE_CONTROLS){
#if IBT_COMPACT_UI
        if(item==5){page=74;selected=0;redraw=true;return;}
#endif
        if(item==3){Settings s=snapshot().settings;s.swipeSkins=!s.swipeSkins;persistSetting(s);redraw=true;return;}
        if(item==4){page=PAGE_IDLE;selected=0;redraw=true;return;}
        if(item==0)editControls(item);
        else if(item==1){
            if(player.isRunning())return;
            uint8_t rotation=state.settings.rotation;
            front::setRotation(0);calibrateTouch(true);front::setRotation(rotation);
            redraw=true;
        }else if(item==2){page=PAGE_BUTTON_MAP;selected=0;redraw=true;}else goBack(p);return;
    }
    if(p==PAGE_STORAGE){if(item==0){memoryParent=PAGE_STORAGE;page=PAGE_MEMORY;selected=0;redraw=true;}else if(item==1&&FEATURE_SD){page=PAGE_SD_INFO;selected=0;redraw=true;}else if(item==2||item==3){page=item==2?PAGE_ART_CACHE:PAGE_FS_INFO;selected=0;redraw=true;}else goBack(p);return;}
    if(p==PAGE_SYSTEM){
        if(item==0)editSystem(item);
        else if(item==1){page=PAGE_FIRMWARE_INFO;selected=0;redraw=true;}
        else if(item==2){page=PAGE_WEB_UPDATE;selected=0;redraw=true;}
        else if(item==4){page=PAGE_BACKUP_MENU;selected=0;redraw=true;}
        else if(item==3){
            if(player.isRunning()||snapshot().pendingSave||audioSettingPending){restartArmedAt=0;return;}
            uint32_t now=millis();
            if(!restartArmedAt||uint32_t(now-restartArmedAt)>5000){restartArmedAt=now;return;}
            ESP.restart();
        }else goBack(p);return;
    }
    if(p==PAGE_DIAGNOSTICS){
        if(item==0)editDiagnostics(item);
        else if(item==1){memoryParent=PAGE_DIAGNOSTICS;page=PAGE_MEMORY;selected=0;redraw=true;}
        else if(item==2){networkParent=PAGE_DIAGNOSTICS;page=PAGE_NETWORK_INFO;selected=0;redraw=true;}
        else if(item==3){page=PAGE_ROTARY_DEBUG;selected=0;redraw=true;}
        else if(item==4){page=PAGE_MEMORY_STRESS;selected=0;stressState=0;redraw=true;}

        else goBack(p);
        return;
    }
    if(p==PAGE_VOLUME_STREAM){if(item<=2)editVolumeStream(item);else if(item==3)send(Action::Balance);else goBack(p);return;}
    if(p==PAGE_EQ){if(item<3)send(item==0?Action::EqBass:item==1?Action::EqMiddle:Action::EqTreble);
        else if(item==3){page=PAGE_EQ10;selected=0;redraw=true;}else goBack(p);return;}
    if(p==PAGE_EQ10){
        if(item<eq10::Bands){Settings s=snapshot().settings;
            const int next=s.graphicEq.db[item]>=6?-12:int(s.graphicEq.db[item])+2;
            eq10::edit(s.graphicEq,item,next);persistSetting(s);player.requestGraphicEq();redraw=true;
        }else if(item==10){eqPresetParent=PAGE_EQ10;page=PAGE_EQ_PRESETS;selected=0;redraw=true;}
        else if(item==11){Settings s=snapshot().settings;s.graphicEq.enabled=!s.graphicEq.enabled;
            persistSetting(s);player.requestGraphicEq();redraw=true;}
        else goBack(p);return;
    }
    if(p==PAGE_EQ_PRESETS){if(item<eq10::PresetCount){Settings s=snapshot().settings;
            if(eq10::apply(s.graphicEq,item)){persistSetting(s);player.requestGraphicEq();}
            goBack(p);
        }else goBack(p);return;}
    if(p==PAGE_OUTPUTS){if(item==0){if(!player.sendCommand({PR_BT,-1})){btRateNotice("BT foglalt, próbáld újra");redraw=true;return;}cancelAlarmForUser();page=PAGE_DAC_INFO;selected=0;redraw=true;}else if(item==2||item==3){openBluetoothMenu(PAGE_OUTPUTS,item,item==2?1:0);}else goBack(p);return;}
}
bool webMenuSupported(unsigned p,unsigned item){
    switch(p){
      case 73:return item<12&&item!=3;
      case PAGE_VOLUME_STREAM:return item<=2;
      case PAGE_EQ:return item<4;
      case PAGE_EQ10:return item<12;
      case PAGE_EQ_PRESETS:return item<eq10::PresetCount;
      case PAGE_BT_RATE:return item<4;
      case PAGE_IDLE:return item<7;
      case PAGE_VU_TIMER:return item<(IBT_NO_PSRAM?7:5);
      case PAGE_BAT_DISPLAY:return item<3;
      case PAGE_HARDWARE:return item==0;
      case PAGE_DNS:return item==0||(item>=3&&item<=5);
      case PAGE_WIFI:return item>=5&&item<=7;
      case PAGE_SURFACE:return (item>0&&item<4)||(item>=6&&item<=13);
      case 72:return item<8;
      case PAGE_RETRO_COLOR:return item<9;
      case PAGE_ALARM:return item<7;
      case 64:return item<content::itemCount(p)&&item!=4;
      case 60:case 61:case 62:case 63:return item<content::itemCount(p);
      case PAGE_DISPLAY:return IBT_NO_PSRAM?(item<6&&item!=2):(item<=3||(item>=5&&item<=7));
      case PAGE_CONTROLS:return item==0||item==3;
      case PAGE_SYSTEM:return item==0;
      case PAGE_DIAGNOSTICS:return item==0;
      default:return false;
    }
}
bool webMenuActivate(unsigned p,unsigned item){
    if(!initialized||!webMenuCommands||!webMenuSupported(p,item))return false;
    const WebMenuCommand command={uint8_t(p),uint16_t(item)};
    return xQueueSend(webMenuCommands,&command,0)==pdTRUE;
}
bool webClockServer(unsigned index,const char* value){
    if(!initialized||!webMenuCommands||index>=4||!clocksync::validServer(value))return false;
    auto s=clockServers();strlcpy(s.values[index],value,64);
    if(!clocksync::anyServer(s.values))return false;
    WebMenuCommand command={254,uint16_t(index)};strlcpy(command.value,value,sizeof(command.value));
    return xQueueSend(webMenuCommands,&command,0)==pdTRUE;
}
bool webStationPlay(uint16_t station){
    if(!initialized||!webMenuCommands||station<1)return false;
    const WebMenuCommand command={255,station};
    return xQueueSend(webMenuCommands,&command,0)==pdTRUE;
}
static void btToggleAcknowledged(){
#if IBT_NO_PSRAM
    front::transportAck();
#endif
    btaudio::control(btaudio::inputPlaying()?1:0);
}
static void btPlayerKey(unsigned key){
    if(key==0)btaudio::control(2);
    else if(key==1)btaudio::control(0);
    else if(key==2)btaudio::control(3);
    else if(key==3)send(Action::VolDown);
    else if(key==4)btaudio::control(1);
    else if(key==5)send(Action::VolUp);
    redraw=true;
}
static void doShortPress() {
    if(page==PAGE_BT_SAVED){if(selected==4)bluetoothMode(0);else reconnectDevice(selected);return;}
    if(page==PAGE_BT_SCAN){
        if(btaudio::deviceCount())btaudio::selectDevice(selected);
        else if(!btaudio::scanning())btaudio::restartScan();
        return;
    }
    if(page==PAGE_BT_PLAYER){btToggleAcknowledged();return;}
    if(page==PAGE_STATIONS_LIST){commitStation((uint16_t)selected);return;}
    if(page==PAGE_FAVORITES_LIST){if(selected)commitStation((uint16_t)selected);else goBack(page);return;}
    if(page==PAGE_STATION_SEARCH){searchKey(selected);return;}
    if(page==PAGE_SEARCH_RESULTS){uint16_t n=stationSearchResult(selected);if(n)commitStation(n);else goBack(page);return;}
    if(page==PAGE_MEMORY_STRESS){runMemoryStress();return;}
    if(menuCount(page)){activateMenu(page,selected);return;}
    if(page!=PAGE_HOME){goBack(page);return;}
    // On the main screen a short rotary press toggles playback.
    send(Action::Toggle);
}
static void input(Action a) {
    const bool clockWasVisible=clockControlsVisible();
    if(a!=Action::None){menuActivityAt=millis();clockWake();}
    if(uiStressActive&&!uiStressInjecting)r10UiStressStop("user_input");
    if(!uiStressInjecting){
        const bool critical=a==Action::EncoderDown||a==Action::EncoderShort||a==Action::EncoderLong||
          a==Action::Menu||a==Action::Toggle||a==Action::Prev||a==Action::Next||
          a==Action::PlayerPlay||a==Action::PlayerPause||
          a==Action::Source||a==Action::SourceWeb||a==Action::SourceSd;
        const bool rotary=a==Action::RotCW||a==Action::RotCCW||a==Action::VolUp||a==Action::VolDown;
        if(!uiGovernor.admit(millis(),critical?UiGovernor::Kind::Critical:
              rotary?UiGovernor::Kind::Rotary:UiGovernor::Kind::Other,player.isRunning())){
            portENTER_CRITICAL(&mux);state.inputDrops++;portEXIT_CRITICAL(&mux);
            return;
        }
    }
    // Handle the second held press before the clock/editor can consume it.
    const bool clockGesturePage=page==PAGE_HOME||page==67;
    if(clockMenuPending&&a==Action::EncoderDown&&clockGesturePage){
        secondDown=clockgesture::secondPress(shortPending,millis(),shortAt);return;
    }
    if(a==Action::EncoderLong&&clockgesture::menu(clockMenuPending&&shortPending,secondDown,clockGesturePage,
       network.status!=SOFT_AP&&(!btaudio::transmitting()||config.getMode()==PM_SDCARD))){
        clockMenuPending=false;shortPending=false;secondDown=false;stationPreview=false;openMainMenu();return;
    }
    if(!clockGesturePage)clockMenuPending=false;
    if(page==PAGE_HOME&&snapshot().settings.alarm.clockHome){
        if(a==Action::RotCW||a==Action::RotCCW||a==Action::Next||a==Action::Prev||a==Action::VolUp||a==Action::VolDown){clockMenuPending=false;shortPending=false;secondDown=false;unsigned f=clockHomeFocus();clockFocus.store((f+((a==Action::RotCW||a==Action::Next||a==Action::VolUp)?1:10))%11,std::memory_order_relaxed);redraw=true;return;}
        if(a==Action::EncoderDown){clockPressWakeOnly=!clockWasVisible;return;}
        if(a==Action::EncoderShort||a==Action::Select||a==Action::Toggle){
            if(a!=Action::EncoderShort&&!clockWasVisible){redraw=true;return;}
            if(a==Action::EncoderShort){
                shortPending=true;secondDown=false;shortAt=millis();clockMenuPending=true;
                // Hidden clock: the first short press only reveals controls.
                if(!clockWasVisible||clockPressWakeOnly){clockPressWakeOnly=false;redraw=true;return;}
            }else clockMenuPending=false;
            unsigned f=clockHomeFocus();
            if(f==9){openMainMenu();return;}
            if(f==10){page=PAGE_ALARM;selected=0;redraw=true;return;}
            if(f==1)activateMenu(60,0);
            else {Settings s=snapshot().settings;if(f==0)alarmui::toggle(s.alarm);else s.alarm.daysMask^=uint8_t(1u<<(f-2));persistSetting(s);}
            redraw=true;return;
        }
    }
    if(page==67){
        if(a==Action::EncoderLong||a==Action::Menu){goBack(page);redraw=true;return;}
        const int delta=(a==Action::RotCW||a==Action::VolUp||a==Action::Next)?1:(a==Action::RotCCW||a==Action::VolDown||a==Action::Prev)?-1:0;
        if(delta){selected=alarmTimeDraft.rotate(selected,delta);alarmTimeError=false;redraw=true;return;}
        if(a==Action::EncoderShort||a==Action::Select){
            if(selected<2){++selected;redraw=true;return;}
            if(selected==2){Settings settings=snapshot().settings;auto &v=settings.alarm;
                if(!alarmTimeDraft.commit(v)){alarmTimeError=true;redraw=true;return;}
                persistSetting(settings);
            }
            goBack(page);redraw=true;
        }return;
    }
    if(page==PAGE_DNS_VALUE){
        if(a==Action::EncoderShort||a==Action::Select||a==Action::EncoderLong||a==Action::Menu){goBack(page);return;}
        const int delta=(a==Action::RotCW||a==Action::VolUp||a==Action::Next)?1:(a==Action::RotCCW||a==Action::VolDown||a==Action::Prev)?-1:0;
        if(delta){dnsDraft[dnsOctet]=radiodns::adjustOctet(dnsDraft[dnsOctet],delta);redraw=true;}return;
    }
    if(page==PAGE_BAT_VALUE){
        if(a==Action::EncoderShort||a==Action::Select||a==Action::EncoderLong||a==Action::Menu){goBack(page);return;}
        int delta=(a==Action::RotCW||a==Action::VolUp||a==Action::Next)?10:
          (a==Action::RotCCW||a==Action::VolDown||a==Action::Prev)?-10:0;
        if(delta){Settings s=snapshot().settings;s.batteryMv[calibrationPoint]=battery::adjust(s.batteryMv[calibrationPoint],delta);persistSetting(s);redraw=true;}
        return;
    }
    if(page==PAGE_SERVICE){
        if(a==Action::EncoderLong||a==Action::Menu)serviceControl(serviceui::Control::Back);
        else if(a==Action::RotCW||a==Action::VolUp||a==Action::Next)serviceControl(serviceui::Control::Plus);
        else if(a==Action::RotCCW||a==Action::VolDown||a==Action::Prev)serviceControl(serviceui::Control::Minus);
        else if(a==Action::EncoderShort||a==Action::Select||a==Action::Toggle)
            serviceControl(player.serviceActive()?serviceui::Control::Stop:serviceui::Control::Start);
        return;
    }
    if(a==Action::EncoderLong){
        if(page==PAGE_SOURCE_PENDING||page==PAGE_BT_SCAN||page==PAGE_WIFI_SCAN){goBack(page);return;}
        if(page==73)monoEdit=false;
        if(page==PAGE_WIFI_PASSWORD)clearWifiPassword();
        // A short press followed by a held second press opens the menu even
        // after the short action has already toggled playback.
        if(page==PAGE_HOME&&shortPending&&secondDown&&network.status!=SOFT_AP&&
           (!btaudio::transmitting()||config.getMode()==PM_SDCARD)){
            shortPending=false;secondDown=false;stationPreview=false;
            openMainMenu();return;
        }
#if defined(BOARD_CYD28)
        front::skinPreview(-1);
#endif
        if(page==PAGE_HOME && network.status!=SOFT_AP){
            shortPending=false;secondDown=false;
            openChannels();return;
        }
        r10DiagRecoverHome();return;
    }
#if defined(BOARD_CYD28)
    if(front::skinPreviewInput(a)){shortPending=false;secondDown=false;redraw=true;return;}
#endif
    uint32_t now=millis();
    if(page==PAGE_WIFI_PASSWORD||page==69){
        if(a==Action::RotCW||a==Action::VolUp||a==Action::Next){selected=wifipasswordui::move(selected,true,page==PAGE_WIFI_PASSWORD);redraw=true;}
        else if(a==Action::RotCCW||a==Action::VolDown||a==Action::Prev){selected=wifipasswordui::move(selected,false,page==PAGE_WIFI_PASSWORD);redraw=true;}
        else if(a==Action::Select||a==Action::Toggle||a==Action::EncoderShort){if(page==69)ntpServerKeyPress(selected);else wifiPasswordKeyPress(selected);}
        else if(a==Action::Menu)goBack(page);
        return;
    }
    if(page==PAGE_WIFI_SCAN){
        const unsigned count=network.scanCount();
        if((a==Action::RotCW||a==Action::VolUp||a==Action::Next)&&count){selected=(selected+1)%count;redraw=true;}
        else if((a==Action::RotCCW||a==Action::VolDown||a==Action::Prev)&&count){selected=(selected+count-1)%count;redraw=true;}
        else if(a==Action::Menu&&count)openSelectedWifiPassword(selected);
        else if(a==Action::Select||a==Action::Toggle||a==Action::EncoderShort){
            if(count)chooseScannedWifi(selected);
            else if(network.scanStatus()!=1){network.requestScan();redraw=true;}
        }
        return;
    }
    if(network.status==SOFT_AP){
        if(page==PAGE_WIFI_QR){if(a==Action::EncoderShort||a==Action::Select||a==Action::EncoderLong){page=PAGE_HOME;selected=0;redraw=true;}return;}
        if(a==Action::RotCW||a==Action::Next||a==Action::VolUp){selected=(selected+1)%4;redraw=true;}
        else if(a==Action::RotCCW||a==Action::Prev||a==Action::VolDown){selected=(selected+3)%4;redraw=true;}
        else if(a==Action::EncoderShort||a==Action::Select||a==Action::Toggle){
            if(selected==0)ESP.restart();
            else if(selected==1){selectConnectivity(1);front::invalidate();}
            else if(selected==2){page=PAGE_WIFI_QR;selected=0;redraw=true;}
            else {page=PAGE_WIFI_SCAN;selected=0;network.requestScan();redraw=true;}
        }else if(a==Action::Menu){page=PAGE_WIFI_SCAN;selected=0;network.requestScan();redraw=true;}
        return;
    }
    if(a==Action::EncoderDown) {
        if(page==PAGE_HOME && shortPending && uint32_t(now-shortAt)<=2500) secondDown=true;
        return;
    }
    if(a==Action::EncoderShort) {
        if(page==73&&selected<10&&selected>=2&&selected!=3){monoEdit=!monoEdit;redraw=true;return;}
        if(page!=PAGE_HOME){doShortPress();return;}
        // Execute the short press on release; retain only its timestamp for
        // the optional following long press. This avoids a delayed SD toggle.
        doShortPress();shortPending=true;secondDown=false;shortAt=now;return;
    }
    if(a==Action::Menu){if(!btaudio::transmitting()||config.getMode()==PM_SDCARD)openMainMenu();return;}
    if(page==PAGE_BT_PLAYER){
        if(a==Action::RotCW||a==Action::VolUp)send(Action::VolUp);
        else if(a==Action::RotCCW||a==Action::VolDown)send(Action::VolDown);
        else if(a==Action::Next)btaudio::control(3);
        else if(a==Action::Prev)btaudio::control(2);
        else if(a==Action::Select||a==Action::Toggle)btToggleAcknowledged();
        return;
    }
    if(page==PAGE_BT_SCAN){
        unsigned n=btaudio::deviceCount();
        if(n){
            if(a==Action::RotCW||a==Action::VolUp||a==Action::Next)selected=(selected+1)%n;
            else if(a==Action::RotCCW||a==Action::VolDown||a==Action::Prev)selected=(selected+n-1)%n;
            else if(a==Action::Select||a==Action::Toggle)btaudio::selectDevice(selected);
        }else if((a==Action::Select||a==Action::Toggle)&&!btaudio::scanning())btaudio::restartScan();
        return;
    }
    if(page==PAGE_BT_SAVED){
        unsigned n=btaudio::savedDeviceCount();
        if(a==Action::RotCW||a==Action::VolUp||a==Action::Next)
            selected=selected==4?(n?0:4):selected+1<n?selected+1:4;
        else if(a==Action::RotCCW||a==Action::VolDown||a==Action::Prev)
            selected=selected==4?(n?n-1:4):selected?selected-1:4;
        else if(a==Action::Select||a==Action::Toggle){if(selected==4)bluetoothMode(0);else reconnectDevice(selected);}
        redraw=true;
        return;
    }
    if(page==PAGE_STATION_SEARCH){
        if(a==Action::RotCW||a==Action::VolUp||a==Action::Next)selected=(selected+1)%30;
        else if(a==Action::RotCCW||a==Action::VolDown||a==Action::Prev)selected=(selected+29)%30;
        else if(a==Action::Select||a==Action::Toggle)searchKey(selected);
        redraw=true;return;
    }
    if(page==PAGE_SEARCH_RESULTS){
        if(searchFound){
            if(a==Action::RotCW||a==Action::VolUp||a==Action::Next)selected=(selected+1)%searchFound;
            else if(a==Action::RotCCW||a==Action::VolDown||a==Action::Prev)selected=(selected+searchFound-1)%searchFound;
            else if(a==Action::Select||a==Action::Toggle)commitStation(stationSearchResult(selected));
        }
        redraw=true;return;
    }
    if(page==PAGE_MEMORY_STRESS){
        if(a==Action::Select||a==Action::Toggle)runMemoryStress();
        return;
    }
    if(page==PAGE_STATIONS_LIST||page==PAGE_FAVORITES_LIST) {
        bool favPage=page==PAGE_FAVORITES_LIST;
        if(a==Action::RotCW||a==Action::VolUp||a==Action::Next) selected=favPage?nextFavorite((uint16_t)selected,true):wrapStation(int(selected)+1);
        else if(a==Action::RotCCW||a==Action::VolDown||a==Action::Prev) selected=favPage?nextFavorite((uint16_t)selected,false):wrapStation(int(selected)-1);
        else if((a==Action::Select||a==Action::Toggle)&&selected)commitStation((uint16_t)selected);
        redraw=true;return;
    }
    if(page!=73)monoEdit=false;
    if(page==73&&(a==Action::Select||a==Action::Toggle)&&selected<12&&selected!=3){
        if(selected==0||selected==1||selected>=10){activateMenu(page,selected);return;}
        monoEdit=!monoEdit;redraw=true;return;
    }
    if(page==73&&monoEdit&&(a==Action::RotCW||a==Action::RotCCW||a==Action::VolUp||a==Action::VolDown)){static const int8_t map[]={0,1,2,-1,3,4,5,6,7,8,9,10};monodsp::change(map[selected],a==Action::RotCW||a==Action::VolUp?1:-1);redraw=true;return;}
    unsigned count=menuCount(page);
    if(count) {
        if(a==Action::RotCW||a==Action::VolUp||a==Action::Next||a==Action::RotCCW||a==Action::VolDown||a==Action::Prev){
            bool forward=a==Action::RotCW||a==Action::VolUp||a==Action::Next;
            for(unsigned i=0;i<count;i++){
                selected=forward?(selected+1)%count:(selected+count-1)%count;
                if(menuEnabled(page,selected))break;
            }
            restartArmedAt=0;redraw=true;
        }
        else if(a==Action::Select||a==Action::Toggle)activateMenu(page,selected);
        return;
    }
    if(page!=PAGE_HOME){
        if(a==Action::Select||a==Action::Toggle)goBack(page);
        return;
    }
    if(a==Action::RotCW||a==Action::RotCCW) {
        Settings s=snapshot().settings;
        if(s.rotaryMode==1) send(a==Action::RotCW?Action::VolUp:Action::VolDown);
        else {
            if(!stationPreview){channelCursor=wrapStation(config.lastStation()?config.lastStation():1);stationPreview=true;}
            channelCursor=wrapStation(int(channelCursor)+(a==Action::RotCW?1:-1));
            showStationPreview(channelCursor);
        }
        return;
    }
    send(a==Action::Select?Action::Toggle:a);
}
static bool __attribute__((noinline)) homeSkinSwipe(Point from,Point to,Size dim,uint32_t elapsed,unsigned heldSkin){
    if(page!=PAGE_HOME||uiStressActive||front::skinPreviewActive()||network.status==SOFT_AP||
       !skinSwipeGesture(from,to,dim,elapsed))return false;
    Settings s;uint32_t generation;char title[192];
    portENTER_CRITICAL(&mux);s=state.settings;generation=state.generation;strlcpy(title,state.title,sizeof(title));portEXIT_CRITICAL(&mux);
    if(s.alarm.clockHome||!s.swipeSkins||playerSkinId(s)!=heldSkin)return false;
    if(!uiGovernor.admit(millis(),UiGovernor::Kind::Critical,player.isRunning()))return false;
    #if IBT_COMPACT_UI
    static constexpr unsigned skins[]={3,12};
#else
    static constexpr unsigned skins[]={1,2,3,6,7,8,11,12,15};
#endif
    constexpr unsigned count=sizeof(skins)/sizeof(skins[0]);
    unsigned index=0;while(index<count&&skins[index]!=heldSkin)++index;
    if(index==count)index=1; // Retired/invalid saved skin starts from Winamp.
    index=(index+(to.x<from.x?count-1:1))%count;
    const unsigned choice=skins[index];
    s.alarm.clockHome=false;s.playerEnabled=choice>=2;s.mediaPlayerSkin=choice==3;s.kajonSkin=false;
    s.cassetteEnabled=choice==1;s.vintageSkin=vintageForSkin(choice);
    persistSetting(s);
    const bool art=surfacepolicy::artwork(surfacepolicy::current())&&s.artworkEnabled&&s.vintageSkin!=5&&s.vintageSkin!=6&&(IBT_NO_PSRAM||!s.playerEnabled||s.vintageSkin||s.mediaPlayerSkin);
    onlineart::setEnabled(art,generation);
    if(art&&config.getMode()==PM_WEB){
        if(config.stationArtworkUrl()[0])onlineart::requestUrl(config.stationArtworkUrl(),generation);
        if(title[0])onlineart::requestTrack(title,generation);
    }
    if(frontendOK)front::invalidate();redraw=true;return true;
}
// Touch handling has large Snapshot temporaries. Keep its stack frame out of
// the rendering call chain; both execute serially on the same UI owner.
static void __attribute__((noinline)) uiTouchTick(uint32_t now) {
    static uiinteraction::Contact contact;
    static bool down=false,longPress=false,dragged=false;static int dragAnchor=0;static uint32_t pressed=0,lastTouchSeen=0;static Point start,last;
    Point p;const bool sensedDown=board::touch(p);
    if(sensedDown)lastTouchSeen=now;
    // Ignore a brief resistive-touch pressure dropout during a drag.
    const bool nowDown=sensedDown||(down&&uint32_t(now-lastTouchSeen)<=60);
    if(!sensedDown&&nowDown)p=last;
    if(nowDown||down)menuActivityAt=now;
    if(nowDown&&!down){
        pressed=now;start=p;last=p;longPress=false;dragged=false;
        unsigned rotation;portENTER_CRITICAL(&mux);rotation=state.settings.rotation;portEXIT_CRITICAL(&mux);
        const unsigned skin=surfacepolicy::skin();
        contact.begin(p.x,p.y,page,skin,rotation,now);
        dragAnchor=toLogical(p,front::nativeSize(),rotation).y;
    }
    if(nowDown&&down){last=p;contact.observe(p.x,p.y);if(page!=contact.page||surfacepolicy::skin()!=contact.skin)contact.consume();}
    if(sensedDown&&down&&!longPress&&!dragged&&!contact.consumed&&page==contact.page&&page==PAGE_HOME){
        const Size native=front::nativeSize();const unsigned rotation=contact.rotation;
        if(homeSkinSwipe(toLogical(start,native,rotation),toLogical(last,native,rotation),logicalSize(native,rotation),uint32_t(now-pressed),contact.skin))
            contact.consume();
    }
    if(sensedDown&&down&&!longPress&&(page==PAGE_STATIONS_LIST||page==PAGE_FAVORITES_LIST)){
        const auto a=toLogical(start,front::nativeSize(),snapshot().settings.rotation),b=toLogical(last,front::nativeSize(),snapshot().settings.rotation);
        if(a.y>=40&&(dragged||uiinteraction::verticalDrag(b.x-a.x,b.y-a.y))){
            dragged=true;const int delta=(b.y-dragAnchor)/26;
            if(delta){if(page==PAGE_FAVORITES_LIST){for(int i=0;i<abs(delta);++i)selected=nextFavorite(selected,delta>0);}
                else {const unsigned count=config.playlistLength();selected=count?1+uiinteraction::scroll(selected?selected-1:0,count,delta):0;}
                dragAnchor=b.y;redraw=true;}
        }
    }
    if(sensedDown&&down&&!longPress&&content::isMenuPage(page)){
        const auto dim=logicalSize(front::nativeSize(),snapshot().settings.rotation);
        const auto a=toLogical(start,front::nativeSize(),snapshot().settings.rotation),b=toLogical(last,front::nativeSize(),snapshot().settings.rotation);
        const unsigned count=menuCount(page);const bool alarm=alarmui::menu(page);
        const int top=alarm?alarmui::rowTop:53,step=alarm?alarmui::rowHeight:count>8?22:count?(dim.h-60)/int(count):22;
        if(a.y>=top&&count&&(dragged||uiinteraction::verticalDrag(b.x-a.x,b.y-a.y))){
            dragged=true;const int delta=(b.y-dragAnchor)/step;
            if(delta){selected=uiinteraction::scroll(selected,count,delta);dragAnchor=b.y;redraw=true;}
        }
    }
    if(sensedDown&&down&&!longPress&&!dragged&&contact.tap(page)&&uint32_t(now-pressed)>=800) {
        const Size native=front::nativeSize();const unsigned rotation=snapshot().settings.rotation;
        const Point a=toLogical(start,native,rotation),b=toLogical(last,native,rotation);
        if(abs(b.x-a.x)<12&&abs(b.y-a.y)<12){longPress=true;input(Action::EncoderLong);}
    }
    if(!nowDown&&down&&!longPress&&!dragged&&contact.tap(page)&&uint32_t(now-pressed)>=40) {
        Snapshot snap=snapshot();Size native=front::nativeSize();Size dim=logicalSize(native,snap.settings.rotation);
        Point q=toLogical(start,native,snap.settings.rotation);
        Point qLast=toLogical(last,native,snap.settings.rotation);
        const bool clockHomeTouch=page==PAGE_HOME&&snap.settings.alarm.clockHome;
        const bool clockSwitchTouch=false; // Clock is entered from the main menu.
        bool essentialTouch=clockHomeTouch||clockSwitchTouch||page==67||page==PAGE_SERVICE||q.y<40||
            (page==PAGE_HOME&&snap.settings.vintageSkin==6&&videotono::hit(q.x,q.y)!=videotono::Hit::None)||
            
            (page==PAGE_HOME&&snap.settings.playerEnabled&&snap.settings.mediaPlayerSkin&&dim.w==320&&dim.h==240&&q.y>=145)||
            (page==PAGE_HOME&&snap.settings.playerEnabled&&dim.w==320&&dim.h==240&&
             q.y>=136&&q.y<225&&q.x<308)||
            (page==PAGE_HOME&&snap.settings.playerEnabled&&dim.w==320&&dim.h==240&&
             ((q.y>=89&&q.y<112&&(q.x<158||(q.x>=185&&q.x<247)))||
              (q.y>=135&&q.y<153&&q.x>=258&&q.x<311)));
#if defined(BOARD_CYD28)
        if(page==PAGE_HOME && cassette::hit(dim,q)==Action::Toggle)essentialTouch=true;
#endif
        if(!uiStressInjecting && !uiGovernor.admit(now,essentialTouch?UiGovernor::Kind::Critical:
              UiGovernor::Kind::Touch,player.isRunning())){
            portENTER_CRITICAL(&mux);state.inputDrops++;portEXIT_CRITICAL(&mux);
        }else{
#if defined(BOARD_CYD28)
        if(front::skinPreviewActive()){
            input(q.x>=dim.w-52&&q.y>=dim.h-52?Action::Menu:q.x<dim.w/2?Action::Prev:Action::Next);
        } else
#endif
        if(page==PAGE_HOME&&network.status!=SOFT_AP&&!clockHomeTouch){
            const int button=lightui::homeButton(q.x,q.y);
            if(button>=0){const Action buttons[]={Action::Prev,Action::Toggle,Action::Next,Action::Source,Action::Menu};if(button==4&&btaudio::outputSelected()){page=PAGE_BT_SCAN;selected=0;redraw=true;}else input(buttons[button]);}
            else if(q.y>=170&&q.y<196&&q.x>=8&&q.x<312){if(player.sendCommand({PR_VOL,constrain((q.x-8)*254/303,0,254)})){cancelAlarmForUser();redraw=true;}}
            else if(q.y<28)openChannels();
        }else if(clockHomeTouch){
            const bool awake=clockControlsVisible();clockWake();redraw=true;
            if(!awake){/* Wake only: header changes, clock pixels stay intact. */}
            else if(q.y>=200&&q.y<240){
                if(q.x<30){Settings s=snapshot().settings;alarmui::toggle(s.alarm);persistSetting(s);}
                else if(q.x<102){activateMenu(60,0);}
                else {const unsigned day=unsigned(q.x-102)*7/unsigned(dim.w-102);if(day<7){Settings s=snapshot().settings;s.alarm.daysMask^=uint8_t(1u<<day);persistSetting(s);}}
            }
            else if(q.y>=4&&q.y<29&&q.x>=5&&q.x<dim.w/2-5){
                Settings s=snapshot().settings;page=PAGE_MAIN;selected=4;front::invalidate();redraw=true;
            }else if(q.y>=4&&q.y<29&&q.x>=dim.w/2+5&&q.x<dim.w-5){
                page=PAGE_ALARM;selected=0;redraw=true;
            }
        }
        else if(clockSwitchTouch){
            Settings s=snapshot().settings;s.alarm.clockHome=true;persistSetting(s);front::invalidate();redraw=true;
        }
        else if(page==67){
            switch(alarmui::hit(q.x,q.y,dim.w,dim.h)){
              case alarmui::Hit::Back:goBack(page);redraw=true;break;
              case alarmui::Hit::Hour:selected=0;redraw=true;break;
              case alarmui::Hit::Minute:selected=1;redraw=true;break;
              case alarmui::Hit::Minus:if(selected<2)input(Action::RotCCW);break;
              case alarmui::Hit::Plus:if(selected<2)input(Action::RotCW);break;
              case alarmui::Hit::Save:selected=2;input(Action::Select);break;
              case alarmui::Hit::Cancel:selected=3;input(Action::Select);break;
              default:break;
            }
        }
        else if(page==PAGE_SERVICE){serviceControl(serviceui::hit(q.x,q.y,serviceInfo));}
        else if(page==PAGE_WIFI_PASSWORD||page==69){
            if(q.y<24)goBack(page);
            else if(page==PAGE_WIFI_PASSWORD&&wifipasswordui::visibilityHit(q.x,q.y,dim.w)){selected=wifipasswordui::visibilityKey;wifiPasswordKeyPress(selected);}
            else if(q.x>=8&&q.x<8+8*((dim.w-16)/8)&&q.y>=91&&q.y<91+5*((dim.h-99)/5)){
                const unsigned key=unsigned((q.y-91)/((dim.h-99)/5))*8+unsigned((q.x-8)/((dim.w-16)/8));
                if(key<40){selected=key;if(page==69)ntpServerKeyPress(key);else wifiPasswordKeyPress(key);}
            }
        } else if(network.status==SOFT_AP){
            if(page==PAGE_WIFI_QR){if(q.y>=dim.h-48){page=PAGE_HOME;redraw=true;}}
            else if(page==PAGE_WIFI_SCAN){
                if(q.y<36){
#if IBT_NO_PSRAM && !IBT_COMPACT_UI
                    // Light renders Vissza on the right; it must never restart a scan.
                    goBack(page);
#else
                    if(q.x>=dim.w-90)network.requestScan();else goBack(page);
#endif
                    redraw=true;}
                else if(q.y>=39&&q.y<39+6*26){
                    const unsigned count=network.scanCount(),rows=6;
                    unsigned first=selected>=rows/2?selected-rows/2:0;
                    if(count>rows&&first>count-rows)first=count-rows;
                    unsigned index=first+unsigned((q.y-39)/26);
                    if(index<count){selected=index;chooseScannedWifi(index);redraw=true;}
                }else if(q.y>=dim.h-40&&q.x<120&&network.scanCount())openSelectedWifiPassword(selected);
            }
            else {
                switch(setupui::hit(q.x,q.y,dim.w,dim.h)){
                  case setupui::Hit::Reboot:ESP.restart();break;
                  case setupui::Hit::Player:selectConnectivity(1);front::invalidate();break;
                  case setupui::Hit::Qr:page=PAGE_WIFI_QR;selected=0;redraw=true;break;
                  case setupui::Hit::Scan:page=PAGE_WIFI_SCAN;selected=0;network.requestScan();redraw=true;break;
                  default:break;
                }
            }
        } else if(page==PAGE_WIFI_SCAN){
            if(q.y<36){goBack(page);}
            else if(q.y>=46&&q.y<46+5*28){
                unsigned count=network.scanCount(),rows=5;
                unsigned first=selected>=rows/2?selected-rows/2:0;
                if(count>rows&&first>count-rows)first=count-rows;
                unsigned index=first+unsigned((q.y-46)/28);if(index<count){selected=index;chooseScannedWifi(index);redraw=true;}
            }else if(q.y>=dim.h-40&&q.x<120&&network.scanCount())openSelectedWifiPassword(selected);
        } else if(page==PAGE_STATIONS_LIST||page==PAGE_FAVORITES_LIST){
            if(q.y<(page==PAGE_STATIONS_LIST&&snap.settings.playerEnabled?23:40))goBack(page);
            else {uint16_t n=stationAtTouch(start,page);if(n){selected=n;commitStation(n);}}
        } else if(content::isMenuPage(page)&&q.y>=5&&q.y<30&&q.x>=8&&q.x<72){
            page=PAGE_MAIN;selected=0;redraw=true;
        } else if(page==PAGE_MAIN&&q.y>=5&&q.y<30&&q.x>=dim.w-81&&q.x<dim.w-7){
            goBack(PAGE_MAIN);
        } else if(content::isMenuPage(page)&&q.y>=5&&q.y<30&&q.x>=dim.w-81&&q.x<dim.w-7){
            goBack(page);
        } else if(page==PAGE_BT_SAVED){
            if(q.y<36)goBack(page);
            else if(q.y<36+5*30){unsigned i=unsigned((q.y-36)/30);
                if(i==4){selected=4;bluetoothMode(0);}
                else if(i<btaudio::savedDeviceCount()){selected=i;reconnectDevice(i);}}
        } else if(page==PAGE_BT_SCAN){
            unsigned n=btaudio::deviceCount(),rows=unsigned((dim.h-36)/26);
            unsigned first=selected>=rows/2?selected-rows/2:0;
            if(n>rows&&first>n-rows)first=n-rows;
            if(q.y<36&&q.x<dim.w-32)goBack(page);
            else if(q.y>=36&&q.y<36+int(rows)*26){unsigned i=first+unsigned((q.y-36)/26);if(i<n){selected=i;btaudio::selectDevice(i);}
                else if(!n&&!btaudio::scanning())btaudio::restartScan();}
        } else if(page==PAGE_BT_PLAYER){
            using Key=btspeakerui::Control;
            switch(btspeakerui::hit(q.x,q.y)){
              case Key::Previous:btPlayerKey(0);break;
              case Key::Toggle:btToggleAcknowledged();redraw=true;break;
              case Key::Next:btPlayerKey(2);break;
              case Key::Menu:bluetoothMode(1);break;
              case Key::Volume:if(player.sendCommand({PR_VOL,int(btspeakerui::percent(q.x)*254/100)})){cancelAlarmForUser();redraw=true;}break;
              default:break;
            }
        } else if(page==PAGE_STATION_SEARCH){
            if(q.y<40)goBack(page);
            else if(q.x>=10&&q.y>=65){
                int keyW=(dim.w-20)/6,keyH=(dim.h-90)/5;
                if(keyW>0&&keyH>0&&q.x<10+keyW*6&&q.y<65+keyH*5){
                    unsigned key=unsigned((q.y-65)/keyH)*6+unsigned((q.x-10)/keyW);
                    if(key<30){selected=key;searchKey(key);}
                }
            }
        } else if(page==PAGE_SEARCH_RESULTS){
            if(q.y<40||q.y>=dim.h-34)goBack(page);
            else if(q.y>=56&&q.y<56+6*23){
                unsigned first=selected>=2?selected-2:0;
                if(searchFound>6&&first>searchFound-6)first=searchFound-6;
                unsigned i=first+unsigned((q.y-56)/23);
                uint16_t n=stationSearchResult(i);if(n){selected=i;commitStation(n);}
            }
        } else if(page==PAGE_BAT_VALUE||page==PAGE_DNS_VALUE){
            if(q.y<40)goBack(page);
            else if(q.y>=180){if(q.x<106)input(Action::RotCCW);else if(q.x>213)input(Action::RotCW);else goBack(page);}
        } else if(page==PAGE_MEMORY_STRESS){
            if(q.y<40||q.y>=dim.h-40)goBack(page);
            else if(q.y>=70&&q.y<150)runMemoryStress();
        } else {
            unsigned count=menuCount(page);
            if(count){
                const bool alarmMenu=alarmui::menu(page);
                const bool longMenu=count>8;
                unsigned first=longMenu&&selected>=4?selected-4:0;
                if(longMenu&&first+8>count)first=count-8;
                if(alarmMenu)first=alarmui::first(selected,count);
#if defined(BOARD_CYD28)
                const bool rowsReady=front::menuViewport(page,first);
#else
                const bool rowsReady=true;
#endif
                int rowHeight=alarmMenu?alarmui::rowHeight:longMenu?22:(dim.h-60)/int(count);
                const unsigned visible=alarmMenu?alarmui::visible(count):longMenu?8:count;
                const int rowTop=IBT_NO_PSRAM?49:alarmMenu?alarmui::rowTop:53;
                int row=(q.y-rowTop)/rowHeight;
                if(count>visible&&q.x>=dim.w-16&&q.y>=rowTop&&q.y<rowTop+int(visible)*rowHeight){
                    selected=unsigned(q.y-rowTop)*(count-1)/(visible*rowHeight-1);
                    redraw=true; // Touching the scrollbar scrolls, never edits a setting.
                }else if(rowsReady&&q.y>=rowTop&&row>=0&&row<int(visible)){selected=first+(unsigned)row;activateMenu(page,selected);}
            } else if(page!=PAGE_HOME){goBack(page);}
            else {
#if IBT_NO_PSRAM
                const int button=lightui::homeButton(q.x,q.y);
                if(button>=0){const Action actions[]={Action::Prev,Action::Toggle,Action::Next,Action::Source,Action::Menu};input(actions[button]);}
                else if(q.y<28)openChannels();
#else
#if defined(BOARD_CYD28)
                if(snap.settings.playerEnabled&&snap.settings.vintageSkin==6&&dim.w==320&&dim.h==240){
                    using videotono::Hit;const auto key=videotono::hit(q.x,q.y);
                    if(key==Hit::Menu)openMainMenu();
                    else if(key==Hit::Channels||key==Hit::Memo)openChannels();
                    else if(key==Hit::Bluetooth){if(!btaudio::inputSelected())bluetoothMode(1);else{page=PAGE_HOME;selected=0;redraw=true;}}
                    else if(key==Hit::Radio)input(Action::SourceWeb);
                    else if(key==Hit::Sd)input(Action::SourceSd);
                    else if(key==Hit::Volume){if(player.sendCommand({PR_VOL,videotono::volume(q.x)})){cancelAlarmForUser();redraw=true;}}
                    else {
                        if(key==Hit::Prev)input(Action::Prev);else if(key==Hit::Toggle)input(Action::Toggle);else if(key==Hit::Next)input(Action::Next);
                    }
                }else if(snap.settings.playerEnabled&&snap.settings.vintageSkin==5&&dim.w==320&&dim.h==240){
                    if(q.y<25&&q.x>=280)openMainMenu();
                    else if(q.y>=216){
                        if(q.x<44){openBluetoothMenu(PAGE_HOME,0,0);}
                        else if(q.x<88)input(Action::Prev);
                        else if(q.x<132)input(Action::Toggle);
                        else if(q.x<177)input(Action::Next);
                        else if(q.x>=205&&q.x<282&&player.sendCommand({PR_VOL,constrain((q.x-207)*254/74,0,254)})){cancelAlarmForUser();redraw=true;}
                    }else if(q.y>=168&&q.y<213)openChannels();
                }else if(snap.settings.playerEnabled&&snap.settings.vintageSkin==4&&dim.w==320&&dim.h==240){
                    if(q.y>=185&&q.y<218){
                        if(q.x<99){openBluetoothMenu(PAGE_HOME,0,0);}
                        else if(q.x<132)input(Action::Prev);
                        else if(q.x<165)input(Action::Toggle);
                        else if(q.x<197)input(Action::Next);
                        else if(q.x<237)openMainMenu();
                        else if(player.sendCommand({PR_VOL,constrain((q.x-240)*254/55,0,254)})){cancelAlarmForUser();redraw=true;}
                    }else if(q.y>=50&&q.y<115)openChannels();
                }else if(snap.settings.vintageSkin&&q.y<27&&q.x>=250){openBluetoothMenu(PAGE_HOME,0,0);
                }else if(snap.settings.playerEnabled&&snap.settings.vintageSkin&&dim.w==320&&dim.h==240){
                    const unsigned style=snap.settings.vintageSkin;
                    const int buttonY=style==1?194:style==2?163:178;
                    if(q.y>=buttonY&&q.y<235){
                        if(q.x<125)input(Action::Prev);
                        else if(q.x<190)input(Action::Toggle);
                        else if(q.x<263)input(Action::Next);
                        else openMainMenu();
                    }else if(q.y>=46&&q.y<116)openChannels();
                }else if(snap.settings.playerEnabled&&snap.settings.mediaPlayerSkin&&dim.w==320&&dim.h==240){
                    if(q.y>=214){
                        if(q.x<108){openBluetoothMenu(PAGE_HOME,0,0);}
                        else if(q.x>=212)openMainMenu();
                    }else if(q.y>=194&&q.y<213&&q.x>=40&&q.x<=175){
                        if(player.sendCommand({PR_VOL,constrain((q.x-40)*254/135,0,254)})){cancelAlarmForUser();redraw=true;}
                    }else if(q.y>=146&&q.y<193){
                        if(q.x<73)input(Action::Prev);
                        else if(q.x<131)input(Action::Toggle);
                        else if(q.x<186){if(player.sendCommand({PR_STOP,0}))cancelAlarmForUser();redraw=true;}
                        else if(q.x<243)input(Action::Next);
                        else openChannels();
                    }else if(q.y>=22&&q.y<42){
                        if(q.x<115)input(Action::SourceWeb);
                        else if(q.x<211)input(Action::SourceSd);
                        // USB remains unavailable on classic ESP32 hardware.
                    }else if(q.y>=46&&q.y<131)openChannels();
                }else if(snap.settings.playerEnabled&&dim.w==320&&dim.h==240){
                    if(q.y>=136&&q.y<155&&q.x>=9&&q.x<84){
                        if(q.x<40){Settings s=snapshot().settings;s.graphicEq.enabled=!s.graphicEq.enabled;
                            persistSetting(s);player.requestGraphicEq();redraw=true;}
                        else {eqPresetParent=PAGE_HOME;page=PAGE_EQ_PRESETS;selected=0;redraw=true;}
                    }else if(q.y>=155&&q.y<225&&q.x>=76&&q.x<307){
                        Settings s=snapshot().settings;
                        const unsigned band=unsigned(min(9,(q.x-76)/22));
                        const int gain=constrain((204-q.y)*18/37-12,-12,6);
                        eq10::edit(s.graphicEq,band,gain);s.graphicEq.enabled=true;
                        persistSetting(s);player.requestGraphicEq();redraw=true;
                    }else if(q.y>=135&&q.y<153&&q.x>=258&&q.x<311){
                        openMainMenu();
                    }else if(q.y>=61&&q.y<75&&q.x>=124&&q.x<=199){
                        const int target=constrain((q.x-124)*254/61,0,254);
                        if(player.sendCommand({PR_VOL,target})){cancelAlarmForUser();redraw=true;}
                    }else if(q.y>=90&&q.y<112){
                        if(q.x<37)input(Action::Prev);
                        else if(q.x<67)input(Action::PlayerPlay);
                        else if(q.x<97)input(Action::PlayerPause);
                        else if(q.x<126){if(player.sendCommand({PR_STOP,0}))cancelAlarmForUser();redraw=true;}
                        else if(q.x<157)input(Action::Next);
                        else if(q.x<185)openChannels();
                        else if(q.x<247){openBluetoothMenu(PAGE_HOME,0,0);}
                        else if(q.x>=279)openMainMenu();
                    }
                }
                else if(snap.settings.cassetteEnabled&&front::cassetteReady()&&cassette::btQuickHit(dim,q))
                    {openBluetoothMenu(PAGE_HOME,0,0);}
                else {Action a=cassette::hit(dim,q);if(a!=Action::None)input(a);}
#else
                if(q.y>=dim.h-44){int col=q.x*5/dim.w;const Action actions[]={Action::Prev,Action::Toggle,Action::Next,Action::Source,Action::Menu};if(col>=0&&col<5)input(actions[col]);}
#endif
#endif
            }
        }
        } // accepted touch
    }
    if(!nowDown&&down)contact.finish(now);
    down=nowDown;
}
#if IBT_COMPACT_UI
static __attribute__((noinline)) void irTickUI(){
    bool pending,clockHome;
    portENTER_CRITICAL(&mux);pending=state.pendingSave;clockHome=state.settings.alarm.clockHome;portEXIT_CRITICAL(&mux);
    const auto irAction=irreceiver::tick(!player.isRunning()&&player.status()==STOPPED&&!btaudio::wanted()&&
        !pending&&!weather::busy()&&uxTaskGetStackHighWaterMark(nullptr)>=3072);
    if(irAction!=smallir::Action::None){
        clockWake();
        switch(irAction){
        case smallir::Action::VolumeDown:input(Action::VolDown);break;
        case smallir::Action::VolumeUp:input(Action::VolUp);break;
        case smallir::Action::Previous:input(Action::Prev);break;
        case smallir::Action::Next:input(Action::Next);break;
        case smallir::Action::PlayPause:input(Action::Toggle);break;
        case smallir::Action::Menu:input(Action::Menu);break;
        case smallir::Action::Back:if(page)goBack(page);break;
        case smallir::Action::Select:input(Action::Select);break;
        case smallir::Action::Source:input(Action::Source);break;
        case smallir::Action::Clock:requestSurfaceSkin(clockHome?3:13);break;
        case smallir::Action::Mute:{static uint8_t unmuted=80;const uint8_t v=config.store.volume;
            if(v){unmuted=v;player.setVol(0);}else player.setVol(unmuted);break;}
        default:break;
        }redraw=true;
    }
}
#endif
void uiLoop() {
    if(!initialized)return;
#if IBT_STREAM_ART_DIAG
    board::poll([](Action a){
      if(a==Action::Select)send(Action::Toggle);
      else if(a==Action::RotCW)send(Action::VolUp);
      else if(a==Action::RotCCW)send(Action::VolDown);
    });
    static bool diagnosticDown=false;static Point diagnosticStart{},diagnosticLast{};Point diagnosticPoint{};
    const bool diagnosticTouch=board::touch(diagnosticPoint);
    uint8_t diagnosticRotation;portENTER_CRITICAL(&mux);diagnosticRotation=state.settings.rotation;portEXIT_CRITICAL(&mux);
    if(diagnosticTouch){diagnosticLast=toLogical(diagnosticPoint,front::nativeSize(),diagnosticRotation);if(!diagnosticDown)diagnosticStart=diagnosticLast;}
    if(!diagnosticTouch&&diagnosticDown&&diagnosticStart.x>=0&&diagnosticStart.x<192&&diagnosticStart.y>=196&&diagnosticStart.y<240&&
       diagnosticLast.x>=0&&diagnosticLast.x<192&&diagnosticLast.y>=196&&diagnosticLast.y<240&&
       diagnosticStart.x/64==diagnosticLast.x/64&&abs(diagnosticLast.x-diagnosticStart.x)<20&&abs(diagnosticLast.y-diagnosticStart.y)<20){
      if(diagnosticStart.x<64)send(Action::Prev);
      else if(diagnosticStart.x<128)send(Action::Toggle);
      else send(Action::Next);
    }
    diagnosticDown=diagnosticTouch;
    const auto diagnostic=snapshot();
    onlineart::listeningTick(diagnostic.generation,diagnostic.elapsed,diagnostic.playing&&diagnostic.connected);
    static uint32_t diagnosticDraw=0;static bool first=true;
    if(frontendOK&&uint32_t(millis()-diagnosticDraw)>=500){diagnosticDraw=millis();front::render(diagnostic,0,0,first);first=false;}
    if(frontendOK){onlineart::uiTick(diagnostic.generation,true);onlineart::serviceReader();front::service();}
    return;
#endif
#if IBT_COMPACT_UI
    irTickUI();
#endif
    serviceConnectivity(); // keep the connection owner running during boot overlays
#if defined(BOARD_CYD28)
    front::btClockTick();
    if(bootprogress::state().armed.load()){
        static unsigned bootFocus=0;static bool down=false;static Point press{},last{};
        // Only three scalar settings are needed during boot. Do not put four
        // full Snapshot return buffers on the 8KiB display task's stack.
        uint8_t rotation,choice,vintage;
        portENTER_CRITICAL(&mux);rotation=state.settings.rotation;choice=state.settings.bootScreen;vintage=state.settings.vintageSkin;portEXIT_CRITICAL(&mux);
        board::poll([](Action){});
        Point p{};const bool touched=board::touch(p);
        if(touched){last=toLogical(p,front::nativeSize(),rotation);if(!down)press=last;}
        if(!touched&&down&&videotono::bootVideoton(choice,vintage)&&!(rotation&1u)&&abs(last.x-press.x)<20&&abs(last.y-press.y)<20){const unsigned key=bootprogress::key(press.x,press.y);if(key<3){bootFocus=key;selectConnectivity(key);}}
        down=touched;
        if(front::bootFrame(network.status==CONNECTED&&WiFi.status()==WL_CONNECTED,network.status==SOFT_AP,network.offlineReady(),bootFocus))return;
        redraw=true;
    }
#endif
    WebMenuCommand webCommand;
    if(webMenuCommands&&xQueueReceive(webMenuCommands,&webCommand,0)==pdTRUE){
        if(webCommand.page==254){
            saveClockServer(webCommand.item,webCommand.value);
        }else if(webCommand.page==255){
            // File-backed station selection stays on the UI task, never the
            // AsyncWebServer callback or the audio task.
            if(!btaudio::wanted()&&config.getMode()==PM_WEB&&
               webCommand.item<=config.playlistLength())commitStation(webCommand.item);
        }else if(webMenuSupported(webCommand.page,webCommand.item)&&
                 menuEnabled(webCommand.page,webCommand.item)){
            const unsigned oldPage=page,oldSelected=selected;
            const unsigned oldBleParent=bleMenuParent,oldEqParent=eqPresetParent;
            activateMenu(webCommand.page,webCommand.item);
            // Web navigation must not replace the physical display's Back history.
            bleMenuParent=oldBleParent;eqPresetParent=oldEqParent;
            page=oldPage;selected=oldSelected;redraw=true;
        }
    }
    // Rare ADC conversion on UI/Core0; never in decoder or Bluetooth callback.
    uint8_t mode,requestedBrightness;portENTER_CRITICAL(&mux);
    mode=state.settings.sensorMode;requestedBrightness=state.settings.brightness;
    portEXIT_CRITICAL(&mux);
    uint16_t mv=0;bool valid=board::sensorSample(mode,millis(),mv);
    static uint8_t appliedBrightness=100;
    const auto alarmSetting=snapshot().settings.alarm;
    const bool manualAlarmLight=alarmPhase!=AlarmPhase::Idle&&alarmLastLight==AlarmManualLight;
    const bool nightClockLight=!manualAlarmLight&&alarmSetting.nightClock&&alarmSetting.clockHome&&
        page==PAGE_HOME&&!player.isRunning();
    const bool alarmControlsLight=(alarmPhase!=AlarmPhase::Idle&&!manualAlarmLight)||nightClockLight;
    static bool wasAlarmLight=false;
    if(wasAlarmLight&&!alarmControlsLight)appliedBrightness=255;
    if(!wasAlarmLight&&nightClockLight&&alarmPhase==AlarmPhase::Idle)
        board::brightness(alarmSetting.lightStart);
    wasAlarmLight=alarmControlsLight;
    if(!alarmControlsLight&&mode==1&&requestedBrightness==0&&valid){
        const uint8_t target=light::brightness(mv);
        if(appliedBrightness==255||target>=appliedBrightness+3||target+3<=appliedBrightness){board::brightness(target);appliedBrightness=target;}
    }else if(!alarmControlsLight&&requestedBrightness){
        if(appliedBrightness!=requestedBrightness)board::brightness(requestedBrightness);
        appliedBrightness=requestedBrightness;
    }
    if(!alarmControlsLight&&!requestedBrightness&&!valid&&appliedBrightness!=100){board::brightness(100);appliedBrightness=100;}
    portENTER_CRITICAL(&mux);
    bool sensorChanged=state.sensorMv!=mv||state.sensorValid!=valid;
    state.sensorMv=mv;state.sensorValid=valid;
    portEXIT_CRITICAL(&mux);
    if(sensorChanged&&page==PAGE_DISPLAY)redraw=true;
    const uint32_t pressureNow=millis();
    const bool beforePressure=uiGovernor.active(pressureNow);
    uint32_t pressureBuffer,pressureHeap,pressureDrops;
    portENTER_CRITICAL(&mux);
    pressureBuffer=state.buffer;pressureHeap=state.freeHeap;pressureDrops=state.queueDrops;
    portEXIT_CRITICAL(&mux);
    uiGovernor.observe(pressureNow,player.isRunning(),pressureBuffer,pressureHeap,
      commands?uxQueueMessagesWaiting(commands):0,player.queueDepth(),player.audioGapEvents(),
      pressureDrops,player.commandDrops());
    if(beforePressure!=uiGovernor.active(pressureNow))redraw=true;
    board::poll(input);
    const int requestedSkin=surfaceRequest.exchange(-1);
    if(requestedSkin>=0){
        if(uiStressActive)r10UiStressStop("surface_change");
        Settings setting=snapshot().settings;
        const unsigned id=unsigned(requestedSkin);
        setting.alarm.clockHome=id==13;
        if(id!=5&&id!=13&&id!=14){setting.playerEnabled=id>=2;setting.mediaPlayerSkin=id==3;setting.kajonSkin=false;setting.cassetteEnabled=id==1;setting.vintageSkin=vintageForSkin(id);}
        // A diagnostic skin selection is RAM-only, never a flash write.
        portENTER_CRITICAL(&mux);state.settings=setting;portEXIT_CRITICAL(&mux);
        page=id==5?PAGE_SERVICE:id==14?PAGE_BT_PLAYER:PAGE_HOME;selected=0;
        publishSurface(setting);front::invalidate();redraw=true;surfaceDiagnostics();
    }
    {
        const auto setting=snapshot().settings;publishSurface(setting);
        const auto surface=surfacepolicy::current();
        static uint32_t policyStopAt=0;static bool policyStopped=false;
        const bool alarmOwns=alarmPhase!=AlarmPhase::Idle&&alarmPhase!=AlarmPhase::Ending;
        const bool stopClock=surface==surfacepolicy::Surface::Clock&&((!alarmOwns&&(player.isRunning()||player.serviceActive()))||btaudio::wanted()||!btaudio::idle());
        if(surface!=surfacepolicy::Surface::Service&&player.serviceActive()&&!alarmFallbackValid())player.serviceStop();
        const bool stopOutput=(surface==surfacepolicy::Surface::Weather||surface==surfacepolicy::Surface::BtInput)&&btaudio::outputSelected();
        if((stopClock||stopOutput)&&(!policyStopped||uint32_t(millis()-policyStopAt)>=500)){
            policyStopAt=millis();policyStopped=true;
            const uint32_t token=alarmOwns?alarmRequestEpoch.load():0;
            if(stopClock&&!alarmOwns){player.serviceStop();player.sendCommand({PR_STOP,0});}
            else player.sendCommand({PR_BT,-1,token});
            Serial.printf("[SURFACE-GATE] action=STOP_FORBIDDEN surface=%s alarm=%u\n",surfacepolicy::name(surface),unsigned(alarmOwns));
        }else if(!stopClock&&!stopOutput)policyStopped=false;
        static int artSurface=-1;
        if(artSurface!=int(surface)){artSurface=int(surface);onlineart::setEnabled(surfacepolicy::artwork(surface)&&setting.artworkEnabled&&setting.vintageSkin!=5&&setting.vintageSkin!=6,snapshot().generation);}
    }
    alarmTick(millis());
    if(alarmPhase==AlarmPhase::Source&&sdman.alarmScan()&&alarmRequestValid(alarmLastDay))
        sdman.stepAlarmFolderScan(sdman.alarmScan());
    else if(alarmPhase==AlarmPhase::Idle||alarmPhase==AlarmPhase::Ending)
        sdman.freeAlarmFolderScan();
    btRateTick(millis());
    if(pendingSourceAt){
        const auto status=player.sdPrepareState();
        if(pendingSourceTarget==PM_SDCARD){
            if(status==SdPrepareState::Ready && config.getMode()==PM_SDCARD){
                pendingSourceAt=0;if(page==PAGE_SOURCE_PENDING){page=PAGE_HOME;selected=0;redraw=true;}
            }else if(uint32_t(millis()-pendingSourceAt)>52000 &&
                     status!=SdPrepareState::Failed && status!=SdPrepareState::Cancelled &&
                     !pendingSourceCancelSent){
                pendingSourceCancelSent=player.sendCommand({PR_CANCEL_SOURCE,0});redraw=true;
            }
        }else if(config.getMode()==PM_WEB && status!=SdPrepareState::Indexing){
            pendingSourceAt=0;if(page==PAGE_SOURCE_PENDING){page=PAGE_HOME;selected=0;redraw=true;}
        }
    }
    if((page==PAGE_BT_SCAN||page==PAGE_BT_SAVED)&&btaudio::transmitting()){
        page=PAGE_HOME;selected=0;redraw=true;
    }
    if(page==PAGE_WIFI_PASSWORD&&clockStore->wifi.submitted&&network.status!=SOFT_AP){
        clearWifiPassword();page=PAGE_HOME;selected=0;redraw=true;
    }
    uint32_t now=millis();
    {const auto s=snapshot();weather::tick(surfacepolicy::ble(surfacepolicy::current())&&page==PAGE_HOME&&!s.alarmActive&&!s.pendingSave&&!player.isRunning()&&player.status()==STOPPED&&!btaudio::wanted());
#if IBT_NO_PSRAM && FEATURE_ARTWORK
    onlineart::listeningTick(s.generation,s.elapsed,s.playing&&!s.sd&&s.connected&&!btaudio::wanted());
#endif
    if(sensors::available()){const uint32_t blockers=(page!=PAGE_HOME?1u:0u)|(!surfacepolicy::ble(surfacepolicy::current())?2u:0u)|(s.playing?4u:0u)|(s.alarmActive?8u:0u)|(s.pendingSave?16u:0u)|(clockControlsVisible()?32u:0u)|(weather::busy()?64u:0u)|(player.isRunning()?128u:0u)|(player.status()!=STOPPED?256u:0u)|(btaudio::wanted()?512u:0u);sensors::setBudget(blockers==0,blockers);}
    }
    r10UiStressTick(now);
    // Keep controls responsive, but leave the display bus idle briefly after
    // a long audio-service gap so the decoder/network path can recover first.
    static uint32_t seenAudioGaps=0,quietUiUntil=0;
    const uint32_t audioGaps=player.audioGapEvents();
    if(audioGaps!=seenAudioGaps){seenAudioGaps=audioGaps;quietUiUntil=now+2000;}
    // The main network loop owns Wi-Fi scan APIs; UI only requests/cancels.
    if(page!=PAGE_WIFI_SCAN && network.scanStatus()==1)network.cancelScan();
    if(page==PAGE_WIFI_SCAN && network.status!=SOFT_AP &&
       (player.isRunning()||player.status()==PLAYING))network.cancelScan();
    if(network.status!=SOFT_AP && page==PAGE_WIFI_QR){page=PAGE_HOME;selected=0;redraw=true;}
    if(shortPending && !secondDown && uint32_t(now-shortAt)>2500){shortPending=false;clockMenuPending=false;}
    uiTouchTick(now);
    // Inactivity navigation never issues stop, source-change or BT disconnect.
    static unsigned idlePage=PAGE_HOME;
    if(page!=idlePage){idlePage=page;menuActivityAt=now;}
    uint16_t idleSeconds;
    portENTER_CRITICAL(&mux);idleSeconds=state.settings.menuIdleSeconds;portEXIT_CRITICAL(&mux);
    if(page==PAGE_HOME)menuActivityAt=now;
    else if(page!=PAGE_SERVICE&&!manualWifiPage(page)&&idleSeconds&&!uiStressActive&&uint32_t(now-menuActivityAt)>=uint32_t(idleSeconds)*1000u){
        backupArmedAt=0;restartArmedAt=0;
        r10DiagRecoverHome();idlePage=PAGE_HOME;menuActivityAt=now;
    }
    serviceRouteTick();
    static uint32_t renderAt=0;
    const auto renderSettings=snapshot().settings;
    const bool sdBtClassic=ibtsdui::classic(btaudio::transmitting(),config.getMode()==PM_SDCARD,renderSettings.playerEnabled);
    const bool conservativeBtHome=(page==PAGE_HOME||page==PAGE_STATIONS_LIST)&&btaudio::outputSelected()&&!sdBtClassic&&!uiStressFullScan;
    if(frontendOK&&conservativeBtHome){
        artab::tick(now);if(artab::consumeRepaint()){front::invalidate();redraw=true;}
        if(!pathprobe::enabled.load(std::memory_order_relaxed))loadprobe::tick();
    }
    unsigned homeRenderMs=vuprobe::dacEnabled.load(std::memory_order_relaxed)?unsigned(vuprobe::refreshMs.load(std::memory_order_relaxed)):250u;
    static FrameCadence cadence;
    int cadenceC0=-1,cadenceC1=-1;bool cadenceKnown=false,cadenceBusy=false;
    if(page==PAGE_HOME||alarmui::menu(page)||page==67){
        // Clock menus and home skins target20Hz; unchanged pixels are not transferred. Audio quiet gates remain.
        static uint32_t loadAt=0;
        // Silent bounded CPU sampler also works without a diagnostic session.
        if(uint32_t(now-loadAt)>=1000){loadAt=now;loadprobe::tick();}
        cadenceKnown=loadprobe::cached(cadenceC0,cadenceC1,&cadenceBusy);
        homeRenderMs=cadence.update(now,cadenceKnown,cadenceC0,cadenceC1,uiGovernor.active(now),cadenceBusy);
    }
    // Keep the old output fallback's conservative periodic budget, but draw
    // the selected skin. An explicit redraw still obeys the500ms audio gate.
    if(conservativeBtHome&&homeRenderMs<1000)homeRenderMs=1000;
    // Full-menu diagnostics obey the same audio headroom gate as normal UI.
    const bool btUiSafe=!player.isRunning()||btaudio::uiHeadroomSafe();
    static bool btUiWasBlocked=false;
    static uint32_t btUiGateLoggedAt=0;
    if(btUiWasBlocked==btUiSafe&&uint32_t(now-btUiGateLoggedAt)>=1000){
        btUiGateLoggedAt=now;
        btUiWasBlocked=!btUiSafe;
        Serial.printf("[UI-BT-GATE] blocked=%u reason=TX_PCM_HEADROOM required_ms=500 stress_bypass=0\n",unsigned(!btUiSafe));
    }
    const bool menuPage=content::isMenuPage(page);
    const bool recoveryReady=menurender::recoveryReady(page==PAGE_SERVICE,player.isRunning(),
        btaudio::transmitting(),menuPage,btUiSafe,int32_t(now-quietUiUntil)>=0);
    bool menuContinuation=false;
#if defined(BOARD_CYD28)
    menuContinuation=menuPage&&front::renderPending();
#endif
    // A full-scan action is already paced once by r10UiStressTick.
    // Its pending first slice must not wait for the same draw interval again.
    const bool scanRequest=menuPage&&uiStressActive&&uiStressFullScan&&uiStressActions!=uiStressRenderedActions;
    const bool cadenceReady=(page==PAGE_SERVICE||uiStressFullScan||ibtsdui::due(sdBtClassic,uint32_t(now-renderAt)))&&
        menurender::due(menuPage,redraw,menuContinuation,scanRequest,uiStressActive,
         uint32_t(now-renderAt),uiStressActive?uiStressDrawMs:(page==PAGE_SERVICE?100u:homeRenderMs));
    static menurender::Wait menuWait;
    menuWait.observe(now,page,selected,frontendOK&&menuPage&&(redraw||menuContinuation||
        (uiStressActive&&uiStressActions!=uiStressRenderedActions)),btUiSafe,recoveryReady,cadenceReady,audioGaps);
    if(frontendOK&&btUiSafe&&recoveryReady&&cadenceReady&&uint32_t(now-renderAt)>=litememory::frameInterval) {
        renderAt=now;Snapshot s=snapshot();prepareTrackText(s);
        const uint32_t drawStart=micros();
        {lightmem::Phase memoryPhase("UI_RENDER");front::render(s,page,selected,redraw);}redraw=false;
        bool complete=true;
#if defined(BOARD_CYD28)
        complete=!front::renderPending();
#endif
        // Per-request wall costs include preemption; they are not UI CPU time.
        static uint32_t sliceCount=0,sliceMax=0,sliceTotal=0;
        static unsigned slicePage=~0u,sliceItem=~0u;
        if(!menuContinuation||slicePage!=page||sliceItem!=selected){sliceCount=sliceMax=sliceTotal=0;slicePage=page;sliceItem=selected;}
        const uint32_t sliceUs=micros()-drawStart;
        if(menuPage){++sliceCount;sliceTotal+=sliceUs;if(sliceUs>sliceMax)sliceMax=sliceUs;}
        if(complete&&menuPage&&(pathprobe::enabled.load(std::memory_order_relaxed)||sliceMax>=100000))
            Serial.printf("[UI-MENU-SLICES] page=%u item=%u calls=%lu max_wall_us=%lu total_wall_us=%lu required_ms=500 complete=1\n",page,selected,(unsigned long)sliceCount,(unsigned long)sliceMax,(unsigned long)sliceTotal);
        // Counts are observed gate checks, not estimates of CPU time or an
        // audible-quality verdict. No per-loop logging or PCM notifications.
        if(complete&&menuWait.active){
            const uint32_t completedAt=millis();
            if(menuWait.elapsed(completedAt)>=1000)Serial.printf("[UI-MENU-WAIT] page=%u item=%u wait_ms=%lu headroom_checks=%lu quiet_checks=%lu cadence_checks=%lu loop_max_ms=%lu draw_us=%lu required_ms=500 headroom_ms=%lu quiet_ms=%lu cadence_ms=%lu gap_events=%lu quiet_left_ms=%lu wall_buckets_overlap=1\n",
                page,selected,(unsigned long)menuWait.elapsed(completedAt),(unsigned long)menuWait.headroomChecks,
                (unsigned long)menuWait.quietChecks,(unsigned long)menuWait.cadenceChecks,
                (unsigned long)menuWait.maxLoopMs,(unsigned long)(micros()-drawStart),
                (unsigned long)menuWait.headroomMs,(unsigned long)menuWait.quietMs,(unsigned long)menuWait.cadenceMs,
                (unsigned long)menuWait.gapEvents,(unsigned long)(int32_t(quietUiUntil-completedAt)>0?quietUiUntil-completedAt:0));
            menuWait.active=false;
        }
        // Diagnostic only: render-call cadence/cost, not physical display refresh rate.
        static uint32_t frameAt=0,drawTotal=0;
        static uint16_t frameCount=0,frameKey=0xffffu;
        const uint16_t key=unsigned(page)|(unsigned(s.settings.vintageSkin)<<6)|
            (unsigned(s.settings.kajonSkin)<<9)|(unsigned(s.settings.mediaPlayerSkin)<<10)|
            (unsigned(s.settings.cassetteEnabled)<<11)|(unsigned(s.settings.playerEnabled)<<12)|
            (unsigned(s.sd)<<13)|(unsigned(btaudio::transmitting())<<14);
        if(!pathprobe::enabled.load(std::memory_order_relaxed)||key!=frameKey){
            frameAt=now;frameCount=drawTotal=0;frameKey=key;
        }else{
            const uint32_t cost=micros()-drawStart;
            ++frameCount;drawTotal+=cost;
            const uint32_t elapsed=now-frameAt;
            if(elapsed>=10000){
                Serial.printf("[UI-FRAME] key=%lu calls=%lu elapsed_ms=%lu draw_us=%lu target_ms=%u\n",
                    (unsigned long)key,(unsigned long)frameCount,(unsigned long)elapsed,
                    (unsigned long)drawTotal,homeRenderMs);
                Serial.printf("[UI-CADENCE] target_ms=%u known=%u busy=%u c0=%d c1=%d reason=%u hold_ms=%lu good_ms=%lu buffer=%lu heap=%lu quiet_ms=%lu\n",
                    homeRenderMs,unsigned(cadenceKnown),unsigned(cadenceBusy),cadenceC0,cadenceC1,
                    unsigned(uiGovernor.reasons(now)),(unsigned long)uiGovernor.holdMs(now),
                    (unsigned long)cadence.goodMs(now),(unsigned long)pressureBuffer,(unsigned long)pressureHeap,
                    (unsigned long)(int32_t(quietUiUntil-now)>0?quietUiUntil-now:0));
                frameAt=now;frameCount=drawTotal=0;
            }
        }
#if defined(BOARD_CYD28)
        if(page==PAGE_HOME)front::renderBattery(s);
#endif
        if(uiStressActive&&complete){
            ++uiStressRenderCalls;
            const unsigned slot=scanSlot(page);
            const bool alarmSlot=slot==UiScanSlots;
            if(uiStressFullScan&&uicoverage::renderable(slot,UiScanSlots,selected)){
                uint8_t* const seen=alarmSlot?uiStressAlarmSeen:uiStressScanSeen[slot];
                if(!(seen[selected>>3]&(uint8_t(1)<<(selected&7)))){
                seen[selected>>3]|=uint8_t(1)<<(selected&7);
                ++uiStressScanRendered;
                Serial.printf("[UI-MENU-SCAN] page=%u item=%u result=RENDERED\n",page,selected);
                }
            }
            if(uiStressActions!=uiStressRenderedActions){++uiStressFrames;uiStressRenderedActions=uiStressActions;}
        }
        if(network.status!=SOFT_AP&&!btaudio::wanted()){
            if(s.settings.artworkEnabled&&(IBT_NO_PSRAM||(s.settings.cassetteEnabled&&!s.settings.playerEnabled)||s.settings.vintageSkin||s.settings.mediaPlayerSkin)){
                if(s.sd)artwork::uiTick(s.generation,page==0);
                else onlineart::uiTick(s.generation,page==0);
            }
        }
    }
    if(frontendOK)front::service();
#if IBT_NO_PSRAM && FEATURE_ARTWORK
    onlineart::serviceReader();
#endif
#if IBT_NO_PSRAM
    if(frontendOK)front::liteMeterTick((page==PAGE_HOME||page==PAGE_BT_PLAYER)&&btUiSafe&&recoveryReady&&int32_t(now-quietUiUntil)>=0&&!uiGovernor.active(now));
#endif
}
void audioLoop() {
    if(!initialized)return;
#if IBT_STREAM_ART_DIAG
    Action diagnosticAction;
    for(unsigned i=0;i<4&&commands&&xQueueReceive(commands,&diagnosticAction,0)==pdTRUE;++i){
      if(diagnosticAction==Action::Toggle)player.toggle();
      else if(diagnosticAction==Action::Prev)player.prev();else if(diagnosticAction==Action::Next)player.next();
      else if(diagnosticAction==Action::VolUp)player.stepVol(true);else if(diagnosticAction==Action::VolDown)player.stepVol(false);
    }
    const uint32_t diagnosticNow=millis(),diagnosticBuffer=player.inBufferFilled();
    const bool diagnosticRunning=player.isRunning();
    ibtlimit::publishAudio(diagnosticRunning||player.status()==PLAYING,diagnosticBuffer,diagnosticNow,diagnosticBuffer+player.inBufferFree());
    static uint32_t diagnosticAt=0;
    if(uint32_t(diagnosticNow-diagnosticAt)>=500){diagnosticAt=diagnosticNow;
      Snapshot s=snapshot();s.playing=diagnosticRunning;s.sd=false;s.volume=config.store.volume;
      s.bitrate=config.station.bitrate;s.sampleRate=player.getSampleRate();s.elapsed=player.getAudioCurrentTime();
      s.buffer=diagnosticBuffer;s.bufferCapacity=diagnosticBuffer+player.inBufferFree();
      s.freeHeap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);s.largestHeap=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
      s.connected=WiFi.status()==WL_CONNECTED;s.rssi=s.connected?WiFi.RSSI():0;textCopy(s.station,sizeof(s.station),config.station.name);
      portENTER_CRITICAL(&mux);state=s;portEXIT_CRITICAL(&mux);
    }
    return;
#endif
    Action a;
    if(commands&&xQueueReceive(commands,&a,0)==pdTRUE) {
        switch(a) {
        case Action::Prev:player.prev();break;
        case Action::Next:player.next();break;
        case Action::Toggle:player.toggle();break;
        case Action::PlayerPlay:
            if(player.status()!=PLAYING)player.toggle();
            break;
        case Action::PlayerPause:
            if(player.status()==PLAYING)player.toggle();
            break;
        case Action::VolDown:player.stepVol(false);break;
        case Action::VolUp:player.stepVol(true);break;
        case Action::Source:
            page=75;selected=0;redraw=true;break;
        case Action::SourceWeb:
        case Action::SourceSd:
#if FEATURE_SD
            {
                auto target=a==Action::SourceWeb?PM_WEB:a==Action::SourceSd?PM_SDCARD:
                    (config.getMode()==PM_WEB?PM_SDCARD:PM_WEB);
                if(target!=config.getMode())player.sendCommand({PR_SOURCE,int(target)});
            }
#endif
            break;
        case Action::EqBass:
        case Action::EqMiddle:
        case Action::EqTreble: {
            int8_t* value=a==Action::EqBass?&config.store.bass:a==Action::EqMiddle?&config.store.middle:&config.store.trebble;
            int8_t next=*value>=6?-12:*value+2;
            config.saveValue(value,next,false);
            player.setTone(config.store.bass,config.store.middle,config.store.trebble);
            audioSettingPending=true;redraw=true;break;
        }
        case Action::Balance: {
            int8_t next=config.store.balance>=16?-16:config.store.balance+2;
            config.saveValue(&config.store.balance,next,false);
            player.setBalance(config.store.balance);
            audioSettingPending=true;redraw=true;break;
        }
        default:break;
        }
    }
    // Publish handled volume immediately instead of waiting for the500ms telemetry snapshot.
    portENTER_CRITICAL(&mux);
    if(state.volume!=config.store.volume){state.volume=config.store.volume;redraw=true;}
    portEXIT_CRITICAL(&mux);
    uint32_t now=millis();bool running=player.isRunning()||btaudio::wanted()||player.serviceActive();
    // Source changes are serialized by the audio owner, after stopping playback.
    uint32_t buffered=player.inBufferFilled();
    ibtlimit::publishAudio(running||player.status()==PLAYING,buffered,now,buffered+player.inBufferFree());
    // Apply queued artwork resets even while BT suppresses optional image I/O.
    artwork::audioTick(running&&!btaudio::wanted(),player.eofHeader||player.getAudioDataStartPos()>0,buffered);
    if(uint32_t(now-lastSnapshot)>=500) {
        lastSnapshot=now;Snapshot s=snapshot();
        s.playing=running;s.sd=config.getMode()==PM_SDCARD;
        s.bitrate=config.station.bitrate;s.volume=config.store.volume;
        s.bass=config.store.bass;s.middle=config.store.middle;s.treble=config.store.trebble;s.balance=config.store.balance;
        s.sampleRate=player.getSampleRate();s.elapsed=player.getAudioCurrentTime();s.duration=player.getAudioFileDuration();
        s.buffer=buffered;s.bufferCapacity=buffered+player.inBufferFree();s.freeHeap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        s.largestHeap=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        s.freePsram=ESP.getFreePsram();s.connected=WiFi.status()==WL_CONNECTED;s.rssi=s.connected?WiFi.RSSI():0;
        textCopy(s.station,sizeof(s.station),config.station.name);
        if(s.sd)snprintf(s.station,sizeof(s.station),"SD / %u",config.lastStation());
        // Song callbacks own s.title; config.station.title also contains transport errors/status.
        IPAddress ip=s.connected?WiFi.localIP():WiFi.softAPIP();
        snprintf(s.ip,sizeof(s.ip),"%u.%u.%u.%u",unsigned(ip[0]),unsigned(ip[1]),unsigned(ip[2]),unsigned(ip[3]));
        // Explicit radio admission policy. It cannot lower bitrate at the transmitting station.
        if(running&&!s.sd&&s.settings.maxKbps&&s.bitrate>s.settings.maxKbps&&!bitrateRejected) {
            bitrateRejected=true;player.sendCommand({PR_STOP,0});
            snprintf(s.notice,sizeof(s.notice),"%u > %u kbit/s. Lower bitrate URL required.",s.bitrate,s.settings.maxKbps);
        }
        if(!btaudio::wanted()&&btaudio::linkFailed())textCopy(s.notice,sizeof(s.notice),"BT hiba: 9 proba utan DAC");
        if(btaudio::wanted()){
            if(btaudio::inputSelected())textCopy(s.station,sizeof(s.station),"BT IN");
            // Transport status has its own UI field; never replace song metadata.
        }
        bool nextLow=player.isRunning()&&buffered<1600;
        inputlow::sample(now,nextLow,buffered,s.generation,!player.isRunning()||btaudio::uiHeadroomSafe());
        if(nextLow&&!low)s.lowBufferEvents++;low=nextLow;
        portENTER_CRITICAL(&mux);
        // Audio metadata callbacks run on this core; only UI settings can change here.
        s.settings=state.settings;s.pendingSave=state.pendingSave||monodsp::pendingSave();s.queueDrops=state.queueDrops;
        s.inputDrops=state.inputDrops;state=s;
        portEXIT_CRITICAL(&mux);
        const bool lightCover=IBT_NO_PSRAM&&!lightui::meterMode(s.settings.artworkEnabled,surfacepolicy::current()==surfacepolicy::Surface::BtInput);
        const bool vuOn=s.settings.playerEnabled&&!lightCover&&!s.settings.mediaPlayerSkin&&s.settings.vintageSkin!=6&&running&&!btaudio::wanted();
        int vuC0=-1,vuC1=-1;const bool vuKnown=loadprobe::cached(vuC0,vuC1);
        const bool btVu=!lightCover&&btinputvu::budget((surfacepolicy::current()==surfacepolicy::Surface::BtInput||(s.settings.playerEnabled&&!s.settings.mediaPlayerSkin&&s.settings.vintageSkin!=6))&&!s.settings.alarm.clockHome,
            btaudio::inputSelected(),btaudio::inputPlaying(),vuKnown,vuC0,vuC1,
            btaudio::inputArtworkSafe()&&s.freeHeap>=ibtlimit::Policy::badInternal&&s.largestHeap>=8192);
        btinputvu::enabled.store(btVu&&(!IBT_NO_PSRAM||lightui::style.load()!=0),std::memory_order_relaxed);
        const bool lightVu=IBT_NO_PSRAM&&!lightCover&&lightui::style.load()!=0&&vuKnown&&vuC0>=0&&vuC0<800&&vuC1>=0&&vuC1<800&&
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=ibtlimit::Policy::badInternal&&s.largestHeap>=8192;
        vuprobe::dacEnabled.store(lightVu&&vuOn,std::memory_order_relaxed);
        const bool localBtVu=s.settings.kajonSkin&&s.settings.playerEnabled&&s.sd&&running&&btaudio::transmitting();
        vuprobe::localBtEnabled.store(lightVu&&s.sd&&running&&btaudio::transmitting(),std::memory_order_relaxed);
        if(!vuprobe::dacEnabled.load()&&!btVu&&!vuprobe::localBtEnabled.load()){vuprobe::dacLeft.store(0,std::memory_order_relaxed);
            vuprobe::dacRight.store(0,std::memory_order_relaxed);}
    }
    monodsp::service(!running&&player.status()==STOPPED,config.store.volume);
    // Flash commits run only with playback stopped; no UI-triggered flash stall during audio.
    portENTER_CRITICAL(&mux);
    bool needsSave=settingsVersion!=savedVersion||clockStore->version!=clockStore->saved;
    portEXIT_CRITICAL(&mux);
    if(!running&&player.status()==STOPPED&&needsSave&&int32_t(now-nextSettingsCommit)>=0) {
        Settings s;clocksync::Servers servers;uint32_t version,clockVersion;
        portENTER_CRITICAL(&mux);s=state.settings;version=settingsVersion;servers=clockStore->servers;clockVersion=clockStore->version;portEXIT_CRITICAL(&mux);
        Preferences p;bool ok=p.begin("yo-dual-v1",false);
        if(ok){
          for(unsigned i=0;i<4&&ok;++i){char key[8];snprintf(key,sizeof(key),"ntp%u",i);const size_t n=strlen(servers.values[i]);
            // Preferences returns zero for a successful empty-string write too.
            if(n)ok=p.putString(key,servers.values[i])==n;
            else {p.putString(key,"");ok=p.isKey(key)&&p.getString(key,"x").isEmpty();}
          }
#if IBT_NO_PSRAM
          ok=ok&&p.putUChar("yovu",lightui::style.load())==1&&p.putBool("yovuv1",true)==1;
#endif
          ok=ok&&p.putUChar("lang",unsigned(s.language))==1 && p.putUChar("theme",unsigned(s.theme))==1
          &&p.putBool("dnsmanual",s.dnsManual)==1
          &&p.putBytes("dnsprimary",s.dnsPrimary,4)==4&&p.putBytes("dnssecond",s.dnsSecondary,4)==4
          &&p.putUChar("sensor",s.sensorMode)==1&&p.putUChar("batshow",s.batteryIndicator?1:0)==1
          &&p.putUChar("batstep",s.batteryStep)==1&&p.putUChar("batstyle",s.batteryAppearance)==1
          &&p.putUShort("bat0",s.batteryMv[0])==2&&p.putUShort("bat25",s.batteryMv[1])==2
          &&p.putUShort("bat50",s.batteryMv[2])==2&&p.putUShort("bat75",s.batteryMv[3])==2&&p.putUShort("bat100",s.batteryMv[4])==2
          &&p.putUChar("rotate",s.rotation)==1&&p.putUChar("bright",s.brightness)==1
          &&p.putUChar("alm_mode",s.alarm.mode)==1
          &&p.putUChar("alm_prof",s.alarm.enabledProfile)==1
          &&p.putUChar("alm_h0",s.alarm.onHour)==1&&p.putUChar("alm_m0",s.alarm.onMinute)==1
          &&p.putUChar("alm_h1",s.alarm.offHour)==1&&p.putUChar("alm_m1",s.alarm.offMinute)==1
          &&p.putUShort("alm_station",s.alarm.station)==2
          &&p.putUChar("alm_v0",s.alarm.volumeStart)==1&&p.putUChar("alm_v1",s.alarm.volumePeak)==1
          &&p.putUChar("alm_v2",s.alarm.volumeEnd)==1&&p.putUChar("alm_vup",s.alarm.volumeRise)==1
          &&p.putUChar("alm_vdown",s.alarm.volumeFall)==1
          &&p.putUChar("alm_l0",s.alarm.lightStart)==1&&p.putUChar("alm_l1",s.alarm.lightPeak)==1
          &&p.putUChar("alm_l2",s.alarm.lightEnd)==1&&p.putUChar("alm_lup",s.alarm.lightRise)==1
          &&p.putUChar("alm_ldown",s.alarm.lightFall)==1&&p.putUChar("alm_clock",s.alarm.nightClock?1:0)==1
          &&p.putUChar("alm_home",s.alarm.clockHome?1:0)==1
          &&p.putUChar("alm_days",s.alarm.daysMask)==1
          &&p.putUChar("alm_view",s.alarm.clockDisplay)==1
          &&p.putUChar("alm_font",s.alarm.clockStyle)==1&&p.putUChar("alm_ink",s.alarm.clockColor)==1&&p.putUChar("alm_text",s.alarm.textColor)==1
          &&p.putBool("alm_blink",s.alarm.blinkColon)==1
          &&p.putUChar("alm_src",s.alarm.source)==1
          &&p.putUChar("alm_sdplay",s.alarm.sdPlayMode)==1
          &&p.putBytes("eq10",&s.graphicEq,sizeof(s.graphicEq))==sizeof(s.graphicEq)
          &&p.putUShort("rate",s.maxKbps)==2
          &&p.putUChar("rotmode",s.rotaryMode)==1&&p.putUChar("volstep",s.volumeStep)==1
          &&p.putUChar("debug",s.debug?1:0)==1&&p.putUChar("artwork",s.artworkEnabled?1:0)==1
          &&p.putUChar("cassette",s.cassetteEnabled?1:0)==1
          &&p.putUChar("bufbar",s.bufferIndicator?1:0)==1
          &&p.putUChar("reels",s.reelAnimation?1:0)==1
          &&p.putUChar("wifibar",s.wifiIndicator?1:0)==1
          &&p.putUChar("btquick",s.btQuickScan?1:0)==1
          &&p.putUChar("hls",s.hlsEnabled?1:0)==1
          &&p.putUChar(playerSettingKey,s.playerEnabled?1:0)==1&&p.putUChar("mediaskin",s.mediaPlayerSkin?1:0)==1&&p.putUChar("kajonskin",s.kajonSkin?1:0)==1&&p.putUChar("vinskin",s.vintageSkin)==1&&p.putUChar("bootart",s.bootScreen)==1&&p.putUChar("bootsound",s.bootSound)==1&&p.putUChar("netmode",s.networkMode)==1&&p.putUChar("retrocol",s.retroColor)==1&&p.putUChar("swipeskin",s.swipeSkins?1:0)==1&&p.putUChar("swipev2",1)==1&&p.putUShort("menuidle",s.menuIdleSeconds)==2&&p.putUShort("vutimer",vuprobe::refreshMs.load(std::memory_order_relaxed))==2&&p.putBool("light100v1",true)==1;p.end();}
        if(ok){portENTER_CRITICAL(&mux);savedVersion=version;clockStore->saved=clockVersion;state.pendingSave=settingsVersion!=savedVersion||favoriteVersion!=favoriteSaved||clockStore->version!=clockStore->saved;portEXIT_CRITICAL(&mux);}
        else nextSettingsCommit=now+5000;
    }
    portENTER_CRITICAL(&mux);bool favoritesNeedSave=favoriteVersion!=favoriteSaved;portEXIT_CRITICAL(&mux);
    if(!running&&player.status()==STOPPED&&favoritesNeedSave&&int32_t(now-nextFavoriteCommit)>=0){
        uint8_t bits[sizeof(favoriteBits)];uint32_t version,hash;
        portENTER_CRITICAL(&mux);memcpy(bits,favoriteBits,sizeof(bits));version=favoriteVersion;hash=favoritePlaylistHash;portEXIT_CRITICAL(&mux);
        if(!hash)hash=playlistHash();
        Preferences p;bool ok=p.begin("yo-dual-v1",false);
        if(ok){ok=p.putBytes("favorites",bits,sizeof(bits))==sizeof(bits)&&p.putUInt("favhash",hash)==4;p.end();}
        if(ok){portENTER_CRITICAL(&mux);favoriteSaved=version;if(favoriteVersion==version)favoritePlaylistHash=hash;state.pendingSave=settingsVersion!=savedVersion||favoriteVersion!=favoriteSaved||clockStore->version!=clockStore->saved;portEXIT_CRITICAL(&mux);}
        else nextFavoriteCommit=now+5000;
    }
    if(!running&&player.status()==STOPPED&&audioSettingPending&&int32_t(now-nextAudioCommit)>=0){
        if(EEPROM.commit())audioSettingPending=false;else nextAudioCommit=now+5000;
    }
    if(debugEnabled() && uint32_t(now-lastDiag)>=10000) {
        lastDiag=now;Snapshot s=snapshot();
        Serial.printf("[DUAL] play=%u heap=%lu largest=%lu psram=%lu input=%lu low=%lu drop=%lu rate=%u\n",s.playing,(unsigned long)s.freeHeap,(unsigned long)s.largestHeap,(unsigned long)s.freePsram,(unsigned long)s.buffer,(unsigned long)s.lowBufferEvents,(unsigned long)s.queueDrops,s.bitrate);
    }
}
}
