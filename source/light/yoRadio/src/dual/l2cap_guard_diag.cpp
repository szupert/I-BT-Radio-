#include <Arduino.h>
#include <stdint.h>

// The SDK keeps its recursion guard and return path. This bounded counter
// replaces only its repetitive UART error; all other SDK errors remain visible.
static portMUX_TYPE guardMux=portMUX_INITIALIZER_UNLOCKED;
static uint32_t guardCallbacks=0;
extern "C" void ibt_l2cap_guard_note(void){
    portENTER_CRITICAL(&guardMux);
    ++guardCallbacks; // unsigned wrap is deliberate; diagnostic deltas use u32
    portEXIT_CRITICAL(&guardMux);
}
extern "C" uint32_t ibt_l2cap_guard_count(void){
    portENTER_CRITICAL(&guardMux);
    const uint32_t value=guardCallbacks;
    portEXIT_CRITICAL(&guardMux);
    return value;
}
