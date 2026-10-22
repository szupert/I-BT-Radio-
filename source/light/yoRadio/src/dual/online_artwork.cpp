#include "../core/options.h"
#include "online_artwork.h"
#if FEATURE_ARTWORK && !IBT_NO_PSRAM
#include "../ui/frontend.h"
#include "resource_limiter.h"
#include <Arduino.h>
#include <JPEGDEC.h>
#include <SPIFFS.h>
#include <esp_heap_caps.h>
#include <new>
#include <string.h>
#include "artwork_transport.h"
#include "cover_lookup.h"
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
namespace onlineart {
struct Image { uint8_t* bytes=nullptr; size_t length=0; uint32_t generation=0,revision=0; bool smoke=false; };
struct FetchState { uint32_t generation=0,nextAllowed=0,startedAt=0; uint8_t failures=0; bool loading=false; };
struct UrlRequest { uint32_t generation=0,revision=0; char url[384]={}; bool track=false; };
struct Payload { UrlRequest pending{},workerRequest{};char stationFallback[384]{},lastTrack[171]{},deferredTitle[171]{}; };
static Payload* payload=nullptr; // Allocated once before callbacks/workers; never freed.
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static Image incoming,cached;
static FetchState fetch;

static uint32_t activeGeneration=0,revision=0,redrawGeneration=UINT32_MAX;

static uint32_t lookupNext=0,settleUntil=0,smokeUntil=0;

static bool hasPending=false,workerActive=false,wantRedraw=false;
static bool enabled=true;
static int drawnW=0,drawnH=0;
static uint32_t failedImageRevision=0;
static constexpr uint32_t kFetchTimeoutMs=15000;
// HTTP/TLS certificate processing exceeded the old 5 KiB worker stack on hardware.
static constexpr uint32_t kFetchStackBytes=16*1024;
static constexpr size_t kFetchStackGrowth=kFetchStackBytes-5120;
static constexpr uint32_t kBackoffMs[]={5000,15000,30000,60000,120000,300000};
#if defined(BOARD_CYD28)
static constexpr size_t kMaxPsramJpeg=96*1024,kMaxInternalJpeg=12*1024;
static constexpr int kTarget=112;
#else
static constexpr size_t kMaxPsramJpeg=256*1024,kMaxInternalJpeg=32*1024;
static constexpr int kTarget=300;
#endif
static void release(Image& i) { free(i.bytes); i={}; }
static uint32_t backoffFor(uint8_t n) { size_t i=n?n-1:0; if(i>=6)i=5; return n?kBackoffMs[i]:0; }
static bool localUrl(const char* u) { return u&&u[0]=='/'&&u[1]!='/'&&!strstr(u,"..")&&!strchr(u,'?')&&!strchr(u,'#')&&strlen(u)<384; }
static bool current(uint32_t g) { if(!payload)return false; portENTER_CRITICAL(&mux); bool ok=enabled&&g==activeGeneration; portEXIT_CRITICAL(&mux); return ok; }
void begin() { if(!payload){void* p=heap_caps_malloc(sizeof(Payload),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(p)payload=new(p)Payload();} }
bool copyCached(uint8_t *&bytes,size_t &length){
    bytes=nullptr;length=0;
    if(!payload)return false;
    if(xPortGetCoreID()!=0||!cached.bytes||cached.generation!=activeGeneration||cached.length>kMaxPsramJpeg)return false;
    bytes=(uint8_t*)heap_caps_malloc(cached.length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!bytes)return false;length=cached.length;memcpy(bytes,cached.bytes,length);return true;
}
void setEnabled(bool on,uint32_t g) {
    if(!payload)return;
    Image old;
    portENTER_CRITICAL(&mux);
    enabled=on;activeGeneration=g;
    if(!on){hasPending=false;revision++;payload->pending.revision=revision;payload->stationFallback[0]=payload->lastTrack[0]=0;old=incoming;incoming={};}
    portEXIT_CRITICAL(&mux);
    release(old);
    if(!on){release(cached);wantRedraw=false;front::artworkClear();}
}
size_t maxJpegBytes() { return ESP.getPsramSize()?kMaxPsramJpeg:kMaxInternalJpeg; }
CacheStatus cacheStatus(uint32_t generation) {
    CacheStatus s{};if(!payload)return s;
    portENTER_CRITICAL(&mux);
    s.failures=fetch.failures;s.pending=hasPending||workerActive;
    const bool active=generation==activeGeneration;
    const uint32_t latest=payload->pending.revision;
    portEXIT_CRITICAL(&mux);
    if(active&&cached.bytes&&cached.generation==generation&&(!cached.revision||cached.revision==latest))s.bytes=cached.length;
    return s;
}
void reset(uint32_t g) {
    if(!payload)return;
    Image old;
    portENTER_CRITICAL(&mux);
    activeGeneration=g;failedImageRevision=0; payload->stationFallback[0]=payload->lastTrack[0]=0; settleUntil=smokeUntil=0;payload->deferredTitle[0]=0; fetch={g,0,0,0,false}; hasPending=false;
    old=incoming; incoming={}; redrawGeneration=UINT32_MAX;
    portEXIT_CRITICAL(&mux); release(old);
    // UI owns cached/decoder/display; invalidate it on the next UI tick.
}
void requestUrl(const char* url,uint32_t g) {
    if(!payload)return;
    if(!localUrl(url)&&!remoteUrl(url))return;
    UrlRequest r{}; r.generation=g; strlcpy(r.url,url,sizeof(r.url));
    portENTER_CRITICAL(&mux);
    if(enabled&&g==activeGeneration) { strlcpy(payload->stationFallback,url,sizeof(payload->stationFallback));payload->lastTrack[0]=0;settleUntil=0;r.revision=++revision; payload->pending=r; hasPending=true; }
    portEXIT_CRITICAL(&mux);
}
void requestTrack(const char* title,uint32_t g) {
    if(!payload)return;
    if(!title||strlen(title)>170)return;
    portENTER_CRITICAL(&mux);
    if(!enabled){portEXIT_CRITICAL(&mux);return;}
    if(g==activeGeneration&&int32_t(millis()-smokeUntil)<0){strlcpy(payload->deferredTitle,title,sizeof(payload->deferredTitle));portEXIT_CRITICAL(&mux);return;}
    portEXIT_CRITICAL(&mux);
    // Only the last title is retained; metadata callbacks never allocate or wait.
    char query[1100];const bool valid=coverQuery(title,query,sizeof(query));
    portENTER_CRITICAL(&mux);
    if(enabled&&g==activeGeneration && strcmp(payload->lastTrack,title)) {
        strlcpy(payload->lastTrack,title,sizeof(payload->lastTrack));
        UrlRequest r{};r.generation=g;r.revision=++revision;r.track=valid;
        strlcpy(r.url,valid?title:payload->stationFallback,sizeof(r.url));payload->pending=r;
        hasPending=valid||payload->stationFallback[0];settleUntil=millis()+2000;
        fetch.nextAllowed=0;fetch.failures=0;
    }
    portEXIT_CRITICAL(&mux);
}
void smoke(uint32_t g) {
    if(!payload)return;
    portENTER_CRITICAL(&mux);
    if(enabled&&g==activeGeneration){UrlRequest r{};r.generation=g;r.revision=++revision;strlcpy(r.url,"/artwork_test.jpg",sizeof(r.url));payload->pending=r;hasPending=true;fetch.nextAllowed=0;settleUntil=0;smokeUntil=millis()+15000;strlcpy(payload->deferredTitle,payload->lastTrack,sizeof(payload->deferredTitle));payload->lastTrack[0]=0;}
    portEXIT_CRITICAL(&mux);
}
bool mayRequest(uint32_t g,uint32_t now) { if(!payload)return false;
    portENTER_CRITICAL(&mux); bool ok=g==activeGeneration&&!fetch.loading&&int32_t(now-fetch.nextAllowed)>=0; portEXIT_CRITICAL(&mux); return ok;
}
void requestStarted(uint32_t g,uint32_t now) {
    portENTER_CRITICAL(&mux); if(g==activeGeneration) { fetch.loading=true; fetch.startedAt=now; } portEXIT_CRITICAL(&mux);
}
void requestFailed(uint32_t g,uint32_t now) {
    portENTER_CRITICAL(&mux);
    if(g==activeGeneration) { fetch.loading=false; if(fetch.failures<255)fetch.failures++; fetch.nextAllowed=now+backoffFor(fetch.failures); }
    portEXIT_CRITICAL(&mux);
}
void requestSucceeded(uint32_t g,uint32_t) {
    portENTER_CRITICAL(&mux); if(g==activeGeneration) { fetch.loading=false; fetch.failures=0; fetch.nextAllowed=0; } portEXIT_CRITICAL(&mux);
}
uint32_t retryAfterMs(uint32_t g,uint32_t now) {
    portENTER_CRITICAL(&mux); uint32_t result=0;
    if(g==activeGeneration) {
        if(fetch.loading) { uint32_t age=now-fetch.startedAt; result=age<kFetchTimeoutMs?kFetchTimeoutMs-age:1; }
        else if(int32_t(now-fetch.nextAllowed)<0)result=fetch.nextAllowed-now;
    }
    portEXIT_CRITICAL(&mux); return result;
}
static bool currentRequest(const UrlRequest& r) { portENTER_CRITICAL(&mux); bool ok=enabled&&r.generation==activeGeneration&&r.revision==payload->pending.revision; portEXIT_CRITICAL(&mux); return ok; }
static bool imageCurrent(const Image& img) {
    portENTER_CRITICAL(&mux);bool ok=img.generation==activeGeneration&&(!img.revision||img.revision==payload->pending.revision);portEXIT_CRITICAL(&mux);return ok;
}
static bool deliver(Image& img) {
    if(!img.bytes||img.length<4||img.length>maxJpegBytes()||img.bytes[0]!=0xff||img.bytes[1]!=0xd8)return false;
    Image old; bool ok=false;
    portENTER_CRITICAL(&mux);
    if(enabled&&img.generation==activeGeneration&&(!img.revision||img.revision==payload->pending.revision)) { old=incoming; incoming=img; img={}; ok=true; }
    portEXIT_CRITICAL(&mux); release(old); return ok;
}
bool submitJpegCopy(const uint8_t* bytes,size_t n,uint32_t g) {
    if(!current(g)||!bytes||n<4||n>maxJpegBytes()||bytes[0]!=0xff||bytes[1]!=0xd8)return false;
    uint32_t caps=ESP.getPsramSize()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if((caps&MALLOC_CAP_INTERNAL)&&(!ibtlimit::allowAllocation(n)||n>kMaxInternalJpeg))return false;
    if(heap_caps_get_largest_free_block(caps)<n+4096)return false;
    Image img{(uint8_t*)heap_caps_malloc(n,caps),n,g}; if(!img.bytes)return false;
    memcpy(img.bytes,bytes,n); bool ok=deliver(img); release(img); return ok;
}
struct FetchWorkspace { UrlRequest req; char fallback[384],query[1100],uuid[37],cover[128]; };
struct RemoteFetch { UrlRequest req; BoundedBody body; uint32_t start; bool failed=false; };
static esp_err_t httpEvent(esp_http_client_event_t* e) {
    RemoteFetch* f=(RemoteFetch*)e->user_data;
    if(!ibtlimit::allowArtworkProgress() || !currentRequest(f->req) || uint32_t(millis()-f->start)>=kFetchTimeoutMs ||
       heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<ibtlimit::Policy::reserve) {
        if(!f->failed)Serial.println("[R10-ARTWORK] fetch=ABORTED reason=buffer_heap_stale_or_cancel_timeout");
        f->failed=true; return ESP_FAIL;
    }
    if(e->event_id==HTTP_EVENT_ON_DATA && esp_http_client_get_status_code(e->client)==200) {
        if(e->data_len<0 || !f->body.append(e->data,(size_t)e->data_len)) { f->failed=true; return ESP_FAIL; }
    }
    return ESP_OK;
}
static bool remoteFetch(const UrlRequest& req,const char* url,Image& img,size_t cap,uint32_t start) {
    // Check before each new HTTP/TLS request, including lookup/fallback.
    if(!ibtlimit::allowArtworkStart()){Serial.println("[R10-ARTWORK] fetch=DEFERRED reason=audio_buffer_or_heap");return false;}
    const uint32_t caps=ESP.getPsramSize()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if((caps&MALLOC_CAP_INTERNAL)&&!ibtlimit::allowAllocation(cap))return false;
    uint8_t* data=(uint8_t*)heap_caps_malloc(cap,caps); if(!data)return false;
    RemoteFetch f{req,{data,cap,0},start,false};
    esp_http_client_config_t c{}; c.url=url;c.event_handler=httpEvent;c.user_data=&f;
    c.timeout_ms=6000;c.buffer_size=1024;c.buffer_size_tx=512;
    c.max_redirection_count=3;c.user_agent="I-BT-Radio/0.40 (personal ESP32 radio)";c.crt_bundle_attach=esp_crt_bundle_attach;
    esp_http_client_handle_t h=esp_http_client_init(&c);bool ok=false;int status=0;esp_err_t error=ESP_FAIL;
    if(h) {
        // Own the read loop: IDF ON_DATA dispatch ignores callback return values.
        // Never close/reconnect recursively inside the event callback.
        uint8_t discard[256];
        for(unsigned redirects=0;redirects<=3;++redirects) {
            if(!ibtlimit::allowArtworkStart()||!currentRequest(req)||uint32_t(millis()-start)>=kFetchTimeoutMs)break;
            esp_http_client_set_timeout_ms(h,6000);
            error=esp_http_client_open(h,0);
            if(error!=ESP_OK)break;
            const int64_t length=esp_http_client_fetch_headers(h);
            status=esp_http_client_get_status_code(h);
            if(length<0||f.failed||!ibtlimit::allowArtworkProgress())break;
            if(status==301||status==302||status==303||status==307||status==308) {
                if(redirects==3||esp_http_client_set_redirection(h)!=ESP_OK)break;
                esp_http_client_close(h);f.body.used=0;continue;
            }
            if(status!=200)break;
            // Bound body-read stalls separately from the TLS/header handshake.
            esp_http_client_set_timeout_ms(h,250);
            while(!f.failed&&currentRequest(req)&&uint32_t(millis()-start)<kFetchTimeoutMs) {
                if(!ibtlimit::allowArtworkProgress()){f.failed=true;Serial.println("[R10-ARTWORK] fetch=ABORTED reason=audio_buffer_or_heap");break;}
                if(esp_http_client_is_complete_data_received(h)){ok=true;break;}
                const int n=esp_http_client_read(h,reinterpret_cast<char*>(discard),sizeof(discard));
                // ON_DATA already appends to the bounded body, including header-prefetched bytes.
                if(n<=0){ok=n==0&&esp_http_client_is_complete_data_received(h);break;}
                vTaskDelay(1);
            }
            break;
        }
        ok=ok&&!f.failed&&currentRequest(req)&&ibtlimit::allowArtworkProgress()&&uint32_t(millis()-start)<kFetchTimeoutMs;
        error=ok?ESP_OK:ESP_FAIL;
        esp_http_client_close(h); // worker-owned cancellation before cleanup, even for failed headers.
        esp_http_client_cleanup(h);
    }
    if(ok) { img={data,f.body.used,req.generation,req.revision};data=nullptr; }
    free(data);
    Serial.printf("[R10-ARTWORK] fetch=%s lane=REMOTE generation=%lu revision=%lu http=%d error=%d bytes=%u ms=%lu free_internal=%u\n",
        ok?"PASS":"FAIL",(unsigned long)req.generation,(unsigned long)req.revision,status,int(error),(unsigned)f.body.used,
        (unsigned long)(millis()-f.start),(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    return ok;
}
static bool localFetch(const UrlRequest& req,const char* url,Image& img,uint32_t start) {
    char path[392];if(!localUrl(url))return false;
    if(!strncmp(url,"/www/",5))strlcpy(path,url,sizeof(path));else snprintf(path,sizeof(path),"/www%s",url);
    File file=SPIFFS.open(path,"r");bool ok=false;
    if(file&&currentRequest(req)) {
        size_t n=file.size();uint32_t caps=ESP.getPsramSize()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        if(n>=4&&n<=maxJpegBytes()&&heap_caps_get_largest_free_block(caps)>=n+4096&&(!(caps&MALLOC_CAP_INTERNAL)||ibtlimit::allowAllocation(n))) {
            img={(uint8_t*)heap_caps_malloc(n,caps),n,req.generation,req.revision,!strcmp(url,"/artwork_test.jpg")};
            if(img.bytes) {size_t used=0;while(used<n&&currentRequest(req)&&uint32_t(millis()-start)<kFetchTimeoutMs){
                size_t take=file.read(img.bytes+used,min(size_t(1024),n-used));if(!take)break;used+=take;vTaskDelay(1);
            }ok=used==n;}
        }
    }file.close();return ok;
}
static bool imageFetch(const UrlRequest& req,const char* url,Image& img,uint32_t start) {
    return remoteUrl(url)?remoteFetch(req,url,img,maxJpegBytes(),start):localFetch(req,url,img,start);
}
static void fetchWorker(void*) {
    auto* workspace=(FetchWorkspace*)heap_caps_malloc(sizeof(FetchWorkspace),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!workspace){
        requestFailed(payload->workerRequest.generation,millis());
        ibtlimit::release(ibtlimit::Job::ArtworkFetch);
        portENTER_CRITICAL(&mux);workerActive=false;portEXIT_CRITICAL(&mux);
        vTaskDelete(nullptr);return;
    }
    new(workspace) FetchWorkspace{};
    auto& req=workspace->req;auto& fallback=workspace->fallback;
    portENTER_CRITICAL(&mux);req=payload->workerRequest;strlcpy(fallback,payload->stationFallback,sizeof(fallback));portEXIT_CRITICAL(&mux);
    Image img{};bool ok=false;const uint32_t start=millis();
    portENTER_CRITICAL(&mux);const bool fallbackOnly=req.revision==failedImageRevision;failedImageRevision=0;portEXIT_CRITICAL(&mux);
    if(req.track) {
        auto& query=workspace->query;auto& uuid=workspace->uuid;auto& cover=workspace->cover;Image json{};
        // Optional cover lookup and its workspace require PSRAM.
        if(!fallbackOnly&&ESP.getPsramSize()&&coverQuery(req.url,query,sizeof(query))&&remoteFetch(req,query,json,16384,start)) {
            const bool found=releaseUuid(json.bytes,json.length,uuid);release(json);
            if(found) {snprintf(cover,sizeof(cover),"https://coverartarchive.org/release/%s/front-250",uuid);ok=imageFetch(req,cover,img,start);}
        }
        release(json);
        ok=ok&&currentRequest(req)&&deliver(img);release(img);
        Serial.printf("[R10-ARTWORK] lookup=%s provider=MUSICBRAINZ_CAA generation=%lu revision=%lu\n",ok?"PASS":"FAIL",(unsigned long)req.generation,(unsigned long)req.revision);
        if(!ok&&currentRequest(req)&&fallback[0]) {
            Image station{};bool loaded=imageFetch(req,fallback,station,millis());
            loaded=loaded&&currentRequest(req)&&deliver(station);release(station);
            Serial.printf("[R10-ARTWORK] fallback=%s generation=%lu\n",loaded?"PASS":"FAIL",(unsigned long)req.generation);
        }
    } else {ok=imageFetch(req,req.url,img,start);ok=ok&&currentRequest(req)&&deliver(img);release(img);}
    if(currentRequest(req)) {
        if(ok){requestSucceeded(req.generation,millis());portENTER_CRITICAL(&mux);if(hasPending&&payload->pending.revision==req.revision)hasPending=false;portEXIT_CRITICAL(&mux);}
        else requestFailed(req.generation,millis());
    } else {portENTER_CRITICAL(&mux);fetch.loading=false;portEXIT_CRITICAL(&mux);}
    free(workspace);
    Serial.printf("[R10-ARTWORK] worker_stack_min=%u stack_bytes=%u\n",
        (unsigned)uxTaskGetStackHighWaterMark(nullptr),(unsigned)kFetchStackBytes);
    ibtlimit::release(ibtlimit::Job::ArtworkFetch);
    portENTER_CRITICAL(&mux);workerActive=false;portEXIT_CRITICAL(&mux);vTaskDelete(nullptr);
}
static void pump() {
    char title[171]={};uint32_t generation;
    portENTER_CRITICAL(&mux);generation=activeGeneration;
    if(smokeUntil&&int32_t(millis()-smokeUntil)>=0){smokeUntil=0;strlcpy(title,payload->deferredTitle,sizeof(title));payload->deferredTitle[0]=0;}
    portEXIT_CRITICAL(&mux);if(title[0])requestTrack(title,generation);
    bool ready;size_t fetchCost=8192;
    portENTER_CRITICAL(&mux);
    ready=enabled&&hasPending&&!workerActive&&payload->pending.generation==activeGeneration&&!fetch.loading&&int32_t(millis()-fetch.nextAllowed)>=0&&int32_t(millis()-settleUntil)>=0&&(!payload->pending.track||int32_t(millis()-lookupNext)>=0);
    // V071 physical TLS runs exhausted internal RAM despite the old estimate.
    // Defer optional remote work unless its conservative headroom is available.
    // Local image loading keeps its smaller, separate budget.
    if(payload->pending.track||remoteUrl(payload->pending.url))fetchCost=65536;
    portEXIT_CRITICAL(&mux);
    if(!ready||!ibtlimit::allowArtworkStart()||!ibtlimit::acquire(ibtlimit::Job::ArtworkFetch,fetchCost+kFetchStackGrowth))return;
    portENTER_CRITICAL(&mux); ready=enabled&&hasPending&&!workerActive&&payload->pending.generation==activeGeneration;
    if(ready) { payload->workerRequest=payload->pending;if(payload->pending.track)lookupNext=millis()+30000;workerActive=true; fetch.loading=true; fetch.startedAt=millis(); }
    portEXIT_CRITICAL(&mux);
    if(!ready) { ibtlimit::release(ibtlimit::Job::ArtworkFetch); return; }
    if(xTaskCreatePinnedToCore(fetchWorker,"ibt_art_fetch",kFetchStackBytes,nullptr,1,nullptr,0)!=pdPASS) {
        requestFailed(payload->workerRequest.generation,millis());
        portENTER_CRITICAL(&mux); workerActive=false; portEXIT_CRITICAL(&mux);
        ibtlimit::release(ibtlimit::Job::ArtworkFetch);
    }
}
static int draw(JPEGDRAW* d) {
    if(!imageCurrent(cached))return 0;
    int w=min(d->iWidthUsed,drawnW-d->x),h=min(d->iHeight,drawnH-d->y);
    if(w>0&&h>0)front::artworkTile(d->x,d->y,w,h,d->iWidth,d->pPixels); vTaskDelay(1); return 1;
}
void redraw(uint32_t g) { portENTER_CRITICAL(&mux); redrawGeneration=g; portEXIT_CRITICAL(&mux); }
void uiTick(uint32_t g,bool visible) {
    if(!payload)return;
    Image img; portENTER_CRITICAL(&mux); img=incoming; incoming={};
    const bool active=enabled;
    if(redrawGeneration==g) { wantRedraw=true; redrawGeneration=UINT32_MAX; }
    portEXIT_CRITICAL(&mux);
    if(!active){release(img);release(cached);wantRedraw=false;return;}
    portENTER_CRITICAL(&mux);const uint32_t latest=payload->pending.revision;portEXIT_CRITICAL(&mux);
    if(cached.bytes&&(cached.generation!=g||(cached.revision&&cached.revision!=latest))) { release(cached); front::artworkClear(); wantRedraw=false; }
    if(img.bytes) { if(img.generation==g&&(!img.revision||img.revision==latest)) { release(cached); cached=img; img={}; wantRedraw=true; } release(img); }
    if(visible&&cached.bytes&&wantRedraw&&imageCurrent(cached)) {
        bool psram=ESP.getPsramSize()!=0; size_t cost=psram?4096:sizeof(JPEGDEC)+4096;
        if(ibtlimit::acquire(ibtlimit::Job::ArtworkDecode,cost)) {
            uint32_t caps=psram?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
            void* storage=heap_caps_malloc(sizeof(JPEGDEC),caps);
            if(storage) {
                JPEGDEC* jpeg=new(storage)JPEGDEC(); bool ok=false;
                if(jpeg->openRAM(cached.bytes,cached.length,draw)) {
                    int w=jpeg->getWidth(),h=jpeg->getHeight(),scale=1,flags=0;
                    if(w>kTarget||h>kTarget) { scale=2; flags=JPEG_SCALE_HALF; }
                    if((w+scale-1)/scale>kTarget||(h+scale-1)/scale>kTarget) { scale=4; flags=JPEG_SCALE_QUARTER; }
                    if((w+scale-1)/scale>kTarget||(h+scale-1)/scale>kTarget) { scale=8; flags=JPEG_SCALE_EIGHTH; }
                    drawnW=(w+scale-1)/scale; drawnH=(h+scale-1)/scale;
                    if(w>0&&h>0&&drawnW<=kTarget&&drawnH<=kTarget&&front::artworkBegin(drawnW,drawnH)) {
                        ok=jpeg->decode(0,0,flags)!=0&&imageCurrent(cached); if(ok)front::artworkEnd();
                    }
                    jpeg->close();
                }
                jpeg->~JPEGDEC(); free(storage); wantRedraw=false;
                if(ok)Serial.printf("[R10-ARTWORK] render=PASS generation=%lu decoder_psram=%u source=%s\n",(unsigned long)g,psram?1:0,cached.smoke?"SMOKE":"LIVE");
                else {
                    const bool fresh=imageCurrent(cached);
                    Serial.printf("[R10-ARTWORK] render=%s generation=%lu\n",fresh?"FAIL":"STALE",(unsigned long)g);
                    if(fresh){portENTER_CRITICAL(&mux);if(cached.generation==activeGeneration&&cached.revision==payload->pending.revision){hasPending=true;failedImageRevision=cached.revision;}portEXIT_CRITICAL(&mux);requestFailed(g,millis());}
                    front::artworkClear();release(cached);
                }
            }
            ibtlimit::release(ibtlimit::Job::ArtworkDecode);
        }
    }
    pump();
}
}
#elif FEATURE_ARTWORK && IBT_NO_PSRAM
#include "light_artwork.inc"
#else
namespace onlineart {bool copyCached(uint8_t *&b,size_t &n){b=nullptr;n=0;return false;}void begin(){}void setEnabled(bool,uint32_t){}void reset(uint32_t){}bool mayRequest(uint32_t,uint32_t){return false;}void requestStarted(uint32_t,uint32_t){}void requestFailed(uint32_t,uint32_t){}void requestSucceeded(uint32_t,uint32_t){}uint32_t retryAfterMs(uint32_t,uint32_t){return 0;}void requestUrl(const char*,uint32_t){}void requestTrack(const char*,uint32_t){}void smoke(uint32_t){}bool submitJpegCopy(const uint8_t*,size_t,uint32_t){return false;}void redraw(uint32_t){}void uiTick(uint32_t,bool){}size_t maxJpegBytes(){return 0;}CacheStatus cacheStatus(uint32_t){return {};}}
#endif
