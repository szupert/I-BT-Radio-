#include "../dual/service_session.h"
#ifndef player_h
#define player_h
#include "../dual/stream_recovery.h"

#if I2S_DOUT!=255 || I2S_INTERNAL
  #include "../audioI2S/AudioEx.h"
#else
  #include "../audioVS1053/audioVS1053Ex.h"
#endif

#ifndef MQTT_BURL_SIZE
  #define MQTT_BURL_SIZE  512
#endif

#ifndef PLQ_SEND_DELAY
  #define PLQ_SEND_DELAY pdMS_TO_TICKS(1000) //portMAX_DELAY
#endif

enum playerRequestType_e : uint8_t { PR_PLAY = 1, PR_STOP = 2, PR_PREV = 3, PR_NEXT = 4, PR_VOL = 5, PR_CHECKSD = 6, PR_VUTONUS = 7, PR_BURL = 8, PR_TOGGLE = 9, PR_TONE = 10, PR_BALANCE = 11, PR_GAIN = 12, PR_SEEK = 13, PR_BT = 14, PR_SOURCE = 15, PR_RESTART_LOCAL = 16, PR_BT_TRANSPORT = 17, PR_ALARM_STOP = 18, PR_STATION_SAVE = 20 };
struct playerRequestParams_t
{
  playerRequestType_e type;
  int payload;
  uint32_t alarmToken=0; // zero means an ordinary/manual command
  uint32_t queuedAtUs=0; // diagnostic wall time, copied with the command
  bool naturalSd=false; // explicit EOF intent; negative payloads also serve diagnostics/alarms
};

enum plStatus_e : uint8_t{ PLAYING = 1, STOPPED = 2 };
enum class SdPrepareState : uint8_t { Idle, Indexing, Ready, Failed, Cancelled };

// No File/FILE pointer crosses a task boundary. The audio owner publishes
// this small value object; UI/network readers only copy it under a short mux.
struct PlaybackProgress {
    uint32_t generation=0,position=0,size=0,elapsed=0,duration=0,dataStart=0,sampledAt=0;
    bool local=false,running=false;
};

class Player: public Audio {
  private:
    dual::StreamRecovery _recovery;
    uint32_t    _volTicks;   /* delayed volume save  */
    bool        _volTimer;   /* delayed volume save  */
    uint32_t    _resumeFilePos;
    bool sdNaturalQueued=false;
    uint32_t sdFirstDecodeStarted=0;
    plStatus_e  _status;
    //char        _plError[PLERR_LN];
  private:
    void _stop(bool alreadyStopped = false);
    void _play(uint16_t stationId, bool persistSdSelection=true,bool persistLifecycle=true,uint32_t alarmToken=0);
    void _loadVol(uint8_t volume);
    bool serviceTick();
    bool bootSoundTick(); // startup-only, same audio owner, bounded blocks
    bool _hasError;
  public:
    bool lockOutput = true;
    bool resumeAfterUrl = false;
    volatile bool connproc = true;
    uint32_t sd_min, sd_max;
    #ifdef MQTT_ROOT_TOPIC
    char      burl[MQTT_BURL_SIZE];  /* buffer for browseUrl  */
    #endif
  public:
    Player();
    void init();
    bool startTask();
    bool armBootSound(uint8_t mode,uint8_t theme,uint16_t autoplayStation); // before startTask only
    void audioDiagnostics();
    void* audioTaskHandle(); // persistent audio owner; compare only, never dereference from UI
    uint32_t audioGapEvents();
    uint32_t playbackGeneration() const;
    bool onAudioTask();
    PlaybackProgress progressSnapshot() const;
    void publishProgress(); // audio owner only, max2Hz plus lifecycle transitions
    uint32_t getFilePos();
    uint32_t getFileSize();
    uint32_t getAudioDataStartPos();
    uint32_t getAudioFileDuration();
    uint32_t getAudioCurrentTime();
    bool serviceStart(serviceui::State settings); // mailbox only; existing audio owner executes
    void serviceStop();
    void alarmToneLevel(uint8_t percent);
    struct ServiceStatus {serviceui::Stage stage=serviceui::Stage::Idle;serviceui::Failure fault=serviceui::Failure::None;serviceui::State settings{};uint64_t frames=0;uint32_t generation=0;uint8_t denied=0;};
    ServiceStatus serviceStatus();
    bool serviceActive();

    bool routeSourceChange(int mode);
    void setTone(int8_t bass,int8_t mid,int8_t treble);
    void requestGraphicEq();
    void setBalance(int8_t balance);
    void setVolume(uint8_t volume);
    bool setFilePos(uint32_t position);
    void loop();
    void initHeaders(const char *file);
    void setError();
    void setError(const char *e);
    //bool hasError() { return strlen(_plError)>0; }
    bool sendCommand(playerRequestParams_t request);
    unsigned queueDepth() const;
    uint32_t commandDrops() const;
    SdPrepareState sdPrepareState() const;
    void resetQueue();
    #ifdef MQTT_ROOT_TOPIC
    void browseUrl();
    #endif
    bool remoteStationName = false;
    plStatus_e status() { return _status; }
    void prev();
    void next(bool naturalSd=false);
    void toggle();
    void stepVol(bool up);
    void setVol(uint8_t volume);
    uint8_t volToI2S(uint8_t volume);
    void stopInfo();
    void setOutputPins(bool isPlaying);
    void setResumeFilePos(uint32_t pos) { _resumeFilePos = pos; }
};

extern Player player;

extern __attribute__((weak)) void player_on_start_play();
extern __attribute__((weak)) void player_on_stop_play();
extern __attribute__((weak)) void player_on_track_change();
extern __attribute__((weak)) void player_on_station_change();

#endif
