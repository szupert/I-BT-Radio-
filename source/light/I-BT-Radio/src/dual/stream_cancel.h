#pragma once
#include <atomic>
#include <cstdint>
namespace streamcancel {
// Accepted command sequence belongs to the audio owner. A producer may wake
// that owner before enqueue returns; acknowledging at dequeue avoids cancelling
// the new open itself or starting an old reconnect between cancel and enqueue.
class State {
 std::atomic<uint32_t> generation{0},accepted{0},opening{0};
 std::atomic<bool> active{false};
 static void advance(std::atomic<uint32_t>& value,uint32_t token){
  auto old=value.load();while(int32_t(token-old)>0&&!value.compare_exchange_weak(old,token)){}
 }
public:
 void cancel(uint32_t token){advance(generation,token);}
 void cancel(){cancel(generation.load()+1);}
 void accept(uint32_t token){advance(accepted,token);}
 void begin(){opening.store(accepted.load());active.store(true);}
 void end(){active.store(false);}
 bool cancelled()const{return active.load()&&int32_t(generation.load()-opening.load())>0;}
};
inline State& state(){static State v;return v;}
struct Scope {Scope(){state().begin();}~Scope(){state().end();}};
}
