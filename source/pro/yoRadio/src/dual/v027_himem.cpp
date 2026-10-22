// Upper external-RAM test. No flash/NVS/filesystem writes.
#include "v027_himem.h"
#include "../core/player.h"
#include <Arduino.h>
#include <esp_psram.h>
#include <esp32/himem.h>
#include <esp_heap_caps.h>
#include <sdkconfig.h>
#if !CONFIG_SPIRAM || !CONFIG_SPIRAM_BANKSWITCH_ENABLE
#error V027 requires enabled PSRAM and HiMem support
#endif
namespace v027 {
static MemoryReport report{};
const MemoryReport& memoryReport(){return report;}
static uint32_t pattern(size_t word, unsigned pass) {
    return (uint32_t(word)*0x9e3779b1u) ^ (pass ? 0x5aa5c33cu : 0xa55a3cc3u);
}
void testHighMemory(){
    report.highResult="NOT_RUN";
    report.highTested=0;
    report.errors=0;
    report.physical=esp_psram_get_size();
    report.mappedHeap=heap_caps_get_total_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    report.mapWindow=esp_himem_reserved_area_size();
    // These APIs refer to the HiMem physical pool, NOT another ordinary malloc heap.
    if(report.physical <= 4u*1024u*1024u || !report.mapWindow){
        Serial.printf("[HIMEM] SELFTEST=NOT_RUN physical=%u window=%u reason=no_high_memory\n",
            unsigned(report.physical),unsigned(report.mapWindow));return;
    }
    report.highTotal=esp_himem_get_phys_size();
    report.highFree=esp_himem_get_free_size();
    Serial.printf("[PSRAM-CAPACITY] physical=%u mapped_heap=%u map_window=%u himem_total=%u himem_free=%u\n",
        unsigned(report.physical),unsigned(report.mappedHeap),unsigned(report.mapWindow),unsigned(report.highTotal),unsigned(report.highFree));
    const size_t block=ESP_HIMEM_BLKSZ;
    size_t tested=report.highFree/block*block;
    if(tested<block || tested>8u*1024u*1024u || report.highTotal>report.physical){
        Serial.println("[HIMEM] SELFTEST=NOT_RUN reason=unusable_pool");return;
    }
    esp_himem_handle_t memory=nullptr;
    esp_himem_rangehandle_t window=nullptr;
    esp_err_t rc=esp_himem_alloc_map_range(block,&window);
    if(rc==ESP_OK)rc=esp_himem_alloc(tested,&memory);
    void* mapped=nullptr;
    size_t verified=0;
    // First write EVERY bank, then revisit in reverse order. Bank aliases cannot
    // pass merely because each bank was read immediately after it was written.
    for(unsigned pass=0;pass<2 && rc==ESP_OK && report.errors==0;pass++){
        for(size_t offset=0;offset<tested && rc==ESP_OK;offset+=block){
            if(player.isRunning()||player.status()==PLAYING){rc=ESP_ERR_INVALID_STATE;break;}
            rc=esp_himem_map(memory,window,offset,0,block,0,&mapped);
            if(rc!=ESP_OK)break;
            volatile uint32_t* words=static_cast<volatile uint32_t*>(mapped);
            for(size_t i=0;i<block/4;i++)words[i]=pattern(offset/4+i,pass);
            rc=esp_himem_unmap(window,mapped,block);
            if(rc==ESP_OK)mapped=nullptr;
            delay(1);
        }
        for(size_t end=tested;end && rc==ESP_OK && report.errors==0;end-=block){
            if(player.isRunning()||player.status()==PLAYING){rc=ESP_ERR_INVALID_STATE;break;}
            const size_t offset=end-block;
            rc=esp_himem_map(memory,window,offset,0,block,0,&mapped);
            if(rc!=ESP_OK)break;
            volatile const uint32_t* words=static_cast<volatile const uint32_t*>(mapped);
            for(size_t i=0;i<block/4;i++){
                const uint32_t got=words[i],want=pattern(offset/4+i,pass);
                if(got!=want){
                    if(report.errors++==0)Serial.printf("[HIMEM] MISMATCH byte=%u got=%08lx expected=%08lx\n",
                        unsigned(offset+i*4),(unsigned long)got,(unsigned long)want);
                    if(report.errors>=16)break;
                }
            }
            rc=esp_himem_unmap(window,mapped,block);
            if(rc==ESP_OK)mapped=nullptr;
            if(!report.errors)verified+=block;
            delay(1);
        }
        Serial.printf("[HIMEM] phase=%u verified_cumulative=%u errors=%lu rc=%s\n",pass+1,
            unsigned(verified),(unsigned long)report.errors,esp_err_to_name(rc));
    }
    bool released=true;
    if(mapped){
        const esp_err_t e=esp_himem_unmap(window,mapped,block);
        if(e==ESP_OK)mapped=nullptr;else{released=false;rc=e;}
    }
    if(!mapped){
        if(memory && esp_himem_free(memory)!=ESP_OK)released=false;
        if(window && esp_himem_free_map_range(window)!=ESP_OK)released=false;
    }
    report.highFree=esp_himem_get_free_size();
    const bool ok=rc==ESP_OK && !report.errors && verified==tested*2 && released;
    report.highTested=ok?tested:0;
    report.highResult=ok?"PASS":"FAIL";
    Serial.printf("[HIMEM] SELFTEST=%s unique_bytes=%u verified_bytes=%u passes=2 errors=%lu rc=%s released=%d free_after=%u\n",
        report.highResult,unsigned(tested),unsigned(verified),(unsigned long)report.errors,
        esp_err_to_name(rc),int(released),unsigned(report.highFree));
}
}
