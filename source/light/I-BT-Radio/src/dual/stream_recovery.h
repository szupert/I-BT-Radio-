#pragma once
#include <stdint.h>
namespace dual {
// Time-based radio recovery. Explicit Stop or source changes cancel the intent.
class StreamRecovery {
public:
    enum class Reason : uint8_t { None, LowBuffer, BtNoProduction, ConnectionFailed, TransportClosed };
    uint16_t station=0;
    Reason lastReason=Reason::None;
    uint32_t alarmToken=0; // audio-owner identity; field order preserves the previous 40-byte size
    static constexpr uint8_t MaxAttempts=9;
    constexpr bool exhausted() const {return limited;}
    constexpr uint8_t retryCount() const {return attempts;}
    constexpr bool takeLimitEvent(){const bool value=limitEvent;limitEvent=false;return value;}
    constexpr void begin(uint16_t id){station=id;alarmToken=0;stalled=false;healthy=false;attempts=0;lastBuffer=0;progressSeen=false;progressHealthy=false;lastReason=Reason::None;limited=false;limitEvent=false;}
    constexpr void cancel(){begin(0);alarmToken=0;}
    constexpr uint32_t connectionDelay() const {
        return attempts==0?60000:attempts==1?120000:attempts==2?240000:300000;
    }
    // Called by the audio owner AFTER the blocking connection attempt completes.
    // Reuse the existing stall clock; no extra persistent RAM or another owner.
    constexpr void connectionResult(uint32_t now,bool accepted){
        stalledAt=now;stalled=!accepted;healthy=false;
        progressSeen=false;progressHealthy=false;lastBuffer=0;
        lastReason=accepted?Reason::None:Reason::ConnectionFailed;
    }

    constexpr bool poll(uint32_t now,bool web,bool connected,bool running,uint32_t buffered,
              bool btProduction=false,uint32_t produced=0,bool transportClosed=false){
        if(!web){cancel();return false;}
        if(!station || limited)return false;
        if(!connected){stalled=false;healthy=false;progressSeen=false;progressHealthy=false;return false;}
        if(lastReason==Reason::ConnectionFailed){
            if(!stalled){stalled=true;stalledAt=now;return false;}
            if(uint32_t(now-stalledAt)<connectionDelay())return false;
            stalled=false;return admitRetry();
        }
        // A confirmed closed socket cannot refill. Do not spend eight seconds
        // waiting on its undecodable tail; keep the retry cap and failed-connect backoff.
        if(transportClosed){
            stalled=false;healthy=false;lastReason=Reason::TransportClosed;
            return admitRetry();
        }
        // Only BT OUT has an existing PCM-production counter. A full input buffer
        // alone cannot prove that the decoder is still producing audible frames.
        if(btProduction&&running&&buffered>=4096){
            if(!progressSeen){
                // Snapshot the callback counter at stream start. During the 75%
                // prebuffer wait it is expected to stay unchanged, often at zero.
                lastProduced=produced;progressAt=now;progressSeen=true;
            }else if(produced!=lastProduced){
                lastProduced=produced;progressAt=now;
                if(!progressHealthy){progressHealthy=true;progressHealthyAt=now;}
                if(uint32_t(now-progressHealthyAt)>=30000)attempts=0;
            }else if(progressHealthy&&uint32_t(now-progressAt)>=uint32_t(12000+(attempts<3?attempts:3)*10000)){
                lastReason=Reason::BtNoProduction;
                progressAt=now;progressHealthy=false;
                stalled=false;healthy=false;
                return admitRetry();
            }
        }else{progressSeen=false;progressHealthy=false;}
        // A small but advancing buffer is not the observed stuck-tail failure.
        if(running&&buffered&&buffered!=lastBuffer)stalled=false;
        lastBuffer=buffered;
        if(running&&buffered>=4096){
            stalled=false;
            if(!healthy){healthy=true;healthyAt=now;}
            if(!btProduction && uint32_t(now-healthyAt)>=30000)attempts=0;
            return false;
        }
        healthy=false;
        if(!stalled){stalled=true;stalledAt=now;return false;}
        const uint32_t delay=attempts==0?8000:attempts==1?10000:attempts==2?20000:30000;
        if(uint32_t(now-stalledAt)<delay)return false;
        stalled=false;
        lastReason=Reason::LowBuffer;
        return admitRetry();
    }
private:
    constexpr bool admitRetry(){
        if(attempts>=MaxAttempts){limited=true;limitEvent=true;return false;}
        ++attempts;return true;
    }
    bool limited=false,limitEvent=false;
    uint8_t attempts=0;
    bool stalled=false,healthy=false,progressSeen=false,progressHealthy=false;
    uint32_t stalledAt=0,healthyAt=0,lastBuffer=0;
    uint32_t progressAt=0,progressHealthyAt=0,lastProduced=0;
};
}
