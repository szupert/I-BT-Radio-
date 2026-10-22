#include "../dual/station_save_control.h"
#include "../dual/sensors/receiver.h"
#include "../dual/weather.h"
#include "../dual/bt_rate_policy.h"
#include "../dual/v026_probe.h"
#include "options.h"
#include "../dual/light_network_gate.h"
#include "player.h"
#include "config.h"
#include "network.h"
#include "telnet.h"
#include "display.h"
#include "sdmanager.h"
#include "netserver.h"
#include "timekeeper.h"
#include "../displays/tools/l10n.h"
#include "../pluginsManager/pluginsManager.h"
#include "../dual/core.h"
#include "../dual/surface_policy.h"
#include "../dual/bt_audio.h"
#include "../dual/bt_source_policy.h"
#include "../dual/scheduling_policy.h"
#include "../dual/mp3_offload.h"
#include "../dual/owner_yield_policy.h"
#include <atomic>
#include <new>
#include <esp_heap_caps.h>
#include "../audioI2S/direct_rx_probe.h"
#include "../dual/boot_progress.h"
#include "../dual/boot_sound_policy.h"
#ifdef USE_NEXTION
#include "../displays/nextion.h"
#endif
Player player;
QueueHandle_t playerQueue;
static std::atomic<TaskHandle_t> audioOwner{nullptr};
static std::atomic<bool> audioTaskStarted{false};
static std::atomic<uint32_t> audioGapMax{0};
static std::atomic<uint32_t> audioGapEventCount{0};
static std::atomic<uint32_t> playbackGenerationCount{0};
// Rotary steps never occupy the five-slot command queue. The audio owner
// drains them together so a fast turn cannot reject station/BT commands.
static std::atomic<int> pendingVolumeSteps{0};
static std::atomic<int> pendingAbsoluteVolume{-1};
static std::atomic<uint32_t> rejectedCommands{0};
static std::atomic<bool> pendingVuMaintenance{false};
// Periodic card checks are coalesced, including while setup has no consumer.
static std::atomic<bool> pendingSdCheck{false};
// The SD scanner owns the card while the audio task remains responsive to
// stop, BT and source commands. State 2/3 is consumed once by the audio task.
static std::atomic<uint8_t> sdWorkerState{0}; // 0 idle, 1 indexing, 2 ready, 3 failed
static std::atomic<SdPrepareState> sdPrepare{SdPrepareState::Idle};
static void sdIndexTask(void*){
#ifdef USE_SD
  const bool good=sdman.start() && sdman.indexSDPlaylist();
  sdWorkerState.store(good?2:3);
#else
  sdWorkerState.store(3);
#endif
  vTaskDelete(nullptr);
}
static bool owner(){return !audioTaskStarted.load()||audioOwner.load()==xTaskGetCurrentTaskHandle();}
static portMUX_TYPE progressMux=portMUX_INITIALIZER_UNLOCKED;
static PlaybackProgress publishedProgress;
PlaybackProgress Player::progressSnapshot() const {
  portENTER_CRITICAL(&progressMux);const auto copy=publishedProgress;portEXIT_CRITICAL(&progressMux);return copy;
}
void Player::publishProgress(){
  if(!owner())return;
  static uint32_t sampledAt=0,seenGeneration=~0u;
  static bool seenRunning=false,seenLocal=false;
  const uint32_t now=millis(),generation=playbackGeneration();
  const bool running=isRunning(),local=config.getMode()==PM_SDCARD;
  if(generation==seenGeneration&&running==seenRunning&&local==seenLocal&&uint32_t(now-sampledAt)<500)return;
  PlaybackProgress next;next.generation=generation;next.sampledAt=now;next.local=local;next.running=running;
  // Read decoder/File state only here, on the same owner that closes it.
  if(running){next.elapsed=Audio::getAudioCurrentTime();next.duration=Audio::getAudioFileDuration();
    if(local){next.position=Audio::getFilePos();next.size=Audio::getFileSize();next.dataStart=Audio::getAudioDataStartPos();}}
  portENTER_CRITICAL(&progressMux);publishedProgress=next;portEXIT_CRITICAL(&progressMux);
  sampledAt=now;seenGeneration=generation;seenRunning=running;seenLocal=local;
}
uint32_t Player::getFilePos(){return owner()?Audio::getFilePos():progressSnapshot().position;}
uint32_t Player::getFileSize(){return owner()?Audio::getFileSize():progressSnapshot().size;}
uint32_t Player::getAudioDataStartPos(){return owner()?Audio::getAudioDataStartPos():progressSnapshot().dataStart;}
uint32_t Player::getAudioFileDuration(){return owner()?Audio::getAudioFileDuration():progressSnapshot().duration;}
uint32_t Player::getAudioCurrentTime(){return owner()?Audio::getAudioCurrentTime():progressSnapshot().elapsed;}

