#pragma once
#include <stddef.h>
#include <stdint.h>
class NetworkClient;
namespace directrx {
void reset();
bool setSupplyMeasurement(bool enabled);
void lowInput();
int available(NetworkClient* client,bool repeat=false);
int read(NetworkClient* client,uint8_t* dst,size_t count);
void consumed(uint32_t bytes);
void decoded(uint32_t frames,uint32_t bitrate,uint32_t rate,uint32_t channels,uint32_t bits);
bool dacBegin();
void dacBreak();
void dacWrite(uint32_t start,uint32_t end,size_t requested,size_t written,bool ok,uint32_t rate,uint32_t dmaFrames);
void refillSample(uint32_t rounds,uint32_t bytes,uint32_t elapsed);
void diagnostics();
void dacDiagnostics();
void dacHardwareSample(uint32_t eofAddress,bool started);
void pcmBegin(uint32_t rate,unsigned channels);
void pcmRaw(const int16_t* pcm,size_t frames,unsigned channels);
void pcmPacked(const uint8_t* frames,size_t count);
void pcmAccepted(const uint8_t* data,size_t bytes);
}
