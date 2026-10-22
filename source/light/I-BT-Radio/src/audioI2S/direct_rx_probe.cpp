#include "pcm_handoff_probe.h"
#include "../dual/memory_checkpoint.h"
#include "direct_rx_probe.h"
#include "supply_counters.h"
#include "dac_continuity.h"
#include "../dual/audio_path_probe.h"
#include "../dual/timing_bucket.h"
#include "../core/options.h"
#include <Arduino.h>
#include <NetworkClient.h>
#include <esp_heap_caps.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <atomic>
#include <new>
namespace directrx {
#if IBT_NO_PSRAM
// Single audio-owner writer, atomic snapshot readers. No PSRAM/heap store.
static std::atomic<uint32_t> refillCalls{0},refillRounds{0},refillBytes{0},refillMaxUs{0},refillMaxRounds{0};
static dacdiag::Counters lightDac;
static pcmaudit::Probe lightPcm;
static std::atomic<uint32_t> hwLastEof{0},hwEofChanges{0},hwSamples{0},hwNotStarted{0};
#endif
// Audio owner is the only writer/socket accessor. Diagnostics read snapshots only.
struct Store {
 SupplyCounters supply;
 dacdiag::Counters dac;
 timingdiag::Bucket poll,repeat,read;
 std::atomic<uint32_t> empty{0},repeatEmpty{0},bytes{0},zeroReads{0},errors{0};
 std::atomic<uint32_t> probes{0},queued{0},noTcp{0},eof{0},peekErrors{0};
 std::atomic<uint32_t> pollGapMax{0},readGapMax{0};
 std::atomic<uint32_t> consumedBytes{0},pcmFrames{0},bitrate{0},rate{0},channels{0},bits{0};
 uint32_t lastPoll=0,lastRead=0,lastPeek=0;
};
static std::atomic<Store*> published{nullptr};
static std::atomic<uint32_t> epoch{0};
static Store* ensureStore(){
 auto* s=published.load();
 if(!s){
  void* memory=heap_caps_malloc(sizeof(Store),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!memory)return nullptr;
  auto* candidate=new(memory)Store();Store* expected=nullptr;
  // First enable may come from diagnostics while the audio owner enables profiling.
  // Publish one fully constructed, permanent store; no reader can see a freed store.
  if(published.compare_exchange_strong(expected,candidate))s=candidate;
  else {candidate->~Store();heap_caps_free(candidate);s=expected;}
 }
 return s;
}
static Store* measurement(){
 if(!pathprobe::enabled.load())return nullptr;
 return ensureStore();
}
bool setSupplyMeasurement(bool on){
 auto* s=on?ensureStore():published.load();
 if(s)s->supply.set(on);
 return !on||s!=nullptr;
}
void lowInput(){auto* s=published.load(std::memory_order_relaxed);if(s)s->supply.lowInput();}
static void increment(std::atomic<uint32_t>& value,uint32_t n=1){value.store(value.load(std::memory_order_relaxed)+n,std::memory_order_relaxed);}
static void maximum(std::atomic<uint32_t>& value,uint32_t n){if(n>value.load(std::memory_order_relaxed))value.store(n,std::memory_order_relaxed);}
void reset(){
 epoch.store(epoch.load()+1);auto* s=published.load();if(!s)return;
 s->supply.resetOwner();s->dac.reset();
 // Reset fields individually; never reconstruct a Store being read by diagnostics.
 for(auto* t:{&s->poll,&s->repeat,&s->read}){t->calls.store(0);t->total.store(0);t->maximum.store(0);}
 for(auto* t:{&s->empty,&s->repeatEmpty,&s->bytes,&s->zeroReads,&s->errors,&s->probes,&s->queued,&s->noTcp,&s->eof,&s->peekErrors,&s->pollGapMax,&s->readGapMax,&s->consumedBytes,&s->pcmFrames,&s->bitrate,&s->rate,&s->channels,&s->bits})t->store(0);
 s->lastPoll=s->lastRead=s->lastPeek=0;
}
int available(NetworkClient* client,bool repeat){
 auto* s=measurement();const uint32_t start=s?uint32_t(micros()):0;
 lightmem::Phase memoryPhase("RX_AVAILABLE");
 const int result=client->available();
 if(!s)return result;
 (repeat?s->repeat:s->poll).add(uint32_t(micros())-start);
 if(result<=0)increment(repeat?s->repeatEmpty:s->empty);
 if(!repeat){const uint32_t now=millis();if(s->poll.calls.load()>1)maximum(s->pollGapMax,uint32_t(now-s->lastPoll));s->lastPoll=now;
  if(result==0 && uint32_t(now-s->lastPeek)>=250){
   s->lastPeek=now;increment(s->probes);
   const int saved=errno;errno=0;uint8_t byte=0;const int fd=client->fd();
   const int peek=fd>=0?recv(fd,&byte,1,MSG_PEEK|MSG_DONTWAIT):-1;
   const int error=fd>=0?errno:EBADF;errno=saved;
   if(peek>0)increment(s->queued);else if(peek==0)increment(s->eof);else if(error==EAGAIN||error==EWOULDBLOCK)increment(s->noTcp);else increment(s->peekErrors);
  }
 }
 return result;
}
int read(NetworkClient* client,uint8_t* dst,size_t count){
 auto* s=measurement();const uint32_t start=s?uint32_t(micros()):0;
 lightmem::Phase memoryPhase("RX_READ");
 const int result=client->read(dst,count);
 auto* counts=published.load(std::memory_order_relaxed);if(counts)counts->supply.readResult(result);
 if(!s)return result;
 s->read.add(uint32_t(micros())-start);
 if(result>0){const uint32_t now=millis();if(s->bytes.load())maximum(s->readGapMax,uint32_t(now-s->lastRead));s->lastRead=now;increment(s->bytes,uint32_t(result));}
 else if(result==0)increment(s->zeroReads);else increment(s->errors);
 return result;
}
void consumed(uint32_t bytes){auto* counts=published.load(std::memory_order_relaxed);if(counts)counts->supply.consumed(bytes);auto* s=measurement();if(s)increment(s->consumedBytes,bytes);}
void decoded(uint32_t frames,uint32_t bitrate,uint32_t rate,uint32_t channels,uint32_t bits){
 auto* counts=published.load(std::memory_order_relaxed);if(counts)counts->supply.decoded(frames);
 auto* s=measurement();if(!s)return;increment(s->pcmFrames,frames);s->bitrate.store(bitrate);s->rate.store(rate);s->channels.store(channels);s->bits.store(bits);
}
bool dacBegin(){
#if IBT_NO_PSRAM
 return true;
#endif
 auto* s=measurement();if(s)return true;
 auto* prior=published.load(std::memory_order_relaxed);if(prior)prior->dac.breakInterval();
 return false;
}
void dacBreak(){
#if IBT_NO_PSRAM
 lightDac.breakInterval();lightPcm.active=0;
#endif
 auto* s=published.load(std::memory_order_relaxed);if(s)s->dac.breakInterval();}
void dacWrite(uint32_t start,uint32_t end,size_t requested,size_t written,bool ok,uint32_t hz,uint32_t dmaFrames){
#if IBT_NO_PSRAM
 lightDac.record(start,end,requested,written,ok,hz,dmaFrames);
 return;
#endif
 auto* s=published.load(std::memory_order_relaxed);if(s)s->dac.record(start,end,requested,written,ok,hz,dmaFrames);
}
void pcmBegin(uint32_t hz,unsigned ch){
#if IBT_NO_PSRAM
 lightPcm.begin(hz,ch);
#endif
}
void pcmRaw(const int16_t*p,size_t n,unsigned ch){
#if IBT_NO_PSRAM
 lightPcm.raw(p,n,ch);
#endif
}
void pcmPacked(const uint8_t*p,size_t n){
#if IBT_NO_PSRAM
 lightPcm.packed(p,n);
#endif
}
void pcmAccepted(const uint8_t*p,size_t n){
#if IBT_NO_PSRAM
 lightPcm.accepted(p,n);
#endif
}
void dacHardwareSample(uint32_t addr,bool started){
#if IBT_NO_PSRAM
 hwSamples.fetch_add(1,std::memory_order_relaxed);if(!started)hwNotStarted.fetch_add(1,std::memory_order_relaxed);
 if(addr&&addr!=hwLastEof.load(std::memory_order_relaxed)){hwLastEof.store(addr,std::memory_order_relaxed);hwEofChanges.fetch_add(1,std::memory_order_relaxed);}
#endif
}
void refillSample(uint32_t rounds,uint32_t bytes,uint32_t elapsed){
#if IBT_NO_PSRAM
 refillCalls.fetch_add(1,std::memory_order_relaxed);refillRounds.fetch_add(rounds,std::memory_order_relaxed);refillBytes.fetch_add(bytes,std::memory_order_relaxed);maximum(refillMaxUs,elapsed);maximum(refillMaxRounds,rounds);
#endif
}
void dacDiagnostics(){
#if IBT_NO_PSRAM
 Serial.printf("[STREAM-REFILL] calls=%lu rounds=%lu bytes=%lu max_us=%lu max_rounds=%lu limits=4096B,16rounds,3000us scope=OWNER_WALL_NOT_CPU_OR_HARD_DEADLINE\n",(unsigned long)refillCalls.load(),(unsigned long)refillRounds.load(),(unsigned long)refillBytes.load(),(unsigned long)refillMaxUs.load(),(unsigned long)refillMaxRounds.load());
 const auto& d=lightDac;
 Serial.printf("[DAC-I2S] enabled=1 scope=LIGHT_LIFETIME calls=%lu accepted_bytes=%lu short_writes=%lu zero_writes=%lu errors=%lu call_max_us=%lu gap_max_us=%lu over_capacity_gaps=%lu rate=%lu method=DRIVER_ACCEPTED_NOT_DMA_IRQ\n",
 (unsigned long)d.calls.load(),(unsigned long)d.accepted.load(),(unsigned long)d.shortWrites.load(),(unsigned long)d.zeroWrites.load(),(unsigned long)d.errors.load(),(unsigned long)d.callMaxUs.load(),(unsigned long)d.gapMaxUs.load(),(unsigned long)d.overCapacityGaps.load(),(unsigned long)d.rate.load());
 Serial.printf("[DAC-DMA] samples=%lu eof_changes=%lu tx_not_started=%lu scope=OBSERVED_DESCRIPTOR_CHANGES_NOT_IRQ_COUNT snapshot=NONATOMIC\n",(unsigned long)hwSamples.load(),(unsigned long)hwEofChanges.load(),(unsigned long)hwNotStarted.load());
 const auto& p=lightPcm;
 Serial.printf("[ADPCM-PCM] active=%lu rate=%lu channels=%lu source_frames=%lu packed_frames=%lu accepted_bytes=%lu raw_hash=%08lx packed_hash=%08lx accepted_hash=%08lx raw_peak=%lu packed_peak=%lu hash_frames=4096 scope=FIRST_FRAMES_LIFETIME_UNTIL_NEXT_EXTRA_FILE snapshot=NONATOMIC_NOT_DMA_IRQ\n",
 (unsigned long)p.active.load(),(unsigned long)p.rate.load(),(unsigned long)p.channels.load(),(unsigned long)p.sourceFrames.load(),(unsigned long)p.packedFrames.load(),(unsigned long)p.acceptedBytes.load(),(unsigned long)p.rawHash.load(),(unsigned long)p.packedHash.load(),(unsigned long)p.acceptedHash.load(),(unsigned long)p.rawPeak.load(),(unsigned long)p.packedPeak.load());
#endif
}
void diagnostics(){
 dacDiagnostics();
 auto* s=published.load();SupplyCounters::Snapshot c{};c.resetStable=true;if(s)c=s->supply.snapshot();
 Serial.printf("[NET-SUPPLY] enabled=%u reset_stable=%u gate=%lu epoch=%lu received_bytes=%lu consumed_bytes=%lu decoded_frames=%lu low_input=%lu zero_reads=%lu errors=%lu method=OWNER_COUNTERS_NONATOMIC_NO_TIMING\n",unsigned(c.enabled),unsigned(c.resetStable),(unsigned long)c.gate,(unsigned long)c.epoch,(unsigned long)c.received,(unsigned long)c.consumed,(unsigned long)c.frames,(unsigned long)c.low,(unsigned long)c.zero,(unsigned long)c.errors);

 Serial.printf("[NET-DIRECT] ready=%u enabled=%u epoch=%lu store_bytes=%u method=OWNER_ONLY_ELAPSED_NOT_CPU\n",unsigned(s!=nullptr),unsigned(pathprobe::enabled.load()),(unsigned long)epoch.load(),unsigned(sizeof(Store)));if(!s)return;
 Serial.printf("[NET-DIRECT-AUDIO] consumed_input_bytes=%lu decoded_pcm_frames=%lu bitrate_bps=%lu source_hz=%lu channels=%lu bits=%lu method=DECODE_OWNER_SNAPSHOT\n",(unsigned long)s->consumedBytes.load(),(unsigned long)s->pcmFrames.load(),(unsigned long)s->bitrate.load(),(unsigned long)s->rate.load(),(unsigned long)s->channels.load(),(unsigned long)s->bits.load());
 const auto& d=s->dac;
 Serial.printf("[DAC-I2S] enabled=%u epoch=%lu calls=%lu accepted_bytes=%lu short_writes=%lu zero_writes=%lu errors=%lu call_max_us=%lu gap_max_us=%lu over_capacity_gaps=%lu rate=%lu method=DRIVER_ACCEPTED_INDIVIDUAL_COUNTERS_NOT_DMA_IRQ\n",
 unsigned(pathprobe::enabled.load()),(unsigned long)epoch.load(),(unsigned long)d.calls.load(),(unsigned long)d.accepted.load(),(unsigned long)d.shortWrites.load(),(unsigned long)d.zeroWrites.load(),(unsigned long)d.errors.load(),(unsigned long)d.callMaxUs.load(),(unsigned long)d.gapMaxUs.load(),(unsigned long)d.overCapacityGaps.load(),(unsigned long)d.rate.load());
 for(unsigned i=0;i<3;++i){const auto& t=i==0?s->poll:i==1?s->repeat:s->read;Serial.printf("[NET-DIRECT-TIME] name=%s calls=%lu wall_us=%lu max_wall_us=%lu\n",i==0?"poll":i==1?"repeat":"read",(unsigned long)t.calls.load(),(unsigned long)t.total.load(),(unsigned long)t.maximum.load());}
 Serial.printf("[NET-DIRECT-IO] empty=%lu repeat_empty=%lu payload_bytes=%lu zero_reads=%lu errors=%lu poll_gap_max_ms=%lu read_gap_max_ms=%lu\n",(unsigned long)s->empty.load(),(unsigned long)s->repeatEmpty.load(),(unsigned long)s->bytes.load(),(unsigned long)s->zeroReads.load(),(unsigned long)s->errors.load(),(unsigned long)s->pollGapMax.load(),(unsigned long)s->readGapMax.load());
 Serial.printf("[NET-DIRECT-TCP] probes=%lu queued=%lu empty=%lu eof=%lu errors=%lu method=PEEK_1BYTE_NONCONSUMING_PARTIAL_TLS_RECORD_POSSIBLE\n",(unsigned long)s->probes.load(),(unsigned long)s->queued.load(),(unsigned long)s->noTcp.load(),(unsigned long)s->eof.load(),(unsigned long)s->peekErrors.load());
}
}
