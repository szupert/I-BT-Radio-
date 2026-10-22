#pragma once
#include <stddef.h>
#include <stdint.h>
class NetworkClient;
namespace netprefetch {
// The core-0 task is the sole socket reader while active. The audio task is
// the sole consumer of this raw-byte ring; protocol parsing stays on core 1.
bool begin(NetworkClient* source,bool tlsOnMain=false);
void service(); // core0 loop: sole HTTPS socket reader while active
bool end(uint32_t timeoutMs=1000);
// Read-only route diagnostics. Call once when a stream bypasses the split reader.
void noteBypass(const char* route, bool ssl);
bool active();
size_t available();
int read();
int read(uint8_t* dst,size_t bytes);
size_t readBytes(char* dst,size_t bytes);
void diagnostics();
}
