#pragma once
#include <stdint.h>
namespace stagecpu {
constexpr unsigned Other=4;
constexpr unsigned bucket(unsigned stage,bool owner){return owner?((stage>=6&&stage<=11)?2:(stage<4?stage:Other)):5;}
void begin(); bool set(bool on); unsigned enter(unsigned stage);void leave(unsigned previous);void diagnostics();
// Substages remain part of Decode in the original six-bucket accounting.
enum DecoderStage : unsigned { Huffman=6, Dequant=7, Imdct=8, Synthesis=9, Fdct=10, Polyphase=11 };
bool decoderActive();
class DecoderScope {
    unsigned previous;
public:
    DecoderScope(bool active,DecoderStage stage):previous(active?enter(unsigned(stage)):0xffffffffu){}
    ~DecoderScope(){if(previous!=0xffffffffu)leave(previous);}
};
}