static void audioTask(void*){
  // Publish from the running task, before any command can reach the decoder.
  audioOwner.store(xTaskGetCurrentTaskHandle());
  Serial.printf("[AUDIO-TASK] core=%d priority=3 dma=8x512\n",xPortGetCoreID());
  unsigned currentPriority=3;
  uint32_t previous=micros(),yieldAt=millis();
  for(;;){
    uint32_t now=micros(),gap=now-previous;previous=now;
    if(player.isRunning()){
      if(gap>audioGapMax.load())audioGapMax.store(gap);
      if(gap>100000)audioGapEventCount.fetch_add(1);
    }
    const unsigned desiredPriority=ibtsched::audioPriority(
      ibtsched::active(btaudio::outputSelected(),btaudio::producerReady()));
    if(currentPriority!=desiredPriority){
      vTaskPrioritySet(nullptr,desiredPriority);currentPriority=desiredPriority;
    }
    // R46: diagnostic offload defaults are set at session boundaries only.
    player.loop();
    player.publishProgress();
    // The I2S writer normally blocks naturally. Yield briefly even on fast
    // network/error paths so the core's idle task can run its watchdog.
    const uint32_t yieldPeriod=owneryield::period(
      btaudio::outputSelected()&&btaudio::producerReady(),owneryield::requested().load(),
      owneryield::bufferedWebMp3(config.getMode()==PM_WEB,player.isRunning(),
        player.getCodec(),btaudio::negotiatedOutputRate(),player.inBufferFilled()));
    owneryield::applied().store(yieldPeriod);
    if(owneryield::due(millis(),yieldAt,yieldPeriod)){const bool measure=mp3offload::timingActive();const uint32_t start=measure?micros():0;vTaskDelay(1);if(measure)mp3offload::recordYield(micros()-start);yieldAt=millis();}
  }
}
// Service mailbox: UI never calls output drivers or decoder directly.
static portMUX_TYPE serviceMux=portMUX_INITIALIZER_UNLOCKED;
static serviceui::Session serviceSession;
static serviceui::State serviceRequested;
static Player::ServiceStatus servicePublished;
static bool serviceStartPending=false,serviceStopPending=false;
static std::atomic<uint8_t> alarmTonePercent{10};
void Player::alarmToneLevel(uint8_t percent){alarmTonePercent.store(percent>100?100:percent);}
static std::atomic<bool> serviceOwned{false};
static btaudio::ServiceLease serviceLease;
static bool serviceDacReady=false,serviceRestoreAck=false;
static playerRequestParams_t serviceDeferred{};
static bool serviceDeferredValid=false;
static uint32_t serviceGeneration=0;
static bool surfaceServiceAllowed(const serviceui::State& settings){
 return settings.alarmPattern?settings.route==serviceui::Route::Dac&&dual::alarmFallbackValid():surfacepolicy::current()==surfacepolicy::Surface::Service;
}
bool Player::serviceStart(serviceui::State settings){
  if(stationsave::holdsAudio()||!surfaceServiceAllowed(settings))return false;
  portENTER_CRITICAL(&serviceMux);
  const bool allowed=!serviceOwned.load()&&!serviceStartPending&&!serviceStopPending;
  if(allowed){serviceRequested=settings;serviceRequested.running=true;serviceStartPending=true;}
  portEXIT_CRITICAL(&serviceMux);return allowed;
}
void Player::serviceStop(){
  portENTER_CRITICAL(&serviceMux);serviceStopPending=true;serviceStartPending=false;portEXIT_CRITICAL(&serviceMux);
}
Player::ServiceStatus Player::serviceStatus(){
  portENTER_CRITICAL(&serviceMux);const auto copy=servicePublished;portEXIT_CRITICAL(&serviceMux);return copy;
}
bool Player::serviceActive(){return serviceOwned.load();}
bool Player::serviceTick(){
  if(!onAudioTask())return false;
  bool start=false,stop=false;serviceui::State desired;
  portENTER_CRITICAL(&serviceMux);
  // R45: ordinary playback does not execute the diagnostic service engine.
  // Inspect pending requests under their publishing lock: a concurrent START
  // or STOP is either consumed now or remains pending for the next audio loop.
  // Restoration, faults and deferred player commands must still be serviced.
  if(!serviceStartPending && !serviceStopPending &&
     serviceSession.stage==serviceui::Stage::Idle &&
     serviceSession.effect==serviceui::Effect::None &&
     !serviceDeferredValid && !serviceOwned.load()){
    portEXIT_CRITICAL(&serviceMux);
    return false;
  }
  start=serviceStartPending;stop=serviceStopPending;desired=serviceRequested;
  serviceStartPending=serviceStopPending=false;
  portEXIT_CRITICAL(&serviceMux);
  uint8_t denied=0;const uint32_t now=millis();
  if(stop){
    if(serviceSession.stage==serviceui::Stage::Fault){
      Audio::stopSong();btaudio::flush();serviceSession.clearFault(!isRunning());
    }else serviceSession.restore(now);
  }
  if(start&&!stop&&surfaceServiceAllowed(desired)&&(!desired.alarmPattern||dual::alarmFallbackValid())){
    const bool routeReady=desired.route==serviceui::Route::Bt?btaudio::producerReady():!btaudio::wanted()&&btaudio::idle();
    if(serviceSession.stage!=serviceui::Stage::Idle||sdWorkerState.load()!=0||queueDepth()!=0||!connproc)denied=1;
    else if(btaudio::inputSelected())denied=2;
    else if(!routeReady)denied=3;
    else {
      if(desired.route==serviceui::Route::Bt)desired.rate=btaudio::negotiatedOutputRate();
      serviceui::Snapshot before;
      before.station=config.lastStation();before.volume=config.store.volume;before.sd=config.getMode()==PM_SDCARD;
      before.playing=desired.alarmPattern?false:isRunning();before.route=btaudio::outputSelected()?serviceui::Route::Bt:serviceui::Route::Dac;
      before.rate=getSampleRate();
      before.savedSdResume=config.sdResumePos;before.savedResume=_resumeFilePos;
      const uint32_t pos=before.sd?getFilePos():0,unread=before.sd?inBufferFilled():0;
      before.filePosition=before.playing?(pos>=unread?pos-unread:0):(config.sdResumePos?config.sdResumePos:_resumeFilePos);
      // Explicit uncalibrated bench mode: these are digital bounds, not physical accuracy claims.
      const uint32_t maxHz=desired.rate>2?std::min<uint32_t>(20000,desired.rate/2-1):0;
      if(!denied&&!serviceSession.begin(desired,{maxHz,false,true},before,now,true,false))denied=4;
      if(!denied){serviceGeneration++;serviceDacReady=false;serviceRestoreAck=false;serviceDeferredValid=false;}
    }
  }
  if(serviceSession.stage!=serviceui::Stage::Idle){
    pendingVuMaintenance.store(false);
  pendingSdCheck.store(false);
  sdNaturalQueued=false;
  pendingVolumeSteps.store(0);pendingAbsoluteVolume.store(-1);
    // Local STOP/source/station commands cancel generation, then execute after restore.
    playerRequestParams_t command;
    for(unsigned n=0;n<5&&xQueueReceive(playerQueue,&command,0)==pdPASS;++n){
      if(!serviceDeferredValid||serviceDeferred.type!=PR_STOP){serviceDeferred=command;serviceDeferredValid=true;}
      serviceSession.restore(now);
    }
    if(serviceSession.stage==serviceui::Stage::Fault&&serviceDeferredValid){
      Audio::stopSong();btaudio::flush();serviceSession.clearFault(!isRunning());
    }
  }
  const auto effect=serviceSession.takeEffect();
  if(effect==serviceui::Effect::StopPlayer){
    _recovery.cancel();const bool oldLock=lockOutput;lockOutput=true;_stop();lockOutput=oldLock;
    const auto before=serviceSession.original;
    config.sdResumePos=before.sd?before.filePosition:before.savedSdResume;
    _resumeFilePos=before.sd?before.filePosition:before.savedResume;
    btaudio::flush();
  }else if(effect==serviceui::Effect::PrepareOutput){
    if(serviceSession.settings.route==serviceui::Route::Dac)
      serviceDacReady=Audio::prepareServicePcm(serviceSession.settings.rate);
    else if(!btaudio::beginServicePcm(serviceLease))serviceSession.restore(now,serviceui::Failure::LinkLost);
    if(serviceSession.settings.route==serviceui::Route::Dac&&!serviceDacReady)
      serviceSession.restore(now,serviceui::Failure::WriteContract);
  }else if(effect==serviceui::Effect::RestoreOriginal){
    Audio::stopSong();btaudio::flush();serviceLease.valid=false;
    const auto before=serviceSession.original;
    config.setVolume(before.volume);_loadVol(before.volume);
    config.sdResumePos=before.sd?before.filePosition:before.savedSdResume;
    _resumeFilePos=before.sd?before.filePosition:before.savedResume;
    if(before.route==serviceui::Route::Dac&&serviceDacReady)
      Audio::prepareServicePcm(before.rate);
    serviceDacReady=false;
    if(before.playing&&!serviceDeferredValid){
      const bool oldLock=lockOutput;lockOutput=true;
      _play(before.station,false,false);lockOutput=oldLock;
      serviceRestoreAck=isRunning();
    }else serviceRestoreAck=true;
  }else if(effect==serviceui::Effect::Mute){
    Audio::stopSong();btaudio::flush();serviceLease.valid=false;serviceDacReady=false;
  }
  const bool bt=serviceSession.settings.route==serviceui::Route::Bt;
  const bool outputReady=bt?(btaudio::producerReady()&&serviceLease.valid):serviceDacReady;
  const bool restoreRouteReady=serviceSession.original.route==serviceui::Route::Bt?btaudio::producerReady():(!btaudio::wanted()&&btaudio::idle());
  serviceSession.tick(now,{true,!isRunning(),outputReady,serviceRestoreAck&&restoreRouteReady,bt&&!btaudio::producerReady()});
  if(serviceSession.settings.alarmPattern)serviceSession.osc.gain=int16_t(10362u*alarmTonePercent.load()/100u);
  if(serviceSession.stage==serviceui::Stage::Running&&serviceSession.prepare()){
    const auto write=bt?btaudio::writeServicePcm(serviceSession.pending(),serviceSession.count,serviceLease):
      Audio::writeServicePcm(serviceSession.pending(),serviceSession.count,serviceSession.settings.rate);
    if(write.fault)serviceSession.restore(now,serviceui::Failure::WriteContract);
    else serviceSession.accepted(write.frames,now);
  }
  const bool busy=serviceSession.stage!=serviceui::Stage::Idle;
  serviceOwned.store(busy);
  portENTER_CRITICAL(&serviceMux);
  servicePublished.stage=serviceSession.stage;servicePublished.fault=serviceSession.failure;
  servicePublished.settings=serviceSession.settings;servicePublished.frames=serviceSession.acceptedFrames;
  servicePublished.generation=serviceGeneration;if(start)servicePublished.denied=denied;
  portEXIT_CRITICAL(&serviceMux);
  if(start||stop||effect!=serviceui::Effect::None)
    Serial.printf("[SERVICE] stage=%u effect=%u fault=%u denied=%u rate=%lu frequency=%lu accepted=%llu bench=1\n",
      unsigned(serviceSession.stage),unsigned(effect),unsigned(serviceSession.failure),unsigned(denied),
      (unsigned long)serviceSession.settings.rate,(unsigned long)serviceSession.settings.hz,
      (unsigned long long)serviceSession.acceptedFrames);
  if(!busy&&serviceDeferredValid){auto command=serviceDeferred;serviceDeferredValid=false;sendCommand(command);}
  return busy;
}
#include "boot_sound.inc"
bool Player::startTask(){
  if(audioTaskStarted.exchange(true))return audioOwner.load()!=nullptr;
  if(playerQueue==nullptr || xTaskCreatePinnedToCore(audioTask,"ibt_audio",20480,nullptr,3,nullptr,1)!=pdPASS){
    // Keep ownership closed on failure: no fallback decoder on the UI task.
    Serial.println("[AUDIO-TASK] FAIL allocation; playback disabled");
    return false;
  }
  return true;
}
bool Player::onAudioTask(){return audioOwner.load()==xTaskGetCurrentTaskHandle();}
bool Player::routeSourceChange(int mode){
  if(!audioTaskStarted.load() || onAudioTask())return false;
  sendCommand({PR_SOURCE,mode});return true;
}
void* Player::audioTaskHandle(){return audioOwner.load();}
void Player::audioDiagnostics(){
  directrx::dacDiagnostics();dacHardwareDiagnostics();
  auto task=audioOwner.load();
  Serial.printf("[AUDIO-TASK] ready=%u core=1 service_core=0 stack_min=%u max_running_gap_us=%lu gap_events_100ms=%lu dma_frames=4096\n",task?1:0,task?unsigned(uxTaskGetStackHighWaterMark(task)):0,(unsigned long)audioGapMax.load(),(unsigned long)audioGapEventCount.load());
}
uint32_t Player::audioGapEvents(){return audioGapEventCount.load();}
uint32_t Player::playbackGeneration() const {return playbackGenerationCount.load();}
void Player::setTone(int8_t b,int8_t m,int8_t t){
  if(owner())Audio::setTone(b,m,t);
  else sendCommand({PR_TONE,int(uint8_t(b))|(int(uint8_t(m))<<8)|(int(uint8_t(t))<<16)});
}
void Player::requestGraphicEq(){} // Winamp EQ is visual only; never schedule audio DSP.
void Player::setBalance(int8_t b){if(owner())Audio::setBalance(b);else sendCommand({PR_BALANCE,b});}
void Player::setVolume(uint8_t v){if(owner())Audio::setVolume(v);else sendCommand({PR_GAIN,v});}
bool Player::setFilePos(uint32_t p){if(owner())return Audio::setFilePos(p);sendCommand({PR_SEEK,int(p)});return true;}


