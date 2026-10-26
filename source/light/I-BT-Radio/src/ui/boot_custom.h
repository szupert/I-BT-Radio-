#pragma once
#include <stdint.h>
#include <stddef.h>
namespace bootcustom {
void prepare(); // Non-formatting mount, CRC check and saved hold time before first paint.
bool open();
void close();
bool row(unsigned y,uint16_t* pixels); // One existing320-pixel stack row; no framebuffer.
unsigned seconds();
bool saveSeconds(unsigned seconds);
bool available();
uint32_t checksum();
bool busy();
bool uploadBegin(size_t total);
bool uploadWrite(size_t index,const uint8_t* data,size_t length);
bool uploadFinish(uint32_t& crc);
void uploadAbort();
}
