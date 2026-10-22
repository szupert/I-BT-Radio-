#pragma once
#include <atomic>
#include <stdint.h>
namespace netpoll {
constexpr bool valid(uint32_t ms){return ms==0||ms==50||ms==100;}
constexpr uint32_t period(uint32_t requested,bool eligible,uint32_t fill){
    return eligible&&fill>=65536&&valid(requested)?requested:0;
}
// Audio-owner-only timing. A changed setting, low buffer or bypass forces a poll.
struct Gate {
    uint32_t last=0,previous=0; bool armed=false;
    constexpr void reset(){last=0;previous=0;armed=false;}
    constexpr bool poll(uint32_t now,uint32_t ms){
        if(!ms){armed=false;previous=0;return true;}
        if(!armed||ms!=previous||uint32_t(now-last)>=ms){
            armed=true;previous=ms;last=now;return true;
        }
        return false;
    }
};
inline std::atomic<uint32_t>& requested(){static std::atomic<uint32_t> value{0};return value;}
inline std::atomic<uint32_t>& applied(){static std::atomic<uint32_t> value{0};return value;}
inline std::atomic<uint32_t>& polls(){static std::atomic<uint32_t> value{0};return value;}
inline std::atomic<uint32_t>& skips(){static std::atomic<uint32_t> value{0};return value;}
inline bool set(uint32_t ms){if(!valid(ms))return false;requested().store(ms,std::memory_order_relaxed);return true;}
inline void reset(){requested().store(0,std::memory_order_relaxed);}
}