#if VS1053_CS!=255 && !I2S_INTERNAL
  #if VS_HSPI
    Player::Player(): Audio(VS1053_CS, VS1053_DCS, VS1053_DREQ, &SPI2) {}
  #else
    Player::Player(): Audio(VS1053_CS, VS1053_DCS, VS1053_DREQ, &SPI) {}
  #endif
  void ResetChip(){
    pinMode(VS1053_RST, OUTPUT);
    digitalWrite(VS1053_RST, LOW);
    delay(30);
    digitalWrite(VS1053_RST, HIGH);
    delay(100);
  }
#else
  #if !I2S_INTERNAL
    Player::Player() {}
  #else
    Player::Player(): Audio(true, I2S_DAC_CHANNEL_BOTH_EN)  {}
  #endif
#endif


void Player::init() {
  Serial.print("##[BOOT]#\tplayer.init\t");
  Serial.printf("[PLAYER-AUDIO] enabled=%d v48=1\n", (int)FEATURE_AUDIO_OUTPUT); // RADIO_PLAYER_AUDIO_FIX_V48_20260910
  playerQueue=NULL;
  pendingSdCheck.store(false);
  pendingVolumeSteps.store(0);
  pendingAbsoluteVolume.store(-1);
  _resumeFilePos = 0;
  _hasError=false;
  playerQueue = xQueueCreate( 5, sizeof( playerRequestParams_t ) );
  setOutputPins(false);
  delay(50);
#ifdef MQTT_ROOT_TOPIC
  memset(burl, 0, MQTT_BURL_SIZE);
#endif
  if(MUTE_PIN!=255) pinMode(MUTE_PIN, OUTPUT);
  #if I2S_DOUT!=255
    #if !I2S_INTERNAL && FEATURE_AUDIO_OUTPUT
      setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    #elif !FEATURE_AUDIO_OUTPUT
      Serial.println("##[BOOT]#\taudio output disabled (bare touch/Wi-Fi test)");
    #endif
  #else
    SPI.begin();
    if(VS1053_RST>0) ResetChip();
    begin();
  #endif
  setBalance(config.store.balance);
  setTone(config.store.bass, config.store.middle, config.store.trebble);
  setVolume(0);
  _status = STOPPED;
  _volTimer=false;
  //randomSeed(analogRead(0));
  #if PLAYER_FORCE_MONO
    forceMono(true);
  #endif
  _loadVol(config.store.volume);
  setConnectionTimeout(CONNECTION_TIMEOUT, CONNECTION_TIMEOUT_SSL);
  Serial.println("done");
}

static bool surfaceRequestAllowed(const playerRequestParams_t& request){
    using namespace surfacepolicy;
    const auto surface=current();
    const bool alarm=request.alarmToken&&dual::alarmRequestValid(request.alarmToken);
    if(request.type==PR_BT){
        if(request.payload<0)return true; // Shutdown never blocked.
        const auto route=request.payload==1?Route::BtInDac:Route::SdBt;
        return (request.payload==1||config.getMode()==PM_SDCARD)&&allows(surface,route,alarm);
    }
    const bool starts=request.type==PR_PLAY||request.type==PR_NEXT||request.type==PR_PREV||request.type==PR_TOGGLE||request.type==PR_RESTART_LOCAL||request.type==PR_BT_TRANSPORT||request.type==PR_BURL||request.type==PR_SOURCE;
    if(!starts)return true; // Volume, balance and stop remain available.
    if(btaudio::inputSelected()&&request.type!=PR_SOURCE)return false; // AVRCP is a separate dedicated-skin path.
    const bool sd=request.type==PR_SOURCE?request.payload==PM_SDCARD:request.type==PR_BURL?false:config.getMode()==PM_SDCARD;
    if(btaudio::outputSelected()&&!sd)return false; // Global normal WEB->BT restriction.
    return allows(surface,btaudio::outputSelected()?Route::SdBt:sd?Route::SdDac:Route::WifiDac,alarm);
}
bool Player::sendCommand(playerRequestParams_t request){
  request.queuedAtUs=micros();
  if(request.type==PR_STOP)stationsave::manualStop();
  if(stationsave::holdsAudio() && request.type!=PR_STATION_SAVE && request.type!=PR_STOP && request.type!=PR_ALARM_STOP && request.type!=PR_VOL && request.type!=PR_GAIN && request.type!=PR_TONE && request.type!=PR_BALANCE)return false;
  if(!surfaceRequestAllowed(request)){Serial.printf("[SURFACE-GATE] result=DENIED type=%u surface=%s\n",unsigned(request.type),surfacepolicy::name(surfacepolicy::current()));return false;}
  if(network.offlinePlayer()&&(request.type==PR_BURL||
      (config.getMode()==PM_WEB&&(request.type==PR_PLAY||request.type==PR_NEXT||request.type==PR_PREV||request.type==PR_TOGGLE))))return false;
  if(playerQueue==NULL) return false;
  if(request.type==PR_VUTONUS){pendingVuMaintenance.store(true);return true;}
  if(request.type==PR_CHECKSD){pendingSdCheck.store(true);return true;}
  // Keep room for stop/source/BT requests when a web client sends repeated
  // station commands. The newest user request may be retried; old work is lost.
  const bool expendable=request.type==PR_PLAY||request.type==PR_NEXT||
    request.type==PR_PREV||request.type==PR_TOGGLE||request.type==PR_CHECKSD;
  if(!owner()&&expendable&&uxQueueMessagesWaiting(playerQueue)>=3){
    rejectedCommands.fetch_add(1);
    return false;
  }
  if(request.type==PR_PLAY||request.type==PR_NEXT||request.type==PR_PREV||request.type==PR_TOGGLE||request.type==PR_SOURCE||request.type==PR_BT||request.type==PR_BT_TRANSPORT||request.type==PR_BURL||request.type==PR_RESTART_LOCAL){sensors::audioRequested();weather::audioRequested();}
  if(xQueueSend(playerQueue, &request, owner()?0:pdMS_TO_TICKS(10))==pdPASS){
#if IBT_NO_PSRAM
    if(request.type==PR_STOP||request.type==PR_PLAY||request.type==PR_NEXT||request.type==PR_PREV||request.type==PR_TOGGLE||request.type==PR_SOURCE||request.type==PR_BT||request.type==PR_BURL||request.type==PR_RESTART_LOCAL)lightnet::gate().cancel();
#endif
    if(request.type==PR_STOP&&!request.alarmToken)dual::alarmStopRequested();
    else if(!request.alarmToken&&(request.type==PR_PLAY||request.type==PR_SOURCE||
        request.type==PR_BT||request.type==PR_TOGGLE||request.type==PR_VOL||
        request.type==PR_SEEK||request.type==PR_RESTART_LOCAL))dual::alarmTakeoverRequested();
    if(request.type==PR_SOURCE && request.payload==PM_SDCARD && sdWorkerState.load()==0)
      sdPrepare.store(SdPrepareState::Idle);
    return true;
  }
  rejectedCommands.fetch_add(1);
  Serial.printf("[AUDIO-QUEUE] FULL type=%d payload=%d depth=%u command rejected\n",int(request.type),int(request.payload),unsigned(uxQueueMessagesWaiting(playerQueue)));
  return false;
}
unsigned Player::queueDepth() const {return playerQueue?uxQueueMessagesWaiting(playerQueue):0;}
uint32_t Player::commandDrops() const {return rejectedCommands.load();}
SdPrepareState Player::sdPrepareState() const {return sdPrepare.load();}

