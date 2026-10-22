#pragma once
#include <atomic>
#include <stdint.h>
namespace bootprogress {
constexpr unsigned artwork(unsigned choice,unsigned skin){return choice==2?(skin==2?3:skin==3?4:skin==15?1:0):choice==1||choice==3||choice==4?choice:0;}
constexpr bool hold(unsigned choice,unsigned skin){return artwork(choice,skin)!=0||skin==15;}
enum class Stage:uint8_t {Display=0,Config,Hardware,Audio,Network,Library,Controls,Ready};
enum class Decision:uint8_t {Released,Wait,Complete,Setup};
struct State {std::atomic<bool> armed{false},setupDone{false};std::atomic<uint8_t> completed{0};std::atomic<unsigned> epoch{0};};
inline State& state(){static State s;return s;}
constexpr bool dedicated(unsigned vintage){return vintage==6;} // Add Orion only with its actual skin.
inline void begin(bool hold,bool runtime=false){auto& s=state();s.completed.store(runtime?4:0);s.setupDone.store(runtime);s.epoch.fetch_add(1);s.armed.store(hold,std::memory_order_release);}
inline void reached(Stage stage){auto& s=state();auto n=uint8_t(stage);auto old=s.completed.load();while(old<n&&!s.completed.compare_exchange_weak(old,n)){};}
inline void ready(){reached(Stage::Ready);state().setupDone.store(true,std::memory_order_release);}
constexpr Decision decide(bool armed,bool setupDone,bool connected,bool setup,bool offline){return !armed?Decision::Released:setup?Decision::Setup:setupDone&&(connected||offline)?Decision::Complete:Decision::Wait;}
inline Decision decision(bool connected,bool setup,bool offline){auto& s=state();return decide(s.armed.load(std::memory_order_acquire),s.setupDone.load(std::memory_order_acquire),connected,setup,offline);}
inline void release(){state().armed.store(false,std::memory_order_release);}
constexpr unsigned fill(unsigned completed,bool connected,bool offline){return completed>7?0:(!connected&&!offline&&completed>4?4:completed);}
constexpr unsigned key(int x,int y){return y>=174&&y<214?(x>=94&&x<138?0:x>=138&&x<182?1:x>=182&&x<226?2:3):3;}
}
