#include "dual/memory_checkpoint.h"
#include "dual/mp3_offload.h"
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_log.h>
#include "dual/deferred_audio.h"
#include "dual/audio_path_probe.h"
#include "dual/v026_probe.h"
#include "Arduino.h"
#include "core/options.h"
#include "dual/resource_limiter.h"
#include "core/config.h"
#include "pluginsManager/pluginsManager.h"
#include "core/telnet.h"
#include "core/player.h"
#include "core/sdmanager.h"
#include "core/display.h"
#include "core/network.h"
#include "core/netserver.h"
#include "core/controls.h"
//#include "core/mqtt.h"
#include "core/optionschecker.h"
#include "core/timekeeper.h"
#include "dual/core.h"
#include "dual/boot_progress.h"
#include "dual/connectivity_policy.h"
#include "dual/r10_diag.h"
#include "dual/bt_audio.h"
#include "dual/sensors/receiver.h"
#if defined(BOARD_CYD28)
namespace front { bool earlyBoot(); void rgbBootTestTick(); }
#endif
#ifdef USE_NEXTION
#include "displays/nextion.h"
#endif

#if USE_OTA
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
#include <NetworkUdp.h>
#else
#include <WiFiUdp.h>
#endif
#include <ArduinoOTA.h>
#endif

#if DSP_HSPI || TS_HSPI || VS_HSPI
SPIClass  SPI2(HSPI);
#endif

extern __attribute__((weak)) void ibt_on_setup();

#if USE_OTA
void setupOTA(){
  if(strlen(config.store.mdnsname)>0)
    ArduinoOTA.setHostname(config.store.mdnsname);
#ifdef OTA_PASS
  ArduinoOTA.setPassword(OTA_PASS);
#endif
  ArduinoOTA
    .onStart([]() {
      player.sendCommand({PR_STOP, 0});
      display.putRequest(NEWMODE, UPDATING);
      telnet.printf("Start OTA updating %s\n", ArduinoOTA.getCommand() == U_FLASH?"firmware":"filesystem");
    })
    .onEnd([]() {
      telnet.printf("\nEnd OTA update, Rebooting...\n");
      ESP.restart();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      telnet.printf("Progress OTA: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      telnet.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        telnet.printf("Auth Failed\n");
      } else if (error == OTA_BEGIN_ERROR) {
        telnet.printf("Begin Failed\n");
      } else if (error == OTA_CONNECT_ERROR) {
        telnet.printf("Connect Failed\n");
      } else if (error == OTA_RECEIVE_ERROR) {
        telnet.printf("Receive Failed\n");
      } else if (error == OTA_END_ERROR) {
        telnet.printf("End Failed\n");
      }
    });
  ArduinoOTA.begin();
}
#endif

