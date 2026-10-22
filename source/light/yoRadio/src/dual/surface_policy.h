#pragma once
#include <stdint.h>
#include <atomic>
namespace surfacepolicy {
enum class Surface:uint8_t {Player,Clock,Weather,BtInput,Service};
enum class Route:uint8_t {WifiDac,SdDac,SdBt,BtInDac,InternalDac};
constexpr bool allows(Surface s,Route r,bool alarm=false){
 if(s==Surface::Clock)return alarm&&(r==Route::WifiDac||r==Route::SdDac||r==Route::InternalDac);
 if(s==Surface::Service)return true; // Existing WEB->BT and resource guards remain separate.
 if(s==Surface::BtInput)return r==Route::BtInDac;
 if(s==Surface::Weather)return r==Route::WifiDac||r==Route::SdDac||r==Route::BtInDac;
 return r!=Route::InternalDac;
}
constexpr bool ble(Surface s){return s==Surface::Clock||s==Surface::Weather;}
constexpr bool artwork(Surface s){return s==Surface::Player||s==Surface::BtInput||s==Surface::Service;}
// Explicit user request: existing transport buttons also control BT-IN in
// every permitted skin. Clock still has no BT audio or remote transport.
constexpr bool remoteTransport(Surface s){return allows(s,Route::BtInDac);}
constexpr bool validSkin(unsigned n){return (n>=1&&n<=8)||n==11||n==12||n==13||n==14||n==15;}
constexpr Surface forSkin(unsigned n){return n==5?Surface::Service:n==12?Surface::Weather:n==13?Surface::Clock:n==14?Surface::BtInput:Surface::Player;}
inline std::atomic<uint16_t>& state(){static std::atomic<uint16_t> v{2};return v;}
inline void publish(unsigned skin){state().store(uint16_t((unsigned(forSkin(skin))<<8)|skin),std::memory_order_release);}
inline unsigned skin(){return state().load(std::memory_order_acquire)&255;}
inline Surface current(){return Surface(state().load(std::memory_order_acquire)>>8);}
inline const char* name(Surface s){return s==Surface::Clock?"CLOCK":s==Surface::Weather?"WEATHER":s==Surface::BtInput?"BT_IN":s==Surface::Service?"SERVICE":"PLAYER";}
}
