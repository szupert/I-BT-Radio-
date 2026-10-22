#include "receiver.h"
#include "start_gate.h"
#include "direct_reader.h"
#include "acquisition_policy.h"
#include "../surface_policy.h"
#include <Arduino.h>
#if CONFIG_BT_BLE_ENABLED && defined(IBT_SENSOR_EXPERIMENTAL)
#include "passive_queue.h"
#include "sensor_decoder.h"
#include "../bt_audio.h"
#include "../resource_limiter.h"
#include "../r10_diag.h"
#include "../audio_path_probe.h"
#include "../../core/player.h"
#include <Preferences.h>
#include <esp_gap_ble_api.h>
#include <esp_bt_main.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <cstring>
namespace sensors {
using namespace radiosensor;
enum class Phase:uint8_t {Off,Parameters,Starting,Running,Stopping,Fault};
static std::atomic<Phase> phase{Phase::Off};
static std::atomic<bool> on{false},budget{false},permit{false},dirty{false},searchRequest{false},paramsReady{false};
static std::atomic<int> selection{-1};
static std::atomic<uint32_t> epoch{1},low{0},high{0};
static std::atomic<bool> chosen{false};
static std::atomic<bool> resetRequest{false};
static std::atomic<int> result{0}; // 0 waiting, 1 decoded, 2 key required, 3 unrecognised, 4 memory
static std::atomic<uint32_t> queued{0},dropped{0},decodedCount{0},requestId{0},uiBlockers{0},runtimeBlockers{0};
static PassiveQueue queue;
static TaskHandle_t worker=nullptr;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
struct Device {uint8_t address[6];uint8_t type;int8_t rssi;uint16_t uuid;uint16_t model;bool seen;};
static Device devices[8]{};static unsigned count=0;
static Reading value{};
static uint32_t nativeProcessed=0,presencePackets=0,authFailures=0,invalidPackets=0,nativeSeenAt=0,nativeMaxUs=0;
static uint8_t boundAddress[6]={},boundKey[16]={};static bool keyValid=false;
static Config bindPending{};static bool bindRequested=false;
static bool loaded=false,attached=false,attempted=false;
// SDK owns this pointer until the asynchronous parameter-complete callback.
static esp_ble_scan_params_t scanParameters{};
static bool configuredActive=false;
static void traceInit(const char* stage,int error=0){Serial.printf("[BLE-INIT] stage=%s error=%d free=%lu largest=%lu stack_hwm=%lu\n",stage,error,(unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned long)uxTaskGetStackHighWaterMark(nullptr));}
static std::atomic<bool> integrityFailed{false};
static std::atomic<uint32_t> gapEvents{0},lastGap{0},workerPackets{0},discoveryPackets{0};
static unsigned auditBursts=0;
static uint32_t admissionAt=0;static bool admissionAttempted=false;
static bool admissionBlocked=false;
static bool checkHeap(const char* step){
 if(integrityFailed.load())return false;
 Serial.printf("[BLE-CHECK] stage=%s result=BEGIN gap=%lu last_gap=%lu queued=%lu worker=%lu\n",step,(unsigned long)gapEvents.load(),(unsigned long)lastGap.load(),(unsigned long)queued.load(),(unsigned long)workerPackets.load());
 const bool ok=heap_caps_check_integrity_all(true);
 Serial.printf("[BLE-CHECK] stage=%s result=%s\n",step,ok?"PASS":"FAIL");
 if(!ok){integrityFailed=true;permit=false;phase=Phase::Fault;result=5;}
 return ok;
}


static std::atomic<bool> searching{false};
static uint32_t last=0,phaseAt=0;static unsigned bursts=0;
static uint32_t initAt=0,saveAt=0;static bool initTried=false,saveTried=false;
static uint32_t packLow(const uint8_t*a){return uint32_t(a[0])|(uint32_t(a[1])<<8)|(uint32_t(a[2])<<16)|(uint32_t(a[3])<<24);}
static uint32_t packHigh(const uint8_t*a){return uint32_t(a[4])|(uint32_t(a[5])<<8);}
static Config configuration(){Config c;c.enabled=on.load();c.selected=chosen.load();uint32_t a=low.load(),b=high.load();for(unsigned i=0;i<4;i++)c.address[i]=a>>(i*8);c.address[4]=b;c.address[5]=b>>8;portENTER_CRITICAL(&lock);c.keyValid=keyValid&&!memcmp(c.address,boundAddress,6);if(c.keyValid)memcpy(c.bindkey,boundKey,16);portEXIT_CRITICAL(&lock);return c;}
static void gap(esp_gap_ble_cb_event_t event,esp_ble_gap_cb_param_t*p){
 ++gapEvents;lastGap=unsigned(event);if(integrityFailed.load())return;
 if(event==ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT){if(p->scan_param_cmpl.status==ESP_BT_STATUS_SUCCESS){paramsReady=true;phase=Phase::Off;}else phase=Phase::Fault;return;}
 if(event==ESP_GAP_BLE_SCAN_START_COMPLETE_EVT){phase=p->scan_start_cmpl.status==ESP_BT_STATUS_SUCCESS?Phase::Running:Phase::Fault;return;}
 if(event==ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT){phase=p->scan_stop_cmpl.status==ESP_BT_STATUS_SUCCESS?Phase::Off:Phase::Fault;return;}
 if(event!=ESP_GAP_BLE_SCAN_RESULT_EVT)return;
 if(p->scan_rst.search_evt==ESP_GAP_SEARCH_INQ_CMPL_EVT){phase=Phase::Off;return;}
 if(p->scan_rst.search_evt!=ESP_GAP_SEARCH_INQ_RES_EVT||!permit.load(std::memory_order_acquire))return;
 Packet packet;packet.epoch=epoch.load();packet.discovery=searching.load();
 if(!searching.load()&&(!chosen.load()||packLow(p->scan_rst.bda)!=low.load()||packHigh(p->scan_rst.bda)!=high.load()))return;
 memcpy(packet.address,p->scan_rst.bda,6);packet.addressType=p->scan_rst.ble_addr_type;packet.rssi=p->scan_rst.rssi;packet.capturedAt=millis();
 const auto extracted=extract(p->scan_rst.ble_adv,p->scan_rst.adv_data_len,packet);
 if(extracted!=Extract::Found){
  const unsigned adv=p->scan_rst.adv_data_len,response=p->scan_rst.scan_rsp_len;
  if(extracted==Extract::Malformed||adv>31||response>31||!packet.discovery||extract(p->scan_rst.ble_adv+adv,response,packet)!=Extract::Found)return;
 }
 if(packet.epoch!=epoch.load()||!permit.load())return;
 if(queue.push(packet))++queued;else ++dropped;
}
static void recordDevice(const Packet& packet){
   portENTER_CRITICAL(&lock);unsigned index=0;while(index<count&&memcmp(devices[index].address,packet.address,6))++index;
   if(index<count||count<8){if(index==count)++count;memcpy(devices[index].address,packet.address,6);devices[index].type=packet.addressType;devices[index].rssi=packet.rssi;devices[index].seen=true;devices[index].uuid=packet.uuid;devices[index].model=packet.uuid==0xfe95&&packet.length>=4?uint16_t(packet.data[2])|(uint16_t(packet.data[3])<<8):0;}
   portEXIT_CRITICAL(&lock);
}
static void retainBoundDevice(){
 Config c=configuration();if(!c.selected||!c.keyValid)return;
 portENTER_CRITICAL(&lock);unsigned i=0;while(i<count&&memcmp(devices[i].address,c.address,6))++i;
 if(i==count&&count<8){Device d{};memcpy(d.address,c.address,6);d.uuid=0xfe95;d.model=0x055b;d.rssi=-127;devices[count++]=d;}portEXIT_CRITICAL(&lock);
}
// Native fixed-size Xiaomi packets avoid the generic JSON worker entirely.
// Runs only in the existing idle owner with a stack guard, at most one/100ms.
static void nativePacket(const Packet& packet){
 if(packet.epoch!=epoch.load()||uxTaskGetStackHighWaterMark(nullptr)<3072)return;
 Config c=configuration();if(!c.keyValid)return;Reading candidate;portENTER_CRITICAL(&lock);candidate=value;portEXIT_CRITICAL(&lock);
 Frame f;memcpy(f.address,packet.address,6);f.service=packet.data;f.length=packet.length;f.uuid=packet.uuid;f.rssi=packet.rssi;
 const uint32_t started=micros();const auto decoded=decodeMiBeacon(c,f,packet.capturedAt,candidate);
 ++nativeProcessed;nativeSeenAt=millis();const uint32_t elapsed=micros()-started;if(elapsed>nativeMaxUs)nativeMaxUs=elapsed;
 if(packet.epoch!=epoch.load())return;
 if(decoded==Result::Decoded){portENTER_CRITICAL(&lock);value=candidate;portEXIT_CRITICAL(&lock);result=1;++decodedCount;Serial.printf("[BLE-NATIVE] result=DECODED elapsed_us=%lu stack_hwm=%lu\n",(unsigned long)(micros()-started),(unsigned long)uxTaskGetStackHighWaterMark(nullptr));}
 else if(decoded==Result::AdvertisementOnly){++presencePackets;if(result!=1&&result!=6&&result!=2)result=7;}
 else if(decoded==Result::AuthenticationFailed){++authFailures;result=6;}
 else if(decoded!=Result::WrongDevice){++invalidPackets;result=3;static uint32_t loggedAt=0;if(invalidPackets<=3||uint32_t(millis()-loggedAt)>=10000){loggedAt=millis();Serial.printf("[BLE-NATIVE] result=REJECT code=%u length=%u fc=%02x%02x model=%02x%02x\n",unsigned(decoded),unsigned(packet.length),packet.data[0],packet.data[1],packet.data[2],packet.data[3]);}}
}
static void run(void*){
 Packet packet,lastPacket;bool seen=false;
 for(;;){
  if(!permit.load()){vTaskDelay(pdMS_TO_TICKS(1000));continue;}
  if(!queue.pop(packet)){vTaskDelay(pdMS_TO_TICKS(100));continue;}
  ++workerPackets;
  if(packet.epoch!=epoch.load())continue;
  if(packet.discovery)recordDevice(packet);
  if(!chosen.load())continue;
  if(seen&&samePayload(packet,lastPacket))continue;
  lastPacket=packet;seen=true;
  Config c=configuration();Reading candidate;portENTER_CRITICAL(&lock);candidate=value;portEXIT_CRITICAL(&lock);
  Frame frame;memcpy(frame.address,packet.address,6);frame.service=packet.data;frame.length=packet.length;frame.uuid=packet.uuid;frame.rssi=packet.rssi;
  auto decoded=decode(c,frame,packet.capturedAt,candidate);
  if(packet.epoch!=epoch.load())continue;
  if(decoded==Result::Decoded){portENTER_CRITICAL(&lock);if(packet.epoch==epoch.load())value=candidate;portEXIT_CRITICAL(&lock);result=1;++decodedCount;}
  else if(decoded==Result::NeedsDecryption)result=2;
  else if(decoded==Result::AuthenticationFailed)result=6;
  else if(decoded!=Result::WrongDevice)result=3;
  vTaskDelay(pdMS_TO_TICKS(100)); // one packet per turn, worker priority zero
 }
}
bool available(){return true;}bool enabled(){return on.load();}
void toggle(){on=!on.load();dirty=true;if(!on.load())permit=false;else resetRequest=true;}
void discover(){on=true;dirty=true;resetRequest=true;searchRequest=true;const auto id=++requestId;Serial.printf("[BLE-REQUEST] id=%lu accepted=1 pending=1\n",(unsigned long)id);}
unsigned deviceCount(){portENTER_CRITICAL(&lock);unsigned n=count;portEXIT_CRITICAL(&lock);return n;}
bool deviceLabel(unsigned index,char*out,size_t size){Device d;portENTER_CRITICAL(&lock);bool ok=index<count;if(ok)d=devices[index];portEXIT_CRITICAL(&lock);if(ok){const char*family=d.uuid==0xfe95?(d.model==0x055b?"LYWSD03MMC":"Xiaomi"):d.uuid==0xfcd2?"BTHome":d.uuid==0x181a?"ATC/PVVX":"Qingping";const bool selected=chosen.load()&&packLow(d.address)==low.load()&&packHigh(d.address)==high.load();if(d.seen)snprintf(out,size,"%s %02X%02X%02X %d dBm%s",family,d.address[3],d.address[4],d.address[5],d.rssi,selected?" *":"");else snprintf(out,size,"%s %02X%02X%02X mentett / vetelre var",family,d.address[3],d.address[4],d.address[5]);}return ok;}
void select(unsigned index){selection=int(index);}
static int hexDigit(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static bool queueBinding(const uint8_t* address,const char* text){
 if(!text||strlen(text)!=32)return false;Config c;c.keyValid=true;memcpy(c.address,address,6);
 for(unsigned i=0;i<16;++i){int a=hexDigit(text[2*i]),b=hexDigit(text[2*i+1]);if(a<0||b<0)return false;c.bindkey[i]=uint8_t(a*16+b);}
 portENTER_CRITICAL(&lock);bool ok=!bindRequested;if(ok){bindPending=c;bindRequested=true;}portEXIT_CRITICAL(&lock);return ok;
}
bool bindDevice(unsigned index,const char* text){
 uint8_t address[6];portENTER_CRITICAL(&lock);bool ok=index<count;if(ok)memcpy(address,devices[index].address,6);portEXIT_CRITICAL(&lock);return ok&&queueBinding(address,text);
}
bool bindAddress(const char* text,const char* key){
 if(!text||strlen(text)!=17)return false;uint8_t address[6];
 for(unsigned i=0;i<6;++i){int a=hexDigit(text[3*i]),b=hexDigit(text[3*i+1]);if(a<0||b<0||(i<5&&text[3*i+2]!=':'))return false;address[i]=uint8_t(a*16+b);}
 return queueBinding(address,key);
}
void bindingStatus(char* address,size_t size,bool& hasKey,bool& pending){Config c=configuration();hasKey=c.keyValid;portENTER_CRITICAL(&lock);pending=bindRequested||dirty.load();portEXIT_CRITICAL(&lock);if(c.selected)snprintf(address,size,"%02X:%02X:%02X:%02X:%02X:%02X",c.address[0],c.address[1],c.address[2],c.address[3],c.address[4],c.address[5]);else if(size)address[0]=0;}
void listDevices(){for(unsigned i=0;i<8;++i){Device d;portENTER_CRITICAL(&lock);bool ok=i<count;if(ok)d=devices[i];portEXIT_CRITICAL(&lock);if(!ok)break;Serial.printf("[BLE-DEVICE] index=%u mac=%02X:%02X:%02X:%02X:%02X:%02X uuid=%04x model=%04x rssi=%d\n",i,d.address[0],d.address[1],d.address[2],d.address[3],d.address[4],d.address[5],d.uuid,d.model,int(d.rssi));}}

void setBudget(bool idleClock,uint32_t blockers){uiBlockers=blockers;budget=idleClock;if(!idleClock)permit=false;}
void audioRequested(){budget=false;permit=false;direct::audioRequested();}
bool quiescent(){return (phase.load()==Phase::Off||phase.load()==Phase::Fault)&&direct::quiet();}
bool reading(Reading&out){portENTER_CRITICAL(&lock);out=value;portEXIT_CRITICAL(&lock);return enabled()&&out.temperatureValid&&out.humidityValid;}
void diagnostics(){Reading r;reading(r);const uint32_t now=millis();Serial.printf("[BLE-SENSOR] available=1 enabled=%u selected=%u phase=%u ui_budget=%u permit=%u state=%d devices=%u queued=%lu queue_drops=%lu decoded=%lu temp_valid=%u hum_valid=%u fresh=%u temp_centi=%d hum_centi=%u temp_age_ms=%lu hum_age_ms=%lu save_pending=%u gap_events=%lu last_gap=%lu worker_packets=%lu integrity_failed=%u discovery_processed=%lu request_id=%lu pending=%u searching=%u ui_mask=0x%lx runtime_mask=0x%lx worker_present=%u\n",unsigned(enabled()),unsigned(chosen.load()),unsigned(phase.load()),unsigned(budget.load()),unsigned(permit.load()),result.load(),deviceCount(),(unsigned long)queued.load(),(unsigned long)dropped.load(),(unsigned long)decodedCount.load(),unsigned(r.temperatureValid),unsigned(r.humidityValid),unsigned(enabled()&&hasFreshPair(r,now)),int(r.temperatureCenti),unsigned(r.humidityCenti),(unsigned long)(r.temperatureValid?uint32_t(now-r.temperatureAt):0),(unsigned long)(r.humidityValid?uint32_t(now-r.humidityAt):0),unsigned(dirty.load()),(unsigned long)gapEvents.load(),(unsigned long)lastGap.load(),(unsigned long)workerPackets.load(),unsigned(integrityFailed.load()),(unsigned long)discoveryPackets.load(),(unsigned long)requestId.load(),unsigned(searchRequest.load()),unsigned(searching.load()),(unsigned long)uiBlockers.load(),(unsigned long)runtimeBlockers.load(),unsigned(worker!=nullptr));Serial.printf("[BLE-RX] native_processed=%lu presence=%lu auth_failures=%lu invalid=%lu last_seen_age_ms=%lu max_decode_us=%lu\n",(unsigned long)nativeProcessed,(unsigned long)presencePackets,(unsigned long)authFailures,(unsigned long)invalidPackets,(unsigned long)(nativeProcessed?uint32_t(now-nativeSeenAt):0),(unsigned long)nativeMaxUs);direct::diagnostics();}
const char*status(){if(!enabled())return "KI";if(integrityFailed.load())return "Memoriahiba: naplo szukseges";if(phase==Phase::Fault)return "BLE hiba: naplo szukseges";if(direct::faulted())return "Kozvetlen olvasas hiba / naplo";if(direct::exclusive())return "Ritka kozvetlen olvasas...";if(result==7)return "Szenzor jelen / meresre var";if(result==6)return "Kulcs nem hitelesit: ellenorizd";if(result==2)return "Titkositott: kulcs kell";if(searchRequest.load()||searching.load()){
 const auto u=uiBlockers.load(),r=runtimeBlockers.load();
 if((u&(4u|128u|256u))||(r&(2u|4u)))return "Var: lejatszas STOP kell";
 if((u&512u)||(r&(64u|128u)))return "Var: BT audio kikapcsolas";
 if(u&16u)return "Var: beallitasok mentese";
 if(u&8u)return "Var: ebresztes aktiv";
 if(u&(1u|2u))return "Var: nyugalmi oralap kell";
 if(u&32u)return "Var: kezelo sav elrejtese";
 if(u&64u)return "Var: idojaras feladat";
 if(r&8u)return "Var: diagnosztikai teszt";
 if(r&16u)return "Var: hangut meres";
 if(r&32u)return "Var: hattermunka";
 }if(!budget.load())return "Var: nyugalmi ora kell";if(result==4)return "Var: keves belso RAM";if(phase==Phase::Running)return searching.load()?"Erzekelok keresese...":"Passziv vetel...";if(searching.load()||searchRequest.load())return "Kereses var: hang/menu/halozat";if(result==3)return "Nem tamogatott adat";return chosen.load()?"Kivalasztva / passziv vetel":"Valassz erzekelot";}
void service(){
 const uint32_t now=millis();
 if(integrityFailed.load()){direct::service(configuration(),false,false,false,now);return;}
 if(!loaded){Preferences p;if(p.begin("ibt_sensor",true)){on=enabledAfterLoad(p.getBool("on",false),searchRequest.load());uint8_t a[6];if(p.getBytesLength("address")==6&&p.getBytes("address",a,6)==6){low=packLow(a);high=packHigh(a);chosen=true;}uint8_t binding[22];if(p.getBytesLength("binding")==22&&p.getBytes("binding",binding,22)==22){portENTER_CRITICAL(&lock);memcpy(boundAddress,binding,6);memcpy(boundKey,binding+6,16);keyValid=true;portEXIT_CRITICAL(&lock);}p.end();}loaded=true;retainBoundDevice();Serial.printf("[BLE-BIND] result=LOADED selected=%u key_set=%u\n",unsigned(chosen.load()),unsigned(configuration().keyValid));}
 const bool running=player.isRunning(),stopped=player.status()==STOPPED,diag=r10diag::active(),probe=pathprobe::enabled.load(),busy=ibtlimit::backgroundBusy(),btWanted=btaudio::wanted(),btIdle=btaudio::idle();
 const uint32_t gateMask=(!surfacepolicy::ble(surfacepolicy::current())?256u:0u)|(!budget.load()?1u:0u)|(running?2u:0u)|(!stopped?4u:0u)|(diag?8u:0u)|(probe?16u:0u)|(busy?32u:0u)|(btWanted?64u:0u)|(!btIdle?128u:0u);
 runtimeBlockers=gateMask;const bool idle=gateMask==0;
 direct::service(configuration(),idle&&!searching.load()&&!searchRequest.load(),attached&&phase!=Phase::Fault,phase==Phase::Off,now);
 Reading directValue;if(direct::take(directValue)&&idle&&chosen.load()){portENTER_CRITICAL(&lock);value=directValue;portEXIT_CRITICAL(&lock);result=1;++decodedCount;}

 static uint32_t gateAt=0,lastGate=UINT32_MAX,lastUi=UINT32_MAX,lastRequest=0;
 const auto id=requestId.load(),ui=uiBlockers.load();
 if((searchRequest.load()||searching.load())&&(gateMask!=lastGate||ui!=lastUi||id!=lastRequest||uint32_t(now-gateAt)>=10000)){
  gateAt=now;lastGate=gateMask;lastUi=ui;lastRequest=id;
  Serial.printf("[BLE-GATE] id=%lu pending=%u searching=%u result=%s ui_mask=0x%lx runtime_mask=0x%lx enabled=%u running=%u stopped=%u diag=%u probe=%u background=%u bt_wanted=%u bt_idle=%u phase=%u\n",(unsigned long)id,unsigned(searchRequest.load()),unsigned(searching.load()),idle?"READY":"WAIT",(unsigned long)ui,(unsigned long)gateMask,unsigned(on.load()),unsigned(running),unsigned(stopped),unsigned(diag),unsigned(probe),unsigned(busy),unsigned(btWanted),unsigned(btIdle),unsigned(phase.load()));
 }
 if(!on.load()||!idle){permit=false;if(phase==Phase::Running||phase==Phase::Starting){if(esp_ble_gap_stop_scanning()==ESP_OK){phase=Phase::Stopping;phaseAt=now;}}}
 if(direct::exclusive()){
  permit=false;if(phase==Phase::Running||phase==Phase::Starting){if(esp_ble_gap_stop_scanning()==ESP_OK){phase=Phase::Stopping;phaseAt=now;}}
  return;
 }
 if(phase!=Phase::Off&&phase!=Phase::Fault&&uint32_t(now-phaseAt)>5000){permit=false;esp_ble_gap_stop_scanning();phase=Phase::Fault;}
 if(idle&&quiescent()){
  Config pending;bool apply=false;portENTER_CRITICAL(&lock);if(bindRequested){pending=bindPending;bindPending=Config{};bindRequested=false;apply=true;}portEXIT_CRITICAL(&lock);
  if(apply){permit=false;chosen=false;epoch.fetch_add(1);low=packLow(pending.address);high=packHigh(pending.address);portENTER_CRITICAL(&lock);memcpy(boundAddress,pending.address,6);memcpy(boundKey,pending.bindkey,16);keyValid=true;value=Reading{};portEXIT_CRITICAL(&lock);chosen=true;on=true;searchRequest=false;searching=false;paramsReady=false;result=0;dirty=true;retainBoundDevice();Serial.println("[BLE-BIND] result=APPLIED key=<redacted>");}
 }
 if(selection.load()>=0&&quiescent()){
  const int index=selection.exchange(-1);Device d;portENTER_CRITICAL(&lock);bool ok=unsigned(index)<count;if(ok)d=devices[index];portEXIT_CRITICAL(&lock);
  if(ok){permit=false;chosen=false;epoch.fetch_add(1);low=packLow(d.address);high=packHigh(d.address);chosen=true;searching=false;result=0;portENTER_CRITICAL(&lock);value=Reading{};portEXIT_CRITICAL(&lock);dirty=true;}
 }
 if(!idle)return;
 // Discovery only stores bounded labels. Do not allocate a decoder task until a
 // device is selected. Callback still owns no UI/decoder work or allocations.
 static uint32_t discoveryAt=0;
 if(!worker&&permit.load()&&uint32_t(now-discoveryAt)>=100){
  discoveryAt=now;Packet packet;
  if(queue.pop(packet)&&packet.epoch==epoch.load()){if(packet.discovery){recordDevice(packet);++discoveryPackets;}else if(idle)nativePacket(packet);}
 }
 if((!saveTried||uint32_t(now-saveAt)>=1000)&&dirty.exchange(false)){saveTried=true;saveAt=now;Preferences p;if(p.begin("ibt_sensor",false)){Config c=configuration();bool saved=p.putBool("on",enabled())==1;if(c.selected)saved=saved&&p.putBytes("address",c.address,6)==6;uint8_t binding[22];bool hasKey;portENTER_CRITICAL(&lock);hasKey=keyValid;if(hasKey){memcpy(binding,boundAddress,6);memcpy(binding+6,boundKey,16);}portEXIT_CRITICAL(&lock);if(hasKey){saved=saved&&p.putBytes("binding",binding,22)==22;uint8_t verify[22];saved=saved&&p.getBytesLength("binding")==22&&p.getBytes("binding",verify,22)==22&&!memcmp(binding,verify,22);}p.end();if(!saved)dirty=true;else if(hasKey)Serial.println("[BLE-BIND] result=SAVED_READBACK_PASS key=<redacted>");}else dirty=true;}
 if(!enabled())return;
 if(!chosen.load()&&!searching.load()&&!searchRequest.load()){permit=false;return;}
 if(resetRequest.exchange(false)){if(result.load()==5)return;initTried=false;if(phase==Phase::Fault){phase=Phase::Off;paramsReady=false;}}
 if(phase==Phase::Fault)return;
 const bool needDecoder=needsDecoderTask(chosen.load(),searchRequest.load(),searching.load(),configuration().keyValid);
 if((needDecoder&&!worker)||!attached){if(initTried&&uint32_t(now-initAt)<60000)return;initTried=true;initAt=now;
  // Fail closed before SDK startup; do not consume the worker stack first.
  if(!attached){
   if(!checkHeap("before_preflight"))return;traceInit("preflight");
   if(esp_bluedroid_get_status()!=ESP_BLUEDROID_STATUS_ENABLED &&
      (heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<65536||heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<32768||uxTaskGetStackHighWaterMark(nullptr)<2048)){
       result=4;traceInit("deferred_budget");return;
   }
   traceInit("stack_begin");if(!btaudio::preparePassiveStack()){result=5;phase=Phase::Fault;Serial.println("[BLE-INIT] stage=stack_failed see_BLE_CHECK=1");return;}if(!checkHeap("after_sdk"))return;traceInit("stack_ready");
  }
  if(!queue.begin()){result=4;return;}
  if(needDecoder&&!worker){if(!checkHeap("before_worker"))return;if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<32768||!psramFound()||!queue.begin()){result=4;traceInit("worker_deferred");return;}if(xTaskCreatePinnedToCore(run,"ble_sensor",6144,nullptr,0,&worker,0)!=pdPASS){worker=nullptr;result=4;traceInit("worker_failed");return;}if(!checkHeap("after_worker"))return;traceInit("worker_ready");}
  if(!attached){const int error=esp_ble_gap_register_callback(gap);if(error!=ESP_OK){traceInit("callback_failed",error);phase=Phase::Fault;return;}attached=true;if(!checkHeap("after_callback_registration"))return;traceInit("callback_ready");}
 }
 if(quiescent()&&searchRequest.exchange(false)){permit=false;epoch.fetch_add(1);portENTER_CRITICAL(&lock);count=0;portEXIT_CRITICAL(&lock);retainBoundDevice();searching=true;paramsReady=false;bursts=10;attempted=false;auditBursts=0;admissionAttempted=false;admissionBlocked=false;}
 if(phase==Phase::Off&&searching.load()&&bursts==0){searching=false;paramsReady=false;}
 if(!chosen.load()&&!searching.load())return;
 // A bound GATT sensor owns periodic acquisition. Passive scans must not
 // steal the shared60s slot while its fresh-reading timer is still closed.
 if(phase==Phase::Off&&directOwnsAcquisition(configuration(),searching.load(),searchRequest.load())){permit=false;return;}
 if(phase==Phase::Off&&paramsReady.load()&&configuredActive!=searching.load())paramsReady=false;
 if(!paramsReady.load()&&phase==Phase::Off){
  scanParameters={};configuredActive=searching.load();scanParameters.scan_type=configuredActive?BLE_SCAN_TYPE_ACTIVE:BLE_SCAN_TYPE_PASSIVE;
  scanParameters.own_addr_type=BLE_ADDR_TYPE_PUBLIC;scanParameters.scan_filter_policy=BLE_SCAN_FILTER_ALLOW_ALL;
  scanParameters.scan_interval=0x80;scanParameters.scan_window=0x10;scanParameters.scan_duplicate=BLE_SCAN_DUPLICATE_ENABLE;
  if(!checkHeap("before_scan_parameters"))return;
  phase=Phase::Parameters;phaseAt=now;const int error=esp_ble_gap_set_scan_params(&scanParameters);
  if(!checkHeap("scan_parameters_submitted"))return;
  traceInit(configuredActive?"active_search_params":"passive_monitor_params",error);if(error!=ESP_OK)phase=Phase::Fault;return;
 }
 // Do not walk the heap continuously during asynchronous setup/scanning.
 // Integrity checks stay at each first-three-burst boundary before the original stats.
 static Phase observed=Phase::Fault;const Phase current=phase.load();
 if(current!=observed){observed=current;if(auditBursts<3&&!checkHeap(current==Phase::Off?"gap_phase_off":current==Phase::Running?"gap_phase_running":"gap_phase_transition"))return;}
 if(current!=Phase::Off||!paramsReady.load())return;
 if(!acquisitionCooldown().due(now)||(attempted&&uint32_t(now-last)<scanBurstMs+scanRestMs))return;
 if(admissionAttempted&&uint32_t(now-admissionAt)<(admissionBlocked?30000u:1000u))return;
 admissionAt=now;admissionAttempted=true;
 if(auditBursts<3&&!checkHeap("before_scan_admission"))return;
 Config c=configuration();if(searching.load())c.selected=true;
 const unsigned heap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
 const bool blocked=heap<32768||largest<16384;
 if(blocked||admissionBlocked){
  Serial.printf("[BLE-ADMISSION] result=%s reason=%s free=%u largest=%u required_free=32768 required_largest=16384 psram_free=%lu worker_hwm=%lu retry_ms=%u\n",blocked?"DEFER":"READY",heap<32768?(largest<16384?"free_and_largest":"free"):largest<16384?"largest":"none",heap,largest,(unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),(unsigned long)(worker?uxTaskGetStackHighWaterMark(worker):0),blocked?30000u:1000u);
 }
 admissionBlocked=blocked;
 if(blocked){permit=false;result=4;return;}
 if(result==4)result=0;
 if(phase==Phase::Off&&paramsReady.load()&&scanDue(c,true,false,false,heap,largest,0,now,last,attempted)){
  if(!acquisitionCooldown().claim(now))return;
  permit=true;phase=Phase::Starting;phaseAt=now;last=now;attempted=true;
  Serial.println("[BLE-CADENCE] acquisition=SCAN minimum_start_gap_ms=60000");
  if(auditBursts<3)Serial.println("[BLE-CHECK] stage=start_scan_call result=BEGIN");
  if(esp_ble_gap_start_scanning(1)!=ESP_OK){permit=false;phase=Phase::Fault;return;}
  if(auditBursts<3&&!checkHeap("start_scan_submitted"))return;
  ++auditBursts;
  if(searching.load()&&bursts)--bursts;
 }
}
}
#else
namespace sensors {
bool available(){return false;}bool enabled(){return false;}void toggle(){}void discover(){}unsigned deviceCount(){return 0;}bool deviceLabel(unsigned,char*,size_t){return false;}void select(unsigned){}bool bindDevice(unsigned,const char*){return false;}bool bindAddress(const char*,const char*){return false;}
void bindingStatus(char*a,size_t n,bool&k,bool&p){if(n)a[0]=0;k=p=false;}
void audioRequested(){}void listDevices(){}void setBudget(bool,uint32_t){}void service(){}bool quiescent(){return true;}const char*status(){return "BLE firmware szukseges";}bool reading(radiosensor::Reading&){return false;}void diagnostics(){Serial.println("[BLE-SENSOR] available=0 enabled=0");}
}
#endif
