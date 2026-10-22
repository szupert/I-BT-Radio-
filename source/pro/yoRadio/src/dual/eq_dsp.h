#pragma once
#include "eq_presets.h"
#include <cmath>
#include <cstring>

namespace eq10 {
// DAC-only peaking filters. Coefficients are calculated outside the PCM loop;
// neutral bands cost no per-sample work. The BT feeder never calls process().
class Dsp {
    struct Band { float b0=1,b1=0,b2=0,a1=0,a2=0,z1[2]={},z2[2]={}; bool active=false; };
    Band bands[Bands];
    Settings settings{};
    uint32_t rate=0;
    float headroom=1;
    bool activeFlag=false;
public:
    void configure(const Settings& next,uint32_t hz){
        settings=next;rate=hz;
        for(auto& b:bands)b=Band{};
        activeFlag=false;
        float boost=0;
        if(!settings.enabled||rate<8000)return;
        for(unsigned i=0;i<Bands;++i){
            const int gain=settings.db[i];
            if(!gain||gain<-12||gain>6||float(Hz[i])>=float(rate)*0.45f)continue;
            auto& b=bands[i];
            const float omega=6.28318530718f*float(Hz[i])/float(rate);
            const float alpha=sinf(omega)/(2.0f*1.4f);
            const float a=powf(10.0f,float(gain)/40.0f);
            const float inv=1.0f/(1.0f+alpha/a);
            b.b0=(1.0f+alpha*a)*inv;
            b.b1=(-2.0f*cosf(omega))*inv;
            b.b2=(1.0f-alpha*a)*inv;
            b.a1=b.b1;
            b.a2=(1.0f-alpha/a)*inv;
            b.active=true;
            activeFlag=true;
            if(gain>boost)boost=float(gain);
        }
        // Leave peak headroom for boosted curves; clamp remains the final guard.
        headroom=powf(10.0f,-boost/20.0f);
    }
    void setRate(uint32_t hz){configure(settings,hz);}
    bool active() const {return activeFlag;}
    bool enabled() const {return settings.enabled;}
    void process(int16_t sample[2]){
        if(!activeFlag)return;
        for(unsigned channel=0;channel<2;++channel){
            float value=float(sample[channel])*headroom;
            for(auto& b:bands){
                if(!b.active)continue;
                const float next=b.b0*value+b.z1[channel];
                b.z1[channel]=b.b1*value-b.a1*next+b.z2[channel];
                b.z2[channel]=b.b2*value-b.a2*next;
                value=next;
            }
            if(value>32767.0f)value=32767.0f;
            if(value< -32768.0f)value=-32768.0f;
            sample[channel]=int16_t(value);
        }
    }
};
}
