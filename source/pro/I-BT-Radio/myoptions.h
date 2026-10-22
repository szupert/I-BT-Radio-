#pragma once
// PlatformIO supplies the board macro. Arduino IDE has no build_flags field,
// therefore the human/manual sketch defaults to the SMALL CYD target.
#if !defined(BOARD_CYD28) && !defined(BOARD_8048S070) && defined(ARDUINO)
#define BOARD_CYD28 1
#endif
#if defined(BOARD_CYD28) == defined(BOARD_8048S070)
#error Select exactly one board: BOARD_CYD28 or BOARD_8048S070
#endif
// V15 SMALL bare-board smoke profile: only the onboard touch is used as input.
// No encoder/buttons/IR, no SD-card dependency, and no external DAC/I2S pins are driven.
#if defined(BOARD_CYD28)
#ifndef FEATURE_BARE_TOUCH_WIFI
#define FEATURE_BARE_TOUCH_WIFI 1
#endif
#ifndef FEATURE_ENCODER
#define FEATURE_ENCODER 1 // V011 rotary enabled
#endif
#ifndef FEATURE_SD
#define FEATURE_SD 0
#endif
#ifndef FEATURE_AUDIO_OUTPUT
#define FEATURE_AUDIO_OUTPUT 1
#endif
#ifndef FEATURE_BT_A2DP_SMALL
#define FEATURE_BT_A2DP_SMALL 1 // V071: A2DP receiver to external I2S DAC
#endif
// V28: dedicated physical BT role button support.
// Pin is intentionally NOT guessed. Set to a confirmed free GPIO later.
// Wiring model when enabled: momentary button from GPIO to GND, internal pull-up.
#ifndef BT_ROLE_BUTTON_PIN
#define BT_ROLE_BUTTON_PIN 255
#endif
#ifndef BT_ROLE_BUTTON_ACTIVE_LOW
#define BT_ROLE_BUTTON_ACTIVE_LOW 1
#endif
#endif
#include "src/dual/features.h"
#if defined(BOARD_CYD28)
#include "src/boards/board_cyd28.h"
#else
#include "src/boards/board_8048s070.h"
#endif
// Keep yoRadio's player/network/SD core, replace only the display/controls frontend.
#define DSP_MODEL 0
#define USE_FBUFFER false
#define TS_MODEL 0
#define I2S_INTERNAL false
#define LED_BUILTIN 255
#define USE_BUILTIN_LED false
#define LED_BUILTIN_S3 255
#define VS1053_RST -1
#define HIDE_VU
#define HIDE_WEATHER
#define PLQ_SEND_DELAY 0
#define DSQ_SEND_DELAY 0
#define CORE_STACK_SIZE 8192 // V108: DspTask runs the full UI and WebUI; V107 overflowed at 4096 bytes
#define DSP_TASK_DELAY pdMS_TO_TICKS(5)
#define DSP_TASK_PRIORITY 1
#define WATCHDOG_TASK_CORE_ID 0
#define SD_AUTOPLAY false
#define LIGHT_SENSOR 255
#define USE_OTA false
#define DMA_BUFCOUNT 8 // V071: 16 KiB I2S DMA, ~93 ms at 44.1 kHz
#define DMA_BUFLEN 512
// Touch / encoder / ADC / MCP23017 are owned by the board HAL, not legacy controls.
#define ENC_BTNL 255
#define ENC_BTNR 255
#define ENC_BTNB 255
