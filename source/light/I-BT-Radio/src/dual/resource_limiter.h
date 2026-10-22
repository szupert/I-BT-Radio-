#pragma once
#include "resource_policy.h"
// Background largest-block walks are paced on DAC and BT; admissions stay fresh.
#ifndef IBT_LIMITER_BT_SAMPLE_MS
#define IBT_LIMITER_BT_SAMPLE_MS 100
#endif
#if IBT_LIMITER_BT_SAMPLE_MS != 0 && IBT_LIMITER_BT_SAMPLE_MS != 100
#error IBT_LIMITER_BT_SAMPLE_MS must be 0 or 100
#endif
namespace ibtlimit {
void begin();
void publishAudio(bool active, uint32_t buffered, uint32_t now, uint32_t capacity);
void tick(bool btMode=false);
bool setCadence(unsigned ms);
void resetCadence();
void cadenceDiagnostics();
bool acquire(Job job, size_t cost);
bool acquireIdleArtworkPlan();
bool acquireAlbumPlan(bool* activePlan=nullptr);
bool acquireAlbumTlsPlan();
void release(Job job);
bool acquireHttp();
bool acquireControlHttp();
void releaseHttp(bool control=false);
bool acquireWs();
void releaseWs();
bool allowAllocation(size_t cost);
bool allowRedraw();
bool backgroundBusy(); // read-only cheap veto for lowest-priority optional work
bool allowArtworkStart();
bool allowArtworkProgress();
bool allowArtworkIdleProgress(); // Light preparation cancels if playback starts.
void telemetry();
}
