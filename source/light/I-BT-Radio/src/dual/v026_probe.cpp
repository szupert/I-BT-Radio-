#include "v026_probe.h"
#include "r10_diag.h"
#include "../core/options.h"
namespace v026 {void early(){Serial.printf("[LITE] psram_configured=%u native_memtest=%u bt=CLASSIC firmware=" IBT_FW_VERSION "\n",unsigned(!IBT_NO_PSRAM),unsigned(!IBT_NO_PSRAM));}void beginBt(){}void tick(){r10diag::serialPoll();r10diag::tick();}size_t held(){return 0;}bool memoryOK(){return false;}bool btOK(){return false;}const char* btName(){return "Classic BT audio";}}
