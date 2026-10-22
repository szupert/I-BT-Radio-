#pragma once
#include <stdint.h>
#include "ram_reserve.h"

namespace dual {
// A small, allocation-free admission controller for human input. It never
// queues another action: under pressure, repeated low-priority input is lost
// instead of delaying the radio or playing it back after the user has stopped.
class UiGovernor {
public:
    enum class Kind : uint8_t { Critical, Rotary, Touch, Other };

    void observe(uint32_t now, bool audioRunning, uint32_t audioBuffer,
                 uint32_t freeInternal, unsigned uiQueue, unsigned audioQueue,
                 uint32_t audioGaps, uint32_t uiDrops, uint32_t audioDrops) {
        if (!windowValid_ || uint32_t(now-windowAt_) >= 1000) {
            windowAt_=now; windowCount_=0; windowValid_=true;
        }
        const bool gap=gapValid_ && audioGaps!=lastGaps_;
        lastGaps_=audioGaps; gapValid_=true;
        const bool inputRejected=dropsValid_ && uiDrops!=lastUiDrops_;
        const bool rejected=inputRejected || (dropsValid_ && audioDrops!=lastAudioDrops_);
        lastUiDrops_=uiDrops;lastAudioDrops_=audioDrops;dropsValid_=true;
        if(uiQueue>=9 || inputRejected)markInputOverload(now);
        const uint8_t reason=(audioRunning&&audioBuffer<8192?1:0)|
            (audioRunning&&gap?2:0)|(freeInternal&&freeInternal<ibtram::reserve?4:0)|
            (uiQueue>=9?8:0)|(audioQueue>=4?16:0)|(rejected?32:0);
        if(reason){reasons_=reason;protect(now);}
    }

    bool admit(uint32_t now, Kind kind, bool audioRunning) {
        if (!windowValid_ || uint32_t(now-windowAt_) >= 1000) {
            windowAt_=now; windowCount_=0; windowValid_=true;
        }
        if (++windowCount_>24 && audioRunning){reasons_=64;protect(now);markInputOverload(now);}
        if (kind==Kind::Critical || !active(now)) return true;
        const uint32_t spacing=kind==Kind::Rotary?100:kind==Kind::Touch?240:180;
        const unsigned slot=kind==Kind::Rotary?0:kind==Kind::Touch?1:2;
        if (accepted_[slot] && uint32_t(now-lastAccepted_[slot])<spacing) {
            ++dropped_;
            markInputOverload(now);
            return false;
        }
        accepted_[slot]=true;lastAccepted_[slot]=now;
        return true;
    }

    bool active(uint32_t now) const { return int32_t(protectUntil_-now)>0; }
    bool inputOverloadActive(uint32_t now) const { return int32_t(inputOverloadUntil_-now)>0; }
    uint8_t reasons(uint32_t now) const {return active(now)?reasons_:0;}
    uint32_t holdMs(uint32_t now) const {return active(now)?protectUntil_-now:0;}
    uint32_t dropped() const { return dropped_; }

private:
    void protect(uint32_t now) { protectUntil_=now+1800; }
    void markInputOverload(uint32_t now) { inputOverloadUntil_=now+1800; }
    uint32_t protectUntil_=0,inputOverloadUntil_=0,windowAt_=0,windowCount_=0,lastGaps_=0;
    uint32_t lastUiDrops_=0,lastAudioDrops_=0;
    uint32_t lastAccepted_[3]={};
    uint32_t dropped_=0;
    uint8_t reasons_=0;
    bool accepted_[3]={},windowValid_=false,gapValid_=false,dropsValid_=false;
};
}
