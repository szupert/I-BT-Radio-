#include "bt_media.h"
#include "bt_audio.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <JPEGDEC.h>
#include <new>
#include <algorithm>
#include <string.h>
namespace btmedia {
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static View info{};
static bool connected=false,capable=false,refresh=false,coverConnected=false;
static bool connecting=false,downloading=false,decodePending=false,workerBusy=false,attempted=false;
static bool discardTransfer=false,closing=false;
static char handle[8]={};
static uint8_t* compressed=nullptr;
static uint16_t* pixels=nullptr;
static constexpr size_t limit=32768;
static size_t length=0,jobLength=0;
static uint32_t transferGen=0,jobGen=0,started=0,lastPoll=0;
static bool allocTried=false;
static int imageW=0,imageH=0;
static void invalidate(){++info.generation;info.image=false;decodePending=false;attempted=false;}
static void copyText(char* dst,size_t cap,const uint8_t* src,int len){
    size_t n=src&&len>0?std::min(cap-1,size_t(len)):0;
    if(n)memcpy(dst,src,n);
    dst[n]=0;
    // Drop an incomplete UTF-8 tail; never expose a split multibyte glyph.
    if(n){size_t s=n-1;while(s&&((uint8_t(dst[s])&0xc0)==0x80))--s;
        unsigned char c=dst[s];unsigned need=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;
        if(n-s<need)dst[s]=0;}
}
void event(esp_avrc_ct_cb_event_t e,esp_avrc_ct_cb_param_t* p){
    portENTER_CRITICAL(&mux);
    if(e==ESP_AVRC_CT_CONNECTION_STATE_EVT){
        connected=p->conn_stat.connected;invalidate();info.title[0]=info.artist[0]=handle[0]=0;
        capable=coverConnected=connecting=downloading=closing=false;refresh=connected;lastPoll=0;
    }else if(connected&&e==ESP_AVRC_CT_REMOTE_FEATURES_EVT){
        capable=(p->rmt_feats.tg_feat_flag&ESP_AVRC_FEAT_FLAG_TG_COVER_ART)!=0;
    }else if(connected&&e==ESP_AVRC_CT_CHANGE_NOTIFY_EVT&&p->change_ntf.event_id==ESP_AVRC_RN_TRACK_CHANGE){
        invalidate();info.title[0]=info.artist[0]=handle[0]=0;refresh=true;
    }else if(connected&&e==ESP_AVRC_CT_METADATA_RSP_EVT){
        auto& m=p->meta_rsp;
        if(m.attr_id==ESP_AVRC_MD_ATTR_TITLE){char next[128];copyText(next,sizeof(next),m.attr_text,m.attr_length);
            if(strcmp(next,info.title)){invalidate();handle[0]=0;memcpy(info.title,next,sizeof(next));}}
        if(m.attr_id==ESP_AVRC_MD_ATTR_ARTIST)copyText(info.artist,sizeof(info.artist),m.attr_text,m.attr_length);
        if(m.attr_id==ESP_AVRC_MD_ATTR_COVER_ART){
            char next[8]={};bool valid=m.attr_text&&m.attr_length==7;
            for(int i=0;valid&&i<7;++i)valid=m.attr_text[i]>='0'&&m.attr_text[i]<='9';
            if(valid){memcpy(next,m.attr_text,7);if(strcmp(next,handle)){invalidate();memcpy(handle,next,8);}}
        }
    }else if(e==ESP_AVRC_CT_COVER_ART_STATE_EVT){
        coverConnected=connected&&p->cover_art_state.state==ESP_AVRC_COVER_ART_CONNECTED;
        connecting=false;if(!coverConnected){downloading=false;closing=false;}
    }else if(e==ESP_AVRC_CT_COVER_ART_DATA_EVT&&downloading){
        auto& d=p->cover_art_data;
        if(!connected||transferGen!=info.generation||d.status!=ESP_BT_STATUS_SUCCESS||
           (d.data_len&&!d.p_data)||d.data_len>limit-length)discardTransfer=true;
        if(!discardTransfer){if(d.data_len)memcpy(compressed+length,d.p_data,d.data_len);length+=d.data_len;}
        // Drain the old operation before requesting another handle.
        if(d.final){downloading=false;decodePending=!discardTransfer&&length>0;}
    }
    portEXIT_CRITICAL(&mux);
}
static int decodeTile(JPEGDRAW* d){
    portENTER_CRITICAL(&mux);bool valid=connected&&jobGen==info.generation;portEXIT_CRITICAL(&mux);
    if(!valid||!btaudio::inputArtworkSafe())return 0;
    // This JPEGDEC revision already shortens iWidth at the right crop edge;
    // iWidthUsed subtracts that edge again. Clamp the actual row stride instead.
    const int w=std::min(d->iWidth,imageW-d->x),h=std::min(d->iHeight,imageH-d->y);
    if(d->x<0||d->y<0||w<0||h<0)return 0;
    for(int y=0;y<h;++y)memcpy(pixels+(d->y+y+(side-imageH)/2)*side+d->x+(side-imageW)/2,
                              d->pPixels+y*d->iWidth,w*2);
    vTaskDelay(1);return 1;
}
static void decode(void*){
    bool ok=false;
    void* memory=heap_caps_malloc(sizeof(JPEGDEC),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(memory){auto jpeg=new(memory)JPEGDEC();memset(pixels,0,side*side*2);
        if(jpeg->openRAM(compressed,jobLength,decodeTile)){
            jpeg->setMaxOutputSize(1); // yield after each small MCU, not a wide strip
            int w=jpeg->getWidth(),h=jpeg->getHeight(),scale=1,flags=0;
            if(w>int(side)||h>int(side)){scale=2;flags=JPEG_SCALE_HALF;}
            if(w>int(side*scale)||h>int(side*scale)){scale=4;flags=JPEG_SCALE_QUARTER;}
            if(w>int(side*scale)||h>int(side*scale)){scale=8;flags=JPEG_SCALE_EIGHTH;}
            imageW=(w+scale-1)/scale;imageH=(h+scale-1)/scale;
            if(w>0&&h>0&&imageW<=int(side)&&imageH<=int(side))ok=jpeg->decode(0,0,flags)!=0;
            jpeg->close();
        }jpeg->~JPEGDEC();free(memory);
    }
    portENTER_CRITICAL(&mux);if(connected&&jobGen==info.generation)info.image=ok;workerBusy=false;portEXIT_CRITICAL(&mux);
    Serial.printf("[BT-ART] decode=%s physical=NOT_VALIDATED\n",ok?"READY":"SKIPPED");vTaskDelete(nullptr);
}
void service(bool active){
    if(!active){portENTER_CRITICAL(&mux);
        bool close=coverConnected||connecting;
        if(connected){connected=false;invalidate();info.title[0]=info.artist[0]=handle[0]=0;}
        coverConnected=connecting=downloading=false;portEXIT_CRITICAL(&mux);
        if(close)esp_avrc_ct_cover_art_disconnect();return;
    }
    const uint32_t now=millis();
    portENTER_CRITICAL(&mux);bool linked=connected;bool query=linked&&(refresh||uint32_t(now-lastPoll)>=10000);
    bool registerTrack=refresh;if(query){refresh=false;lastPoll=now;}portEXIT_CRITICAL(&mux);
    if(query){esp_avrc_ct_send_metadata_cmd(14,ESP_AVRC_MD_ATTR_TITLE|ESP_AVRC_MD_ATTR_ARTIST|ESP_AVRC_MD_ATTR_COVER_ART);
        if(registerTrack)esp_avrc_ct_send_register_notification_cmd(15,ESP_AVRC_RN_TRACK_CHANGE,0);}
    if(!linked)return;
    if(!allocTried){allocTried=true;
        if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=32768){
            compressed=(uint8_t*)heap_caps_malloc(limit,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            pixels=(uint16_t*)heap_caps_malloc(side*side*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            if(!compressed||!pixels){free(compressed);free(pixels);compressed=nullptr;pixels=nullptr;}
        }
    }
    if(!compressed)return; // Metadata continues without artwork memory.
    portENTER_CRITICAL(&mux);
    bool timeout=(connecting||downloading)&&uint32_t(now-started)>8000;
    if(timeout){connecting=downloading=coverConnected=false;attempted=true;closing=true;}
    bool connect=!closing&&capable&&handle[0]&&!attempted&&!coverConnected&&!connecting&&!workerBusy&&!downloading;
    if(connect){connecting=true;attempted=true;started=now;}
    bool fetch=!closing&&coverConnected&&handle[0]&&!downloading&&!workerBusy&&!decodePending&&transferGen!=info.generation;
    uint8_t selected[8];memcpy(selected,handle,8);
    if(fetch){transferGen=info.generation;length=0;discardTransfer=false;downloading=true;started=now;info.image=false;}
    bool run=decodePending&&!workerBusy;
    if(run){decodePending=false;workerBusy=true;jobGen=transferGen;jobLength=length;}
    portEXIT_CRITICAL(&mux);
    if(timeout){esp_avrc_ct_cover_art_disconnect();Serial.println("[BT-ART] timeout=1 audio_unchanged=1");return;}
    if(connect&&esp_avrc_ct_cover_art_connect(512)!=ESP_OK){portENTER_CRITICAL(&mux);connecting=false;portEXIT_CRITICAL(&mux);}
    if(fetch&&esp_avrc_ct_cover_art_get_linked_thumbnail(selected)!=ESP_OK){portENTER_CRITICAL(&mux);downloading=false;portEXIT_CRITICAL(&mux);}
    if(run&&xTaskCreatePinnedToCore(decode,"bt_art",6144,nullptr,1,nullptr,0)!=pdPASS){portENTER_CRITICAL(&mux);workerBusy=false;portEXIT_CRITICAL(&mux);}
}
View view(){portENTER_CRITICAL(&mux);View v=info;portEXIT_CRITICAL(&mux);return v;}
bool row(uint32_t generation,unsigned y,uint16_t* out,unsigned width,unsigned height){
    if(!width||!height||width>320||height>320||y>=height)return false;
    portENTER_CRITICAL(&mux);bool valid=info.image&&info.generation==generation&&!workerBusy;
    if(valid){
        unsigned cropW=imageW,cropH=imageH;
        if(cropW*height>cropH*width)cropW=cropH*width/height;
        else cropH=cropW*height/width;
        const unsigned top=(side-imageH)/2+(imageH-cropH)/2;
        const unsigned left=(side-imageW)/2+(imageW-cropW)/2;
        for(unsigned x=0;x<width;++x)out[x]=pixels[(top+y*cropH/height)*side+left+x*cropW/width];
    }
    portEXIT_CRITICAL(&mux);return valid;
}
}
