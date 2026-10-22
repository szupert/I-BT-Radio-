#include "direct_reader.h"
#include "direct_policy.h"
#include <Arduino.h>
#if CONFIG_BT_BLE_ENABLED && defined(IBT_SENSOR_EXPERIMENTAL)
#include <esp_gattc_api.h>
#include <esp_gap_ble_api.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <cstring>
namespace sensors {namespace direct {
using namespace radiosensor;
enum class Stage:uint8_t {Idle,WaitScan,Registering,Opening,Searching,NotifyRegister,Descriptor,Waiting,Closing,Fault};
static std::atomic<const char*> lastReason{"none"};static std::atomic<unsigned> lastStage{0};
static std::atomic<Stage> stage{Stage::Idle};static std::atomic<bool> veto{false};
static esp_gatt_if_t interfaceId=ESP_GATT_IF_NONE;
static const uint16_t appId=0x118;
static esp_bt_uuid_t uuid(uint8_t part){esp_bt_uuid_t u{};u.len=ESP_UUID_LEN_128;const uint8_t bytes[16]={0xa6,0xa3,0x7d,0x99,0xf2,0x6f,0x1a,0x8a,0x0c,0x4b,0x0a,0x7a,part,0xcc,0xe0,0xeb};memcpy(u.uuid.uuid128,bytes,16);return u;}
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
enum:uint32_t {Reg=1,Open=2,Search=4,Subscribed=8,Written=16,Data=32,Down=64,Cancelled=128,ServiceChanged=256};
struct Mail {uint32_t flags=0;esp_gatt_if_t iface=ESP_GATT_IF_NONE;int regStatus=0,openStatus=0,searchStatus=0,notifyStatus=0,writeStatus=0,cancelStatus=0;uint16_t conn=0,start=0,end=0,handle=0,descriptor=0;bool link=false;uint8_t address[6]={},bytes[5]={};bool active=false;};
static Mail mail;
static bool samplePending=false;static uint8_t pendingSample[5]={};static unsigned serviceChanges=0;
static bool callbackSet=false,attempted=false,stable=false,haveValue=false,notifyRegistered=false,closeSent=false,cancelSent=false,openPending=false,registering=false,faultBusy=false;
static uint32_t stableAt=0,lastAttempt=0,started=0,closingAt=0,vetoAt=0,attempts=0,successes=0,failures=0,aborts=0,deferred=0,loggedAt=0;
static uint16_t connection=0,characteristic=0;static uint8_t address[6]={};static Reading reading{};
static void callback(esp_gattc_cb_event_t event,esp_gatt_if_t iface,esp_ble_gattc_cb_param_t* p){
 if(!p)return;portENTER_CRITICAL(&mux);
 if(event==ESP_GATTC_REG_EVT){if(p->reg.app_id==appId){mail.iface=iface;mail.regStatus=p->reg.status;mail.flags|=Reg;}portEXIT_CRITICAL(&mux);return;}
 if((iface!=mail.iface&&iface!=ESP_GATT_IF_NONE)||!mail.active){portEXIT_CRITICAL(&mux);return;}
 switch(event){
 case ESP_GATTC_CONNECT_EVT:if(!memcmp(p->connect.remote_bda,mail.address,6)){mail.link=true;mail.conn=p->connect.conn_id;}break;
 case ESP_GATTC_OPEN_EVT:if(!memcmp(p->open.remote_bda,mail.address,6)){mail.conn=p->open.conn_id;mail.openStatus=p->open.status;if(p->open.status==ESP_GATT_OK)mail.link=true;mail.flags|=Open;}break;
 case ESP_GATTC_SEARCH_RES_EVT:if(p->search_res.conn_id==mail.conn&&p->search_res.srvc_id.uuid.len==ESP_UUID_LEN_128){const auto expected=uuid(0xb0);if(!memcmp(expected.uuid.uuid128,p->search_res.srvc_id.uuid.uuid.uuid128,16)){mail.start=p->search_res.start_handle;mail.end=p->search_res.end_handle;}}break;
 case ESP_GATTC_SEARCH_CMPL_EVT:if(p->search_cmpl.conn_id==mail.conn){mail.searchStatus=p->search_cmpl.status;mail.flags|=Search;}break;
 case ESP_GATTC_REG_FOR_NOTIFY_EVT:if(p->reg_for_notify.handle==mail.handle){mail.notifyStatus=p->reg_for_notify.status;mail.flags|=Subscribed;}break;
 case ESP_GATTC_WRITE_DESCR_EVT:if(p->write.conn_id==mail.conn&&p->write.handle==mail.descriptor&&mail.descriptor){mail.writeStatus=p->write.status;mail.flags|=Written;}break;
 case ESP_GATTC_NOTIFY_EVT:if(p->notify.conn_id==mail.conn&&p->notify.handle==mail.handle&&!memcmp(p->notify.remote_bda,mail.address,6)&&p->notify.value_len==5&&p->notify.value){memcpy(mail.bytes,p->notify.value,5);mail.flags|=Data;}break;
 case ESP_GATTC_SRVC_CHG_EVT:if(!memcmp(p->srvc_chg.remote_bda,mail.address,6))mail.flags|=ServiceChanged;break;
 case ESP_GATTC_DISCONNECT_EVT:if(!memcmp(p->disconnect.remote_bda,mail.address,6)){mail.link=false;mail.flags|=Down;}break;
 case ESP_GATTC_CLOSE_EVT:break; // Physical DISCONNECT acknowledgement owns quiescence.
 case ESP_GATTC_CANCEL_OPEN_EVT:if(!memcmp(p->cancel_open.remote_bda,mail.address,6)){mail.cancelStatus=p->cancel_open.status;mail.flags|=Cancelled;}break;
 default:break;
 }
 portEXIT_CRITICAL(&mux);
}
static void finish(){
 // Normal disconnect does not invalidate the shared remote GATT cache.
 portENTER_CRITICAL(&mux);mail.active=false;mail.flags=0;mail.link=false;portEXIT_CRITICAL(&mux);
 samplePending=false;characteristic=0;notifyRegistered=false;closeSent=cancelSent=false;stage=Stage::Idle;
}
static void closing(uint32_t now,const char* reason){
 if(stage==Stage::Closing||stage==Stage::Fault)return;
 lastReason.store(reason);lastStage.store(unsigned(stage.load()));stage=Stage::Closing;closingAt=now;closeSent=cancelSent=false;Serial.printf("[BLE-DIRECT] result=CLOSING reason=%s\n",reason);
}
void stackStopped(){
 if(!quiet())return;
 portENTER_CRITICAL(&mux);mail=Mail{};portEXIT_CRITICAL(&mux);
 interfaceId=ESP_GATT_IF_NONE;callbackSet=false;registering=openPending=faultBusy=false;
 samplePending=notifyRegistered=closeSent=cancelSent=stable=haveValue=false;characteristic=connection=0;stage=Stage::Idle;
}
void audioRequested(){veto.store(true,std::memory_order_release);}
bool exclusive(){return stage.load()!=Stage::Idle;}
bool faulted(){return stage==Stage::Fault;}
bool quiet(){return !exclusive()||(stage==Stage::Fault&&!faultBusy);}
bool take(Reading& out){if(!haveValue)return false;out=reading;haveValue=false;return true;}
void diagnostics(){Serial.printf("[BLE-DIRECT] stage=%u attempts=%lu success=%lu failures=%lu aborts=%lu deferred=%lu interval_ms=60000 idle_ms=15000 timeout_ms=15000 required_free=32768 required_largest=16384 last_reason=%s last_stage=%u retry_in_ms=%lu\n",unsigned(stage.load()),(unsigned long)attempts,(unsigned long)successes,(unsigned long)failures,(unsigned long)aborts,(unsigned long)deferred,lastReason.load(),lastStage.load(),(unsigned long)(attempted&&uint32_t(millis()-lastAttempt)<directIntervalMs?directIntervalMs-uint32_t(millis()-lastAttempt):0));}
void service(const Config& c,bool idle,bool ready,bool scanQuiet,uint32_t now){
 if(veto.exchange(false)){vetoAt=now;stable=false;haveValue=false;if(exclusive()){++aborts;closing(now,"audio_request");}}
 const bool held=vetoAt&&uint32_t(now-vetoAt)<15000;
 const bool eligible=idle&&c.enabled&&c.selected&&c.keyValid&&!held;
 if(!eligible){stable=false;haveValue=false;}
 Mail m;portENTER_CRITICAL(&mux);m=mail;mail.flags=0;portEXIT_CRITICAL(&mux);
 if(m.flags&Data){memcpy(pendingSample,m.bytes,5);samplePending=true;}
 if(m.flags&Reg){registering=false;if(m.regStatus==ESP_GATT_OK){interfaceId=m.iface;if(stage==Stage::Registering)stage=Stage::WaitScan;}else{++failures;stage=Stage::Fault;}}
 if(m.flags&Open)openPending=false;
 if(m.flags&Cancelled&&m.cancelStatus==ESP_GATT_OK)openPending=false;
 if(m.flags&Down){const bool fault=stage==Stage::Fault;finish();if(fault){stage=Stage::Fault;faultBusy=false;}return;}
 if(stage==Stage::Fault){faultBusy=m.link||openPending||registering;if(m.link&&!closeSent){closeSent=true;esp_ble_gattc_close(interfaceId,m.conn);}return;}
 if(stage==Stage::Idle&&!eligible)return; // No heap polling in playback/disabled mode.
 if(exclusive()&&(!eligible||memcmp(address,c.address,6))&&stage!=Stage::Closing){++aborts;haveValue=false;closing(now,"foreground_or_selection");}
 const unsigned heap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
 if(exclusive()&&stage!=Stage::Closing&&(!directBudget(true,heap,largest,uxTaskGetStackHighWaterMark(nullptr)))){++aborts;closing(now,"memory_or_stack");}
 if(exclusive()&&stage!=Stage::Closing&&uint32_t(now-started)>=directTimeoutMs){++failures;closing(now,"timeout");}
 if(stage==Stage::Closing){
  if(!m.link&&!openPending&&!registering){finish();return;}
  if(m.link){if(!closeSent){if(notifyRegistered&&characteristic)esp_ble_gattc_unregister_for_notify(interfaceId,address,characteristic);closeSent=true;if(esp_ble_gattc_close(interfaceId,m.conn)!=ESP_OK){stage=Stage::Fault;faultBusy=true;}}}
  else if((m.flags&Cancelled)&&m.cancelStatus==ESP_GATT_OK){finish();return;}
  else if((m.flags&Open)&&m.openStatus!=ESP_GATT_OK){finish();return;}
  else if(!cancelSent&&interfaceId!=ESP_GATT_IF_NONE){esp_ble_gattc_cancel_open_params_t p{};p.gattc_if=interfaceId;memcpy(p.remote_bda,address,6);cancelSent=true;if(esp_ble_gattc_cancel_open(&p)!=ESP_OK){stage=Stage::Fault;faultBusy=openPending;}}
  else if(interfaceId==ESP_GATT_IF_NONE&&(m.flags&Reg)){finish();return;}
  if(uint32_t(now-closingAt)>=directCleanupMs){stage=Stage::Fault;faultBusy=m.link||openPending||registering;haveValue=false;Serial.println("[BLE-DIRECT] result=FAULT cleanup_unconfirmed=1 no_retry=1");}
  return;
 }
 if((m.flags&ServiceChanged)&&m.link&&stage>=Stage::Searching&&stage<=Stage::Waiting){
  samplePending=false;
  if(serviceChanges++){++failures;closing(now,"service_changed_repeat");return;}
  notifyRegistered=false;characteristic=0;
  portENTER_CRITICAL(&mux);mail.start=mail.end=mail.handle=mail.descriptor=0;mail.flags=0;portEXIT_CRITICAL(&mux);
  stage=Stage::Searching;auto service=uuid(0xb0);
  Serial.println("[BLE-GATT] phase=SERVICE_CHANGED action=REDISCOVER deadline_unchanged=1");
  if(esp_ble_gattc_search_service(interfaceId,connection,&service)!=ESP_OK){++failures;closing(now,"service_changed_search");}return;
 }
 if(stage==Stage::Idle){
  if(!eligible){stable=false;return;}if(!stable){stable=true;stableAt=now;return;}
  if(uint32_t(now-stableAt)<directIdleMs||!ready||(!attempted&&hasFreshPair(reading,now)&&uint32_t(now-reading.receivedAt)<directIntervalMs)||!directDue(true,attempted,now,lastAttempt))return;
  if(!directBudget(true,heap,largest,uxTaskGetStackHighWaterMark(nullptr))){if(uint32_t(now-loggedAt)>=30000){loggedAt=now;++deferred;Serial.printf("[BLE-DIRECT] result=DEFER free=%u largest=%u\n",heap,largest);}return;}
  if(!acquisitionCooldown().claim(now))return;
  Serial.println("[BLE-CADENCE] acquisition=DIRECT minimum_start_gap_ms=60000");
  samplePending=false;serviceChanges=0;attempted=true;lastAttempt=started=now;++attempts;memcpy(address,c.address,6);haveValue=false;stage=Stage::WaitScan;Serial.printf("[BLE-DIRECT] result=BEGIN attempt=%lu free=%u largest=%u\n",(unsigned long)attempts,heap,largest);
 }
 if(stage==Stage::WaitScan&&scanQuiet){
  if(interfaceId==ESP_GATT_IF_NONE){
   if(!callbackSet){if(esp_ble_gattc_register_callback(callback)!=ESP_OK){++failures;stage=Stage::Fault;return;}callbackSet=true;}
   stage=Stage::Registering;registering=true;if(esp_ble_gattc_app_register(appId)!=ESP_OK){registering=false;++failures;stage=Stage::Fault;}return;
  }
  portENTER_CRITICAL(&mux);mail.active=true;mail.link=false;mail.start=mail.end=mail.handle=mail.descriptor=0;mail.flags=0;memcpy(mail.address,address,6);portEXIT_CRITICAL(&mux);
  stage=Stage::Opening;openPending=true;if(esp_ble_gattc_open(interfaceId,address,BLE_ADDR_TYPE_PUBLIC,true)!=ESP_OK){openPending=false;++failures;finish();}return;
 }
 if(stage==Stage::Opening&&(m.flags&Open)){
  if(m.openStatus!=ESP_GATT_OK){++failures;finish();return;}connection=m.conn;stage=Stage::Searching;auto service=uuid(0xb0);if(esp_ble_gattc_search_service(interfaceId,connection,&service)!=ESP_OK){++failures;closing(now,"service_search");}return;
 }
 if(stage==Stage::Searching&&(m.flags&Search)){
  esp_gattc_char_elem_t element{};uint16_t count=1;
  if(m.searchStatus!=ESP_GATT_OK||!m.start||!m.end||esp_ble_gattc_get_char_by_uuid(interfaceId,connection,m.start,m.end,uuid(0xc1),&element,&count)!=ESP_GATT_OK||count!=1||!(element.properties&ESP_GATT_CHAR_PROP_BIT_NOTIFY)){++failures;closing(now,"characteristic");return;}
  characteristic=element.char_handle;Serial.printf("[BLE-GATT] phase=CHAR iface=%u conn=%u value_handle=0x%04x properties=0x%02x\n",unsigned(interfaceId),connection,characteristic,element.properties);portENTER_CRITICAL(&mux);mail.handle=characteristic;portEXIT_CRITICAL(&mux);stage=Stage::NotifyRegister;
  if(esp_ble_gattc_register_for_notify(interfaceId,address,characteristic)!=ESP_OK){++failures;closing(now,"notify_register");}return;
 }
 if(stage==Stage::NotifyRegister&&(m.flags&Subscribed)){
  Serial.printf("[BLE-GATT] phase=REGISTER value_handle=0x%04x status=%d\n",characteristic,m.notifyStatus);
  if(m.notifyStatus!=ESP_GATT_OK){++failures;closing(now,"notify_status");return;}notifyRegistered=true;
  esp_bt_uuid_t descriptor{};descriptor.len=ESP_UUID_LEN_16;descriptor.uuid.uuid16=ESP_GATT_UUID_CHAR_CLIENT_CONFIG;esp_gattc_descr_elem_t element{};uint16_t count=1;
  if(esp_ble_gattc_get_descr_by_char_handle(interfaceId,connection,characteristic,descriptor,&element,&count)!=ESP_GATT_OK||count!=1){++failures;closing(now,"descriptor");return;}
  portENTER_CRITICAL(&mux);mail.descriptor=element.handle;portEXIT_CRITICAL(&mux);
  Serial.printf("[BLE-GATT] phase=CCCD value_handle=0x%04x descriptor_handle=0x%04x\n",characteristic,element.handle);
  uint8_t enable[2]={1,0};stage=Stage::Descriptor;if(esp_ble_gattc_write_char_descr(interfaceId,connection,element.handle,2,enable,ESP_GATT_WRITE_TYPE_RSP,ESP_GATT_AUTH_REQ_NONE)!=ESP_OK){++failures;closing(now,"subscribe");}return;
 }
 if(stage==Stage::Descriptor&&(m.flags&Written)){Serial.printf("[BLE-GATT] phase=CCCD_ACK status=%d buffered_sample=%u\n",m.writeStatus,unsigned(samplePending));if(m.writeStatus!=ESP_GATT_OK){++failures;closing(now,"subscribe_status");return;}stage=Stage::Waiting;}
 if(stage==Stage::Waiting&&samplePending){
  samplePending=false;
  Reading candidate;if(eligible&&directValue(pendingSample,5,now,candidate)){reading=candidate;haveValue=true;++successes;Serial.printf("[BLE-DIRECT] result=READ_OK source=GATT_NOTIFY temperature_centi=%d humidity_centi=%u battery_mv=%u\n",int(candidate.temperatureCenti),unsigned(candidate.humidityCenti),unsigned(pendingSample[3])|(unsigned(pendingSample[4])<<8));closing(now,"sample_received");}
  else{++failures;closing(now,"invalid_sample");}
 }
}
}}
#else
namespace sensors {namespace direct {void stackStopped(){}void service(const radiosensor::Config&,bool,bool,bool,uint32_t){}bool exclusive(){return false;}bool quiet(){return true;}bool faulted(){return false;}void audioRequested(){}bool take(radiosensor::Reading&){return false;}void diagnostics(){}}}
#endif
