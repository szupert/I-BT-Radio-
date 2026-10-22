#pragma once
#define I2S_DOUT 17
#define I2S_BCLK 22
#define I2S_LRC 27
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define BRIGHTNESS_PIN 21
#define DUAL_TFT_SCK 14
#define DUAL_TFT_MISO 12
#define DUAL_TFT_MOSI 13
#define DUAL_TOUCH_SCK 25
#define DUAL_TOUCH_MISO 39
#define DUAL_TOUCH_MOSI 32
#define DUAL_TOUCH_CS 33
#define DUAL_ENCODER_A 26 // V020: onboard DAC/audio unused; dedicated rotary CLK
#define DUAL_ENCODER_B 35
#define DUAL_ENCODER_SW 4
#define DUAL_ADC_PIN 34
#define SD_SPIPINS 18,19,23
#define DUAL_SD_HOST VSPI
#if FEATURE_SD
#define SDC_CS 5
#else
#define SDC_CS 255
#endif
// A single physical calibration; all four rotations derive from it.
// Guard each endpoint independently, so partial calibration overrides stay valid.
#ifndef DUAL_TOUCH_XMIN
#define DUAL_TOUCH_XMIN 250
#endif
#ifndef DUAL_TOUCH_XMAX
#define DUAL_TOUCH_XMAX 3850
#endif
#ifndef DUAL_TOUCH_YMIN
#define DUAL_TOUCH_YMIN 250
#endif
#ifndef DUAL_TOUCH_YMAX
#define DUAL_TOUCH_YMAX 3850
#endif

// SMALL has one large application slot. Firmware web OTA is intentionally disabled.
#define DUAL_ALLOW_WEB_FW_UPDATE 0
#ifndef DUAL_TOUCH_SWAP_XY
#define DUAL_TOUCH_SWAP_XY 0
#endif
#ifndef DUAL_TOUCH_INVERT_X
#define DUAL_TOUCH_INVERT_X 0
#endif
#ifndef DUAL_TOUCH_INVERT_Y
#define DUAL_TOUCH_INVERT_Y 0
#endif