void setup() {
#if IBT_NO_PSRAM
  pinMode(16,OUTPUT);digitalWrite(16,HIGH); // Existing PSRAM CS stays deselected in the no-PSRAM profile.
#endif
#if defined(BOARD_CYD28)
  front::earlyBoot();
#endif
  // UART interrupt drains diagnostic bursts without stalling the audio loop.
  Serial.setTxBufferSize(4096);
  Serial.begin(115200);
#ifdef NETSERVER_LOOP1
  netserver.bindServiceTask();
#endif
  Serial.printf("[BOOT-RESET] reason=%d\n",int(esp_reset_reason()));
  ibtlimit::begin();
  lightmem::checkpoint("EARLY");
  Serial.printf("[I-BT-Radio] firmware=%s diagnostics=V027 branding=OWN\n",IBT_FW_VERSION);
#if defined(BOARD_CYD28) && IBT_BOOT_RGB_TEST
  Serial.println("[RGB-TEST] ST7789 rotation=1 320x240 RED/GREEN/BLUE interval_ms=3000");
  front::rgbBootTestTick();
  return; // Exclusive display diagnostic: no secondary renderer or radio tasks.
#endif
  v026::early();
  // Normal decoding is inline. The optional diagnostic split already has an
  // explicit `dqprepare` command; do not reserve its unused internal stack and
  // queues before Wi-Fi/setup, where the HTTP memory gate needs that headroom.
  Serial.println("[MP3-BALANCE] diagnostic_worker=LAZY default=INLINE prepare=EXPLICIT_ONLY");
  if(REAL_LEDBUILTIN!=255) pinMode(REAL_LEDBUILTIN, OUTPUT);
  if (ibt_on_setup) ibt_on_setup();
  pm.on_setup();
  config.init();
#if IBT_STREAM_ART_DIAG
  config.store.play_mode=PM_WEB;config.store.smartstart=1;
  config.store.showweather=false;config.store.vumeter=false;
  Serial.printf("[DIAG-SERVICES] mode=STREAM_ART_ONLY reserve=%u AUDIO_GUARD_MS=500 EQ=OFF MONO=OFF BT=OFF BLE=OFF SD=OFF WEBUI=OFF NTP=OFF WEATHER=OFF VU=OFF CONTROLS=PLAY_PAUSE_VOLUME_PREV_NEXT SETTINGS_SAVE=OFF\n",unsigned(ibtlimit::Policy::reserve));
#endif
  lightmem::checkpoint("CONFIG");
  bootprogress::reached(bootprogress::Stage::Config);
#if FEATURE_BARE_TOUCH_WIFI
  // Bare-board test must connect to the known Wi-Fi list without waiting for SD mode.
  // Keep radio auto-play disabled because no external DAC is attached.
  config.store.play_mode = PM_WEB;
  config.store.smartstart = 2;
#endif
  display.init();
  lightmem::checkpoint("DISPLAY");
  bootprogress::reached(bootprogress::Stage::Hardware);
audiod::begin();
#if !IBT_STREAM_ART_DIAG
  pathprobe::begin();
#endif
player.init();
  lightmem::checkpoint("PLAYER_INIT");
  bootprogress::reached(bootprogress::Stage::Audio);
  r10diag::begin();
  bootprogress::reached(bootprogress::Stage::Network);
  network.begin();
  lightmem::checkpoint("WIFI");
  if(network.offlinePlayer()){
    network.service(); // Association has returned; same network owner completes shutdown.
    config.store.play_mode=dual::snapshot().settings.networkMode==2?PM_WEB:PM_SDCARD;
  }
  Serial.println("[BT-SPP] START=DEFERRED reason=RADIO_PRIORITY A2DP=OFF");
  if (network.status != CONNECTED && network.status!=SDREADY) {
    netserver.begin();
  lightmem::checkpoint("HTTP");
#if !IBT_STREAM_ART_DIAG
    initControls();
#endif
    display.putRequest(DSP_START);
    while(!display.ready()) delay(10);
    player.startTask();
    bootprogress::ready();
    ESP_LOGI("IBT-BOOT","[BOOT-READY] firmware=%s setup=1",IBT_FW_VERSION);
    return;
  }
  if(SDC_CS!=255) {
    display.putRequest(WAITFORSD, 0);
    Serial.print("##[BOOT]#\tSD search\t");
  }
  config.initPlaylistMode();
  bootprogress::reached(bootprogress::Stage::Library);
  // Lite: no optional PSRAM search cache.
  // dual::preloadStationSearch(); // Before auto-play: subsequent menu searches read PSRAM only.
  netserver.begin();
  lightmem::checkpoint("HTTP");
#if !IBT_STREAM_ART_DIAG
  telnet.begin();
  initControls();
#endif
  display.putRequest(DSP_START);
  while(!display.ready()) delay(10);
  #if USE_OTA
    setupOTA();
  #endif
  if (config.getMode()==PM_SDCARD) player.initHeaders(config.station.url);
  player.lockOutput=false;
  pm.on_end_setup();
  bootprogress::reached(bootprogress::Stage::Controls);
  const auto bootSettings=dual::snapshot().settings;
  const bool autoplay=connectivity::bootAutoplay(bootSettings.networkMode,bootSettings.alarm.clockHome,config.store.smartstart==1)&&config.lastStation()>0;
  bool soundArmed=false;
  if(!IBT_STREAM_ART_DIAG&&bootSettings.bootSound&&bootSettings.networkMode==0&&!bootSettings.alarm.clockHome&&network.status==CONNECTED&&WiFi.status()==WL_CONNECTED){
    if(bootSettings.bootSound==2&&!sdman.ready)sdman.start(); // Before the audio owner exists; no index worker.
    soundArmed=player.armBootSound(bootSettings.bootSound,bootprogress::artwork(bootSettings.bootScreen,dual::playerSkinId(bootSettings)),autoplay?config.lastStation():0);
  }
  const bool audioReady=player.startTask();
  lightmem::checkpoint("AUDIO_TASK");
  if (autoplay&&!soundArmed) {
    const bool accepted=audioReady && player.sendCommand({PR_PLAY, config.lastStation()});
    Serial.printf("[BOOT-PLAY] station=%u audio_ready=%u queued=%u\n",
      unsigned(config.lastStation()),unsigned(audioReady),unsigned(accepted));
  }
  bootprogress::ready();
  ESP_LOGI("IBT-BOOT","[BOOT-READY] firmware=%s setup=1",IBT_FW_VERSION);
}

#include "dual/scheduling_policy.h"
#include "audioI2S/net_prefetch.h"

void loop() {
#if defined(BOARD_CYD28) && IBT_BOOT_RGB_TEST
  front::rgbBootTestTick();
  delay(10);
  return;
#endif
#if IBT_STREAM_ART_DIAG
  audiod::service(); // Required stream metadata handoff to the album worker.
  ibtlimit::tick(false);v026::tick();network.service();dual::audioLoop();
  static uint32_t diagAt=0,diagMinimum=UINT32_MAX;
  const uint32_t diagFree=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  if(diagFree<diagMinimum)diagMinimum=diagFree;
  if(uint32_t(millis()-diagAt)>=1000){diagAt=millis();
    Serial.printf("[DIAG-MEM] free=%u largest=%u loop_sample_min=%u reserve=%u playing=%u input=%u capacity=%u\n",unsigned(diagFree),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(diagMinimum),unsigned(ibtlimit::Policy::reserve),unsigned(player.isRunning()),unsigned(player.inBufferFilled()),unsigned(player.inBufferFilled()+player.inBufferFree()));
  }
  vTaskDelay(pdMS_TO_TICKS(2));return;
#endif
  netprefetch::service();
  sensors::service();
  if(sensors::quiescent())btaudio::service();
  audiod::service();
  pathprobe::service();
  ibtlimit::tick(btaudio::webBlocked());
  timekeeper.loop1();
  v026::tick();
  network.service();
  telnet.loop();
  if (network.status == CONNECTED || network.status==SDREADY) {
    // The dedicated core-1 task is the only decoder/I2S owner.
#if USE_OTA
    ArduinoOTA.handle();
#endif
  }
  loopControls();
  dual::audioLoop();
  #ifdef NETSERVER_LOOP1
  netserver.loop();
  #endif
  // Audio no longer blocks this loop. Arduino's dual-core loopTask does not
  // sleep automatically: explicitly let IDLE0 and background work run.
  // This delay is on core 0 only, never on the dedicated audio owner.
  vTaskDelay(pdMS_TO_TICKS(ibtsched::serviceDelayMs(
    ibtsched::active(btaudio::outputSelected(),btaudio::transmitting()))));
}

#include "core/audiohandlers.h"
