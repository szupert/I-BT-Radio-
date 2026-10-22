// Native, optional Light lookup. One serialized worker, no album flash writes.
// Separate smaller split-allocation plan allows background work during playback.
// Fresh75% input to start /50% to continue; audio is never paused here.
namespace album {
static void diagnosticMemory(const char* stage,int result=0){
#if IBT_STREAM_ART_DIAG
 const unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
 Serial.printf("[DIAG-ALBUM] phase=%s result=%d free=%u largest=%u reserve=%u\n",stage,result,unsigned(heap_caps_get_free_size(caps)),unsigned(heap_caps_get_largest_free_block(caps)),unsigned(ibtlimit::Policy::reserve));
#endif
}
constexpr uint32_t budget=40960,deadlineMs=15000,encodedLimit=32768;
static_assert(budget==40960,"Native split-allocation charge");
struct Track {char text[171]{};uint32_t generation=0,revision=0;uint64_t station=0,key=0;};
struct Image {uint32_t* words=nullptr;uint64_t station=0,key=0;uint8_t width=0,height=0;};
struct Work {Track req;JDEC decoder{};uint8_t* scratch=nullptr;char query[700];uint8_t discard[256];uint32_t* words=nullptr;esp_http_client_handle_t client=nullptr;uint32_t deadline=0,received=0,freeSampleMinimum=UINT32_MAX,networkToken=0;bool lightweight=false,redirectSeen=false,redirectAllowed=false,networkHeld=false,deferred=false,dnsDeferred=false;
#if IBT_STREAM_ART_DIAG
 nativealbum::Http* native=nullptr;bool itunes=false;
#endif
};
static_assert(sizeof(Work)<=1536,"Artwork allocation exceeds plan");
enum State:uint8_t {None,Pending,AudioDeferred,RamDeferred,Loading,Ready,Missing,Failed};
static Track pending,workerTrack;
static bool workerLightweight=false;
static uint32_t workerNetworkToken=0;
static Image incoming,shown;
static uint64_t readyKey=0;
static bool queued=false,dirty=false,fallbackDirty=false;
static uint8_t row=0,failures=0,dnsRetries=0;
static State state=None;
static uint32_t nextAllowed=0,settleUntil=0;
static bool currentLocked(const Track& r){return enabled&&r.generation==generation&&r.station==activeStation&&r.key==pending.key&&r.revision==pending.revision;}
static bool current(const Track& r){portENTER_CRITICAL(&mux);bool ok=currentLocked(r);portEXIT_CRITICAL(&mux);return ok;}
static void resetLocked(uint32_t g,uint64_t station){
 if(pending.station!=station){pending={};readyKey=0;queued=false;state=None;failures=0;}
 else if(pending.key){pending.generation=g;++pending.revision;queued=readyKey!=pending.key;state=queued?Pending:Ready;}
}
static void request(const char* title,uint32_t g){
 lightalbum::Pair parts{};bool valid=lightalbum::pair(title,parts);const uint64_t key=valid?lightart::key(title):0;
 portENTER_CRITICAL(&mux);
 if(enabled&&g==generation&&(key!=pending.key||pending.generation!=g||(key&&!queued&&readyKey!=key&&!failures))){
  const uint32_t rev=pending.revision+1;pending={};pending.generation=g;pending.revision=rev;pending.station=activeStation;pending.key=key;
  if(key)strlcpy(pending.text,title,sizeof(pending.text));failures=0;queued=key!=0;settleUntil=millis()+2000;state=queued?Pending:Missing;readyKey=0;dnsRetries=0;
 }
 portEXIT_CRITICAL(&mux);
}
static void release(Image& image){free(image.words);image={};}
static void service(){
 Image stale;uint64_t station,key;bool on;
 portENTER_CRITICAL(&mux);station=activeStation;key=pending.key;on=enabled;
 if(incoming.words&&(!on||incoming.station!=station||incoming.key!=key)){stale=incoming;incoming={};if(readyKey==stale.key)readyKey=0;}
 portEXIT_CRITICAL(&mux);release(stale);
 if(shown.words&&(!on||shown.station!=station||shown.key!=key)){uint64_t oldKey=shown.key;release(shown);portENTER_CRITICAL(&mux);if(readyKey==oldKey)readyKey=0;portEXIT_CRITICAL(&mux);dirty=false;fallbackDirty=true;}
}
static bool progress(Work* w){
 const uint32_t freeNow=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);if(freeNow<w->freeSampleMinimum)w->freeSampleMinimum=freeNow;
 return lightnet::gate().current(w->networkToken)&&current(w->req)&&ibtlimit::allowArtworkProgress()&&int32_t(millis()-w->deadline)<0;
}
// JSON/JPEG scratch is acquired after TLS handshake and freed before the next
// connection. It is never resident during either handshake peak.
static bool scratch(Work* w,size_t bytes){
 lightmem::Phase memoryPhase("ALBUM_SCRATCH");
 free(w->scratch);w->scratch=nullptr;
 if(!progress(w)||heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<ibtlimit::Policy::reserve+bytes||
    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<bytes+4096)return false;
 diagnosticMemory("SCRATCH_BEFORE",int(bytes));w->scratch=static_cast<uint8_t*>(heap_caps_calloc(1,bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));diagnosticMemory("SCRATCH_AFTER",w->scratch?int(bytes):0);return w->scratch!=nullptr;
}
static void close(Work* w){
#if IBT_STREAM_ART_DIAG
 delete w->native;w->native=nullptr;
#endif
 if(w->client){esp_http_client_close(w->client);esp_http_client_cleanup(w->client);w->client=nullptr;}if(w->networkHeld){lightnet::gate().release();w->networkHeld=false;}}
