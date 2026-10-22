#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ram_reserve.h"
#include "artwork_buffer_policy.h"
namespace ibtlimit {
enum class Job : uint8_t { None, ArtworkFetch, ArtworkDecode, TimeSync, Weather, Diagnostic };
struct Sample { uint32_t now, freeInternal, largestInternal, buffer; bool audioActive, audioFresh; };
// No allocation, queue, I/O or platform lock in the policy. Callers serialize it.
class Policy {
public:
    static constexpr uint32_t reserve = ibtram::reserve;
    static constexpr uint32_t httpCost = 8192;
    static constexpr uint32_t badInternal = reserve;
    // User R208: all general free-RAM gates use the same10000B floor.
    // Keep1.5s recovery, audio freshness, concurrency and charged allocation costs.
    static constexpr uint32_t recoveryInternal = reserve;
    static constexpr uint8_t maxHttp = 2, maxWs = 1;
    bool pressured = true;
    uint8_t http = 0, ws = 0, controlHttp = 0;
    Job heavy = Job::None;
    uint32_t denied = 0, admitted = 0, deniedHttp = 0;
    void observe(const Sample& s, bool largestFresh=true) {
        const bool bad = !s.audioFresh || s.freeInternal < badInternal || (largestFresh && s.largestInternal < reserve) || (s.audioActive && s.buffer < 2048);
        const bool recovered = s.audioFresh && s.freeInternal >= recoveryInternal && (!largestFresh || s.largestInternal >= reserve) && (!s.audioActive || s.buffer >= 4096);
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
    // Idle native artwork uses separate <=32KiB allocations (16KiB task,
    // Work/JSON, TLS buffers), not one64KiB allocation. Retain the full64KiB
    // aggregate charge, reserve,4KiB single-block margin and all concurrency.
    bool acquireIdleArtworkPlan(const Sample& s){
        observe(s);
        if(s.audioActive||!s.audioFresh||pressured||heavy!=Job::None||http||
           s.freeInternal<reserve+65536u||s.largestInternal<32768u+4096u){denied++;return false;}
        heavy=Job::ArtworkFetch;++admitted;return true;
    }
    // Public HTTP while active: <=1536B Work,6144B task,4096B scratch,
    //512B HTTP buffers and bounded TCP storage. Idle HTTPS retains40KiB charge.
    // Generic ArtworkFetch64KiB gate and all retained reserves stay unchanged.
    bool acquireAlbumPlan(const Sample& s,uint32_t capacity){
        observe(s);const uint32_t cost=s.audioActive?20480u:40960u;
        const uint32_t largest=s.audioActive?10240u:20480u;
        if(!artworkBufferReady(s.audioActive,s.audioFresh,s.buffer,capacity,true)||pressured||heavy!=Job::None||http||
           s.freeInternal<reserve+cost||s.largestInternal<largest){++denied;return false;}
        heavy=Job::ArtworkFetch;++admitted;return true;
    }
    bool acquireAlbumTlsPlan(const Sample& s,uint32_t capacity){
        observe(s);
        if(!artworkBufferReady(s.audioActive,s.audioFresh,s.buffer,capacity,true)||pressured||heavy!=Job::None||http||
           s.freeInternal<reserve+40960u||s.largestInternal<20480u){++denied;return false;}
        heavy=Job::ArtworkFetch;++admitted;return true;
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
constexpr uint32_t kSmallMinInternalHeap = ibtram::reserve;
constexpr uint32_t kSmallArtworkDecodeHeap = ibtram::reserve;
constexpr uint32_t kSmallArtworkReadFloor = 8u*1024u;
constexpr uint32_t kSmallArtworkMaxJpeg = 12u*1024u;
inline bool heapHealthy(uint32_t freeHeap,uint32_t largest){return freeHeap>=kSmallMinInternalHeap && largest>=ibtram::reserve;}
inline bool artworkMayRead(uint32_t buffered){return buffered>=kSmallArtworkReadFloor;}
}
