#include "../core/options.h"
#include "artwork.h"
#if FEATURE_ARTWORK && FEATURE_SD
#include "apic.h"
#include "logic.h"
#include <SD.h>
#include "../core/sdmanager.h"
#include "../ui/frontend.h"
#include <JPEGDEC.h>
#include <esp_heap_caps.h>
#include <new>
namespace artwork {
struct Image { uint8_t *bytes=nullptr;size_t length=0;uint32_t generation=0; };
struct Retry {uint32_t generation;uint8_t next;};
static QueueHandle_t images=nullptr;
static QueueHandle_t retries=nullptr;
static uint8_t sentStage=0;
static Image loading,cached;
static File file;
// Decoder producer / Core0 file-owner consumer. Never hold this mux for I/O.
struct TrackRequest {char path[256]{};uint32_t generation=0,revision=0;size_t pos=0,size=0;};
static portMUX_TYPE requestMux=portMUX_INITIALIZER_UNLOCKED;
static TrackRequest requestedTrack;
static uint32_t appliedRevision=0;
static char trackPath[256];
static size_t apicPos=0,apicSize=0,filled=0;
static uint32_t trackGeneration=0,lastRead=0;
static uint8_t step=0; // 0 wait for decoder headers, 1 APIC, 2 cover, 3 folder, 4 file-name, 5 done
static bool wantRedraw=false;
static int drawnW=0,drawnH=0;
#if defined(BOARD_CYD28)
constexpr size_t maxBytes=12*1024;
constexpr int target=112;
constexpr uint32_t memoryCap=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
#else
constexpr size_t maxBytes=192*1024;
constexpr int target=300;
constexpr uint32_t memoryCap=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
#endif
void begin(){images=xQueueCreate(1,sizeof(Image));retries=xQueueCreate(1,sizeof(Retry));}
void reset(const char *path,uint32_t generation) {
    portENTER_CRITICAL(&requestMux);
    dual::textCopy(requestedTrack.path,sizeof(requestedTrack.path),path);
    requestedTrack.generation=generation;requestedTrack.pos=requestedTrack.size=0;++requestedTrack.revision;
    portEXIT_CRITICAL(&requestMux);
}
void hint(size_t pos,size_t length){
    portENTER_CRITICAL(&requestMux);requestedTrack.pos=pos;requestedTrack.size=length;portEXIT_CRITICAL(&requestMux);
}
static void applyTrackRequest(){
    TrackRequest request;
    portENTER_CRITICAL(&requestMux);request=requestedTrack;portEXIT_CRITICAL(&requestMux);
    if(request.revision!=appliedRevision){
        file.close();free(loading.bytes);loading={};filled=0;
        dual::textCopy(trackPath,sizeof(trackPath),request.path);trackGeneration=request.generation;
        apicPos=apicSize=0;step=request.path[0]?0:5;appliedRevision=request.revision;
        Image old;if(images&&xQueueReceive(images,&old,0)==pdTRUE)free(old.bytes);
    }
    if(step==0){apicPos=request.pos;apicSize=request.size;}
}
static void fail(){file.close();free(loading.bytes);loading={};filled=0;}
static bool openCandidate() {
    if(step>=5)return false;
    size_t skip=0,length=0;
    if(step==1) {
        if(!apicSize)return false;
        file=sdman.open(trackPath,"r");
        if(!file||apicPos>file.size()||apicSize>file.size()-apicPos)return false;
        if(!file.seek(apicPos))return false;
        uint8_t head[512];size_t n=file.read(head,min(apicSize,sizeof(head)));
        if(!apicJpegOffset(head,n,skip)||skip>=apicSize)return false;
        length=apicSize-skip;skip+=apicPos;
    } else {
        char path[280];dual::textCopy(path,sizeof(path),trackPath);
        char *slash=strrchr(path,'/');if(!slash)return false;
        if(step==2||step==3)strcpy(slash+1,step==2?"cover.jpg":"folder.jpg");
        else {char *dot=strrchr(slash,'.');if(!dot)return false;strcpy(dot,".jpg");}
        file=sdman.open(path,"r");if(!file)return false;length=file.size();
    }
    if(length<4||length>maxBytes)return false;
    uint32_t freeInternal=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(freeInternal<64*1024+sizeof(JPEGDEC)+(memoryCap&(MALLOC_CAP_INTERNAL)?length:0))return false;
    if(heap_caps_get_largest_free_block(memoryCap)<length+4096)return false;
    loading.bytes=(uint8_t*)heap_caps_malloc(length,memoryCap);
    if(!loading.bytes)return false;
    loading.length=length;loading.generation=trackGeneration;
    filled=0;return file.seek(skip);
}
void audioTick(bool running,bool headerDone,uint32_t buffered) {
    applyTrackRequest(); // also close/reset obsolete artwork while stopped
    Retry retry;
    if(retries&&xQueueReceive(retries,&retry,0)==pdTRUE&&retry.generation==trackGeneration&&step==5)step=sentStage+1;
    if(!images||!running||step>=5)return;
    uint32_t now=millis();if(buffered<8*1024||uint32_t(now-lastRead)<8)return;
    lastRead=now;
    if(step==0){if(!headerDone)return;step=1;}
    if(!loading.bytes) {
        if(!openCandidate()){fail();step++;}return;
    }
    size_t want=min(size_t(512),loading.length-filled);
    size_t n=file.read(loading.bytes+filled,want);
    if(n!=want){fail();step++;return;}
    filled+=n;
    if(filled==loading.length) {
        file.close();
        if(loading.bytes[0]!=0xff||loading.bytes[1]!=0xd8){fail();step++;return;}
        Image old;if(xQueueReceive(images,&old,0)==pdTRUE)free(old.bytes);
        if(xQueueSend(images,&loading,0)!=pdTRUE)free(loading.bytes);
        sentStage=step;loading={};filled=0;step=5;
    }
}
static int draw(JPEGDRAW *d) {
    int w=min(d->iWidthUsed,drawnW-d->x),h=min(d->iHeight,drawnH-d->y);
    if(w>0&&h>0)front::artworkTile(d->x,d->y,w,h,d->iWidth,d->pPixels);
    vTaskDelay(1);return 1;
}
void redraw(uint32_t generation){if(cached.generation==generation)wantRedraw=true;}
void uiTick(uint32_t generation,bool visible) {
    if(cached.bytes&&cached.generation!=generation){free(cached.bytes);cached={};front::artworkClear();}
    Image incoming;
    if(images&&xQueueReceive(images,&incoming,0)==pdTRUE) {
        if(incoming.generation==generation){free(cached.bytes);cached=incoming;wantRedraw=true;}
        else free(incoming.bytes);
    }
    if(!visible||!cached.bytes||!wantRedraw)return;
    wantRedraw=false;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<sizeof(JPEGDEC)+48*1024)return;
    void* workspace=heap_caps_malloc(sizeof(JPEGDEC),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!workspace)return;
    JPEGDEC *jpeg=new(workspace)JPEGDEC();
    bool success=false;
    if(jpeg->openRAM(cached.bytes,cached.length,draw)) {
        int w=jpeg->getWidth(),h=jpeg->getHeight(),scale=1,flags=0;
        if(w>target||h>target){scale=2;flags=JPEG_SCALE_HALF;}
        if((w+scale-1)/scale>target||(h+scale-1)/scale>target){scale=4;flags=JPEG_SCALE_QUARTER;}
        if((w+scale-1)/scale>target||(h+scale-1)/scale>target){scale=8;flags=JPEG_SCALE_EIGHTH;}
        drawnW=(w+scale-1)/scale;drawnH=(h+scale-1)/scale;
        if(w>0&&h>0&&drawnW<=target&&drawnH<=target&&front::artworkBegin(drawnW,drawnH)) {
            success=jpeg->decode(0,0,flags)!=0;if(success)front::artworkEnd();
        }
        jpeg->close();
    }
    jpeg->~JPEGDEC();heap_caps_free(workspace);
    if(!success){Retry retry{cached.generation,0};if(retries)xQueueOverwrite(retries,&retry);front::artworkClear();free(cached.bytes);cached={};}
}
}
#else
namespace artwork {
void begin(){} void reset(const char*,uint32_t){} void hint(size_t,size_t){}
void audioTick(bool,bool,uint32_t){}void uiTick(uint32_t,bool){}void redraw(uint32_t){}
}
#endif
