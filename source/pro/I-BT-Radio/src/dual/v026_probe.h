#pragma once
#include <Arduino.h>
namespace v026 {void early(); void beginBt(); void tick(); size_t held(); bool memoryOK(); bool btOK(); const char* btName();}
#define V026_STREAM_MAIN "http://stream-uk1.radioparadise.com/mp3-128"
#define V026_STREAM_ALT "http://stream-dc1.radioparadise.com/mp3-128"
