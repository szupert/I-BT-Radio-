#pragma once
#include <stdint.h>
#include <stddef.h>
namespace artwork {
void begin();
void reset(const char *path,uint32_t generation);
void hint(size_t position,size_t length);
void audioTick(bool running,bool headerDone,uint32_t buffered);
void uiTick(uint32_t generation,bool visible);
void redraw(uint32_t generation);
}
