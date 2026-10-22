#pragma once
#include <cstdint>
#include <cstring>
namespace ibtaudio {
// Linear 300 ms PCM envelope. No heap, clock, float, lock or per-frame division.
// Each instance belongs to one output owner. User volume stays independent.
class StartFade {
    uint32_t remaining_=0, phase_=0, step_=0;
public:
    void begin(uint32_t rate){
        remaining_=(rate>=8000 && rate<=192000)?rate*300u/1000u:0;
        phase_=0;step_=remaining_? (1u<<24)/remaining_:0;
    }
    bool active()const{return remaining_!=0;}
    void scale(int16_t& left,int16_t& right)const{
        if(!remaining_)return;
        const int32_t gain=int32_t(phase_>>9); // Q15, bounded below unity
        left=int16_t(int32_t(left)*gain/32768);
        right=int16_t(int32_t(right)*gain/32768);
    }
    void advance(){if(remaining_){phase_+=step_;--remaining_;}}
    void apply(uint8_t* pcm,uint32_t frames){
        for(uint32_t i=0;i<frames && remaining_;++i){
            int16_t s[2];std::memcpy(s,pcm+i*4,4);
            scale(s[0],s[1]);std::memcpy(pcm+i*4,s,4);advance();
        }
    }
};
}