void Player::resetQueue(){
  if(playerQueue!=NULL) xQueueReset(playerQueue);
  pendingSdCheck.store(false);
  pendingVolumeSteps.store(0);
  pendingAbsoluteVolume.store(-1);
}

void Player::stopInfo() {
  config.setSmartStart(0);
  netserver.requestOnChange(MODE, 0);
}

void Player::setError(){
  _hasError=true;
  config.setTitle(config.tmpBuf);
  telnet.printf("##ERROR#:\t%s\n", config.tmpBuf);
}

void Player::setError(const char *e){
  strlcpy(config.tmpBuf, e, sizeof(config.tmpBuf));
  setError();
}

void Player::_stop(bool alreadyStopped){
  sdFirstDecodeStarted=0;
  log_i("%s called", __func__);
  if(config.getMode()==PM_SDCARD && !alreadyStopped) config.sdResumePos = player.getFilePos();
  _status = STOPPED;
  setOutputPins(false);
  if(!_hasError) config.setTitle((display.mode()==LOST || display.mode()==UPDATING)?"":LANG::const_PlStopped);
  config.station.bitrate = 0;
  config.setBitrateFormat(BF_UNKNOWN);
  #ifdef USE_NEXTION
    nextion.bitrate(config.station.bitrate);
  #endif
  setDefaults(false,!IBT_NO_PSRAM);
  if(!alreadyStopped) stopSong();
  netserver.requestOnChange(BITRATE, 0);
  display.putRequest(DBITRATE);
  display.putRequest(PSTOP);
  //setDefaults();
  //if(!alreadyStopped) stopSong();
  if(!lockOutput) stopInfo();
  if (player_on_stop_play) player_on_stop_play();
  pm.on_stop_play();
}

void Player::initHeaders(const char *file) {
  if(strlen(file)==0 || true) return; //TODO Read TAGs
  connecttoFS(sdman,file);
  eofHeader = false;
  while(!eofHeader) Audio::loop();
  //netserver.requestOnChange(SDPOS, 0);
  setDefaults();
}
void resetPlayer(){
  if(!config.store.watchdog) return;
  player.resetQueue();
  player.sendCommand({PR_STOP, 0});
  player.loop();
}

#ifndef PL_QUEUE_TICKS
  #define PL_QUEUE_TICKS 0
#endif
#ifndef PL_QUEUE_TICKS_ST
  #define PL_QUEUE_TICKS_ST 15
