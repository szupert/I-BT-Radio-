#include "service_output.h"
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace btaudio {
bool producerReady(); // connected + configured: permits PCM before media START
enum class Mode : uint8_t { Off, Out, In };
void request(Mode mode); // audio owner only
void request(bool enabled); // compatibility: true = OUT, false = OFF
void flush(); // audio owner, invalidate queued old track
Mode mode();
bool wanted();
bool outputSelected();
bool inputSelected();
bool inputPlaying();
bool inputArtworkSafe(); // bounded RX headroom gate; no waiting
bool transmitting();
uint32_t producedFrames(); // existing BT OUT PCM counter, read-only recovery signal
bool connected(); // A2DP link is connected, including paused playback.
unsigned signalBars(); // coarse 0..4 Classic BT RSSI-delta indication; 0 when unavailable
bool idle();
inline bool webBlocked(){return wanted()||!idle();} // includes scan, pause and teardown
unsigned pcmTaskId();
bool setPcmStaging(bool enabled); // diagnostic lease only; OFF always allowed
bool setDiagnosticRate(unsigned hz); // RAM only, fully OFF guard
unsigned negotiatedOutputRate(); // zero until producer ready; actual SBC clock
unsigned outputRate(); // selected BT OUT SBC rate, saved in NVS
bool setOutputRate(unsigned hz); // only while the BT profile is fully off
void service(); // core 0: asynchronous, bounded profile lifecycle
void sample(int16_t left,int16_t right,uint32_t rate); // fallback for non-16-bit decoded output
void feedBlock(const int16_t* samples,uint32_t frames,unsigned channels,unsigned rate,unsigned volume,int balance); // V029 OUT path, raw 16-bit PCM
// A stopped audio owner reserves the native rate/generation, bypassing resampling.
struct ServiceLease {uint32_t generation=0,epoch=0,rate=0;bool valid=false;};
bool beginServicePcm(ServiceLease& lease);
serviceui::Transfer writeServicePcm(const int16_t* samples,uint16_t frames,const ServiceLease& lease);
size_t pcm(uint8_t* out,size_t capacity,uint32_t& rate); // core 1, IN -> DAC
const char* status();
struct Device { char name[64]; uint8_t address[6]; int8_t rssi; };
unsigned deviceCount();
bool device(unsigned index,Device& result);
bool targetDevice(Device& result); // pending/selected OUT peer, bounded RAM-only lookup
bool connectedDevice(Device& result); // bounded RAM lookup, no scan or NVS read
bool selectDevice(unsigned index);
bool scanning();
bool linkFailed();
bool connecting(); // selected OUT peer, waiting for its audio stream
bool takeAutoDacFallback(); // audio owner consumes a failed/bounded OUT attempt
void loadSavedDevice(); // once during normal startup, before the BT menu is drawn
bool hasSavedDevice();
bool reconnectSaved(); // one user-requested attempt; never an endless background retry
unsigned savedDeviceCount();
bool savedDevice(unsigned index,Device& result);
bool connectSavedDevice(unsigned index); // select a previously connected OUT peer
bool restartScan(); // only after an empty, completed OUT scan
bool control(unsigned key); // 0 play, 1 pause, 2 previous, 3 next
const char* controlStatus();
void diagnostics();
}
