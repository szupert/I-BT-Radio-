#pragma once
#include <stdint.h>
// Streaming linear stereo resampler for the selected BT OUT SBC rate.
// State belongs exclusively to the decoder task. No allocation.
class PcmResampler {
    uint32_t rate=0, output=0, phase=0; int16_t left=0,right=0; bool valid=false;
public:
    void reset(){valid=false;rate=0;output=0;phase=0;}
    template<class Emit> void feed(int16_t l,int16_t r,uint32_t hz,uint32_t outHz,Emit emit){
        if(hz<8000||hz>96000||!(outHz==16000||outHz==32000||outHz==44100||outHz==48000))return;
        // Native A2DP rate: exact passthrough avoids two 64-bit divisions per frame.
        if(hz==outHz){rate=hz;output=outHz;phase=hz;left=l;right=r;valid=true;emit(l,r);return;}
        if(!valid||rate!=hz||output!=outHz){rate=hz;output=outHz;phase=hz;left=l;right=r;valid=true;emit(l,r);return;}
        while(phase<=outHz){
            int16_t a=int32_t(left)+(int64_t(int32_t(l)-left)*phase)/outHz;
            int16_t b=int32_t(right)+(int64_t(int32_t(r)-right)*phase)/outHz;
            emit(a,b);phase+=rate;
        }
        phase-=outHz;left=l;right=r;
    }
};