static int read(Work* w,uint8_t* bytes,size_t size){
#if IBT_STREAM_ART_DIAG
 if(w->native)return w->native->read(bytes,size);
#endif
 return esp_http_client_read(w->client,reinterpret_cast<char*>(bytes),size);
}
static bool done(Work* w){
#if IBT_STREAM_ART_DIAG
 if(w->native)return w->native->done();
#endif
 return esp_http_client_is_complete_data_received(w->client);
}
static esp_err_t httpEvent(esp_http_client_event_t* event){
 auto* w=static_cast<Work*>(event->user_data);
 if(w&&event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&event->header_value&&!strcasecmp(event->header_key,"Location")){
  const bool allowed=!w->redirectSeen&&(lightalbum::publicHttpUrl(event->header_value)||(!w->lightweight&&lightalbum::publicUrl(event->header_value,true)));w->redirectSeen=true;w->redirectAllowed=allowed;
 }
 return ESP_OK;
}
static bool open(Work* w,const char* url,size_t cap){
 lightmem::Phase memoryPhase("ALBUM_OPEN");
#if IBT_STREAM_ART_DIAG
 if(w->itunes){
  if(w->scratch||!progress(w)||!lightalbum::appleHttpsUrl(url))return false;close(w);
  // acquireAlbumTlsPlan charged the full40KiB before this explicit second-TLS lease.
  if(!lightnet::gate().acquire(true,w->networkToken,true)){w->deferred=true;return false;}w->networkHeld=true;
  diagnosticMemory("NATIVE_HTTPS_BEFORE");w->native=new(std::nothrow) nativealbum::Http;
  const bool ok=w->native&&w->native->open(url,cap);diagnosticMemory("NATIVE_HTTPS_AFTER",w->native?w->native->status:0);
  if(!ok&&w->native&&w->native->dnsFailure&&dnsRetries<1){++dnsRetries;w->deferred=w->dnsDeferred=true;radiodnsauto::artworkFailed();}
  if(ok)w->received=0;return ok&&progress(w);
 }
#endif
 if(w->scratch||!progress(w)||(w->lightweight&&!lightalbum::publicHttpUrl(url)))return false;close(w);
 if(!lightnet::gate().acquire(!strncmp(url,"https://",8),w->networkToken)){w->deferred=true;Serial.println("[LIGHT-NET] album=DEFERRED reason=stream_or_cancel");return false;}w->networkHeld=true;
 esp_http_client_config_t cfg{};cfg.url=url;cfg.timeout_ms=1000;cfg.buffer_size=512;cfg.buffer_size_tx=512;cfg.disable_auto_redirect=true;cfg.crt_bundle_attach=esp_crt_bundle_attach;cfg.user_data=w;cfg.event_handler=httpEvent;cfg.user_agent="I-BT-Radio-Light/203 (https://github.com/szupert/I-BT-Radio-)";
 diagnosticMemory("CLIENT_INIT_BEFORE");w->client=esp_http_client_init(&cfg);diagnosticMemory("CLIENT_INIT_AFTER",w->client?1:0);if(!w->client)return false;
 for(unsigned redirects=0;redirects<=3&&progress(w);++redirects){
  Serial.printf("[LIGHT-NET] album_phase=OPEN tls=%u free=%u largest=%u reserve=%u\n",unsigned(!strncmp(url,"https://",8)),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(ibtlimit::Policy::reserve));
  const esp_err_t openResult=esp_http_client_open(w->client,0);diagnosticMemory("SOCKET_OPEN_AFTER",int(openResult));
  if(openResult!=ESP_OK){
   int tls=0,flags=0;const int transport=esp_http_client_get_and_clear_last_tls_error(w->client,&tls,&flags);
   if(transport==ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME)radiodnsauto::failed();
   Serial.printf("[LIGHT-ALBUM-NET] result=FAILED transport=%d tls=%d flags=%d dns_retry=%u\n",transport,tls,flags,unsigned(transport==ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME));return false;
  }
  if(!progress(w))return false;
  w->redirectSeen=w->redirectAllowed=false;int64_t bytes=esp_http_client_fetch_headers(w->client);int status=esp_http_client_get_status_code(w->client);diagnosticMemory("HEADERS_AFTER",status);
  if(bytes<0||bytes>int64_t(cap)||!progress(w)){diagnosticMemory("HEADERS_REJECTED",int(bytes));return false;}
  if(status==200){w->received=0;esp_http_client_set_timeout_ms(w->client,250);return true;}
  if(!w->redirectAllowed||redirects==3||(status!=301&&status!=302&&status!=303&&status!=307&&status!=308)||esp_http_client_set_redirection(w->client)!=ESP_OK)return false;
  esp_http_client_close(w->client);
 }return false;
}
static UINT input(JDEC* j,BYTE* dst,UINT n){
 auto* w=static_cast<Work*>(j->device);UINT count=0;
 while(count<n&&progress(w)&&w->received<encodedLimit){size_t part=min(size_t(n-count),min(sizeof(w->discard),size_t(encodedLimit-w->received)));
  int got=read(w,dst?dst+count:w->discard,part);if(got<=0)break;
  count+=got;w->received+=got;vTaskDelay(1);
 }return count;
}
static UINT output(JDEC* j,void* bitmap,JRECT* rect){
 auto* w=static_cast<Work*>(j->device);if(!progress(w)||rect->right>=64||rect->bottom>=64)return 0;
 auto* rgb=static_cast<uint8_t*>(bitmap);
 // EXEC/32BIT RAM must ONLY be accessed as aligned32bit words, never memcpy or uint8_t.
 for(unsigned y=rect->top;y<=rect->bottom;++y)for(unsigned x=rect->left;x<=rect->right;++x){unsigned pixel=y*64+x,shift=(pixel&3)*8;uint32_t packed=lightart::pack(rgb[0],rgb[1],rgb[2]);uint32_t old=w->words[pixel>>2];w->words[pixel>>2]=(old&~(255u<<shift))|(packed<<shift);rgb+=3;}
 vTaskDelay(1);return 1;
}
static void worker(void*){
 const unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
 const bool allocation=ibtlimit::allowArtworkProgress()&&heap_caps_get_free_size(caps)>=ibtlimit::Policy::reserve+sizeof(Work)&&heap_caps_get_largest_free_block(caps)>=sizeof(Work)+4096;
 auto* w=allocation?static_cast<Work*>(heap_caps_calloc(1,sizeof(Work),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)):nullptr;
 bool ok=false,matched=false,searched=false,published=false;Image result{};
 if(w){new(w)Work{};w->req=workerTrack;w->deadline=millis()+deadlineMs;w->lightweight=workerLightweight;w->networkToken=workerNetworkToken;
#if IBT_STREAM_ART_DIAG
  w->itunes=true;
#endif
  bool queryReady=false;
#if IBT_STREAM_ART_DIAG
  queryReady=lightalbum::itunesQuery(w->req.text,w->query,sizeof(w->query));
#else
  queryReady=coverQuery(w->req.text,w->query,sizeof(w->query));
#endif
  if(queryReady){
  if(w->lightweight)memmove(w->query+4,w->query+5,strlen(w->query+5)+1);
  if(open(w,w->query,4096)&&scratch(w,4096)){size_t count=0;
   while(count<4096&&progress(w)&&!done(w)){
    int n=read(w,w->scratch+count,min(size_t(256),4096-count));if(n<=0)break;count+=n;vTaskDelay(1);
   }
   bool complete=done(w);JsonView valid{reinterpret_cast<const char*>(w->scratch),reinterpret_cast<const char*>(w->scratch)+count};searched=complete&&valid.skip();valid.ws();searched=searched&&valid.p==valid.end;close(w);char uuid[37];
   #if IBT_STREAM_ART_DIAG
   matched=searched&&lightalbum::matchingItunes(w->req.text,w->scratch,count,w->query,sizeof(w->query));
#else
   matched=searched&&lightalbum::matchingRelease(w->req.text,w->scratch,count,uuid);
#endif
   diagnosticMemory(matched?"EXACT_MATCH":"NO_EXACT_MATCH",int(count));free(w->scratch);w->scratch=nullptr;
   if(matched){
#if !IBT_STREAM_ART_DIAG
    snprintf(w->query,sizeof(w->query),"%s://coverartarchive.org/release/%s/front-250",w->lightweight?"http":"https",uuid);
#endif
    const unsigned caps=MALLOC_CAP_EXEC|MALLOC_CAP_32BIT;
    // Preserve the existing MP3 EXEC reserve4KiB, without consuming the scarce8bit audio heap.
    if(progress(w)&&heap_caps_get_free_size(caps)>=pixels+4096&&heap_caps_get_largest_free_block(caps)>=pixels+4096)w->words=static_cast<uint32_t*>(heap_caps_malloc(pixels,caps));
    if(w->words){for(unsigned i=0;i<pixels/4;++i)w->words[i]=0;
     if(open(w,w->query,encodedLimit)&&scratch(w,3100)){
      const auto prepared=jd_prepare(&w->decoder,input,w->scratch,3100,w);const int scale=prepared==JDR_OK?lightart::scale(w->decoder.width,w->decoder.height):-1;
      Serial.printf("[ALBUM-JPEG] phase=HEADER result=%u width=%u height=%u scale=%d received=%lu\n",unsigned(prepared),unsigned(w->decoder.width),unsigned(w->decoder.height),scale,(unsigned long)w->received);
      if(scale>=0){const auto decoded=jd_decomp(&w->decoder,output,BYTE(scale));ok=decoded==JDR_OK&&progress(w);Serial.printf("[ALBUM-JPEG] phase=DECODE result=%u accepted=%u received=%lu\n",unsigned(decoded),unsigned(ok),(unsigned long)w->received);}
      if(ok){result={w->words,w->req.station,w->req.key,uint8_t(w->decoder.width>>scale),uint8_t(w->decoder.height>>scale)};w->words=nullptr;}
     }
    }
   }
  }
  }
  close(w);
 }
 Image stale;portENTER_CRITICAL(&mux);
 if(ok&&lightnet::gate().current(workerNetworkToken)&&currentLocked(workerTrack)){stale=incoming;incoming=result;result={};queued=false;state=Ready;readyKey=workerTrack.key;published=true;}
 else if(currentLocked(workerTrack)){
  if(!lightnet::gate().current(workerNetworkToken)||(w&&w->deferred)){state=AudioDeferred;queued=true;}
  else {state=searched&&!matched?Missing:Failed;queued=++failures<2;}
 }
 nextAllowed=millis()+((w&&w->dnsDeferred)?5000u:60000u);portEXIT_CRITICAL(&mux);
 release(stale);release(result);
 if(w){Serial.printf("[LIGHT-ALBUM-MEM] free_sample_min=%lu work_bytes=%u scratch_max=4096 task_bytes=%u job_charge=%lu lightweight_http=%u tls_peak_measured=0\n",(unsigned long)w->freeSampleMinimum,unsigned(sizeof(Work)),w->lightweight?6144u:8192u,(unsigned long)(w->lightweight?20480:budget),unsigned(w->lightweight));free(w->scratch);free(w->words);free(w);}
 Serial.printf("[LIGHT-ALBUM] result=%s native=1 generation=%lu station=%016llx track=%016llx flash_writes=0 thumbnail=64 stack_min=%u\n",published?"READY":(ok?"STALE":"FALLBACK"),(unsigned long)workerTrack.generation,(unsigned long long)workerTrack.station,(unsigned long long)workerTrack.key,unsigned(uxTaskGetStackHighWaterMark(nullptr)));
 ibtlimit::release(ibtlimit::Job::ArtworkFetch);portENTER_CRITICAL(&mux);busy=false;portEXIT_CRITICAL(&mux);vTaskDelete(nullptr);
}
static bool ui(uint32_t g,bool visible){
 service();Image next;portENTER_CRITICAL(&mux);next=incoming;incoming={};uint64_t station=activeStation,key=pending.key;bool on=enabled;portEXIT_CRITICAL(&mux);
 if(next.words){if(on&&next.station==station&&next.key==key){release(shown);shown=next;next={};dirty=true;row=0;}release(next);}
 if(fallbackDirty&&visible){front::artworkClear();fallbackDirty=false;onlineart::dirty=true;onlineart::row=0;}
 if(shown.words&&visible&&dirty&&ibtlimit::allowArtworkProgress()){
  for(unsigned slice=0;slice<8&&dirty&&ibtlimit::allowArtworkProgress();++slice){if(!row&&!front::artworkBegin(shown.width,shown.height))break;
   uint16_t line[64];for(unsigned x=0;x<shown.width;++x){uint32_t word=shown.words[(row*64+x)>>2];line[x]=lightart::expand(uint8_t(word>>((x&3)*8)));}
   front::artworkTile(0,row,shown.width,1,shown.width,line);if(++row==shown.height){dirty=false;row=0;front::artworkEnd();}
  }
 }
 bool start;portENTER_CRITICAL(&mux);start=on&&queued&&failures<2&&pending.generation==g&&pending.station==station&&pending.key&&!busy&&int32_t(millis()-nextAllowed)>=0&&int32_t(millis()-settleUntil)>=0;portEXIT_CRITICAL(&mux);
 if(start&&lightnet::gate().permitted()){
  bool lightweight=false;
  const char* dns=radiodnsauto::status();
  if(!strcmp(dns,"DNS_MEASURING")||!strcmp(dns,"DNS_NOT_MEASURED"))return shown.words!=nullptr;
  if(!ibtlimit::allowArtworkStart()){portENTER_CRITICAL(&mux);state=AudioDeferred;portEXIT_CRITICAL(&mux);}
  else if(!
#if IBT_STREAM_ART_DIAG
   ibtlimit::acquireAlbumTlsPlan()
#else
   ibtlimit::acquireAlbumPlan(&lightweight)
#endif
  ){portENTER_CRITICAL(&mux);state=RamDeferred;portEXIT_CRITICAL(&mux);}
  else{portENTER_CRITICAL(&mux);start=enabled&&!busy&&pending.generation==g&&pending.station==activeStation;if(start){workerTrack=pending;workerLightweight=lightweight;workerNetworkToken=lightnet::gate().token();busy=true;state=Loading;}portEXIT_CRITICAL(&mux);
   if(!start||xTaskCreatePinnedToCore(worker,"light_album",lightweight?6144:8192,nullptr,1,nullptr,0)!=pdPASS){if(start){portENTER_CRITICAL(&mux);busy=false;state=RamDeferred;queued=++failures<2;nextAllowed=millis()+60000;portEXIT_CRITICAL(&mux);}ibtlimit::release(ibtlimit::Job::ArtworkFetch);}
  }
 }
 return shown.words!=nullptr;
}
static const char* status(){portENTER_CRITICAL(&mux);State s=state;bool on=enabled;portEXIT_CRITICAL(&mux);if(!on)return "DISABLED";switch(s){case Pending:return "PENDING";case AudioDeferred:return "DEFERRED_AUDIO";case RamDeferred:return "DEFERRED_RAM";case Loading:return "LOADING";case Ready:return "READY";case Missing:return "NO_MATCH";case Failed:return "FAILED";default:return "NO_METADATA";}}
}
