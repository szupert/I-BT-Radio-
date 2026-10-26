#include "service_model.h"
#pragma once
#include "logic.h"
#include "clock_sync_policy.h"
namespace dual {
bool sdFilesAvailable(); // connected SD-BT picker can open files without teardown
bool alarmFallbackValid();
bool requestSurfaceSkin(unsigned skin); // UI-owner mailbox, RAM only
void surfaceDiagnostics();
void trackTextDiagnostics(); // Explicit read-only raw/clean codepoints, no audio command.
void clockWake();
unsigned clockHomeFocus();
bool clockControlsVisible();
struct Snapshot {
    char device[40]={}; char station[170]={}; char artist[170]={};
    char title[170]={}; char album[170]={}; char ip[20]={};char notice[100]={};
    uint32_t generation=0, elapsed=0, duration=0, sampleRate=0, buffer=0, bufferCapacity=0, freeHeap=0, largestHeap=0, freePsram=0;
    uint32_t queueDrops=0, inputDrops=0, lowBufferEvents=0;
    int rssi=0; uint16_t bitrate=0;uint8_t volume=0;
    uint8_t audioCodec=0,sourceBits=0;uint16_t formatKbps=0; // audio-owner snapshot; display only
    int8_t bass=0,middle=0,treble=0,balance=0;
    bool playing=false, sd=false, connected=false, pendingSave=false, ready=false, alarmActive=false;
    uint16_t sensorMv=0; bool sensorValid=false;
    Settings settings;
};
struct WifiPasswordUi {
    char ssid[33]={};
    char password[64]={}; // local opt-in preview only; never diagnostic logging
    char error[48]={};
    uint8_t mode=0;
    bool submitted=false;
    bool visible=false; // Reset on entry/cancel; never persisted.
};
const WifiPasswordUi& wifiPasswordUi();
const WifiPasswordUi& ntpServerUi();
clocksync::Servers clockServers();
bool webClockServer(unsigned index,const char* value);
char wifiPasswordKeyChar(unsigned mode,unsigned key);
uint8_t dnsDraftOctet(unsigned index);
unsigned dnsEditOctet();
void begin();
void uiLoop();
unsigned sensorCalibrationPoint();
void bluetoothMode(int mode); // 1 phone input, 0 scan/output, -1 radio DAC
void audioLoop();
bool alarmSdEof(bool failed=false); // audio-owner callback only; the UI task queues the next file
bool alarmDraftError();
uint8_t alarmDraftHour();
uint8_t alarmDraftMinute();
bool alarmDraftEnding();
time_t alarmEndTime(); // UI task only: active window deadline, otherwise zero
uint32_t alarmLastOccurrence(); // UI task only, includes an aborted daily attempt
bool alarmRequestValid(uint32_t token);
bool alarmCleanupValid(uint32_t token); // a queued cleanup must not stop later manual playback
void alarmStopRequested(); // accepted ordinary STOP from UI, serial or web
void alarmTakeoverRequested(); // manual source/volume/transport keeps user's value
void track(const char *path);
void metadataArtist(const char *s);
void metadataAlbum(const char *s);
void metadataTitle(const char *s);
void imageHint(size_t pos,size_t size);
void clockConfigure(const char* server=nullptr);
uint32_t clockSyncCount();
void displayEvent(int type,int payload);
Snapshot snapshot();
Settings settingsSnapshot();
void prepareTrackText(Snapshot& s); // presentation copy only; never alters decoder state
serviceui::State serviceSettings();
bool serviceDiagnostics();
bool debugEnabled();
bool uiOverloadActive();
bool menuAwaitingBtHeadroom(); // display task only
bool sourcePendingSd();
const char* sourceRouteError();
void send(Action action);
bool webMenuSupported(unsigned page,unsigned item);
bool webMenuActivate(unsigned page,unsigned item);
bool webStationPlay(uint16_t station);
enum class R10DiagUiAction : uint8_t { RotCW, RotCCW, Short, Long, Double, Menu };
bool r10DiagUi(R10DiagUiAction action);
bool r10UiSelfcheck(); // exercises the real menu input state machine, without saving settings
bool r10UiStressStart(unsigned actionMs,unsigned drawMs,unsigned tour=0);
void r10UiStressStop(const char* reason);
void r10UiStressDrain();
void r10UiStressStatus();
bool r10UiStressInfo(uint16_t& actionMs,uint16_t& drawMs,uint32_t& actions,uint32_t& frames);
void r10DiagRecoverHome();
unsigned r10DiagPage();
unsigned r10DiagSelected();
bool favorite(uint16_t station);
unsigned favoritesCount();
uint16_t nextFavorite(uint16_t station,bool forward);
void invalidateFavorites();
const char* stationSearchQuery();
unsigned stationSearchCount();
uint16_t stationSearchResult(unsigned index);
bool stationSearchResultName(unsigned index,char* out,size_t cap);
unsigned stationSearchStatus();
bool preloadStationSearch();
void invalidateStationSearchCache();
unsigned memoryStressStatus();
bool backupRestoreArmed();
}
