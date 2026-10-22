#pragma once
#include <atomic>
#include <initializer_list>
#include <stdint.h>
namespace directrx {
// One audio-owner writer, read-only diagnostic snapshots. No allocation, clock,
// socket access or per-event logging. Independent from the expensive profiler.
class SupplyCounters {
 std::atomic<bool> enabled_{false};
 std::atomic<uint32_t> gate_{0},epoch_{0};
 std::atomic<uint32_t> received_{0},consumed_{0},frames_{0},low_{0},zero_{0},errors_{0};
 static void add(std::atomic<uint32_t>& v,uint32_t n){
  v.store(v.load(std::memory_order_relaxed)+n,std::memory_order_relaxed);
 }
public:
 struct Snapshot {bool enabled,resetStable;uint32_t gate,epoch,received,consumed,frames,low,zero,errors;};
 // Session controller alone writes enabled/gate; audio owner alone resets/counts.
 // Load/store only: no atomic RMW instruction against external PSRAM.
 void set(bool on){if(enabled_.load()!=on){enabled_.store(on);gate_.store(gate_.load()+1);}}
 void resetOwner(){
  epoch_.store(epoch_.load()+1); // Odd while source counters are being reset.
  for(auto* p:{&received_,&consumed_,&frames_,&low_,&zero_,&errors_})p->store(0,std::memory_order_relaxed);
  epoch_.store(epoch_.load()+1);
 }
 void readResult(int n){if(!enabled_.load(std::memory_order_relaxed))return;
  if(n>0)add(received_,uint32_t(n));else if(n==0)add(zero_,1);else add(errors_,1);
 }
 void consumed(uint32_t n){if(enabled_.load(std::memory_order_relaxed))add(consumed_,n);}
 void decoded(uint32_t n){if(enabled_.load(std::memory_order_relaxed))add(frames_,n);}
 void lowInput(){if(enabled_.load(std::memory_order_relaxed))add(low_,1);}
 Snapshot snapshot()const{
  const auto g=gate_.load(),e=epoch_.load();
  Snapshot s{enabled_.load(),false,g,e,received_.load(),consumed_.load(),frames_.load(),low_.load(),zero_.load(),errors_.load()};
  s.resetStable=!(e&1u)&&e==epoch_.load()&&g==gate_.load();
  return s; // Counter fields are separate instants, not an atomic conservation proof.
 }
};
}
