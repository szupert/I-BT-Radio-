#if IBT_COMPACT_UI
#include "ir_receiver.h"
#include "../core/player.h"
#include "bt_audio.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <driver/rmt_rx.h>
#include <freertos/queue.h>
namespace irreceiver {
static smallir::Learner learner;
static rmt_channel_handle_t channel=nullptr;
static QueueHandle_t queue=nullptr;
// ISR-owned storage is internal RAM, never PSRAM. Only the UI owner decodes.
static rmt_symbol_word_t symbols[64];
static smallir::Pulse pulses[64];
static bool loaded=false,confirmed=false,enabled=false,pendingSave=false,armed=false;
static uint32_t saveAttempt=0;
static const char* status="GPIO36 nem ellenorzott; IR kikapcsolva";
static const char* names[]={"","Halkitas","Hangositas","Elozo","Kovetkezo","Play / Pause","Menu","Vissza","Kivalasztas","Forras","Ora","Nema"};
static bool IRAM_ATTR received(rmt_channel_handle_t,const rmt_rx_done_event_data_t* data,void*){
 BaseType_t wake=pdFALSE;xQueueSendFromISR(queue,data,&wake);return wake==pdTRUE;
}
static void stop(){
 if(channel){rmt_disable(channel);rmt_del_channel(channel);channel=nullptr;}
 if(queue){vQueueDelete(queue);queue=nullptr;}
 enabled=armed=false;learner.cancel();
}
static bool arm(){
 rmt_receive_config_t cfg{};cfg.signal_range_min_ns=100000;cfg.signal_range_max_ns=12000000;
 armed=rmt_receive(channel,symbols,sizeof(symbols),&cfg)==ESP_OK;return armed;
}
static bool start(){
 if(player.isRunning()||player.status()!=STOPPED||btaudio::wanted()||uxTaskGetStackHighWaterMark(nullptr)<3072){status="IR inditas csak nyugalomban";return false;}
 if(!confirmed){status="Elobb GPIO36 / touch IRQ ellenorzes";return false;}
 if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<32768||
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<16384){
  status="RAM tartalek miatt halasztva";return false;
 }
 queue=xQueueCreate(1,sizeof(rmt_rx_done_event_data_t));if(!queue){status="IR sor foglalasi hiba";return false;}
 rmt_rx_channel_config_t cfg{};cfg.gpio_num=GPIO_NUM_36;cfg.clk_src=RMT_CLK_SRC_DEFAULT;
 cfg.resolution_hz=1000000;cfg.mem_block_symbols=64;
 if(rmt_new_rx_channel(&cfg,&channel)!=ESP_OK){stop();status="IR RMT foglalasi hiba";return false;}
 rmt_rx_event_callbacks_t cb{};cb.on_recv_done=received;
 if(rmt_rx_register_event_callbacks(channel,&cb,nullptr)!=ESP_OK||rmt_enable(channel)!=ESP_OK||!arm()){
  stop();status="IR inditasi hiba";return false;
 }
 if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<32768||
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<16384){
  stop();status="IR RAM tartalek nem eleg";return false;
 }
 enabled=true;status="NEC / extended NEC vetel";return true;
}
struct Stored {uint32_t magic;smallir::Binding slots[12];};
static void load(){
 if(loaded)return;loaded=true;
 Preferences p;if(!p.begin("ibt_ir",true))return;
 Stored s{};if(p.getBytesLength("keys")==sizeof(s)&&p.getBytes("keys",&s,sizeof(s))==sizeof(s)&&
    s.magic==0x49523168)learner.restore(s.slots,12);
 p.end();
 // Never arm GPIO36 from old NVS: explicit physical confirmation each boot.
}
void cancel(){learner.cancel();}
void menu(unsigned item){
 load();
 if(item==0){if(enabled){stop();status="IR kikapcsolva";}else start();return;}
 if(item==1){if(!enabled){confirmed=!confirmed;status=confirmed?"GPIO36 leválasztása jelölve":"GPIO36 nem ellenorzott";}return;}
 if(item>=2&&item<=12){if(!enabled){status="Elobb IR bekapcsolas";return;}
  learner.begin(smallir::Action(item-1),millis());status="Nyomd meg ketszer, kulon nyomassal";return;}
 if(item==13){if(learner.commit()){pendingSave=true;status="Mentesre var: lejatszas STOP";}return;}
 if(item==14){cancel();status="Tanitas megszakitva";return;}
}
void label(char* out,size_t cap,unsigned item){
 load();const auto st=learner.status();
 if(item==0)snprintf(out,cap,"IR vevo: %s",enabled?"BE":"KI");
 else if(item==1)snprintf(out,cap,"GPIO36 / IRQ levalasztva: %s",confirmed?"IGEN":"NEM");
 else if(item>=2&&item<=12){const auto& b=learner.bindings()[item-1];
  snprintf(out,cap,"%s: %s",names[item-1],b.action!=smallir::Action::None?"tanitva":"--");}
 else if(item==13)snprintf(out,cap,"Mentes %s",st==smallir::Learn::Ready?"(kesz)":"");
 else if(item==14)snprintf(out,cap,"Megse");
 else if(item==15)snprintf(out,cap,"%s",st==smallir::Learn::Ready?"Ket nyomas OK - Mentes":
  st==smallir::Learn::Collision?"Mar mas funkciohoz tanitva":
  st==smallir::Learn::Timeout?"Tanitas idotullepes":status);
 else snprintf(out,cap,"Vissza");
}
smallir::Action tick(bool saveAllowed){
 load();learner.tick(millis());
 if(pendingSave&&saveAllowed&&uint32_t(millis()-saveAttempt)>=5000&&
    heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=32768){
  saveAttempt=millis();
  Stored s{};s.magic=0x49523168;memcpy(s.slots,learner.bindings(),sizeof(s.slots));
  Preferences p;
  if(p.begin("ibt_ir",false)){
   Stored check{};const bool ok=p.putBytes("keys",&s,sizeof(s))==sizeof(s)&&
    p.getBytes("keys",&check,sizeof(check))==sizeof(check)&&!memcmp(&s,&check,sizeof(s));p.end();
   if(ok){pendingSave=false;status="IR kiosztas elmentve";}else status="IR mentesi hiba";
  }
 }
 if(!enabled)return smallir::Action::None;
 rmt_rx_done_event_data_t event{};
 if(xQueueReceive(queue,&event,0)!=pdTRUE)return smallir::Action::None;
 armed=false;smallir::Frame frame{};
 if(event.num_symbols<=64){
  bool levels=true;
  for(size_t i=0;i<event.num_symbols;++i){const auto& s=event.received_symbols[i];
   if(s.level0!=0||(s.duration1&&s.level1!=1))levels=false;
   pulses[i]={uint16_t(s.duration0),uint16_t(s.duration1)};
  }
  if(levels)frame=smallir::nec(pulses,event.num_symbols);
 }
 const auto action=learner.receive(frame,millis());
 if(!arm()){stop();status="IR ujrainditasi hiba; kikapcsolva";}
 return action;
}
}
#endif
