#pragma once
#include <stdint.h>
namespace audiod {
enum class Kind:uint8_t { Info, Bitrate, Title, Station, Artist, Album, Id3Title, Id3Data, Icy };
void begin();
void newStream();
// Returns true on Core1 even when full: diagnostic work never falls back onto audio.
bool defer(Kind kind,const char* text);
void service();
void diagnostics();
}
