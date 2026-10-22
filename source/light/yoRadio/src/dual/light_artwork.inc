// Light: bounded native album lookup plus four persistent RGB332 station logos.
// Native album fetch has a smaller split-allocation plan and can run during
// playback. Generic legacy logo-fetch admission remains unchanged.
#include "resource_limiter.h"
#include "memory_checkpoint.h"
#include "dns_auto.h"
#include "light_art_policy.h"
#include "light_art_history.h"
#include "light_album_policy.h"
#include "light_album_transport.h"
#include "light_network_gate.h"
#include "../ui/frontend.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <SD.h>
#include "../core/sdmanager.h"
#include "../core/config.h"
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_tls_errors.h>
#include <esp_crt_bundle.h>
#include <esp32/rom/tjpgd.h>
#include <cstring>
#include <new>
#if IBT_STREAM_ART_DIAG
#include "native_album_http.h"
#endif
namespace onlineart {
namespace {
constexpr size_t maxJpeg=4096, pixels=64*64, poolBytes=3100, fetchBudget=32768;
struct Request {char url[384]{};uint32_t generation=0,revision=0;uint64_t station=0;};
struct Thumb {bool valid=false;uint32_t generation=0,revision=0;uint8_t slot=0,width=64,height=64;uint64_t station=0,source=0;uint32_t pixelsHash=0;};
struct DiskImage {uint32_t magic=0x3241524c;uint16_t version=1,width=0,height=0,reserved=0;uint64_t station=0,source=0;uint32_t pixelsHash=0,headerHash=0;};
struct DiskHistory {uint32_t magic=0x3248524c,version=1,sequence=0;uint64_t ids[4]{};uint32_t hash=0;};
struct Work {Request req;JDEC decoder{};uint8_t pool[poolBytes],jpeg[maxJpeg];size_t length=0,position=0;uint8_t* output=nullptr;uint32_t deadline=0;};
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static Request pending,workerRequest;
static uint32_t workerNetToken=0;
static Thumb incoming,cached;
// UI-owned, opened and primed only under the original allocation admission.
// Retain one 128-byte stdio buffer across playback restarts: painting never
// opens a file or allocates a framebuffer while the audio heap is pressured.
static File paintFile;
static Thumb paintImage;
static void closePaint(){paintFile.close();paintFile=File{};paintImage={};}
static bool enabled=false,busy=false,queued=false,dirty=false;
static uint32_t generation=0,revision=0,nextTry=0;
static uint8_t failures=0,row=0;
static lightart::History history;
static lightart::Listening listening;
static bool historyLoaded=false,historyDirty=false;
static uint32_t historySequence=0,historyChangedAt=0,historySavedAt=0,historyTriedAt=0;
static uint64_t activeStation=0;
static uint32_t lookupRevision=0,lookupRetryAt=0;
static bool lookupRetry=false;
static bool historyContains(uint64_t id){portENTER_CRITICAL(&mux);bool found=history.contains(id);portEXIT_CRITICAL(&mux);return found;}
static void imagePath(char* path,unsigned slot){snprintf(path,32,"/light-art-%u.rgb",slot);}
static bool validHistory(const DiskHistory& h){return h.magic==0x3248524c&&h.version==1&&h.hash==lightart::checksum(reinterpret_cast<const uint8_t*>(&h),offsetof(DiskHistory,hash));}
static bool loadHistory(){
 if(historyLoaded)return true;if(!ibtlimit::allowAllocation(1024))return false;
 DiskHistory best{};bool found=false;
 for(unsigned i=0;i<2;++i){char path[32];snprintf(path,sizeof(path),"/light-art-history%u",i);File f=SPIFFS.open(path,"r");DiskHistory h{};
  if(f&&f.setBufferSize(128)&&f.size()==sizeof(h)&&f.read(reinterpret_cast<uint8_t*>(&h),sizeof(h))==sizeof(h)&&validHistory(h)&&(!found||int32_t(h.sequence-best.sequence)>0)){best=h;found=true;}}
 if(found){for(unsigned i=0;i<4;++i)history.ids[i]=best.ids[i];historySequence=best.sequence;}
 historyLoaded=true;return true;
}
static bool readHeader(unsigned slot,DiskImage& h,bool* ioFailed=nullptr){
 char path[32];imagePath(path,slot);File f=SPIFFS.open(path,"r");
 if(!f)return false;if(!f.setBufferSize(128)){if(ioFailed)*ioFailed=true;return false;}
 return f.size()==sizeof(h)+pixels&&f.read(reinterpret_cast<uint8_t*>(&h),sizeof(h))==sizeof(h)&&h.magic==0x3241524c&&h.version==1&&h.width&&h.height&&h.width<=64&&h.height<=64&&h.headerHash==lightart::checksum(reinterpret_cast<const uint8_t*>(&h),offsetof(DiskImage,headerHash));
}
static bool saveHistory(){
 historyTriedAt=millis();
 DiskHistory h{};uint32_t seq;
 portENTER_CRITICAL(&mux);seq=historySequence+1;h.sequence=seq;for(unsigned i=0;i<4;++i)h.ids[i]=history.ids[i];portEXIT_CRITICAL(&mux);
 h.hash=lightart::checksum(reinterpret_cast<const uint8_t*>(&h),offsetof(DiskHistory,hash));
 char path[32];snprintf(path,sizeof(path),"/light-art-history%u",seq&1);File f=SPIFFS.open(path,"w");bool ok=f&&f.setBufferSize(128)&&f.write(reinterpret_cast<const uint8_t*>(&h),sizeof(h))==sizeof(h);f.close();
 if(ok){portENTER_CRITICAL(&mux);bool same=true;for(unsigned i=0;i<4;++i)same=same&&history.ids[i]==h.ids[i];historySequence=seq;if(same)historyDirty=false;historySavedAt=millis();portEXIT_CRITICAL(&mux);}return ok;
}
static bool lookup(const Request& r,Thumb& image,bool* ioFailed=nullptr){
 if(!historyContains(r.station))return false;
 for(unsigned slot=0;slot<4;++slot){DiskImage h{};if(readHeader(slot,h,ioFailed)&&h.reserved==1&&h.station==r.station&&h.source==lightart::key(r.url)){image={true,r.generation,r.revision,uint8_t(slot),uint8_t(h.width),uint8_t(h.height),h.station,h.source,h.pixelsHash};return true;}}return false;
}

static bool current(const Request& r){portENTER_CRITICAL(&mux);bool ok=enabled&&r.generation==generation&&r.revision==revision;portEXIT_CRITICAL(&mux);return ok;}
static bool safeUrl(const char* u){return lightart::source(u);}

static UINT input(JDEC* j,BYTE* dst,UINT n){auto* w=static_cast<Work*>(j->device);size_t count=min(size_t(n),w->length-w->position);if(dst)memcpy(dst,w->jpeg+w->position,count);w->position+=count;return UINT(count);}
static UINT output(JDEC* j,void* bitmap,JRECT* r){
 auto* w=static_cast<Work*>(j->device);
 if(!current(w->req)||!ibtlimit::allowArtworkIdleProgress()||int32_t(millis()-w->deadline)>=0||r->right>=64||r->bottom>=64)return 0;
 auto* src=static_cast<uint8_t*>(bitmap);
 for(unsigned y=r->top;y<=r->bottom;++y)for(unsigned x=r->left;x<=r->right;++x){w->output[y*64+x]=lightart::pack(src[0],src[1],src[2]);src+=3;}
 vTaskDelay(1);return 1;
}
static bool readImage(Work* w){
 if(w->req.url[0]=='/'){
  File f=SPIFFS.open(w->req.url,"r");if(!f&&sdman.ready)f=SD.open(w->req.url,"r");if(!f||!f.size()||f.size()>maxJpeg)return false;
  w->length=f.size();return f.read(w->jpeg,w->length)==w->length;
 }
 const uint32_t token=workerNetToken;
 if(!lightnet::gate().acquire(false,token))return false;
 struct NetworkLease {~NetworkLease(){lightnet::gate().release();}} networkLease;
 esp_http_client_config_t cfg{};cfg.url=w->req.url;cfg.timeout_ms=1000;cfg.buffer_size=512;cfg.buffer_size_tx=512;cfg.disable_auto_redirect=true;
 auto c=esp_http_client_init(&cfg);if(!c)return false;
 bool ok=false;
 if(esp_http_client_open(c,0)==ESP_OK){
  const int64_t length=esp_http_client_fetch_headers(c);
  if(esp_http_client_get_status_code(c)==200&&length>0&&length<=int64_t(maxJpeg)){
   while(w->length<size_t(length)&&lightnet::gate().current(token)&&current(w->req)&&ibtlimit::allowArtworkIdleProgress()&&int32_t(millis()-w->deadline)<0){
    int n=esp_http_client_read(c,reinterpret_cast<char*>(w->jpeg+w->length),min(512,int(length-w->length)));
    if(n<=0)break;w->length+=size_t(n);vTaskDelay(1);
   }ok=lightnet::gate().current(token)&&w->length==size_t(length);
  }
 }
 esp_http_client_close(c);esp_http_client_cleanup(c);return ok;
}
static void worker(void*){
 auto* w=(ibtlimit::allowArtworkIdleProgress()&&ibtlimit::allowAllocation(sizeof(Work)+pixels+4096))?static_cast<Work*>(heap_caps_calloc(1,sizeof(Work),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)):nullptr;bool ok=false;
 if(w){new(w)Work{};w->req=workerRequest;w->deadline=millis()+3000;
  if(current(w->req)&&ibtlimit::allowArtworkIdleProgress()&&readImage(w)){
   w->output=(ibtlimit::allowArtworkIdleProgress()&&ibtlimit::allowAllocation(pixels))?static_cast<uint8_t*>(heap_caps_calloc(1,pixels,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)):nullptr;
   if(w->output&&ibtlimit::allowArtworkIdleProgress()&&jd_prepare(&w->decoder,input,w->pool,poolBytes,w)==JDR_OK){
    const int scale=lightart::scale(w->decoder.width,w->decoder.height);
    if(scale>=0)
     ok=jd_decomp(&w->decoder,output,BYTE(scale))==JDR_OK&&current(w->req);
   }
  }
 }
 if(ok)ok=current(w->req)&&ibtlimit::allowArtworkIdleProgress();
 Thumb prepared{};
 if(ok){
  unsigned target=4;DiskImage old{};
  for(unsigned slot=0;slot<4;++slot){DiskImage h{};bool found=readHeader(slot,h);if(found&&h.station==w->req.station){target=slot;old=h;break;}if(target==4&&(!found||!historyContains(h.station)))target=slot;}
  ok=target<4&&historyContains(w->req.station);
  if(ok){
   const int scale=lightart::scale(w->decoder.width,w->decoder.height);DiskImage h{};
   h.reserved=1;h.station=w->req.station;h.source=lightart::key(w->req.url);h.width=w->decoder.width>>scale;h.height=w->decoder.height>>scale;
   h.pixelsHash=lightart::checksum(w->output,pixels);h.headerHash=lightart::checksum(reinterpret_cast<const uint8_t*>(&h),offsetof(DiskImage,headerHash));
   // Identical content never rewrites its flash slot.
   bool same=old.reserved==h.reserved&&old.station==h.station&&old.source==h.source&&old.width==h.width&&old.height==h.height&&old.pixelsHash==h.pixelsHash;
   if(!same){
    File f=SPIFFS.open("/light-art-cache.tmp","w");ok=f&&f.setBufferSize(128)&&f.write(reinterpret_cast<const uint8_t*>(&h),sizeof(h))==sizeof(h)&&f.write(w->output,pixels)==pixels;f.close();
    char path[32];imagePath(path,target);
    if(ok&&current(w->req)&&ibtlimit::allowArtworkIdleProgress()){
     SPIFFS.remove("/light-art-cache.bak");bool moved=SPIFFS.exists(path);if(moved)ok=SPIFFS.rename(path,"/light-art-cache.bak");
     if(ok){ok=SPIFFS.rename("/light-art-cache.tmp",path);if(!ok&&moved)SPIFFS.rename("/light-art-cache.bak",path);else SPIFFS.remove("/light-art-cache.bak");}
    }else ok=false;
    SPIFFS.remove("/light-art-cache.tmp");
   }
   if(ok){prepared={true,w->req.generation,w->req.revision,uint8_t(target),uint8_t(h.width),uint8_t(h.height),h.station,h.source,h.pixelsHash};if(historyDirty&&ibtlimit::allowArtworkIdleProgress())saveHistory();}
  }
 }
 portENTER_CRITICAL(&mux);
 if(ok&&enabled&&w->req.generation==generation&&w->req.revision==revision){incoming=prepared;failures=0;queued=false;}
 else {if(failures<6)++failures;nextTry=millis()+60000;}
 portEXIT_CRITICAL(&mux);
 if(w){free(w->output);free(w);}
 Serial.printf("[LIGHT-ART] prepared=%u max_jpeg=4096 thumbnail=4096 stack_min=%u\n",unsigned(ok),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
 ibtlimit::release(ibtlimit::Job::ArtworkFetch);
 portENTER_CRITICAL(&mux);busy=false;portEXIT_CRITICAL(&mux);vTaskDelete(nullptr);
}
#include "light_album.inc"
}
// Browser sends exactly 4096 RGB332 pixels. It performs resize and conversion;
// no JPEG/TLS/task allocation is needed on the radio. The admitted HTTP owner
// excludes new heavy jobs. Raw upload has no Work/JPEG/output buffer or task:
// reserve 8KiB for one file/stdio allocation, paths and response headroom.
// The existing reserve, pressure and largest-block policy remains unchanged.
static constexpr size_t rawUploadBudget=8192, uploadFileBuffer=128;
static_assert(rawUploadBudget>=4096+2048,"Keep room for a VFS file and bookkeeping");
static File uploadFile;
static Request uploadRequest;
static DiskImage uploadHeader;
static size_t uploadWritten=0;
static bool uploading=false;
static bool uploadCurrent(){return uploading&&current(uploadRequest)&&ibtlimit::allowArtworkIdleProgress()&&ibtlimit::allowAllocation(rawUploadBudget);}
UploadStatus uploadStatus(){
 portENTER_CRITICAL(&mux);UploadStatus s{activeStation,generation,0,enabled,history.contains(activeStation),busy};portEXIT_CRITICAL(&mux);
 // Read-only status: report only a checksum-verified retained header.
 if(!s.busy&&ibtlimit::allowAllocation(1024))for(unsigned slot=0;slot<4;++slot){DiskImage h{};if(readHeader(slot,h)&&h.reserved==1&&h.station==s.station){s.hash=h.pixelsHash;break;}}
 return s;
}
const char* albumStatus(){return album::status();}
void uploadAbort(){
 if(!uploading)return;uploadFile.close();SPIFFS.remove("/light-art-upload.tmp");
 portENTER_CRITICAL(&mux);uploading=false;busy=false;portEXIT_CRITICAL(&mux);
}
bool uploadBegin(uint32_t g,uint64_t station){
 if(!loadHistory()||!ibtlimit::allowArtworkIdleProgress()||!ibtlimit::allowAllocation(rawUploadBudget))return false;
 portENTER_CRITICAL(&mux);
 const bool ok=enabled&&!busy&&!incoming.valid&&g==generation&&station&&station==activeStation&&history.contains(station)&&pending.url[0];
 if(ok){busy=true;uploading=true;uploadRequest=pending;uploadWritten=0;}
 portEXIT_CRITICAL(&mux);if(!ok)return false;
 uploadHeader=DiskImage{};uploadHeader.reserved=1;uploadHeader.width=uploadHeader.height=64;uploadHeader.station=station;
 uploadHeader.source=lightart::key(uploadRequest.url);uploadHeader.pixelsHash=2166136261u;
 uploadFile=SPIFFS.open("/light-art-upload.tmp","w");
 if(!uploadFile||!uploadFile.setBufferSize(uploadFileBuffer)||uploadFile.write(reinterpret_cast<const uint8_t*>(&uploadHeader),sizeof(uploadHeader))!=sizeof(uploadHeader)){uploadAbort();return false;}
 return true;
}
bool uploadWrite(size_t index,const uint8_t* data,size_t length){
 if(!uploadCurrent()||index!=uploadWritten||!length||length>pixels-uploadWritten)return false;
 if(uploadFile.write(data,length)!=length)return false;
 for(size_t i=0;i<length;++i){uploadHeader.pixelsHash^=data[i];uploadHeader.pixelsHash*=16777619u;}
 uploadWritten+=length;return true;
}
bool uploadFinish(uint32_t& hash){
 hash=0;if(!uploadCurrent()||uploadWritten!=pixels){uploadAbort();return false;}
 uploadHeader.headerHash=lightart::checksum(reinterpret_cast<const uint8_t*>(&uploadHeader),offsetof(DiskImage,headerHash));
 bool ok=uploadFile.seek(0)&&uploadFile.write(reinterpret_cast<const uint8_t*>(&uploadHeader),sizeof(uploadHeader))==sizeof(uploadHeader);uploadFile.close();
 // Verify the complete persisted payload, bounded to one 64-byte row on stack.
 if(ok){File f=SPIFFS.open("/light-art-upload.tmp","r");uint8_t line[64];uint32_t check=2166136261u;ok=f&&f.setBufferSize(uploadFileBuffer)&&f.size()==sizeof(DiskImage)+pixels&&f.seek(sizeof(DiskImage));
  for(unsigned row=0;ok&&row<64;++row){ok=f.read(line,64)==64;for(unsigned x=0;ok&&x<64;++x){check^=line[x];check*=16777619u;}}
  ok=ok&&check==uploadHeader.pixelsHash;
 }
 unsigned target=4;DiskImage old{};
 for(unsigned slot=0;ok&&slot<4;++slot){DiskImage h{};bool found=readHeader(slot,h);if(found&&h.station==uploadRequest.station){target=slot;old=h;break;}if(target==4&&(!found||!historyContains(h.station)))target=slot;}
 ok=ok&&target<4&&uploadCurrent();
 if(ok){char path[32];imagePath(path,target);
  const bool same=old.reserved==uploadHeader.reserved&&old.station==uploadHeader.station&&old.source==uploadHeader.source&&old.pixelsHash==uploadHeader.pixelsHash;
  if(!same){
   SPIFFS.remove("/light-art-upload.bak");const bool moved=SPIFFS.exists(path);if(moved)ok=SPIFFS.rename(path,"/light-art-upload.bak");
   if(ok){ok=SPIFFS.rename("/light-art-upload.tmp",path);if(!ok&&moved)SPIFFS.rename("/light-art-upload.bak",path);else SPIFFS.remove("/light-art-upload.bak");}
  }
  if(ok&&historyDirty)ok=saveHistory();
  if(ok){hash=uploadHeader.pixelsHash;
   portENTER_CRITICAL(&mux);if(enabled&&generation==uploadRequest.generation&&revision==uploadRequest.revision){incoming={true,uploadRequest.generation,uploadRequest.revision,uint8_t(target),64,64,uploadHeader.station,uploadHeader.source,uploadHeader.pixelsHash};queued=false;failures=0;}portEXIT_CRITICAL(&mux);
  }
 }
 uploadAbort();return ok;
}
void begin(){}
void serviceReader(){
 album::service();
 // UI/Core0 only, including when Cover/BT/SD makes uiTick unreachable.
 // Release the admitted descriptor without any LCD work or new allocation.
 if(!paintFile)return;
 portENTER_CRITICAL(&mux);const bool keep=enabled&&activeStation==paintImage.station&&pending.url[0]&&lightart::key(pending.url)==paintImage.source;portEXIT_CRITICAL(&mux);
 if(!keep)closePaint();
}
void listeningTick(uint32_t g,uint32_t elapsed,bool playing){
 if(!loadHistory())return;const uint32_t now=millis();
 portENTER_CRITICAL(&mux);const uint64_t station=g==generation?activeStation:0;
 if(listening.observe(station,g,elapsed,playing,now)&&history.heard(station)){historyDirty=true;historyChangedAt=now;}portEXIT_CRITICAL(&mux);
}
bool copyCached(uint8_t*& p,size_t& n){p=nullptr;n=0;return false;}
void setEnabled(bool on,uint32_t g){
 portENTER_CRITICAL(&mux);const bool changed=on&&(!enabled||generation!=g);enabled=on;generation=g;
 if(!on){queued=false;++revision;}
 else if(changed&&pending.url[0]){pending.generation=g;pending.station=activeStation;pending.revision=++revision;queued=true;nextTry=0;failures=0;}
 const bool seed=on&&!pending.url[0];portEXIT_CRITICAL(&mux);
 // Enabling Cover after 60 seconds in VU mode must not wait for new metadata.
 if(seed){char path[40];snprintf(path,sizeof(path),"/artwork/%u.jpg",unsigned(config.lastStation()));requestUrl(path,g);}
}
void reset(uint32_t g){
 char path[40];snprintf(path,sizeof(path),"/artwork/%u.jpg",unsigned(config.lastStation()));
 const uint64_t station=config.getMode()==PM_WEB&&config.station.url[0]?lightart::key(config.station.url):0;
 // Publish the connection and its source together: UI must never observe a
 // same-station restart between clearing pending.url and seeding its request.
 portENTER_CRITICAL(&mux);generation=g;activeStation=station;queued=false;pending={};++revision;album::resetLocked(g,station);
 if(enabled){strlcpy(pending.url,path,sizeof(pending.url));pending.generation=g;pending.station=station;pending.revision=revision;queued=true;nextTry=0;failures=0;}
 portEXIT_CRITICAL(&mux);
}
void requestUrl(const char* url,uint32_t g){
 if(!safeUrl(url)){Serial.println("[LIGHT-ART] source=UNSUPPORTED use=local_or_http_baseline_jpeg_4096B");return;}
 portENTER_CRITICAL(&mux);
 if(enabled&&g==generation){strlcpy(pending.url,url,sizeof(pending.url));pending.generation=g;pending.station=activeStation;pending.revision=++revision;queued=true;nextTry=0;failures=0;}
 portEXIT_CRITICAL(&mux);
}
void requestTrack(const char* title,uint32_t g){album::request(title,g);portENTER_CRITICAL(&mux);bool empty=!pending.url[0];portEXIT_CRITICAL(&mux);if(empty){char path[40];snprintf(path,sizeof(path),"/artwork/%u.jpg",unsigned(config.lastStation()));requestUrl(path,g);}}
bool submitJpegCopy(const uint8_t*,size_t,uint32_t){return false;}
void smoke(uint32_t g){requestUrl("/light-art.jpg",g);}
size_t maxJpegBytes(){return maxJpeg;}
bool mayRequest(uint32_t g,uint32_t now){portENTER_CRITICAL(&mux);bool ok=enabled&&g==generation&&!busy&&int32_t(now-nextTry)>=0;portEXIT_CRITICAL(&mux);return ok;}
void requestStarted(uint32_t,uint32_t){}
void requestFailed(uint32_t,uint32_t){}
void requestSucceeded(uint32_t,uint32_t){}
uint32_t retryAfterMs(uint32_t,uint32_t now){portENTER_CRITICAL(&mux);uint32_t n=int32_t(nextTry-now)>0?nextTry-now:0;portEXIT_CRITICAL(&mux);return n;}
CacheStatus cacheStatus(uint32_t){portENTER_CRITICAL(&mux);CacheStatus s{0,failures,queued||busy};portEXIT_CRITICAL(&mux);return s;}
void redraw(uint32_t){dirty=true;row=0;album::dirty=true;album::row=0;}
void uiTick(uint32_t g,bool visible){
 Thumb image;bool active;uint32_t rev;Request request;
 portENTER_CRITICAL(&mux);
 if(g!=generation){portEXIT_CRITICAL(&mux);return;} // stale UI snapshot: don't consume a newer result or clear its reader.
 image=incoming;incoming={};active=enabled;rev=revision;request=pending;portEXIT_CRITICAL(&mux);
 // A generation identifies an audio connection, not the retained station art.
 // Rebind only an already verified same-station, same-source thumbnail. Stale
 // worker results below still need an exact generation AND revision match.
 const bool same=active&&request.generation==g&&request.revision==rev&&request.station&&cached.valid&&cached.station==request.station&&cached.source==lightart::key(request.url);
 if(same){
  if(cached.generation!=g||cached.revision!=rev){cached.generation=g;cached.revision=rev;dirty=true;row=0;}
  // Already verified, no need to reopen four slot headers on this connection.
  portENTER_CRITICAL(&mux);if(generation==g&&revision==rev)queued=false;portEXIT_CRITICAL(&mux);
 }
 if(image.valid&&active&&image.generation==g&&image.revision==rev){cached=image;dirty=true;row=0;}
 // Lookup is read-only, bounded, and gated by file-open headroom. It may
 // display an already retained logo before the next 60-second qualification.
 if(active&&queued&&!busy&&(lookupRevision!=rev||(lookupRetry&&int32_t(millis()-lookupRetryAt)>=0))&&loadHistory()&&ibtlimit::allowArtworkProgress()&&ibtlimit::allowAllocation(1024)){
  Thumb hit{};
  lookupRevision=rev;lookupRetryAt=millis()+1000;lookupRetry=false;
  if(lookup(request,hit,&lookupRetry)&&current(request)){cached=hit;queued=false;dirty=true;row=0;}
 }
 if(cached.valid&&(!active||cached.generation!=g||cached.revision!=rev)){cached={};dirty=false;closePaint();front::artworkClear();}
 if(paintFile&&(!cached.valid||paintImage.slot!=cached.slot||paintImage.station!=cached.station||paintImage.source!=cached.source||paintImage.pixelsHash!=cached.pixelsHash))closePaint();
 if(!busy&&cached.valid&&!paintFile&&ibtlimit::allowArtworkProgress()&&ibtlimit::allowAllocation(1024)){
  char path[32];imagePath(path,cached.slot);paintFile=SPIFFS.open(path,"r");DiskImage h{};
  if(!paintFile||!paintFile.setBufferSize(128)||paintFile.size()!=sizeof(h)+pixels||paintFile.read(reinterpret_cast<uint8_t*>(&h),sizeof(h))!=sizeof(h)||h.magic!=0x3241524c||h.version!=1||h.width!=cached.width||h.height!=cached.height||h.headerHash!=lightart::checksum(reinterpret_cast<const uint8_t*>(&h),offsetof(DiskImage,headerHash))||h.station!=cached.station||h.source!=cached.source||h.pixelsHash!=cached.pixelsHash){closePaint();return;}
  paintImage=cached;
 }
 // This is only a bounded pixel copy, not a fetch/decode admission bypass.
 // The buffer was already admitted and primed above. Keep the unchanged
 // reserve/audio-fill gate on EACH row, plus the caller's 500ms PCM UI guard.
 const bool albumOwns=album::ui(g,visible);
 if(!albumOwns&&!busy&&visible&&cached.valid&&paintFile&&dirty&&ibtlimit::allowArtworkProgress()){
  for(unsigned slice=0;slice<8&&dirty&&ibtlimit::allowArtworkProgress();++slice){
  if(!row&&!front::artworkBegin(cached.width,cached.height))return;
  uint8_t source[64];
  if(!paintFile.seek(sizeof(DiskImage)+row*64)||paintFile.read(source,64)!=64){cached={};dirty=false;closePaint();front::artworkClear();return;}
  uint16_t line[64];for(unsigned x=0;x<64;++x)line[x]=lightart::expand(source[x]);
  front::artworkTile(0,row,64,1,64,line);if(++row==cached.height){dirty=false;row=0;front::artworkEnd();}
  }
 }
 // Coalesce ordering writes for five minutes; no flash writes while audio
 // is active. Image preparation can checkpoint the same pending order sooner.
 if(historyDirty&&!busy&&uint32_t(millis()-historyChangedAt)>=300000&&uint32_t(millis()-historySavedAt)>=300000&&uint32_t(millis()-historyTriedAt)>=60000&&ibtlimit::allowArtworkIdleProgress()&&ibtlimit::acquire(ibtlimit::Job::ArtworkFetch,fetchBudget)){
  saveHistory();ibtlimit::release(ibtlimit::Job::ArtworkFetch);
 }
 bool start;
 portENTER_CRITICAL(&mux);start=active&&queued&&!busy&&!incoming.valid&&history.contains(pending.station)&&failures<3&&pending.generation==g&&int32_t(millis()-nextTry)>=0;portEXIT_CRITICAL(&mux);
 if(!start||(pending.url[0]!='/'&&!lightnet::gate().permitted())||!ibtlimit::allowArtworkStart()||!ibtlimit::acquire(ibtlimit::Job::ArtworkFetch,fetchBudget))return;
 portENTER_CRITICAL(&mux);start=enabled&&queued&&!busy&&pending.generation==g;if(start){workerRequest=pending;workerNetToken=lightnet::gate().token();busy=true;}portEXIT_CRITICAL(&mux);
 if(!start){ibtlimit::release(ibtlimit::Job::ArtworkFetch);return;}
 if(xTaskCreatePinnedToCore(worker,"light_art",6144,nullptr,1,nullptr,0)!=pdPASS){portENTER_CRITICAL(&mux);busy=false;nextTry=millis()+60000;portEXIT_CRITICAL(&mux);ibtlimit::release(ibtlimit::Job::ArtworkFetch);}
}
}
