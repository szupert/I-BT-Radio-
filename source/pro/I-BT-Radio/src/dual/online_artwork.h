#pragma once
#include <stddef.h>
#include <stdint.h>
namespace onlineart {
struct CacheStatus { size_t bytes; uint8_t failures; bool pending; };
// Decode/cache boundary. Network work must happen outside uiTick().
// Generation changes invalidate stale downloads. Fetch policy provides bounded
// retry/backoff so a missing logo can never create a request storm.
void begin();
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
