#pragma once

namespace ibtsignal {
// Classic Bluetooth reports RSSI relative to its good-reception range,
// not an absolute dBm value. These bars are a coarse, non-calibrated hint.
constexpr unsigned btBars(int delta) {
    if (delta < -128) return 0; // no measurement yet
    if (delta < -20) return 1;
    if (delta < -10) return 2;
    if (delta < 0) return 3;
    return 4;
}
constexpr unsigned wifiBars(bool connected, int rssi) {
    if (!connected) return 0;
    return rssi >= -55 ? 4 : rssi >= -67 ? 3 :
           rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
}
}