#endif
void Player::loop() {
  if(playerQueue==NULL || !owner()) return;
  if(bootSoundTick())return;
  if(pendingVuMaintenance.exchange(false)&&config.vuThreshold>10)config.vuThreshold-=10;
  static bool inLoop=false;
  if(inLoop)return; // SD indexing must not recursively service decoder commands.
  struct Guard {bool& value;Guard(bool& v):value(v){value=true;}~Guard(){value=false;}} guard(inLoop);
  static bool btStarted=false,returnToDac=false,sourcePaused=false;
  static uint32_t returnToDacAt=0;
  static bool sdWanted=false,sdResumeWeb=false,sdKeepBt=false;
  static uint16_t sdWebStation=0;
  static uint32_t sdBtStopAt=0;
  static int deferredBt=-1000,deferredPlay=-1000,deferredSource=-1;
  static bool sourceResumeAfterRetry=false;
  static uint32_t sdSourceAlarmToken=0;
  static bool serviceHandled=false;
  if(stationsave::holdsAudio())serviceStop();
  if(serviceTick()){serviceHandled=true;return;}
  if(serviceHandled){btStarted=btaudio::outputSelected()&&isRunning();sourcePaused=!isRunning();returnToDac=false;serviceHandled=false;}
  // Keep reading controls immediately, but combine volume events before
  // updating the decoder, display and web clients at most every 100 ms.
  static uint32_t nextVolumeApply=0;
  const uint32_t volumeNow=millis();
  if(int32_t(volumeNow-nextVolumeApply)>=0 &&
     (pendingVolumeSteps.load()!=0 || pendingAbsoluteVolume.load()>=0)){
    const int absolute=pendingAbsoluteVolume.exchange(-1);
    const int steps=pendingVolumeSteps.exchange(0);
    int next=(absolute>=0?absolute:int(config.store.volume))+steps*max(1,int(config.store.volsteps));
    if(next<0)next=0;
    if(next>254)next=254;
    if(next!=config.store.volume){
      _volTicks=volumeNow;_volTimer=true;
      config.setVolume(uint8_t(next));
      Audio::setVolume(volToI2S(uint8_t(next)));
    }
    nextVolumeApply=volumeNow+100;
  }
  auto applyBt=[&](int mode){
      if(!surfaceRequestAllowed({PR_BT,mode})){Serial.println("[SURFACE-GATE] action=CANCEL_DEFERRED_BT");return;}
    if(mode==0&&config.getMode()==PM_WEB){
      Serial.println("[BT-ROUTE] result=DENIED source=WEB target=BT_OUT keep=DAC");
      return;
    }
    sourcePaused=false;
    // Capture on the audio owner before stop resets decoder metadata.
    const unsigned localRate=config.getMode()==PM_SDCARD?getSampleRate():0;
    _recovery.cancel();_stop();btStarted=false;
    if(mode==0&&config.getMode()==PM_SDCARD&&btaudio::idle()&&!btaudio::wanted()){
        const unsigned target=btrate::native(localRate,btaudio::outputRate());
        const bool selected=btaudio::setDiagnosticRate(target); // RAM only, no NVS write during audio
        Serial.printf("[BT-LOCAL-RATE] source_hz=%u target_hz=%u result=%s\n",localRate,target,selected?"SELECTED":"DENIED");
    }
    returnToDac=mode<0;
    if(returnToDac)returnToDacAt=millis();
#if IBT_NO_PSRAM
    // _stop joined the producer and freed decoder/input buffers on this owner.
    // The unused DAC DMA must not consume the controller's startup workspace.
    if(mode==0&&btaudio::idle()&&!btaudio::wanted()){
#ifdef USE_SD
      if(!sdman.releaseIdlePath()){Serial.println("[BT-STARTUP] request=DENIED reason=sd_path_not_quiescent");return;}
#endif
      if(!suspendDacForBtSearch()){Serial.println("[BT-STARTUP] request=DENIED reason=dac_release");return;}
    }
#endif
    btaudio::request(returnToDac?btaudio::Mode::Off:mode==1?btaudio::Mode::In:btaudio::Mode::Out);
    const auto source=config.getMode()==PM_SDCARD?ibtsource::Source::Sd:ibtsource::Source::Web;
    if(source!=ibtsource::afterBt(source,mode))config.changeMode(PM_WEB);
    _loadVol(config.store.volume);
  };
  if(btaudio::takeAutoDacFallback()){
    _recovery.cancel();_stop();btStarted=false;
    returnToDac=true;returnToDacAt=millis();
    btaudio::request(false);
    Serial.println("[BT-LINK] auto_dac=REQUESTED");
  }
  playerRequestParams_t requestP;
  // Invalidate an in-flight alarm index operation before its completion can
  // select SD or resume playback. New manual requests keep their own lifetime.
  if(sdSourceAlarmToken&&!dual::alarmRequestValid(sdSourceAlarmToken)){
    if(sdWorkerState.load()!=0){sdWanted=false;sdResumeWeb=false;sdman.cancelIndex();}
    sdSourceAlarmToken=0;
  }
  bool haveRequest=xQueueReceive(playerQueue, &requestP, (isRunning()||btaudio::wanted())?PL_QUEUE_TICKS:PL_QUEUE_TICKS_ST)==pdPASS;
  // User commands take precedence; one card check remains pending until idle.
  if(!haveRequest && pendingSdCheck.exchange(false)){
    requestP={PR_CHECKSD,0};haveRequest=true;
  }
  if(haveRequest){
    const bool naturalRequest=requestP.type==PR_PLAY&&requestP.naturalSd;
    const bool naturalValid=sdNaturalQueued&&config.getMode()==PM_SDCARD;
    if(naturalRequest||requestP.type==PR_STOP||requestP.type==PR_ALARM_STOP||requestP.type==PR_SOURCE||requestP.type==PR_BT||requestP.type==PR_TOGGLE||requestP.type==PR_NEXT||requestP.type==PR_PREV||requestP.type==PR_PLAY)sdNaturalQueued=false;
    if(naturalRequest&&!naturalValid){Serial.println("[SD-QUEUE] result=CANCELLED_SUPERSEDED");requestP.type=playerRequestType_e(0);}
    if(requestP.type==PR_PLAY&&config.getMode()==PM_SDCARD)Serial.printf("[SD-QUEUE] wait_us=%lu natural=%u\n",(unsigned long)(micros()-requestP.queuedAtUs),requestP.naturalSd?1:0);
    const bool allowed=requestP.type==PR_ALARM_STOP?dual::alarmCleanupValid(requestP.alarmToken):
        (!requestP.alarmToken||dual::alarmRequestValid(requestP.alarmToken));
    if(!allowed){
      Serial.printf("[ALARM-QUEUE] result=CANCELLED type=%u\n",unsigned(requestP.type));
      requestP.type=playerRequestType_e(0);
    }
    if(!surfaceRequestAllowed(requestP)){
      Serial.printf("[SURFACE-GATE] result=CANCEL_QUEUED type=%u\n",unsigned(requestP.type));requestP.type=playerRequestType_e(0);
    }
    // The owner can dequeue before the sender returns from xQueueSend. Apply
    // manual ownership here too, before any later queued alarm cleanup.
    if(!requestP.alarmToken&&(requestP.type==PR_PLAY||requestP.type==PR_SOURCE||
        requestP.type==PR_BT||requestP.type==PR_TOGGLE||requestP.type==PR_VOL||
        requestP.type==PR_SEEK||requestP.type==PR_RESTART_LOCAL))dual::alarmTakeoverRequested();
    if(stationsave::holdsAudio() && requestP.type!=PR_STATION_SAVE && requestP.type!=PR_STOP && requestP.type!=PR_ALARM_STOP && requestP.type!=PR_VOL && requestP.type!=PR_GAIN && requestP.type!=PR_TONE && requestP.type!=PR_BALANCE)requestP.type=playerRequestType_e(0);
    switch (requestP.type){
      case PR_STATION_SAVE:
        if(stationsave::status().job!=uint32_t(requestP.payload)||stationsave::status().phase!=stationsave::WaitingStop)break;
        stationsave::ownerCapture(uint32_t(requestP.payload),config.getMode(),config.lastStation(),
            (isRunning()||status()==PLAYING)&&!btaudio::wanted(),config.station.url);
        dual::alarmStopRequested();
        if(sdWorkerState.load()!=0){sdWanted=false;sdResumeWeb=false;sdman.cancelIndex();}
        deferredBt=-1000;deferredPlay=-1000;deferredSource=-1;sourceResumeAfterRetry=false;
        sourcePaused=true;returnToDac=false;sdNaturalQueued=false;
        _recovery.cancel();_stop();btaudio::request(false);btStarted=false;
        stationsave::ownerStopped(uint32_t(requestP.payload));
        Serial.printf("[STATION-SAVE] owner_stop_ack bootstrap=1 lease=3 job=%lu\n",(unsigned long)uint32_t(requestP.payload));
        break;
      case PR_CANCEL_SOURCE:
        // Cancelling preparation is neither WEB playback nor a BT disconnect.
        if(sdWorkerState.load()!=0){sdWanted=false;sdResumeWeb=false;sdman.cancelIndex();}
        deferredSource=-1;deferredPlay=-1000;sourceResumeAfterRetry=false;
        break;
      case PR_ALARM_STOP:
      case PR_STOP:
        if(sdWorkerState.load()!=0){sdWanted=false;sdResumeWeb=false;sdman.cancelIndex();}
        deferredBt=-1000;deferredPlay=-1000;deferredSource=-1;sourceResumeAfterRetry=false;
        returnToDac=false;btaudio::request(false); _recovery.cancel(); _stop(); break;
      case PR_BT_TRANSPORT:
        if(!btaudio::outputSelected()||sdWorkerState.load()!=0||!btaudio::producerReady())break;
        if(isRunning()){sourcePaused=true;_recovery.cancel();_stop();btaudio::flush();}
        else {sourcePaused=false;btaudio::flush();_recovery.begin(config.lastStation());_play(config.lastStation(),false,false);}
        btStarted=true;
        Serial.printf("[BT-TRANSPORT] playing=%u route_preserved=1\n",unsigned(isRunning()));break;
      case PR_BT:
        sourcePaused=false;
        deferredSource=-1;sourceResumeAfterRetry=false;
        if(requestP.payload<0&&!btaudio::wanted()&&btaudio::idle())break;
        if(sdWorkerState.load()!=0){sdWanted=false;sdResumeWeb=false;sdman.cancelIndex();deferredBt=requestP.payload;break;}
        applyBt(requestP.payload);
        break;
      case PR_SOURCE: {
        const auto target=playMode_e(requestP.payload);
        if(target!=PM_WEB&&target!=PM_SDCARD)break;
        if(target==PM_WEB&&network.offlinePlayer()){
          // Existing Back/timeout uses WEB as the SD cancellation command.
          // Preserve cancellation without selecting or starting the network source.
          if(sdWorkerState.load()!=0){sdWanted=false;sdResumeWeb=false;sdman.cancelIndex();}
          break;
        }
        if(target==PM_WEB&&btaudio::outputSelected()){
          Serial.println("[SOURCE-SWITCH] result=DENIED source=WEB route=BT_OUT select_DAC_first=1");
          break;
        }
        // Phone input owns the decoder/output. Source switching is local playback only.
        if(btaudio::inputSelected()){
          Serial.println("[SOURCE-SWITCH] result=DENIED bt_input=1");break;
        }
        returnToDac=false;
        if(sdWorkerState.load()!=0){
          if(requestP.alarmToken){dual::alarmStopRequested();break;}
          deferredSource=target;
          if(target==PM_WEB){sdWanted=false;sdman.cancelIndex();}
          break;
        }
        if(target==config.getMode()&&!(target==PM_SDCARD&&!sdman.catalogReady.load()))break;
        const bool resume=isRunning()||sourceResumeAfterRetry;
        sourceResumeAfterRetry=false;
        sdKeepBt=btaudio::outputSelected();sourcePaused=!resume;
        _recovery.cancel();_stop();dual::track("");
        if(sdKeepBt)btaudio::flush();
        btStarted=false;
        Serial.printf("[SOURCE-SWITCH] target=%s route=%s resume=%u\n",
          target==PM_SDCARD?"SD":"WEB",sdKeepBt?"BT":"DAC",unsigned(resume));
        if(target==PM_SDCARD){
          sdSourceAlarmToken=requestP.alarmToken;
          sdWebStation=config.lastStation();sdResumeWeb=resume;sdWanted=true;
          sdBtStopAt=millis();
          sdman.clearIndexCancel();sdWorkerState.store(1);sdPrepare.store(SdPrepareState::Indexing);
          if(xTaskCreatePinnedToCore(sdIndexTask,"ibt_sd_index",6144,nullptr,1,nullptr,0)!=pdPASS){
            sdWorkerState.store(3);
            Serial.println("[SD-AUDIO] FAIL index task allocation");
          }else Serial.println("[SD-AUDIO] INDEX=STARTED audio_task=FREE");
        }else{
          config.changeMode(PM_WEB);
          if(resume&&(!btaudio::outputSelected()||btaudio::producerReady())){
            _recovery.begin(config.lastStation());_play(config.lastStation());
            btStarted=btaudio::outputSelected()&&isRunning();
          }
        }
        break;
      }
      case PR_TONE: Audio::setTone(int8_t(requestP.payload),int8_t(requestP.payload>>8),int8_t(requestP.payload>>16));break;
      case PR_BALANCE: Audio::setBalance(int8_t(requestP.payload));break;
      case PR_GAIN: Audio::setVolume(uint8_t(requestP.payload));break;
      case PR_SEEK: Audio::setFilePos(uint32_t(requestP.payload));break;
      case PR_RESTART_LOCAL: {
        if(!ibtsource::canRestartLocal(config.getMode()==PM_SDCARD,
            sdWorkerState.load()!=0,btaudio::inputSelected(),
            btaudio::outputSelected(),btaudio::producerReady())){
          Serial.println("[LOCAL-RESTART] result=DENIED");break;
        }
        const uint16_t track=config.lastStation();
        returnToDac=false;_recovery.cancel();_stop();
        // _stop captures the current file position, so clear it afterwards.
        config.sdResumePos=0;_resumeFilePos=0;
        if(btaudio::outputSelected())btaudio::flush();
        _play(track);
        // The generic BT startup branch must not reopen this file a second time.
        btStarted=btaudio::outputSelected();
        Serial.printf("[LOCAL-RESTART] result=%s station=%u generation=%lu\n",
          isRunning()?"PASS":"FAIL",unsigned(track),(unsigned long)playbackGeneration());
        break;
      }
      case PR_PLAY: {
        deferredSource=-1;sourceResumeAfterRetry=false;
        sourcePaused=false;
        if(sdWorkerState.load()!=0){
          if(requestP.alarmToken){dual::alarmStopRequested();break;}
          sdWanted=false;sdResumeWeb=false;sdman.cancelIndex();deferredPlay=requestP.payload;break;
        }
        if (requestP.payload>0&&!requestP.alarmToken) {
          config.setLastStation((uint16_t)requestP.payload);
        }
        if(btaudio::inputSelected()){btaudio::request(false);returnToDac=true;break;}
        if(btaudio::outputSelected()){
          btaudio::flush();if(!btaudio::producerReady())break;
        }
        _recovery.begin(config.getMode()==PM_WEB?(uint16_t)abs(requestP.payload):0);
        if(config.getMode()==PM_SDCARD && requestP.payload<0){config.sdResumePos=0;_resumeFilePos=0;}
        _play((uint16_t)abs(requestP.payload),requestP.payload>=0,!requestP.alarmToken&&!requestP.naturalSd,requestP.alarmToken);
        if(requestP.alarmToken&&!dual::alarmRequestValid(requestP.alarmToken))break;
        if (player_on_station_change) player_on_station_change(); 
        pm.on_station_change();
        break;
      }
      case PR_TOGGLE: {
        deferredSource=-1;sourceResumeAfterRetry=false;
        sourcePaused=false;
        if(sdWorkerState.load()!=0){sdWanted=false;sdResumeWeb=false;sdman.cancelIndex();break;}
        toggle();
        break;
      }
      case PR_VOL:
        if(requestP.alarmToken){
          // Alarm gain precedes its queued PLAY on this audio owner. Do not
          // put it in the 100 ms manual-volume mailbox or persist ramp steps.
          const uint8_t volume=uint8_t(constrain(requestP.payload,0,254));
          config.setVolume(volume);Audio::setVolume(volToI2S(volume));
        }else pendingAbsoluteVolume.store(requestP.payload);
        break;
      #ifdef USE_SD
      case PR_CHECKSD: {
        if(config.getMode()==PM_SDCARD){
          if(!sdman.cardPresent()){
            // Card loss can switch the selected source to WEB while BT OUT is
            // still selected. Close that route before changing the source.
            if(btaudio::outputSelected())btaudio::request(false);
            sdman.stop();
            config.changeMode(PM_WEB);
          }
        }
        break;
      }
      #endif
      case PR_VUTONUS: {
        if(config.vuThreshold>10) config.vuThreshold -=10;
        break;
      }
      case PR_BURL: {
        if(network.offlinePlayer())break;
      #ifdef MQTT_ROOT_TOPIC
        if(strlen(burl)>0){
          browseUrl();
        }
      #endif
        break;
      }
          
      default: break;
    }
  }
  bool sourceRetryQueued=false;
  const uint8_t sdDone=sdWorkerState.load();
  if(sdDone>=2 && sdWanted && !sdKeepBt && !btaudio::idle() &&
     uint32_t(millis()-sdBtStopAt)>=10000){
    sdWanted=false;sdResumeWeb=false;
    Serial.println("[SD-AUDIO] FAIL BT teardown timeout; audio remains stopped");
  }
  if(sdDone>=2 && (!sdWanted || sdKeepBt || btaudio::idle())){
    sdSourceAlarmToken=0;
    sdWorkerState.store(0);
    bool ready=sdDone==2 && sdWanted;
    if(ready){
      config.changeMode(PM_SDCARD);
      ready=config.getMode()==PM_SDCARD && sdman.catalogReady.load();
    }
    if(ready){
      sdPrepare.store(SdPrepareState::Ready);
      Serial.printf("[SD-AUDIO] INDEX=PASS tracks=%lu\n",(unsigned long)sdman.indexedTracks.load());
      if(sdResumeWeb&&(!btaudio::outputSelected()||btaudio::producerReady())){
        _recovery.begin(0);_play(config.lastStation());
        btStarted=btaudio::outputSelected()&&isRunning();
      }
    }else{
      if(sdman.ready)sdman.stop();
      sdPrepare.store(sdWanted?SdPrepareState::Failed:SdPrepareState::Cancelled);
      Serial.printf("[SD-AUDIO] INDEX=%s\n",sdWanted?"FAIL":"CANCELLED");
      if(deferredBt!=-1000)applyBt(deferredBt);
      else if(deferredPlay!=-1000){
        if(deferredPlay>0)config.setLastStation(uint16_t(deferredPlay));
        _recovery.begin(uint16_t(abs(deferredPlay)));_play(uint16_t(abs(deferredPlay)));
        if(player_on_station_change)player_on_station_change();
        pm.on_station_change();
      }
      else if(deferredSource==PM_SDCARD){
        // WEB then SD during cancellation: wait for the old card owner, then
        // retry the newest source once without reopening the cancelled WEB.
        sourceResumeAfterRetry=sdResumeWeb;
        sourceRetryQueued=sendCommand({PR_SOURCE,PM_SDCARD});
        if(!sourceRetryQueued){sourceResumeAfterRetry=false;sdPrepare.store(SdPrepareState::Failed);Serial.println("[SOURCE-SWITCH] FAIL retry queue full");}
      }
      else if(sdResumeWeb&&(!btaudio::outputSelected()||btaudio::producerReady())){
        _recovery.begin(sdWebStation);_play(sdWebStation);
        btStarted=btaudio::outputSelected()&&isRunning();
      }
    }
    sdWanted=false;sdResumeWeb=false;deferredBt=-1000;deferredPlay=-1000;deferredSource=-1;
    if(sourceRetryQueued)return;
  }
  // The index worker owns SD until it publishes completion. Keep commands
  // responsive, but never reopen the old source through generic BT startup.
  if(sdWorkerState.load()!=0)return;
  if(stationsave::holdsAudio()){
    returnToDac=false; // Discard late BT fallback intent while this save owns the route.
    // No decoder/SD/BT resource is reused until each old owner is quiescent.
    if(stationsave::ownerMayFinishStop()&&!isRunning()&&status()==STOPPED&&!btaudio::wanted()&&btaudio::idle()){
      if(config.getMode()!=PM_WEB)config.changeMode(PM_WEB);
      if(config.getMode()==PM_WEB)stationsave::ownerReady();
    }
    stationsave::Resume resume;
    if(stationsave::ownerTakeFinish(resume)){
      bool selected=resume.mode==PM_WEB;
#ifdef USE_SD
      if(resume.mode==PM_SDCARD)selected=sdman.ready&&sdman.catalogReady.load();
#endif
      if(selected&&config.getMode()!=resume.mode)config.changeMode(resume.mode);
      const bool play=selected&&resume.play&&surfaceRequestAllowed({PR_PLAY,int(resume.track)});
      if(selected&&!play&&resume.mode==PM_WEB)config.loadStation(resume.track?resume.track:1,false,false);
      stationsave::ownerReleased(play);sourcePaused=!play;
      Serial.printf("[STATION-SAVE] owner_release resume_requested=%u station=%u\n",unsigned(play),unsigned(resume.track));
      if(play){_recovery.begin(resume.mode==PM_WEB?resume.track:0);_play(resume.track,false,false);}
    }
    return;
  }
  if(returnToDac){
    if(!btaudio::idle()&&uint32_t(millis()-returnToDacAt)<10000)return;
    if(!btaudio::idle()){
      // Do not start another decoder against a non-quiescent Bluetooth host.
      returnToDac=false;setError("BT leallasi hiba: ujrainditas szukseges");
      Serial.println("[BT-LINK] teardown_timeout=1 DAC_RESTORE=DENIED");return;
    }
    returnToDac=false;_recovery.begin(config.lastStation());_play(config.lastStation());
  }
  if(btaudio::inputSelected()){
#if IBT_NO_PSRAM
    if(btaudio::connected()&&!restoreDacAfterBtSearch()){
      btaudio::request(false);returnToDac=true;returnToDacAt=millis();return;
    }
#endif
    uint8_t pcm[1024];uint32_t rate=44100;
    auto n=btaudio::pcm(pcm,sizeof(pcm),rate);
    if(n)Audio::writePcm16(pcm,n,rate);
    return;
  }
  if(btaudio::outputSelected()){
    if(sourcePaused)return;
    if(!btaudio::producerReady()){
      if(isRunning())_stop();
      btStarted=false;return; // Free decoder RAM during pairing; no busy decoding.
    }
    if(!btStarted){btStarted=true;_recovery.begin(config.lastStation());_play(config.lastStation());}
  }else btStarted=false;
#if IBT_NO_PSRAM
  // Also restore the idle/paused DAC after a denied/cancelled discovery.
  static uint32_t nextDacRestore=0;
  if(!btaudio::wanted()&&btaudio::idle()&&!isRunning()&&int32_t(millis()-nextDacRestore)>=0){
    nextDacRestore=millis()+1000;restoreDacAfterBtSearch();
  }
#endif
  Audio::loop();
  if(sdFirstDecodeStarted&&hasDecodedAudio()){Serial.printf("[SD-FIRST-DECODED] elapsed_us=%lu generation=%lu\n",(unsigned long)(micros()-sdFirstDecodeStarted),(unsigned long)playbackGeneration());sdFirstDecodeStarted=0;}
  if(btaudio::outputSelected())btaudio::sourceContext(playbackGenerationCount.load(),unsigned(getCodec()),isRunning());
  if(_recovery.alarmToken&&!dual::alarmRequestValid(_recovery.alarmToken))_recovery.cancel();
  // Redirects may reconnect inside Audio, outside Player::_play(). Header time
  // belongs to the existing HTTP timeout, not the PCM low-buffer stall clock.
  // Retry counts/backoff stay intact; only a real AUDIO_DATA stall can age it.
  if(IBT_NO_PSRAM&&streamHeaderPending())_recovery.connectionResult(millis(),true);
  if(_recovery.poll(millis(),config.getMode()==PM_WEB,WiFi.status()==WL_CONNECTED,isRunning(),inBufferFilled(),
                    btaudio::producerReady(),btaudio::producedFrames(),IBT_NO_PSRAM && streamTransportClosed())){
    Serial.printf("[STREAM-RECOVERY] retry reason=%s station=%u buffer=%u produced=%lu\n",
                  _recovery.lastReason==dual::StreamRecovery::Reason::TransportClosed?"TRANSPORT_CLOSED":
                  _recovery.lastReason==dual::StreamRecovery::Reason::ConnectionFailed?"CONNECT_FAILED":
                  _recovery.lastReason==dual::StreamRecovery::Reason::BtNoProduction?"BT_NO_PRODUCTION":"LOW_BUFFER",
                  _recovery.station,(unsigned)inBufferFilled(),(unsigned long)btaudio::producedFrames());
    // The owner of the decoder closes and recreates its own connection.
    // Preserve the user's station and avoid persistent stop/start writes.
    const bool priorLock=lockOutput;lockOutput=true;
    _stop();_play(_recovery.station,true,!_recovery.alarmToken,_recovery.alarmToken);lockOutput=priorLock;
  }
  if(_recovery.takeLimitEvent()){
    sourcePaused=true; // A BT reconnect must not silently rearm exhausted recovery.
    Serial.printf("[STREAM-RECOVERY] exhausted retries=%u station=%u\n",
                  (unsigned)_recovery.retryCount(),(unsigned)_recovery.station);
    const bool priorLock=lockOutput;lockOutput=true;
    _stop();lockOutput=priorLock;
    setError("Stream retry limit. Press Play.");
  }
  if(!isRunning() && _status==PLAYING){
    // An accepted EOF successor is a transient stop, not a user stop. Preserve
    // smart-start until that command succeeds or its normal failure path stops.
    const bool oldLock=lockOutput;if(sdNaturalQueued)lockOutput=true;
    _stop(true);lockOutput=oldLock;
  }
  if(_volTimer){
    if(!isRunning() && !btaudio::wanted() && (millis()-_volTicks)>3000){
      config.saveVolume();
      _volTimer=false;
    }
  }
  /*
#ifdef MQTT_ROOT_TOPIC
  if(strlen(burl)>0){
    browseUrl();
  }
#endif*/
}

