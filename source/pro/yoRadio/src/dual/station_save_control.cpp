#include "station_save_control.h"
#include "../core/options.h"
#include "../core/player.h"
#include "../core/config.h"
#include "../core/network.h"
#include "bt_audio.h"
#include <SPIFFS.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <new>
namespace stationsave {
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
// Allocated once, preferably in PSRAM; never placed on the decoder/display stack.
struct Context {
    Status state{};uint32_t since=0;bool queued=false,captured=false,stopped=false,ready=false;
    bool wasPlaying=false,stopByUser=false,finishReady=false,finishTaken=false;int oldMode=PM_WEB;
    uint16_t oldTrack=0;char oldUrl[BUFLEN]{};
    char line[BUFLEN*3]{},name[BUFLEN]{},url[BUFLEN]{};
};
static Context* ctx=nullptr;
static bool initializing=false;
static bool active(Phase p){return p>=WaitingStop&&p<=Finishing;}
const char* phaseName(Phase p){static const char* names[]={"idle","stopping","ready","uploading","saved","finishing","complete","failed"};return unsigned(p)<8?names[p]:"failed";}
Status status(){portENTER_CRITICAL(&mux);Status s=ctx?ctx->state:Status{};portEXIT_CRITICAL(&mux);return s;}
bool holdsAudio(){return active(status().phase);}
bool begin(uint32_t job){
    if(!job||job>0x7fffffffu)return false;
    portENTER_CRITICAL(&mux);
    if(initializing){portEXIT_CRITICAL(&mux);return false;}
    if(ctx&&ctx->state.job==job){portEXIT_CRITICAL(&mux);return true;} // idempotent retry, never upload twice
    if(ctx&&active(ctx->state.phase)){portEXIT_CRITICAL(&mux);return false;}
    bool allocate=!ctx;initializing=true;portEXIT_CRITICAL(&mux);
    Context* memory=ctx;
    if(allocate){
        if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<24576u+sizeof(Context))memory=nullptr;
        else{
            memory=(Context*)heap_caps_calloc(1,sizeof(Context),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            if(!memory)memory=(Context*)heap_caps_calloc(1,sizeof(Context),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        }
    }
    portENTER_CRITICAL(&mux);
    if(memory){ctx=new(memory) Context{};ctx->oldMode=PM_WEB;ctx->state.job=job;ctx->state.phase=WaitingStop;ctx->since=millis();}
    initializing=false;portEXIT_CRITICAL(&mux);return memory;
}
void manualStop(){portENTER_CRITICAL(&mux);if(ctx&&active(ctx->state.phase))ctx->stopByUser=true;portEXIT_CRITICAL(&mux);}
void ownerCapture(uint32_t job,int mode,uint16_t track,bool playing,const char* url){
    portENTER_CRITICAL(&mux);
    if(ctx&&ctx->state.job==job&&ctx->state.phase==WaitingStop&&!ctx->captured){
        ctx->oldMode=mode;ctx->oldTrack=track;ctx->wasPlaying=playing;strlcpy(ctx->oldUrl,url?url:"",sizeof(ctx->oldUrl));ctx->captured=true;
    }
    portEXIT_CRITICAL(&mux);
}
void ownerStopped(uint32_t job){portENTER_CRITICAL(&mux);if(ctx&&ctx->state.job==job&&ctx->captured)ctx->stopped=true;portEXIT_CRITICAL(&mux);}
bool ownerMayFinishStop(){portENTER_CRITICAL(&mux);bool ok=ctx&&ctx->stopped&&ctx->state.phase==WaitingStop;portEXIT_CRITICAL(&mux);return ok;}
void ownerReady(){portENTER_CRITICAL(&mux);if(ctx&&ctx->stopped&&ctx->state.phase==WaitingStop){ctx->state.phase=Ready;ctx->ready=true;ctx->since=millis();}portEXIT_CRITICAL(&mux);}
bool acceptsUpload(uint32_t job){auto s=status();return job&&s.job==job&&s.phase==Ready;}
bool uploadStarted(uint32_t job){portENTER_CRITICAL(&mux);bool ok=ctx&&ctx->state.job==job&&ctx->state.phase==Ready;if(ok){ctx->state.phase=Uploading;ctx->since=millis();}portEXIT_CRITICAL(&mux);return ok;}
void uploadAborted(uint32_t job){portENTER_CRITICAL(&mux);if(ctx&&ctx->state.job==job&&ctx->state.phase==Uploading){ctx->state.phase=Ready;ctx->since=millis();}portEXIT_CRITICAL(&mux);}
void imported(uint32_t job,bool good){portENTER_CRITICAL(&mux);if(ctx&&ctx->state.job==job&&ctx->state.phase==Uploading){ctx->state.installed=good;ctx->state.phase=good?Saved:Ready;ctx->since=millis();}portEXIT_CRITICAL(&mux);}
bool finish(uint32_t job,bool cancel){
    portENTER_CRITICAL(&mux);
    bool ok=ctx&&ctx->state.job==job&&((ctx->state.phase==Saved&&!cancel)||(cancel&&ctx->state.phase==Ready));
    if(ok){ctx->state.phase=Finishing;ctx->since=millis();ctx->finishReady=false;ctx->finishTaken=false;}
    portEXIT_CRITICAL(&mux);return ok;
}
static uint16_t findPrevious(){
    if(!ctx||!ctx->oldUrl[0])return 0;
    File f=SPIFFS.open(PLAYLIST_PATH,"r");if(!f)return 0;
    uint16_t row=0;uint32_t bytes=0;
    while(f.available()&&row<480){
        size_t n=0;bool overflow=false;
        while(f.available()){
            int c=f.read();if(c<0){overflow=true;break;}if(++bytes%128==0)vTaskDelay(1);
            if(c=='\n')break;if(!c)overflow=true;else if(n<sizeof(ctx->line)-1)ctx->line[n++]=char(c);else overflow=true;
        }
        ctx->line[n]=0;if(n&&ctx->line[n-1]=='\r')ctx->line[--n]=0;
        int volume=0;++row;
        if(overflow||!n||!config.parseCSV(ctx->line,ctx->name,ctx->url,volume))return 0;
        // Match the saved stream, never its obsolete numeric position.
        if(!strcmp(ctx->oldUrl,ctx->url))return row;
    }
    return 0;
}
void poll(){
    auto s=status();if(!active(s.phase))return;
    portENTER_CRITICAL(&mux);const uint32_t since=ctx->since;const bool finishReady=ctx->finishReady;portEXIT_CRITICAL(&mux);
    if(s.phase==WaitingStop){
        portENTER_CRITICAL(&mux);bool send=ctx&&!ctx->queued;portEXIT_CRITICAL(&mux);
        // No resource was stopped if the queue never accepted our request.
        if(send&&uint32_t(millis()-since)>30000){portENTER_CRITICAL(&mux);if(!ctx->queued&&ctx->state.phase==WaitingStop)ctx->state.phase=Failed;portEXIT_CRITICAL(&mux);return;}
        if(send&&player.sendCommand({PR_STATION_SAVE,int(s.job)})){portENTER_CRITICAL(&mux);ctx->queued=true;portEXIT_CRITICAL(&mux);}
        return; // no false READY while a DNS/BT/SD owner is still finishing
    }
    if((s.phase==Ready||s.phase==Saved)&&uint32_t(millis()-since)>90000){
        // A closed browser must not hold the player forever. No import is retried.
        finish(s.job,s.phase==Ready);s=status();
    }
    if(s.phase!=Finishing||finishReady)return;
    uint16_t resume=ctx->oldMode==PM_WEB?findPrevious():ctx->oldTrack;
    portENTER_CRITICAL(&mux);
    if(ctx->state.phase==Finishing&&ctx->state.job==s.job){ctx->state.station=resume;ctx->finishReady=true;}
    portEXIT_CRITICAL(&mux);
}
bool ownerTakeFinish(Resume& plan){
    portENTER_CRITICAL(&mux);
    bool ok=ctx&&ctx->state.phase==Finishing&&ctx->finishReady&&!ctx->finishTaken;
    if(ok){plan.mode=ctx->oldMode;plan.track=ctx->state.station;plan.play=ctx->wasPlaying&&!ctx->stopByUser&&plan.track;ctx->finishTaken=true;}
    portEXIT_CRITICAL(&mux);return ok;
}
void ownerReleased(bool resumeRequested){portENTER_CRITICAL(&mux);if(ctx&&ctx->state.phase==Finishing){ctx->state.resume=resumeRequested;ctx->state.phase=ctx->state.installed?Complete:Failed;}portEXIT_CRITICAL(&mux);}
}
