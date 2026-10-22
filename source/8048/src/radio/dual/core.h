#include "service_model.h"
#pragma once
#include "logic.h"
namespace dual {
bool alarmFallbackValid();
struct Snapshot {
    char device[40]={}; char station[170]={}; char artist[170]={};
    char title[170]={}; char album[170]={}; char ip[20]={};char notice[100]={};
    uint32_t generation=0, elapsed=0, duration=0, sampleRate=0, buffer=0, bufferCapacity=0, freeHeap=0, largestHeap=0, freePsram=0;
    uint32_t queueDrops=0, inputDrops=0, lowBufferEvents=0;
    int rssi=0; uint16_t bitrate=0;uint8_t volume=0;
    int8_t bass=0,middle=0,treble=0,balance=0;
    bool playing=false, sd=false, connected=false, pendingSave=false, ready=false, alarmActive=false;
    uint16_t sensorMv=0; bool sensorValid=false;
    Settings settings;
};
struct WifiPasswordUi {
    char ssid[33]={};
    char password[64]={}; // never rendered or written to a diagnostic log
    char error[48]={};
    uint8_t mode=0;
    bool submitted=false;
};
const WifiPasswordUi& wifiPasswordUi();
char wifiPasswordKeyChar(unsigned mode,unsigned key);
uint8_t dnsDraftOctet(unsigned index);
unsigned dnsEditOctet();
void begin();
void uiLoop();
unsigned sensorCalibrationPoint();
void bluetoothMode(int mode); // 1 phone input, 0 scan/output, -1 radio DAC
void audioLoop();
bool alarmSdEof(bool failed=false); // audio-owner callback only; the UI task queues the next file
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
void clockConfigure();
void displayEvent(int type,int payload);
Snapshot snapshot();
serviceui::State serviceSettings();
bool serviceDiagnostics();
bool debugEnabled();
bool uiOverloadActive();
bool sourcePendingSd();
void send(Action action);
bool webMenuSupported(unsigned page,unsigned item);
bool webMenuActivate(unsigned page,unsigned item);
bool webStationPlay(uint16_t station);
enum class R10DiagUiAction : uint8_t { RotCW, RotCCW, Short, Long, Double, Menu };
bool r10DiagUi(R10DiagUiAction action);
bool r10UiSelfcheck(); // exercises the real menu input state machine, without saving settings
bool r10UiStressStart(unsigned actionMs,unsigned drawMs,unsigned tour=0);
void r10UiStressStop(const char* reason);
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
