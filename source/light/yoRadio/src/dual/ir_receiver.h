#pragma once
#include <stddef.h>
#include "ir_learn.h"
namespace irreceiver {
void menu(unsigned item);
void label(char* out,size_t cap,unsigned item);
smallir::Action tick(bool saveAllowed);
void cancel();
}
