#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ram_reserve.h"
namespace ibtlimit {
enum class Job : uint8_t { None, ArtworkFetch, ArtworkDecode, TimeSync, Weather, Diagnostic };
struct Sample { uint32_t now, freeInternal, largestInternal, buffer; bool audioActive, audioFresh; };
// No allocation, queue, I/O or platform lock in the policy. Callers serialize it.
class Policy {
public:
    static constexpr uint32_t reserve = ibtram::reserve;
    static constexpr uint32_t httpCost = 8192;
    static constexpr uint32_t badInternal = 30000;
    // Enough for the largest 8 KiB background permit while preserving reserve.
    // V037 physical audio baseline is 33-35 KiB; 36 KiB never recovered there.
    static constexpr uint32_t recoveryInternal = 32768;
    static constexpr uint8_t maxHttp = 2, maxWs = 1;
    bool pressured = true;
    uint8_t http = 0, ws = 0, controlHttp = 0;
    Job heavy = Job::None;
    uint32_t denied = 0, admitted = 0, deniedHttp = 0;
    void observe(const Sample& s, bool largestFresh=true) {
        const bool bad = !s.audioFresh || s.freeInternal < badInternal || (largestFresh && s.largestInternal < 8192) || (s.audioActive && s.buffer < 2048);
        const bool recovered = s.audioFresh && s.freeInternal >= recoveryInternal && (!largestFresh || s.largestInternal >= 12288) && (!s.audioActive || s.buffer >= 4096);
        if (bad) { pressured = true; recovering = false; }
        else if (pressured) {
            if (!recovered) recovering = false;
            // Cheap bad/intermediate free/audio values still cancel recovery.
            // Only a fresh largest-block measurement can start or finish it.
            else if (largestFresh) {
                if (!recovering) { recovering = true; recoveryAt = s.now; }
                else if (uint32_t(s.now - recoveryAt) >= 1500) { pressured = false; recovering = false; }
            }
        }
    }
    bool budget(const Sample& s, size_t cost) const {
        return cost <= UINT32_MAX - reserve && s.freeInternal >= reserve + cost && cost <= UINT32_MAX - 4096 && s.largestInternal >= cost + 4096;
    }
    bool acquire(Job j, const Sample& s, size_t cost) {
        observe(s);
        if ((j==Job::ArtworkFetch || j==Job::ArtworkDecode) && s.audioActive && s.buffer<65536) { denied++; return false; }
        if (j == Job::None || pressured || heavy != Job::None || http || !budget(s,cost)) { denied++; return false; }
        heavy = j; admitted++; return true;
    }
    void release(Job j) { if (heavy == j) heavy = Job::None; }
    bool acquireHttp(const Sample& s) {
        observe(s);
        if (pressured || http >= maxHttp || heavy != Job::None || !budget(s,httpCost*(http+1u))) { denied++; deniedHttp++; return false; }
        http++; admitted++; return true;
    }
    // Only read-only station-editor bootstrap and bounded save controls use it.
    // It overrides audio-pressure admission, never RAM/largest/cost/concurrency.
    bool acquireControlHttp(const Sample& s) {
        observe(s);
        if(controlHttp || http>=maxHttp || heavy!=Job::None || !budget(s,httpCost*(http+1u))){denied++;deniedHttp++;return false;}
        ++http;++controlHttp;++admitted;return true;
    }
    void releaseHttp(bool control=false) { if (http) http--;if(control&&controlHttp)controlHttp--; }
    bool acquireWs(const Sample& s) {
        observe(s);
        if (pressured || ws >= maxWs || !budget(s,2048)) { denied++; return false; }
        ws++; admitted++; return true;
    }
    void releaseWs() { if (ws) ws--; }
private:
    bool recovering = false;
    uint32_t recoveryAt = 0;
};
}
// Retain the existing V035 policy API for source compatibility.
namespace dual {
constexpr uint32_t kSmallMinInternalHeap = 48u*1024u;
constexpr uint32_t kSmallArtworkDecodeHeap = 48u*1024u;
constexpr uint32_t kSmallArtworkReadFloor = 8u*1024u;
constexpr uint32_t kSmallArtworkMaxJpeg = 12u*1024u;
inline bool heapHealthy(uint32_t freeHeap,uint32_t largest){return freeHeap>=kSmallMinInternalHeap && largest>=16u*1024u;}
inline bool artworkMayRead(uint32_t buffered){return buffered>=kSmallArtworkReadFloor;}
}
