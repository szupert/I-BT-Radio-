#include "../core/options.h"
#if IBT_NO_PSRAM
#include "artwork_ab.h"
#include <Arduino.h>
namespace artab {bool prepare(){return false;}bool setRows(unsigned){return false;}bool set(bool){return false;}
bool on(){return false;}void stop(){}void tick(uint32_t){}bool consumeRepaint(){return false;}
void diagnostics(){Serial.println("[ART-AB] compiled=0 reason=LIGHT_DISABLED");}}
#else
#include "../core/options.h"
#include "artwork_ab.h"
#include "online_artwork.h"
#include "bt_audio.h"
#include "../ui/frontend.h"
#include "artwork_ab_asset.h"
#include <Arduino.h>
#include <JPEGDEC.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <new>
#include <cstring>
namespace artab {
static unsigned batchRows=4;
bool setRows(unsigned n){if(n!=4&&n!=8&&n!=16&&n!=32&&n!=64&&n!=128&&n!=240)return false;batchRows=n;return true;}
struct State {
 uint16_t *frame=nullptr;uint8_t *jpegBytes=nullptr;JPEGDEC *decoder=nullptr;
 size_t jpegLength=0;bool active=false,ready=false,repaint=false;
 unsigned row=240,decodedW=0,decodedH=0;
 uint32_t nextDecode=0,nextRow=0,decodes=0,renders=0,failures=0;
 uint64_t decodeUs=0,drawUs=0;uint32_t decodeMax=0,drawMax=0;
 uint32_t frameStart=0,lastRow=0,rowGapMax=0,frameMax=0;
 const char* source="NONE";
};
static State* state=nullptr;
static int draw(JPEGDRAW *d){
    if(!state->frame||!state->decodedW||!state->decodedH||d->x<0||d->y<0||d->iWidth<=0||d->iHeight<=0)return 0;
    unsigned w=320,h=state->decodedH*320/state->decodedW;
    if(h>240){h=240;w=state->decodedW*240/state->decodedH;}
    if(!w||!h)return 0;
    const unsigned x0=unsigned(d->x)*w/state->decodedW,x1=unsigned(d->x+d->iWidth)*w/state->decodedW;
    const unsigned y0=unsigned(d->y)*h/state->decodedH,y1=unsigned(d->y+d->iHeight)*h/state->decodedH;
    const unsigned padX=(320-w)/2,padY=(240-h)/2;
    for(unsigned y=y0;y<y1&&y<h;y++){
        int sy=int(y*state->decodedH/h)-d->y;if(sy<0)sy=0;if(sy>=d->iHeight)sy=d->iHeight-1;
        for(unsigned x=x0;x<x1&&x<w;x++){
            int sx=int(x*state->decodedW/w)-d->x;if(sx<0)sx=0;if(sx>=d->iWidth)sx=d->iWidth-1;
            state->frame[(padY+y)*320+padX+x]=d->pPixels[sy*d->iWidth+sx];
        }
    }
    vTaskDelay(1);return 1;
}
static bool decode(){
    if(!state->decoder||!state->frame||!state->jpegBytes)return false;
    const auto start=esp_timer_get_time();memset(state->frame,0,320*240*2);
    bool ok=false;
    if(state->decoder->openRAM(state->jpegBytes,int(state->jpegLength),draw)){
        unsigned w=state->decoder->getWidth(),h=state->decoder->getHeight(),scale=1;int flags=0;
        if(w&&h&&w<=2560&&h<=1920){
            while((w+scale-1)/scale>320||(h+scale-1)/scale>240){scale*=2;if(scale>8)break;}
            if(scale<=8){flags=scale==8?JPEG_SCALE_EIGHTH:scale==4?JPEG_SCALE_QUARTER:scale==2?JPEG_SCALE_HALF:0;
                state->decodedW=(w+scale-1)/scale;state->decodedH=(h+scale-1)/scale;
                state->decoder->setPixelType(RGB565_LITTLE_ENDIAN);ok=state->decoder->decode(0,0,flags)!=0;}
        }
        state->decoder->close();
    }
    uint32_t us=uint32_t(esp_timer_get_time()-start);state->decodeUs+=us;if(us>state->decodeMax)state->decodeMax=us;
    if(ok)state->decodes++;else state->failures++;
    Serial.printf("[ART-AB-DECODE] result=%s core=%d wall_us=%u bytes=%u source=%s\n",ok?"PASS":"FAIL",xPortGetCoreID(),us,unsigned(state->jpegLength),state->source);
    return ok;
}
bool prepare(){
    if(btaudio::wanted()||!btaudio::idle()||xPortGetCoreID()!=0)return false;
    if(!state){void* memory=heap_caps_malloc(sizeof(State),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!memory)return false;state=new(memory)State();}
    if(state->ready)return true;
    if(!ESP.getPsramSize()||heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<24576){stop();return false;}
    // UI owns cached cover; copy once before BT, then freeze it for the A/B comparison.
    uint8_t *copy=nullptr;size_t n=0;
    if(onlineart::copyCached(copy,n)){state->jpegBytes=copy;state->jpegLength=n;state->source="CACHED_COVER";}
    else {state->jpegLength=sizeof(artworkAbJpeg);state->jpegBytes=(uint8_t*)heap_caps_malloc(state->jpegLength,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(state->jpegBytes){memcpy(state->jpegBytes,artworkAbJpeg,state->jpegLength);state->source="BUNDLED_CONTROL";}}
    state->frame=(uint16_t*)heap_caps_malloc(320*240*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    void* storage=heap_caps_malloc(sizeof(JPEGDEC),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(storage)state->decoder=new(storage)JPEGDEC();
    if(!state->frame||!state->jpegBytes||!state->decoder){stop();return false;}
    state->ready=decode();
    if(!state->ready){stop();return false;}
    Serial.printf("[ART-AB-PREPARE] result=PASS core=%d source=%s psram_frame=153600 jpeg_bytes=%u fetches=0\n",xPortGetCoreID(),state->source,unsigned(state->jpegLength));
    return true;
}
bool set(bool enabled){
    if(!state)return !enabled;
    if(enabled&&!state->ready)return false;
    state->active=enabled;state->row=enabled?0:240;state->nextDecode=millis()+30000;state->nextRow=millis();state->repaint=true;
    Serial.printf("[ART-AB-SET] enabled=%u ready=%u source=%s generation_policy=FROZEN repeat_ms=30000\n",unsigned(state->active),unsigned(state->ready),state->source);
    return true;
}
bool on(){return state&&state->active;}
void stop(){batchRows=4;if(!state)return;state->active=false;state->ready=false;state->row=240;state->repaint=true;
    if(state->decoder){state->decoder->~JPEGDEC();free(state->decoder);state->decoder=nullptr;}free(state->frame);state->frame=nullptr;free(state->jpegBytes);state->jpegBytes=nullptr;state->jpegLength=0;state->source="NONE";state->~State();free(state);state=nullptr;}
void tick(uint32_t now){
    if(!state||!state->active||!state->ready)return;
    if(state->row>=240&&int32_t(now-state->nextDecode)>=0){state->nextDecode=now+30000;
        if(!decode()){state->active=false;state->repaint=true;return;}state->row=0;}
    if(state->row<240&&int32_t(now-state->nextRow)>=0){
        state->nextRow=now+20;unsigned n=240-state->row<batchRows?240-state->row:batchRows;
        if(state->row==0){state->frameStart=now;state->lastRow=now;}else{const uint32_t gap=now-state->lastRow;if(gap>state->rowGapMax)state->rowGapMax=gap;state->lastRow=now;}
        const auto start=esp_timer_get_time();front::artworkTestRows(state->row,n,state->frame+state->row*320);
        uint32_t us=uint32_t(esp_timer_get_time()-start);state->drawUs+=us;if(us>state->drawMax)state->drawMax=us;state->row+=n;
        if(state->row==240){const uint32_t frameMs=millis()-state->frameStart;if(frameMs>state->frameMax)state->frameMax=frameMs;
            Serial.printf("[DISPLAY-FRAME] wall_ms=%lu max_wall_ms=%lu max_row_gap_ms=%lu batch_rows=%u\n",(unsigned long)frameMs,(unsigned long)state->frameMax,(unsigned long)state->rowGapMax,batchRows);
            state->renders++;state->repaint=true;Serial.printf("[ART-AB-FRAME] result=PASS core=%d frames=%u draw_wall_us=%llu\n",xPortGetCoreID(),state->renders,(unsigned long long)state->drawUs);}
    }
}
bool consumeRepaint(){if(!state)return false;bool result=state->repaint;state->repaint=false;return result;}
void diagnostics(){if(!state){Serial.println("[ART-AB] enabled=0 ready=0 source=NONE decodes=0 frames=0 failures=0 decode_wall_us=0 draw_wall_us=0 fetches=0");return;}Serial.printf("[ART-AB] enabled=%u ready=%u core=%d source=%s decodes=%u frames=%u failures=%u decode_wall_us=%llu decode_max_us=%u draw_wall_us=%llu draw_max_us=%u pending_rows=%u fetches=0\n",unsigned(state->active),unsigned(state->ready),xPortGetCoreID(),state->source,state->decodes,state->renders,state->failures,(unsigned long long)state->decodeUs,state->decodeMax,(unsigned long long)state->drawUs,state->drawMax,240-state->row);}
}

#endif
