#pragma once
#define DUAL_VERSION "I-BT-Radio-V029"
#ifndef FEATURE_SD
#define FEATURE_SD 1
#endif
#ifndef FEATURE_ARTWORK
#define FEATURE_ARTWORK 1
#endif
#ifndef FEATURE_BOOT_ANIMATION
#define FEATURE_BOOT_ANIMATION 0
#endif
#ifndef FEATURE_ENCODER
#define FEATURE_ENCODER 1
#endif
// Enable ONLY after removing the physical LDR branch from GPIO34.
#ifndef FEATURE_ADC_BUTTONS
#define FEATURE_ADC_BUTTONS 0
#endif
#ifndef FEATURE_MCP23017
#if defined(BOARD_8048S070)
#define FEATURE_MCP23017 1
#else
#define FEATURE_MCP23017 0
#endif
#endif
#ifndef FEATURE_BT_A2DP_SMALL
#define FEATURE_BT_A2DP_SMALL 0
#endif
#ifndef FEATURE_BT_BRIDGE_BIG
#define FEATURE_BT_BRIDGE_BIG 0
#endif
#ifndef FEATURE_SPOTIFY_REMOTE
#define FEATURE_SPOTIFY_REMOTE 0
#endif
#ifndef FEATURE_SPOTIFY_CONNECT
#define FEATURE_SPOTIFY_CONNECT 0
#endif
#ifndef FEATURE_WEATHER
#define FEATURE_WEATHER 0
#endif
#ifndef FEATURE_ONLINE_ARTWORK
#define FEATURE_ONLINE_ARTWORK FEATURE_ARTWORK
#endif
// Reserved flags fail explicitly instead of silently pretending an adapter exists.
#if FEATURE_BT_BRIDGE_BIG || FEATURE_SPOTIFY_REMOTE || FEATURE_SPOTIFY_CONNECT || FEATURE_WEATHER
#error Optional network/BT adapters are not implemented in this source trial. See docs/STATUS.md.
#endif
#ifndef DUAL_DEVICE_NAME
#define DUAL_DEVICE_NAME ""  // empty: I-BT-Radio V029 SMALL/BIG-XXXXXX from factory MAC
#endif
#define DUAL_TZ "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define DUAL_NTP1 "pool.ntp.org"
#define DUAL_NTP2 "time.cloudflare.com"
