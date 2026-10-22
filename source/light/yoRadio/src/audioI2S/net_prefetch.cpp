#include "direct_rx_probe.h"
#include "net_prefetch.h"
#include <Arduino.h>
#include <NetworkClient.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <lwip/sockets.h>
#include "net_socket_probe.h"

// V142 one-variable control: keep HTTPS socket reads on the audio owner.
// Set to 0 only for the preserved V141 host regression/control comparison.
#ifndef IBT_NET_TLS_AUDIO_OWNER_CONTROL
#define IBT_NET_TLS_AUDIO_OWNER_CONTROL 1
#endif
#if IBT_NET_TLS_AUDIO_OWNER_CONTROL != 0 && IBT_NET_TLS_AUDIO_OWNER_CONTROL != 1
#error IBT_NET_TLS_AUDIO_OWNER_CONTROL_must_be_0_or_1
#endif

namespace netprefetch {
static constexpr uint32_t kCapacity=65536;
static uint8_t* ring=nullptr;
static NetworkClient* socket=nullptr; // accessed only while busy is held
static TaskHandle_t task=nullptr;
static std::atomic<TaskHandle_t> tlsTask{nullptr};
static std::atomic<bool> enabled{false},owned{false},busy{false},mainPump{false};
static std::atomic<uint32_t> produced{0},consumed{0};
static std::atomic<uint32_t> bytesFromSocket{0},bytesToAudio{0},emptyReads{0},readErrors{0},peakUsed{0};
static std::atomic<const char*> route{"UNSEEN"},scheme{"UNKNOWN"};
static std::atomic<uint32_t> attempts{0},starts{0},internalAtBegin{0},largestAtBegin{0};
static std::atomic<uint32_t> tlsCalls{0},tlsEmpty{0},tlsWall{0},tlsMax{0};
static std::atomic<int> tlsCore{-1};
// Snapshot only: diagnostics must never touch the socket from another task.
static std::atomic<uint32_t> ioVisits{0},noDataPolls{0},zeroSocketReads{0};
static std::atomic<uint32_t> lastVisitMs{0},lastProgressMs{0},callStartMs{0};
static std::atomic<int> ioStage{0},lastSocketAvailable{0},lastSocketRead{0},lastFd{-1};
static std::atomic<uint32_t> lastPeekMs{0},peekCalls{0},peekEof{0},peekErrors{0};
static std::atomic<int> peekResult{-2},peekErrno{0};
static std::atomic<ibtnet::PeekState> peekState{ibtnet::PeekState::Unseen};
void noteBypass(const char* why,bool ssl){
  scheme.store(ssl?"HTTPS":"HTTP");route.store(why?why:"BYPASS");
}
static bool lockIo(){bool expected=false;return busy.compare_exchange_strong(expected,true);}
static void unlockIo(){busy.store(false);}
// One available/read pair per visit; never loop waiting for TLS/network data.
// The socket is exclusively owned until end() successfully acquires this lock.
static void pump(bool fromMain){
  if(!enabled.load()||mainPump.load()!=fromMain||!lockIo())return;
  if(!enabled.load()||mainPump.load()!=fromMain){unlockIo();return;}
  const uint32_t w=produced.load(),r=consumed.load();
  const uint32_t used=w-r;
  if(!socket||used>=kCapacity){unlockIo();return;}
  ioVisits.fetch_add(1);lastVisitMs.store(millis());
  const uint32_t start=fromMain?micros():0;
  if(fromMain){tlsTask.store(xTaskGetCurrentTaskHandle());tlsCore.store(xPortGetCoreID());tlsCalls.fetch_add(1);}
  callStartMs.store(millis());ioStage.store(1); // available() entered
  const int availableNow=socket->available();
  lastSocketAvailable.store(availableNow);lastFd.store(socket->fd());ioStage.store(0);
  if(availableNow>0){
    const uint32_t offset=w&(kCapacity-1);
    const size_t count=std::min<size_t>({size_t(availableNow),size_t(kCapacity-used),size_t(kCapacity-offset),size_t(fromMain?8192:2048)});
    callStartMs.store(millis());ioStage.store(2); // read() entered
    const int received=socket->read(ring+offset,count);
    lastSocketRead.store(received);ioStage.store(0);
    if(received>0&&size_t(received)<=count){
      produced.store(w+uint32_t(received));bytesFromSocket.fetch_add(received);
      lastProgressMs.store(millis());
      const uint32_t fill=used+uint32_t(received);
      if(fill>peakUsed.load())peakUsed.store(fill);
    }else if(received<0||size_t(received)>count)readErrors.fetch_add(1);
    else zeroSocketReads.fetch_add(1);
  }else {
    noDataPolls.fetch_add(1);if(fromMain)tlsEmpty.fetch_add(1);
    const uint32_t now=millis();
    if(ibtnet::shouldPeek(fromMain,availableNow,now,lastPeekMs.load())){
      // busy is still held: this is the sole socket owner. Never consume bytes,
      // wait for data, stop the client, or initiate recovery from this worker.
      lastPeekMs.store(now);peekCalls.fetch_add(1);
      const int savedErrno=errno;
      errno=0;uint8_t byte=0;
      callStartMs.store(now);ioStage.store(3);
      const int fd=lastFd.load();
      const int result=fd>=0?recv(fd,&byte,1,MSG_PEEK|MSG_DONTWAIT):-1;
      const int error=fd>=0?errno:EBADF;
      ioStage.store(0);errno=savedErrno;
      const auto state=ibtnet::classifyPeek(result,error);
      peekResult.store(result);peekErrno.store(result<0?error:0);peekState.store(state);
      if(state==ibtnet::PeekState::Eof)peekEof.fetch_add(1);
      if(state==ibtnet::PeekState::Error)peekErrors.fetch_add(1);
    }
  }
  if(fromMain){const uint32_t elapsed=micros()-start;tlsWall.fetch_add(elapsed);if(elapsed>tlsMax.load())tlsMax.store(elapsed);}
  unlockIo();
}
static void worker(void*){
  for(;;){pump(false);vTaskDelay(enabled.load()&&!mainPump.load()?1:2);}
}
void service(){if(xPortGetCoreID()==0)pump(true);}
bool begin(NetworkClient* source,bool tlsOnMain){
  attempts.fetch_add(1);scheme.store(tlsOnMain?"HTTPS":"HTTP");
  const uint32_t freeInternal=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  const uint32_t largestInternal=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  internalAtBegin.store(freeInternal);largestAtBegin.store(largestInternal);
  if(!source){route.store("NO_SOURCE");return false;}
  if(enabled.load()){route.store("ALREADY_ACTIVE");return false;}
  if(!end()){route.store("STOP_TIMEOUT");return false;}
#if IBT_NET_TLS_AUDIO_OWNER_CONTROL
  if(tlsOnMain){
    // Only successful end() releases ownership. A timeout must never bypass it.
    // Returning false now keeps the existing Audio.cpp direct TLS path active.
    route.store("HTTPS_AUDIO_OWNER");
    Serial.printf("[NET-POLICY] tls_owner=audio core=%d split=0\n",xPortGetCoreID());
    return false;
  }
#endif
  if(!ring)ring=static_cast<uint8_t*>(heap_caps_malloc(kCapacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!ring){route.store("NO_PSRAM");return false;}
  // HTTPS uses the existing core0 loop stack; no additional task or internal stack.
  if(!tlsOnMain&&!task){
    if(freeInternal<30720||largestInternal<10240){route.store("LOW_INTERNAL_HEAP");return false;}
    if(xTaskCreatePinnedToCore(worker,"ibt_net_rx",4096,nullptr,2,&task,0)!=pdPASS){route.store("TASK_ALLOC");return false;}
  }
  // Both producers refuse work while disabled, but still lock for publication.
  if(!lockIo()){route.store("START_BUSY");return false;}
  socket=source;produced.store(0);consumed.store(0);mainPump.store(tlsOnMain);
  bytesFromSocket.store(0);bytesToAudio.store(0);emptyReads.store(0);readErrors.store(0);peakUsed.store(0);
  ioVisits.store(0);noDataPolls.store(0);zeroSocketReads.store(0);
  lastVisitMs.store(millis());lastProgressMs.store(millis());callStartMs.store(millis());
  ioStage.store(0);lastSocketAvailable.store(0);lastSocketRead.store(0);lastFd.store(-1);
  lastPeekMs.store(millis());peekCalls.store(0);peekEof.store(0);peekErrors.store(0);
  peekResult.store(-2);peekErrno.store(0);peekState.store(ibtnet::PeekState::Unseen);
  tlsCalls.store(0);tlsEmpty.store(0);tlsWall.store(0);tlsMax.store(0);tlsTask.store(nullptr);tlsCore.store(-1);
  owned.store(true);enabled.store(true);starts.fetch_add(1);
  route.store(tlsOnMain?"HTTPS_CORE0":"ACTIVE");unlockIo();
  return true;
}
bool end(uint32_t timeoutMs){
  enabled.store(false);
  const uint32_t start=millis();
  while(!lockIo()){
    if(uint32_t(millis()-start)>=timeoutMs){route.store("STOP_TIMEOUT");return false;}
    vTaskDelay(1);
  }
  socket=nullptr;owned.store(false);unlockIo();return true;
}
// Keep ownership on timeout: never let the consumer touch a still-busy socket.
bool active(){return owned.load();}
size_t available(){
  if(!active())return 0;
  const uint32_t r=consumed.load(),w=produced.load();
  return std::min<uint32_t>(w-r,kCapacity); // diagnostic snapshots may span consumer progress
}
int read(uint8_t* dst,size_t bytes){
  if(!dst||!bytes||!active())return 0;
  const uint32_t r=consumed.load(),w=produced.load();
  const size_t count=std::min<size_t>(bytes,w-r);
  if(!count){emptyReads.fetch_add(1);return 0;}
  const size_t offset=r&(kCapacity-1),first=std::min<size_t>(count,kCapacity-offset);
  memcpy(dst,ring+offset,first);if(first<count)memcpy(dst+first,ring,count-first);
  consumed.store(r+count);bytesToAudio.fetch_add(count);return int(count);
}
int read(){uint8_t value=0;return read(&value,1)==1?value:-1;}
size_t readBytes(char* dst,size_t bytes){return size_t(read(reinterpret_cast<uint8_t*>(dst),bytes));}
void diagnostics(){directrx::diagnostics();
  const uint32_t now=millis();
  Serial.printf("[NET-RX-PEEK] state=%s result=%d errno=%d calls=%lu eof=%lu errors=%lu age_ms=%lu\n",
    ibtnet::peekName(peekState.load()),peekResult.load(),peekErrno.load(),
    (unsigned long)peekCalls.load(),(unsigned long)peekEof.load(),(unsigned long)peekErrors.load(),
    (unsigned long)uint32_t(now-lastPeekMs.load()));
  const int stage=ioStage.load();
  Serial.printf("[NET-RX-IO] enabled=%u owned=%u busy=%u stage=%d call_ms=%lu visit_age_ms=%lu progress_age_ms=%lu visits=%lu no_data_polls=%lu zero_reads=%lu last_avail=%d last_read=%d fd=%d free_internal=%u largest_internal=%u\n",
    unsigned(enabled.load()),unsigned(owned.load()),unsigned(busy.load()),stage,
    (unsigned long)(stage?uint32_t(now-callStartMs.load()):0),
    (unsigned long)uint32_t(now-lastVisitMs.load()),(unsigned long)uint32_t(now-lastProgressMs.load()),
    (unsigned long)ioVisits.load(),(unsigned long)noDataPolls.load(),(unsigned long)zeroSocketReads.load(),
    lastSocketAvailable.load(),lastSocketRead.load(),lastFd.load(),
    unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
    unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
  Serial.printf("[NET-RX] active=%u task=%u core=0 fill=%u peak=%u rx_bytes=%lu delivered_bytes=%lu empty_reads=%lu read_errors=%lu stack_min=%u route=%s scheme=%s attempts=%lu starts=%lu begin_free=%lu begin_largest=%lu\n",
    active()?1:0,task?1:0,unsigned(available()),unsigned(peakUsed.load()),
    (unsigned long)bytesFromSocket.load(),(unsigned long)bytesToAudio.load(),(unsigned long)emptyReads.load(),(unsigned long)readErrors.load(),
    task?unsigned(uxTaskGetStackHighWaterMark(task)):0,route.load(),scheme.load(),(unsigned long)attempts.load(),(unsigned long)starts.load(),(unsigned long)internalAtBegin.load(),(unsigned long)largestAtBegin.load());
  const auto t=tlsTask.load();
  Serial.printf("[NET-TLS] main=%u core=%d calls=%lu empty=%lu wall_us=%lu max_wall_us=%lu loop_stack_min=%u\n",
    unsigned(mainPump.load()&&active()),tlsCore.load(),(unsigned long)tlsCalls.load(),(unsigned long)tlsEmpty.load(),(unsigned long)tlsWall.load(),(unsigned long)tlsMax.load(),t?unsigned(uxTaskGetStackHighWaterMark(t)):0);
}
}
