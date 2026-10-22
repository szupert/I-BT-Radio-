#pragma once
#include <stdint.h>
namespace stationsave {
enum Phase : uint8_t { Idle, WaitingStop, Ready, Uploading, Saved, Finishing, Complete, Failed };
struct Status {uint32_t job=0;Phase phase=Idle;bool installed=false,resume=false;uint16_t station=0;};
struct Resume {int mode=0;uint16_t track=0;bool play=false;};
bool begin(uint32_t job);
Status status();
bool holdsAudio();
bool acceptsUpload(uint32_t job);
bool uploadStarted(uint32_t job);
void uploadAborted(uint32_t job);
void imported(uint32_t job,bool good);
bool finish(uint32_t job,bool cancel=false);
void poll(); // network service owner; never called by an AsyncTCP callback
void ownerCapture(uint32_t job,int mode,uint16_t track,bool playing,const char* url);
void ownerStopped(uint32_t job);
bool ownerMayFinishStop();
void ownerReady();
bool ownerTakeFinish(Resume& plan);
void ownerReleased(bool resumeRequested);
void manualStop();
const char* phaseName(Phase phase);
}
