#include "sbc_probe.h"
#include <Arduino.h>
namespace sbcdiag {void start(){Serial.println("[SBC-PROBE] NOT_AVAILABLE_LITE");}bool active(){return false;}void cancel(){}bool startPhase(unsigned){return false;}}