void Player::setOutputPins(bool isPlaying) {
  if(REAL_LEDBUILTIN!=255) digitalWrite(REAL_LEDBUILTIN, LED_INVERT?!isPlaying:isPlaying);
  bool _ml = MUTE_LOCK?!MUTE_VAL:(isPlaying?!MUTE_VAL:MUTE_VAL);
  if(MUTE_PIN!=255) digitalWrite(MUTE_PIN, _ml);
}

void Player::_play(uint16_t stationId, bool persistSdSelection,bool persistLifecycle,uint32_t alarmToken) {
  if(!surfacepolicy::allows(surfacepolicy::current(),btaudio::outputSelected()?surfacepolicy::Route::SdBt:config.getMode()==PM_SDCARD?surfacepolicy::Route::SdDac:surfacepolicy::Route::WifiDac,alarmToken&&dual::alarmRequestValid(alarmToken))){_recovery.cancel();return;}
  if(network.offlinePlayer()&&config.getMode()==PM_WEB)return;
#if IBT_NO_PSRAM
  // Existing decoder paths still require I2S; never decode into a missing driver.
  if(!restoreDacAfterBtSearch()){
    _recovery.cancel();setError("DAC memoria nem elegendo");
    if(btaudio::outputSelected())btaudio::request(false);
    return;
  }
#endif
  sensors::audioRequested();weather::audioRequested(); // Also covers recovery and deferred starts; atomic veto only.
  _recovery.alarmToken=alarmToken;
  if(alarmToken&&!dual::alarmRequestValid(alarmToken)){_recovery.cancel();return;}
  playbackGenerationCount.fetch_add(1);
  log_i("%s called, stationId=%d", __func__, stationId);
  _hasError=false;
  const uint32_t sdPlayStarted=micros();
  sdFirstDecodeStarted=config.getMode()==PM_SDCARD?sdPlayStarted:0;
  if(!setDefaults(false,!IBT_NO_PSRAM)){_recovery.cancel();return;}
  const uint32_t sdResetDone=micros();
  _status = STOPPED;
  setOutputPins(false);
  remoteStationName = false;
  
  if(!config.prepareForPlaying(stationId,persistSdSelection,persistLifecycle)){
    if(alarmToken)dual::alarmSdEof(true);
    _recovery.cancel();return;
  }
  // A queued play can outlive its source selection (for example, SD card loss).
  // Recheck the route immediately before opening a WEB station.
  if(config.getMode()==PM_WEB&&btaudio::outputSelected()){
    Serial.println("[BT-ROUTE] result=DENIED source=WEB target=BT_OUT reason=SOURCE_CHANGED");
    btaudio::request(false);_recovery.cancel();return;
  }
  dual::track(config.station.url);
  _loadVol(config.store.volume);
  
  const uint32_t sdPrepareDone=micros();
  bool isConnected = false;
  if(config.getMode()==PM_SDCARD && SDC_CS!=255){
    // getFilePos() and connecttoFS() both use absolute file offsets.
    isConnected=connecttoFSPrepared(sdman,sdman.selectedPath(),config.sdResumePos==0?_resumeFilePos:config.sdResumePos);
    Serial.printf("[SD-PREPARE] reset_us=%lu prepare_us=%lu open_us=%lu total_us=%lu ok=%u\n",(unsigned long)(sdResetDone-sdPlayStarted),(unsigned long)(sdPrepareDone-sdResetDone),(unsigned long)(micros()-sdPrepareDone),(unsigned long)(micros()-sdPlayStarted),isConnected?1:0);
  }else {
    if(persistLifecycle)config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB));
  }
  connproc = false;
  if(config.getMode()==PM_WEB) isConnected=connecttohost(config.station.url);
  if(!isConnected && stationId==65001){
    strlcpy(config.station.url,V026_STREAM_ALT,sizeof(config.station.url));
    Serial.printf("[STREAM-TEST] fallback=%s\n",config.station.url);
    isConnected=connecttohost(config.station.url);
  }
  Serial.printf("[V026-PLAYER] station=%u connect=%d\n",stationId,(int)isConnected);
  if(config.getMode()==PM_WEB){
    _recovery.connectionResult(millis(),isConnected);
    if(!isConnected)Serial.printf("[STREAM-CONNECT] failed=1 station=%u retry_after_ms=%lu attempts=%u\n",stationId,(unsigned long)_recovery.connectionDelay(),unsigned(_recovery.retryCount()));
  }

  connproc = true;
  // Connect may be slow. A stop received during connect must win before the
  // output is enabled or a start notification is published.
  if(alarmToken&&!dual::alarmRequestValid(alarmToken)){
    _recovery.cancel();_stop();
    Serial.println("[ALARM-QUEUE] result=CANCELLED_DURING_CONNECT");return;
  }
  if(isConnected){
    _status = PLAYING;
    const uint32_t notifyStarted=micros();
    config.configPostPlaying(stationId,persistLifecycle,persistSdSelection);
    setOutputPins(true);
    if (player_on_start_play) player_on_start_play();
    pm.on_start_play();
    if(config.getMode()==PM_SDCARD)Serial.printf("[SD-NOTIFY] elapsed_us=%lu persist=%u\n",(unsigned long)(micros()-notifyStarted),persistLifecycle?1:0);
  }else{
    sdFirstDecodeStarted=0;
    telnet.printf("##ERROR#:\tError connecting to %.128s\n", config.station.url);
    if(alarmToken)dual::alarmSdEof(true);
    snprintf(config.tmpBuf, sizeof(config.tmpBuf), "Error connecting to %.128s", config.station.url); setError();
    _stop(true);
  };
}

