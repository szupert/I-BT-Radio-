#include "r10_diag.h"
#include "sbc_probe.h"
#include "bt_transport_probe.h"
#include "load_probe.h"
#include "audio_path_probe.h"
#include "../audioI2S/direct_rx_probe.h"
#include "../audioI2S/mp3_decoder/mp3_decoder.h"
#include "vu_probe.h"
#include "bt_audio.h"
#include "sensors/receiver.h"
#include "resource_limiter.h"
#include "input_low_probe.h"
#include "core.h"
#include "../core/options.h"
#include "online_artwork.h"
#include "artwork_ab.h"
#include "mp3_offload.h"
#include "owner_yield_policy.h"
#include "net_poll_policy.h"
#include "../core/config.h"
#include "../core/player.h"
#include "../core/network.h"
#include "../core/display.h"
#ifdef USE_SD
#include "../core/sdmanager.h"
#endif
#include "../core/telnet.h"
#include "v026_probe.h"
#include "../ui/frontend.h"
#include "../ui/light_ui_policy.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

namespace r10diag {
static char line[192];
static size_t lineLen=0;
static bool engaged=false;
static bool saved=false;
static int8_t originalBass=0, originalMiddle=0, originalTreble=0;
static uint8_t originalVolume=0, originalSmartstart=0;
static bool originalLockOutput=false, originalPlaying=false;
static uint16_t originalStation=1, shadowStation=1;
static uint8_t restoreStage=0; static uint32_t restoreAt=0;
static uint32_t eventCount=0, rejected=0, memChecks=0, memErrors=0;
static uint32_t lastTelem=0, lastIntegrity=0;
static bool sdTestActive=false,sdWasPlaying=false;
static uint8_t sdRestoreStage=0;
static uint32_t sdRestoreAt=0;

static void captureOriginal(){
  if(saved)return;
  originalBass=config.store.bass; originalMiddle=config.store.middle; originalTreble=config.store.trebble;
  originalVolume=config.store.volume; originalSmartstart=config.store.smartstart; originalLockOutput=player.lockOutput;
  originalPlaying=player.status()==PLAYING; originalStation=config.lastStation()?config.lastStation():1; shadowStation=originalStation;
  // Stress-safe runtime guard: prevents smart-start and stop-info persistence paths.
  config.store.smartstart=2; player.lockOutput=true; saved=true;
}
static int argInt(const char* s,const char* key,int fallback){
  char needle[32];snprintf(needle,sizeof(needle),"%s=",key);const char* p=strstr(s,needle);if(!p)return fallback;
  return atoi(p+strlen(needle));
}
static void printCaps(){
  Serial.printf("[R10-CAPS] version=1 firmware=%s limiter=1 events=fw_rot_cw,fw_rot_ccw,fw_short,fw_long,fw_double,fw_menu,fw_eq_jitter,fw_memory_churn,fw_net_probe,fw_volume_jitter,fw_station_next,fw_station_prev,fw_audio_toggle,fw_stream_reconnect\n",IBT_FW_VERSION);
}
#if IBT_NO_PSRAM
// Bounded owner-tick output. No UART-sized heap buffer, task, or audio callback IO.
extern "C" unsigned ibt_sbc_wire_byte(unsigned index);
static uint8_t wireOffset=0;
static bool wirePending=false;
static uint8_t transportPending=0;
static uint8_t compactStage=0;
static const char* compactTag="tick";
static const char* pendingTag=nullptr;
static uint32_t compactAt=0;
static void compactTick(){
  if(transportPending&&uint32_t(millis()-compactAt)>=20&&Serial.availableForWrite()>=192){
    char out[192];int n;
    if(transportPending==1)n=snprintf(out,sizeof(out),"[BT-TRANSPORT] l2_ok=%lu l2_congested=%lu l2_failed=%lu acl_calls=%lu media_calls=%lu\n",
      (unsigned long)ibt_tx_get(IBT_L2_OK),(unsigned long)ibt_tx_get(IBT_L2_CONGESTED),(unsigned long)ibt_tx_get(IBT_L2_FAILED),
      (unsigned long)ibt_tx_get(IBT_ACL_TX),(unsigned long)ibt_tx_get(IBT_MEDIA_TX));
    else if(transportPending==2)n=snprintf(out,sizeof(out),"[BT-HCI-TX] submitted=%lu short=%lu bad=%lu fragmented=%lu acl_completed=%lu handle=%lu cid=%lu\n",
      (unsigned long)ibt_tx_get(IBT_MEDIA_SUBMITTED),(unsigned long)ibt_tx_get(IBT_MEDIA_SHORT),(unsigned long)ibt_tx_get(IBT_BAD_FRAME),
      (unsigned long)ibt_tx_get(IBT_MEDIA_FRAGMENT),(unsigned long)ibt_tx_get(IBT_ACL_COMPLETED),
      (unsigned long)ibt_tx_get(IBT_MEDIA_HANDLE),(unsigned long)ibt_tx_get(IBT_MEDIA_CID));
    else n=snprintf(out,sizeof(out),"[BT-HCI-FIRST] bytes=%lu fnv=%08lx rtp_time=%lu scope=CONTROLLER_API_NOT_PEER_ACK\n",
      (unsigned long)ibt_tx_get(IBT_FIRST_BYTES),(unsigned long)ibt_tx_get(IBT_FIRST_HASH),(unsigned long)ibt_tx_get(IBT_FIRST_TIME));
    if(n<=0||size_t(n)>=sizeof(out)){Serial.println("[R10-DIAG] transport=OVERFLOW");transportPending=0;return;}
    Serial.write(reinterpret_cast<const uint8_t*>(out),size_t(n));compactAt=millis();transportPending=transportPending==3?0:transportPending+1;return;
  }
  if(wirePending&&uint32_t(millis()-compactAt)>=20&&Serial.availableForWrite()>=192){
    char out[100];const unsigned length=ibt_sbc_wire_byte(128);
    int n=snprintf(out,sizeof(out),"[BT-SBC-FRAME] len=%u offset=%u hex=",length,unsigned(wireOffset));
    unsigned end=std::min<unsigned>(length,unsigned(wireOffset)+16);
    for(unsigned i=wireOffset;i<end;++i)n+=snprintf(out+n,sizeof(out)-n,"%02x",ibt_sbc_wire_byte(i));
    out[n++]='\n';Serial.write(reinterpret_cast<const uint8_t*>(out),n);
    wireOffset=uint8_t(end);wirePending=length&&end<length;compactAt=millis();return;
  }
  if(!compactStage||uint32_t(millis()-compactAt)<20||Serial.availableForWrite()<192)return;
  char out[192];int n=0;
  if(compactStage==1){
    const unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    n=snprintf(out,sizeof(out),"[R10-TELEM] tag=%s heap_internal=%lu heap_largest=%lu heap_min=%lu psram_free=%u unit=bytes snapshot=PER_LINE\n",compactTag,
      (unsigned long)heap_caps_get_free_size(caps),(unsigned long)heap_caps_get_largest_free_block(caps),
      (unsigned long)heap_caps_get_minimum_free_size(caps),unsigned(ESP.getFreePsram()));
  }else if(compactStage==2){
    n=snprintf(out,sizeof(out),"[R10-STACK] stack_free=%lu dsp_stack_hwm=%lu mem_checks=%lu mem_errors=%lu unit=bytes\n",
      (unsigned long)uxTaskGetStackHighWaterMark(nullptr),(unsigned long)dspTaskStackHighWater(),
      (unsigned long)memChecks,(unsigned long)memErrors);
  }else if(compactStage==3){
    const auto task=static_cast<TaskHandle_t>(player.audioTaskHandle());
    n=snprintf(out,sizeof(out),"[AUDIO-TASK] ready=%u stack_min=%lu stack_alloc=12288 unit=bytes core=1\n",task?1:0,
      task?(unsigned long)uxTaskGetStackHighWaterMark(task):0ul);
  }else if(compactStage==4){
    int c0=-1,c1=-1;const bool known=loadprobe::cached(c0,c1);
    n=snprintf(out,sizeof(out),"[VU-STATE] style=%u dac=%u local_bt=%u left=%u right=%u cpu_valid=%u core0_pm=%d core1_pm=%d\n",unsigned(lightui::style.load()),unsigned(vuprobe::dacEnabled.load()),unsigned(vuprobe::localBtEnabled.load()),unsigned(vuprobe::dacLeft.load()),unsigned(vuprobe::dacRight.load()),unsigned(known),c0,c1);
  }else n=int(ibtlimit::compactTelemetry(out,sizeof(out)));
  if(n<=0||size_t(n)>=sizeof(out)){Serial.println("[R10-DIAG] format=OVERFLOW");compactStage=0;return;}
  if(Serial.availableForWrite()<n)return;
  Serial.write(reinterpret_cast<const uint8_t*>(out),size_t(n));compactAt=millis();
  if(compactStage==5){compactStage=pendingTag?1:0;if(pendingTag){compactTag=pendingTag;pendingTag=nullptr;}}
  else ++compactStage;
}
#endif
static void telemetry(const char* tag){
#if IBT_NO_PSRAM
  // Coalesce periodic requests, but retain an explicit request until emitted.
  if(!compactStage){compactTag=tag?tag:"tick";compactStage=1;}
  else if(tag&&strcmp(tag,"tick"))pendingTag=tag;
  return;
#else
  // Full status + limiter output is under 1200 bytes. Defer if UART is busy.
  if(Serial.availableForWrite()<1200)return;
  const uint32_t freeInt=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  const uint32_t largestInt=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  const uint32_t minInt=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  const uint32_t stackBytes=(uint32_t)uxTaskGetStackHighWaterMark(nullptr);
  const auto s=dual::snapshot(); IPAddress ip=WiFi.status()==WL_CONNECTED?WiFi.localIP():WiFi.softAPIP();
  Serial.printf("[R10-TELEM] tag=%s active=%u events=%lu rejected=%lu heap_internal=%lu heap_largest=%lu heap_min=%lu psram_free=%u stack_free=%lu dsp_stack_hwm=%lu queue_drops=%lu input_drops=%lu audio_command_drops=%lu ui_overload=%u input_buffer=%lu low_buffer=%lu ip=%u.%u.%u.%u mem_checks=%lu mem_errors=%lu page=%u selected=%u audio_playing=%u\n",
    tag?tag:"tick",engaged?1:0,(unsigned long)eventCount,(unsigned long)rejected,
    (unsigned long)freeInt,(unsigned long)largestInt,(unsigned long)minInt,(unsigned)ESP.getFreePsram(),(unsigned long)stackBytes,(unsigned long)dspTaskStackHighWater(),
    (unsigned long)s.queueDrops,(unsigned long)s.inputDrops,(unsigned long)player.commandDrops(),dual::uiOverloadActive()?1:0,
    (unsigned long)s.buffer,(unsigned long)s.lowBufferEvents,
    unsigned(ip[0]),unsigned(ip[1]),unsigned(ip[2]),unsigned(ip[3]),(unsigned long)memChecks,(unsigned long)memErrors,
    dual::r10DiagPage(),dual::r10DiagSelected(),s.playing?1:0);
  ibtlimit::telemetry();
  inputlow::telemetry();
  player.audioDiagnostics();
#endif
}
static bool memoryChurn(const char* args){
  size_t bytes=(size_t)argInt(args,"bytes",4096); int cycles=argInt(args,"cycles",1);
  if(bytes<256)bytes=256;if(bytes>32768)bytes=32768;if(cycles<1)cycles=1;if(cycles>8)cycles=8;
  if(!ibtlimit::acquire(ibtlimit::Job::Diagnostic,bytes+2048))return false;
  bool ok=true;
  for(int c=0;c<cycles;c++){
    uint32_t caps=((eventCount+c)&1)?(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT):(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if((caps&MALLOC_CAP_INTERNAL)&&!ibtlimit::allowAllocation(bytes)){ok=false;break;}
    uint8_t* p=(uint8_t*)heap_caps_malloc(bytes,caps);if(!p){ok=false;memErrors++;break;}
    for(size_t i=0;i<bytes;i++)p[i]=(uint8_t)((i*33u+c*17u+eventCount)&0xffu);
    for(size_t i=0;i<bytes;i+=97)if(p[i]!=(uint8_t)((i*33u+c*17u+eventCount)&0xffu)){ok=false;memErrors++;break;}
    free(p);if(!ok)break;
  }
  ibtlimit::release(ibtlimit::Job::Diagnostic);
  return ok;
}
static uint16_t wrapStation(int v){
  int n=(int)config.playlistLength();if(n<1)return 1;while(v<1)v+=n;while(v>n)v-=n;return (uint16_t)v;
}
static bool playShadow(int delta,bool forceReconnect){
  if(config.getMode()!=PM_WEB || config.playlistLength()<1)return false;
  shadowStation=wrapStation((int)shadowStation+delta);
  if(forceReconnect)player.sendCommand({PR_STOP,0});
  // Negative payload deliberately bypasses persistent last-station update in Player::loop().
  player.sendCommand({PR_PLAY,-(int)shadowStation});return true;
}
static bool event(const char* name,const char* args){
  captureOriginal(); engaged=true; bool ok=true;
  if(!strcmp(name,"fw_rot_cw"))ok=dual::r10DiagUi(dual::R10DiagUiAction::RotCW);
  else if(!strcmp(name,"fw_rot_ccw"))ok=dual::r10DiagUi(dual::R10DiagUiAction::RotCCW);
  else if(!strcmp(name,"fw_short"))ok=dual::r10DiagUi(dual::R10DiagUiAction::Short);
  else if(!strcmp(name,"fw_long"))ok=dual::r10DiagUi(dual::R10DiagUiAction::Long);
  else if(!strcmp(name,"fw_double"))ok=dual::r10DiagUi(dual::R10DiagUiAction::Double);
  else if(!strcmp(name,"fw_menu"))ok=dual::r10DiagUi(dual::R10DiagUiAction::Menu);
  else if(!strcmp(name,"fw_eq_jitter")){
    int b=argInt(args,"bass",0),t=argInt(args,"treble",0);if(b<-12)b=-12;if(b>12)b=12;if(t<-12)t=-12;if(t>12)t=12;
    player.setTone((int8_t)b,0,(int8_t)t);
  }
  else if(!strcmp(name,"fw_volume_jitter")){
    int v=argInt(args,"value",originalVolume);if(v<0)v=0;if(v>254)v=254;player.setVolume(player.volToI2S((uint8_t)v));
  }
  else if(!strcmp(name,"fw_station_next"))ok=playShadow(+1,false);
  else if(!strcmp(name,"fw_station_prev"))ok=playShadow(-1,false);
  else if(!strcmp(name,"fw_stream_reconnect"))ok=playShadow(0,true);
  else if(!strcmp(name,"fw_audio_toggle")){
    if(config.getMode()!=PM_WEB)ok=false;
    else if(player.status()==PLAYING)player.sendCommand({PR_STOP,0});
    else ok=playShadow(0,false);
  }
  else if(!strcmp(name,"fw_memory_churn"))ok=memoryChurn(args);
  else if(!strcmp(name,"fw_net_probe")){telemetry("net-probe");}
  else ok=false;
  if(ok){eventCount++;Serial.printf("[R10-EVENT] ok=1 index=%lu name=%s\n",(unsigned long)eventCount,name);}
  else {rejected++;Serial.printf("[R10-EVENT] ok=0 rejected=%lu name=%s\n",(unsigned long)rejected,name);}
  return ok;
}
static void stop(){
  player.resetQueue(); player.sendCommand({PR_STOP,0});
  restoreStage=1;restoreAt=millis();engaged=false;dual::r10DiagRecoverHome();
  Serial.println("[R10] STOP restore=QUEUED persistent_writes=0");telemetry("stop");
}
static void restoreTick(){
  if(!restoreStage||!saved)return;const uint32_t now=millis();
  if(restoreStage==1 && uint32_t(now-restoreAt)>=80){
    if(originalPlaying && config.getMode()==PM_WEB){player.sendCommand({PR_PLAY,-(int)originalStation});restoreStage=2;restoreAt=now;}
    else restoreStage=3;
  }
  if(restoreStage==2 && (player.status()==PLAYING || uint32_t(now-restoreAt)>5000))restoreStage=3;
  if(restoreStage==3){
    player.setTone(originalBass,originalMiddle,originalTreble);player.setVolume(player.volToI2S(originalVolume));
    config.store.smartstart=originalSmartstart;player.lockOutput=originalLockOutput;restoreStage=0;saved=false;
    Serial.println("[R10] RESTORE=PASS persistent_writes=0");telemetry("restored");
  }
}
static void status(){
#if !IBT_NO_PSRAM
if(Serial.availableForWrite()<1280)return;
#endif
Serial.printf("[R10-STATUS] active=%u events=%lu rejected=%lu persistent_writes=0\n",engaged?1:0,(unsigned long)eventCount,(unsigned long)rejected);telemetry("status");}
#include "path_session.inc"
static void handle(char* s){
  if(!strcmp(s,"r10 surface status")){dual::surfaceDiagnostics();return;}
  if(!strncmp(s,"r10 surface select=",19)){
    char* end=nullptr;const long id=strtol(s+19,&end,10);
    const bool ok=end!=s+19&&!*end&&id>=0&&dual::requestSurfaceSkin(unsigned(id));
    Serial.printf("[SURFACE-REQUEST] result=%s skin=%ld\n",ok?"QUEUED":"DENIED",id);return;
  }
  if(pathHandle(s))return;
  while(*s==' ')s++;if(!*s)return;
  if(!strncmp(s,"r10 vu ",7)){
    const char* arg=s+7;int mode=!strcmp(arg,"off")?0:!strcmp(arg,"legacy")?1:!strcmp(arg,"sparse")?2:!strcmp(arg,"restore")?-1:-2;
    if(mode>=-1)vuprobe::mode.store(mode);
    Serial.printf("[VU-PROBE] mode=%d configured=%u display=CASSETTE_NO_VU fps_limit=15\n",vuprobe::mode.load(),config.store.vumeter?1:0);return;
  }
  if(!strcmp(s,"r10 metadata")){dual::trackTextDiagnostics();return;}
  if(!strcmp(s,"r10 load")){loadprobe::sample();return;}
#if IBT_NO_PSRAM && FEATURE_ARTWORK
  if(!strcmp(s,"r10 artwork status")){const auto a=onlineart::uploadStatus();Serial.printf("[R10-ARTWORK] enabled=%u busy=%u qualified=%u album=%s generation=%lu retained_floor=%u active_job_cost=%u\n",unsigned(a.enabled),unsigned(a.busy),unsigned(a.qualified),onlineart::albumStatus(),(unsigned long)a.generation,unsigned(ibtlimit::Policy::reserve),unsigned(IBT_STREAM_ART_DIAG?40960u:20480u));return;}
#endif
  if(!strcmp(s,"r10 artwork smoke")){onlineart::smoke(dual::snapshot().generation);Serial.println("[R10-ARTWORK] smoke=QUEUED hold_ms=15000");return;}
  if(!strcmp(s,"r10 bt spp")){Serial.println("[BT-SPP] disabled: A2DP owns Bluetooth");return;}
  if(!strcmp(s,"r10 bt in")){dual::bluetoothMode(1);return;}
  if(!strcmp(s,"r10 bt out")){dual::bluetoothMode(0);return;}
  if(!strcmp(s,"r10 bt on")){dual::bluetoothMode(0);return;}
  if(!strcmp(s,"r10 bt off")){dual::bluetoothMode(-1);return;}
#if IBT_NO_PSRAM
  if(!strcmp(s,"r10 bt transport")){transportPending=1;return;}
  if(!strcmp(s,"r10 bt frame")){wireOffset=0;wirePending=true;return;}
#endif
  if(!strcmp(s,"r10 bt status")){btaudio::diagnostics();return;}
  if(!strcmp(s,"r10 ui selfcheck")){dual::r10UiSelfcheck();return;}
  if(!strncmp(s,"r10 ui stress start",19)){
    unsigned action=unsigned(argInt(s,"action",500));
    unsigned draw=unsigned(argInt(s,"draw",500));
    const unsigned tour=unsigned(argInt(s,"tour",0));
    Serial.printf("[UI-STRESS] start=%s action_ms=%u draw_ms=%u tour=%u\n",
      dual::r10UiStressStart(action,draw,tour)?"PASS":"DENIED",action,draw,tour);
    dual::r10UiStressStatus();return;
  }
  if(!strcmp(s,"r10 ui stress drain")){dual::r10UiStressDrain();dual::r10UiStressStatus();return;}
  if(!strcmp(s,"r10 ui stress stop")){dual::r10UiStressStop("host_stop");dual::r10UiStressStatus();return;}
  if(!strcmp(s,"r10 ui stress status")){dual::r10UiStressStatus();return;}
  if(!strcmp(s,"r10 bt reconnect")){
    const bool ok=btaudio::reconnectSaved();
    if(ok)dual::bluetoothMode(0);
    Serial.printf("[BT-SELFTEST] reconnect_saved=%s\n",ok?"QUEUED":"NO_SAVED_DEVICE");return;
  }
  if(!strcmp(s,"r10 bt scan restart")){
    Serial.printf("[BT-SELFTEST] empty_scan_restart=%s\n",btaudio::restartScan()?"QUEUED":"DENIED");return;
  }
#ifdef USE_SD
  if(!strcmp(s,"r10 sd status")){
    const bool ready=sdman.ready;
    Serial.printf("[SD-SELFTEST] mode=%u mounted=%u catalog=%u tracks=%u playing=%u\n",
      unsigned(config.getMode()),ready?1:0,sdman.catalogReady.load()?1:0,
      ready?unsigned(config.playlistLength()):0,player.isRunning()?1:0);return;
  }
  if(!strcmp(s,"r10 sd start")){
    if(sdTestActive||config.getMode()!=PM_WEB||btaudio::wanted()||network.status==SOFT_AP||display.mode()==LOST){
      Serial.println("[SD-SELFTEST] start=DENIED unsafe_mode");return;
    }
    sdWasPlaying=player.status()==PLAYING;sdTestActive=true;sdRestoreStage=0;
    player.sendCommand({PR_SOURCE,PM_SDCARD});
    Serial.println("[SD-SELFTEST] start=QUEUED");return;
  }
  if(!strcmp(s,"r10 sd restore")){
    if(!sdTestActive && config.getMode()!=PM_SDCARD){Serial.println("[SD-SELFTEST] restore=NOT_ACTIVE");return;}
    player.sendCommand({PR_SOURCE,PM_WEB});sdRestoreStage=1;sdRestoreAt=millis();
    Serial.println("[SD-SELFTEST] restore=QUEUED");return;
  }
#endif
  #if defined(BOARD_CYD28)
  if(!strcmp(s,"r10 skins")){front::skinPreview(0);Serial.println("[SKINS] preview=ON count=7 rotary=next_prev long=exit");return;}
  if(!strcmp(s,"r10 skins off")){front::skinPreview(-1);Serial.println("[SKINS] preview=OFF");return;}
  if(!strcmp(s,"r10 cassette")){Serial.printf("[R10-CASSETTE] ready=%u page=%u asset_fnv=%08lx\n",front::cassetteReady()?1:0,dual::r10DiagPage(),(unsigned long)0xcbc3820au);return;}
#endif
  if(!strcmp(s,"r10 sensor search")){sensors::discover();return;}
  if(!strcmp(s,"r10 sensor list")){sensors::listDevices();return;}
  if(!strncmp(s,"r10 sensor pair ",16)){
    char* address=s+16;char* key=strchr(address,' ');bool ok=false;
    if(key){*key++=0;ok=sensors::bindAddress(address,key);memset(key,0,strlen(key));}
    Serial.printf("[BLE-BIND] result=%s key=<redacted>\n",ok?"QUEUED":"REJECTED");return;
  }
  if(!strncmp(s,"r10 sensor bind ",16)){
    char* index=s+16;char* key=strchr(index,' ');bool ok=false;
    if(key){*key++=0;bool digits=*index!=0;for(char*p=index;*p;++p)if(*p<'0'||*p>'9')digits=false;if(digits&&strlen(index)==1)ok=sensors::bindDevice(unsigned(*index-'0'),key);memset(key,0,strlen(key));}
    Serial.printf("[BLE-BIND] result=%s key=<redacted>\n",ok?"QUEUED":"REJECTED");return;
  }
  if(!strcmp(s,"r10 sensor")){sensors::diagnostics();return;}
  if(!strcmp(s,"r10 clock")){network.requestTimeSync(true);return;}
  if(!strcmp(s,"r10 caps")){printCaps();return;}
  if(!strcmp(s,"r10 status")||!strcmp(s,"status")){status();return;}
  if(!strcmp(s,"r10 heap")||!strcmp(s,"heap")){telemetry("heap");return;}
  if(!strcmp(s,"r10 sbc stop")){if(!engaged)sbcdiag::cancel();return;}
  if(!strcmp(s,"r10 sbc probe")){if(engaged){Serial.println("[SBC-PROBE] start=DENIED active_lease");return;}sbcdiag::start();return;}
  if(!strcmp(s,"r10 stop")){stop();return;}
  if(!strcmp(s,"T")||!strcmp(s,"t")){Serial.println("[STREAM-TEST] requested via R10 shell");player.sendCommand({PR_PLAY,-65001});return;}
  if(!strcmp(s,"S")||!strcmp(s,"s")){player.sendCommand({PR_STOP,0});return;}
  if(!strncmp(s,"r10 event ",10)){
    char* name=s+10;while(*name==' ')name++;char* args=strchr(name,' ');if(args){*args++=0;}else args=(char*)"";event(name,args);return;
  }
  telnet.serialCommand(s);
}
void begin(){lineLen=0;printCaps();if(dual::debugEnabled())Serial.println("[R10] diag=READY ram_only=1 persistent_writes=0");}
void serialPoll(){
  for(unsigned budget=0;budget<64&&Serial.available()>0;++budget){int c=Serial.read();if(c<0)break;if(c=='\r')continue;if(c=='\n'){line[lineLen]=0;handle(line);lineLen=0;line[0]=0;}
    else if(lineLen+1<sizeof(line))line[lineLen++]=(char)c;else{lineLen=0;rejected++;Serial.println("[R10] line_too_long");}}
}
void tick(){
#if IBT_NO_PSRAM
  compactTick();
#endif
  pathTick();
  restoreTick();
#ifdef USE_SD
  if(sdRestoreStage==1 && config.getMode()==PM_WEB && uint32_t(millis()-sdRestoreAt)>=500){
    if(!sdWasPlaying)player.sendCommand({PR_STOP,0});
    sdRestoreStage=0;sdTestActive=false;
    Serial.println("[SD-SELFTEST] restore=PASS");
  }
#endif
  const uint32_t now=millis();if(dual::debugEnabled() && uint32_t(now-lastTelem)>=2000){lastTelem=now;telemetry("tick");}
  if(uint32_t(now-lastIntegrity)>=30000){lastIntegrity=now;memChecks++;if(!heap_caps_check_integrity(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,true)){memErrors++;Serial.println("[R10-HEAP] integrity=FAIL scope=internal");}}
}
bool active(){return engaged;}
}
