#include "../core/options.h"
#include "v026_probe.h"
#include "v027_himem.h"
#include "r10_diag.h"
#include "core.h"
#include "../core/network.h"
#include "../core/player.h"
#include <BluetoothSerial.h>
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <WiFi.h>
#include <sdkconfig.h>
#if !CONFIG_SPIRAM || CONFIG_D0WD_PSRAM_CLK_IO != 6 || CONFIG_D0WD_PSRAM_CS_IO != 16
#error V026 requires source-built SDK PSRAM CLK=SD_CLK/GPIO6 CS=GPIO16
#endif
#if defined(CONFIG_ESP_WIFI_IRAM_OPT) && CONFIG_ESP_WIFI_IRAM_OPT
#error V026 requires SDK WiFi IRAM optimization disabled to fit Classic BT
#endif
static_assert(DUAL_ENCODER_A==26 && DUAL_ENCODER_B==35 && DUAL_ENCODER_SW==4,"rotary pin lock");
static_assert(I2S_BCLK==22 && I2S_DOUT==17 && I2S_LRC==27,"PCM pin lock");
namespace v026 {
static uint32_t* hold=nullptr; static size_t holdBytes=0; static bool memoryPass=false;
static BluetoothSerial bt; static bool btStarted=false; static char name[40];
size_t held(){return holdBytes;} bool memoryOK(){return memoryPass;}
bool btOK(){return btStarted;} const char* btName(){return name;}
static uint32_t pattern(size_t i){return (uint32_t(i)*0x9e3779b1u)^0xa55a3cc3u;}
void early(){
 v027::testHighMemory();
 const esp_partition_t* part=esp_ota_get_running_partition();
 Serial.printf("[V027] BOOT app_offset=0x%08lx psram_clk=6 psram_cs=16\n",part?(unsigned long)part->address:0UL);
 const bool found=psramFound() || psramInit();
 const size_t available=heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 Serial.printf("[PSRAM] found=%d total=%u free=%u largest=%u\n",found,(unsigned)ESP.getPsramSize(),(unsigned)ESP.getFreePsram(),(unsigned)available);
 if(!found || available<131072){Serial.println("[PSRAM] SELFTEST=NOT_RUN reason=no_usable_external_ram");return;}
 size_t wanted=available/2; if(wanted>1024*1024)wanted=1024*1024; wanted&=~size_t(3);
 hold=(uint32_t*)heap_caps_malloc(wanted,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!hold){Serial.println("[PSRAM] SELFTEST=FAIL reason=allocation");return;}
 volatile uint32_t* checked=hold;
 for(size_t i=0;i<wanted/4;i++){checked[i]=pattern(i);if((i&8191)==0)delay(1);}
 size_t errors=0;
 for(size_t i=0;i<wanted/4;i++){if(checked[i]!=pattern(i)){if(++errors==16)break;}if((i&8191)==0)delay(1);}
 memoryPass=errors==0;
 if(memoryPass)holdBytes=wanted;else{free(hold);hold=nullptr;}
 Serial.printf("[PSRAM] SELFTEST=%s checked=%u held=%u errors=%u\n",memoryPass?"PASS":"FAIL",(unsigned)wanted,(unsigned)holdBytes,(unsigned)errors);
}
void beginBt(){
 if(btStarted)return;
 if(player.isRunning()||player.status()==PLAYING){Serial.println("[BT-SPP] START=DEFERRED reason=AUDIO_ACTIVE");return;}
 snprintf(name,sizeof(name),"I-BT-Radio-DIAG-%06lX",(unsigned long)(ESP.getEfuseMac()&0xffffff));
 // The source-built controller is BR/EDR-only. The default begin(name)
 // requests BTDM, which is incompatible with that controller configuration.
 Serial.println("[BT-SPP] REQUEST mode=CLASSIC_BT disableBLE=1 A2DP=OFF");
 btStarted=bt.begin(name,false,true);
 Serial.printf("[BT-SPP] START=%s name=%s A2DP=OFF\n",btStarted?"PASS":"FAIL",name);
}
void tick(){
 network.service();
 static uint32_t last=0; const uint32_t now=millis();
 if(uint32_t(now-last)>=1000){
  last=now;
  if(dual::debugEnabled()){
   const auto& mem=v027::memoryReport();
   Serial.printf("[HIMEM-STATUS] physical=%u mapped_heap=%u window=%u high_total=%u high_free=%u high_test=%s checked=%u errors=%lu\n",unsigned(mem.physical),unsigned(mem.mappedHeap),unsigned(mem.mapWindow),unsigned(mem.highTotal),unsigned(mem.highFree),mem.highResult?mem.highResult:"NOT_RUN",unsigned(mem.highTested),(unsigned long)mem.errors);
   Serial.printf("[V027] HEARTBEAT ms=%lu wifi=%d playing=%d psram_total=%u psram_free=%u psram_hold=%u psram_test=%s bt_spp=%d bt_client=%d heap_internal=%u\n",(unsigned long)now,(int)WiFi.status(),(int)player.isRunning(),(unsigned)ESP.getPsramSize(),(unsigned)ESP.getFreePsram(),(unsigned)holdBytes,memoryPass?"PASS":"NOT_PASS",btStarted,(int)(btStarted&&bt.hasClient()),(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  }
 }
 r10diag::serialPoll();
 r10diag::tick();
 // Bounded echo only; SPP is explicitly a pairing/data diagnostic, NOT A2DP audio.
 if(btStarted){for(unsigned n=0;n<64&&bt.available();++n)bt.write((uint8_t)bt.read());}
}
}