#ifdef MQTT_ROOT_TOPIC
void Player::browseUrl(){
  _hasError=false;
  remoteStationName = true;
  config.setDspOn(1);
  resumeAfterUrl = _status==PLAYING;
  display.putRequest(PSTOP);
  setOutputPins(false);
  config.setTitle(LANG::const_PlConnect);
  if (connecttohost(burl)){
    _status = PLAYING;
    config.setTitle("");
    netserver.requestOnChange(MODE, 0);
    setOutputPins(true);
    display.putRequest(PSTART);
    if (player_on_start_play) player_on_start_play();
    pm.on_start_play();
  }else{
    telnet.printf("##ERROR#:\tError connecting to %.128s\n", burl);
    snprintf(config.tmpBuf, sizeof(config.tmpBuf), "Error connecting to %.128s", burl); setError();
    _stop(true);
  }
  //memset(burl, 0, MQTT_BURL_SIZE);
}
#endif

void Player::prev() {
  uint16_t lastStation = config.lastStation();
  if(config.getMode()==PM_WEB || !config.store.sdsnuffle){
    if (lastStation == 1) config.lastStation(config.playlistLength()); else config.lastStation(lastStation-1);
  }
  sendCommand({PR_PLAY, config.lastStation()});
}

void Player::next(bool naturalSd) {
  const uint16_t count=config.playlistLength();
  if(!count)return;
  const bool automatic=naturalSd&&config.getMode()==PM_SDCARD&&owner();
  uint16_t lastStation = config.lastStation();
  if(config.getMode()==PM_WEB || !config.store.sdsnuffle){
    if(automatic)lastStation=lastStation>=count?1:lastStation+1;
    else if (lastStation >= count) config.lastStation(1); else config.lastStation(lastStation+1);
  }else{
    const uint16_t chosen=static_cast<uint16_t>(random(1L, static_cast<long>(count) + 1L));
    if(automatic)lastStation=chosen;else config.lastStation(chosen);
  }
  if(automatic)sdNaturalQueued=sendCommand({PR_PLAY,-int(lastStation),0,0,true});
  else sendCommand({PR_PLAY, config.lastStation()});
}

void Player::toggle() {
  if (_status == PLAYING) {
    sendCommand({PR_STOP, 0});
  } else {
    sendCommand({PR_PLAY, config.lastStation()});
  }
}

void Player::stepVol(bool up) {
  dual::alarmTakeoverRequested();
  int prior=pendingVolumeSteps.load();
  for(;;){
    int next=prior+(up?1:-1);
    if(next>254)next=254;
    if(next<-254)next=-254;
    if(pendingVolumeSteps.compare_exchange_weak(prior,next))return;
  }
}

uint8_t Player::volToI2S(uint8_t volume) {
  int vol = map(volume, 0, 254 - config.station.ovol * 3 , 0, 254);
  if (vol > 254) vol = 254;
  if (vol < 0) vol = 0;
  return vol;
}

void Player::_loadVol(uint8_t volume) {
  setVolume(volToI2S(volume));
}

void Player::setVol(uint8_t volume) {
  dual::alarmTakeoverRequested();
  pendingAbsoluteVolume.store(volume);
}

