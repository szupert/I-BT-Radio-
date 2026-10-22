#pragma once
#include <atomic>
#include <stdint.h>
#include <esp_timer.h>
#include "stage_cpu.h"
namespace pathprobe {
enum Stage { Poll, Read, Decode, Output, Worker, Count };
struct Counters { std::atomic<uint32_t> calls{0},us{0},maxUs{0},units{0},empty{0},errors{0}; };
extern std::atomic<bool> enabled;
extern Counters counters[Count];
extern std::atomic<uint32_t> lowInput,decodedFrames;
// Elapsed wall time includes preemption and blocking; it is never labelled CPU time.
struct Record {uint32_t elapsed,units,empty,errors;Stage stage;};
void begin();void service();void publish(const Record& record);
class Scope {
    bool on;uint32_t start;Record record{};unsigned previousStage;
public:
    explicit Scope(Stage s):on(enabled.load(std::memory_order_relaxed)),start(on?uint32_t(esp_timer_get_time()):0){record.stage=s;previousStage=stagecpu::enter(unsigned(s));}
    void units(int32_t n){if(on){if(n>0)record.units+=uint32_t(n);else if(n==0)++record.empty;else ++record.errors;}}
    ~Scope(){stagecpu::leave(previousStage);if(on){record.elapsed=uint32_t(esp_timer_get_time())-start;publish(record);}}
};
inline void low(){if(enabled.load(std::memory_order_relaxed))lowInput.fetch_add(1,std::memory_order_relaxed);}
inline void frames(unsigned n){if(enabled.load(std::memory_order_relaxed))decodedFrames.fetch_add(n,std::memory_order_relaxed);}
void diagnostics();
}
