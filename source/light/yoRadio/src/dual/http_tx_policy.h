#pragma once
#include <stddef.h>
#include <stdint.h>
namespace ibtlimit {
// Application response credit, including headers and chunk framing. Polling
// cannot fill the entire TCP send queue while the previous bytes await ACK.
struct HttpTx {
  static constexpr size_t window = 1460;
  static constexpr size_t tcpReserve = 2048;
  static bool acknowledge(size_t written, size_t& acked, size_t len) {
    if (acked > written || len > written - acked) return false;
    acked += len;
    return true;
  }
  static size_t stripInterim(size_t& interim, size_t len) {
    const size_t n = len < interim ? len : interim;
    interim -= n;
    return len - n;
  }
  static size_t credit(size_t space, size_t written, size_t acked, size_t interim = 0) {
    if (acked > written || written - acked >= window || interim >= window - (written - acked)) return 0;
    const size_t left = window - (written - acked) - interim;
    return space < left ? space : left;
  }
};
}
