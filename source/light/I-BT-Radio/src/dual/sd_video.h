#pragma once
namespace sdvideo {
bool scan();
unsigned count();
const char* name(unsigned index);
bool open(unsigned index);
void close();
void clear();
void tick();
bool active();
bool paused();
void togglePause();
const char* status();
}
