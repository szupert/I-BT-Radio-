#pragma once
#include <stdint.h>
namespace artab {
bool prepare();
bool setRows(unsigned rows);
bool set(bool enabled);
bool on();
void stop();
void tick(uint32_t now);
bool consumeRepaint();
void diagnostics();
}
