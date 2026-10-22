#pragma once
#include <stdint.h>
namespace connectivity {
enum class Mode:uint8_t {Online=0,Sd=1,BtInput=2};
constexpr bool valid(unsigned n){return n<=2;}
constexpr bool offline(unsigned n){return n==1||n==2;}
// Offline start belongs exclusively to the UI transition after the audio task exists.
constexpr bool bootAutoplay(unsigned mode,bool clockHome,bool smartstart){return !clockHome&&mode==0&&smartstart;}
constexpr bool apply(bool offlineReady,bool audioTaskReady,bool initializing){return offlineReady&&audioTaskReady&&!initializing;}
constexpr bool shouldAssociate(bool requestedOffline,bool attemptOwned){return !requestedOffline&&!attemptOwned;}
}
