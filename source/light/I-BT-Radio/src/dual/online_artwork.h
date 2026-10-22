#pragma once
#include <stddef.h>
#include <stdint.h>
namespace onlineart {
struct CacheStatus { size_t bytes; uint8_t failures; bool pending; };
// Decode/cache boundary. Network work must happen outside uiTick().
// Generation changes invalidate stale downloads. Fetch policy provides bounded
// retry/backoff so a missing logo can never create a request storm.
void begin();
#if IBT_NO_PSRAM && FEATURE_ARTWORK
void serviceReader(); // UI/Core0: release inactive retained file, no painting/allocation.
const char* albumStatus(); // Native Light lookup; DEFERRED_* preserves the original safety limits.
void listeningTick(uint32_t generation,uint32_t elapsed,bool playing);
struct UploadStatus {uint64_t station;uint32_t generation,hash;bool enabled,qualified,busy;};
UploadStatus uploadStatus();
bool uploadBegin(uint32_t generation,uint64_t station);
bool uploadWrite(size_t index,const uint8_t* data,size_t length);
bool uploadFinish(uint32_t& hash);
void uploadAbort();
#endif
bool copyCached(uint8_t *&bytes,size_t &length); // UI/Core0 only; owned copy in PSRAM
void setEnabled(bool enabled,uint32_t generation);
void reset(uint32_t generation);
bool mayRequest(uint32_t generation,uint32_t nowMs);
void requestStarted(uint32_t generation,uint32_t nowMs);
void requestFailed(uint32_t generation,uint32_t nowMs);
void requestSucceeded(uint32_t generation,uint32_t nowMs);
uint32_t retryAfterMs(uint32_t generation,uint32_t nowMs);
void smoke(uint32_t generation);
void requestTrack(const char *title,uint32_t generation);
void requestUrl(const char *url,uint32_t generation);
bool submitJpegCopy(const uint8_t *bytes,size_t length,uint32_t generation);
void redraw(uint32_t generation);
void uiTick(uint32_t generation,bool visible);
size_t maxJpegBytes();
CacheStatus cacheStatus(uint32_t generation);
}
