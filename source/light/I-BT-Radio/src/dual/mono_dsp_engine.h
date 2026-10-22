#pragma once
#include <stdint.h>
#include <cmath>
#include <algorithm>
#include <cstring>
namespace monodsp {
// Float32 throughout the sample path; RBJ normalization and transposed DF-II
// follow the existing eq_dsp.h implementation. Legacy EQ remains disabled.
constexpr unsigned Parameters=11;
struct Config { int16_t v[Parameters]={0,0,70,160,20,500,-15,10,10,1,1}; };
constexpr int16_t Minimum[]={0,0,40,80,-60,250,-60,-30,-30,0,0};
constexpr int16_t Maximum[]={1,2,200,300,60,1000,30,40,40,1,1};
constexpr int16_t Step[]={1,1,5,10,5,25,5,5,5,1,1};
inline bool valid(const Config& c){for(unsigned i=0;i<Parameters;++i)if(c.v[i]<Minimum[i]||c.v[i]>Maximum[i])return false;return true;}
struct Coeff {float b0=1,b1=0,b2=0,a1=0,a2=0;};
struct Plan {Config config;Coeff filters[5];uint32_t rate=44100;float attack=0,release=0;};
inline Coeff design(unsigned kind,float hz,float db,uint32_t rate){
    hz=std::min(hz,float(rate)*0.4f);
    const float w=6.28318530718f*hz/float(rate),c=cosf(w),s=sinf(w);
    const float a=powf(10.0f,db/40.0f),alpha=s*0.70710678118f;
    float b0,b1,b2,a0,a1,a2;
    if(kind==0){b0=(1+c)*0.5f;b1=-(1+c);b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;}
    else if(kind==1){ // peaking, Q=1 (same normalized form as eq10::Dsp)
        const float al=s*0.5f;
        b0=1+al*a;b1=-2*c;b2=1-al*a;a0=1+al/a;a1=b1;a2=1-al/a;
    }else{
        const float q=2*sqrtf(a)*alpha;
        if(kind==2){ // low shelf, S=1
            b0=a*((a+1)-(a-1)*c+q);b1=2*a*((a-1)-(a+1)*c);b2=a*((a+1)-(a-1)*c-q);
            a0=(a+1)+(a-1)*c+q;a1=-2*((a-1)+(a+1)*c);a2=(a+1)+(a-1)*c-q;
        }else{ // high shelf
            b0=a*((a+1)+(a-1)*c+q);b1=-2*a*((a-1)+(a+1)*c);b2=a*((a+1)+(a-1)*c-q);
            a0=(a+1)-(a-1)*c+q;a1=2*((a-1)-(a+1)*c);a2=(a+1)-(a-1)*c-q;
        }
    }
    const float inv=1/a0;return {b0*inv,b1*inv,b2*inv,a1*inv,a2*inv};
}
inline Plan prepare(const Config& config,uint32_t rate,unsigned volume){
    Plan p;p.config=config;p.rate=rate;
    const float protection=config.v[10]?std::max(0.0f,std::min(1.0f,(254.0f-float(volume))/100.0f)):1.0f;
    float bass=float(config.v[4])*0.1f;if(bass>0)bass*=protection;
    p.filters[0]=design(0,float(config.v[2]),0,rate);
    p.filters[1]=design(2,float(config.v[3]),bass,rate);
    p.filters[2]=design(1,float(config.v[5]),float(config.v[6])*0.1f,rate);
    p.filters[3]=design(1,2500,float(config.v[7])*0.1f,rate);
    p.filters[4]=rate>13333?design(3,6000,float(config.v[8])*0.1f,rate):Coeff{};
    p.attack=expf(-1.0f/(float(rate)*0.0005f));p.release=expf(-1.0f/(float(rate)*0.150f));
    return p;
}
struct Engine {
    Plan plan{};float z1[5]={},z2[5]={},limiterGain=1;
    void set(const Plan& next){plan=next;memset(z1,0,sizeof(z1));memset(z2,0,sizeof(z2));limiterGain=1;}
    float process(int16_t l,int16_t r,unsigned volume){
        float x=plan.config.v[1]==1?float(l):plan.config.v[1]==2?float(r):(float(l)+float(r))*0.5f;
        // Float intermediate headroom prevents boosted filters from clipping
        // before the output limiter. No int16 conversion between stages.
        x*=1.0f/32768.0f;
        for(unsigned i=0;i<5;++i){
            const auto& b=plan.filters[i];const float y=b.b0*x+z1[i];
            z1[i]=b.b1*x-b.a1*y+z2[i];z2[i]=b.b2*x-b.a2*y;x=y;
        }
        x*=float(volume)/256.0f;
        if(!std::isfinite(x)){memset(z1,0,sizeof(z1));memset(z2,0,sizeof(z2));return 0;}
        if(plan.config.v[9]){
            const float peak=fabsf(x),wanted=peak>0.92f?0.92f/peak:1.0f;
            const float time=wanted<limiterGain?plan.attack:plan.release;
            limiterGain=wanted+(limiterGain-wanted)*time;x*=limiterGain;
            // Instantaneous ceiling protects peaks during the 0.5ms attack.
            x=std::max(-0.98f,std::min(0.98f,x));
        }
        return std::max(-1.0f,std::min(0.9999694824f,x));
    }
};
// Settings are crossfaded between two complete filter states, never by
// interpolating IIR poles. Audio owner only; no allocation or coefficient math.
struct Processor {
    Engine engines[2];unsigned current=0,remaining=0,total=1;bool initialized=false,transition=false;
    void install(const Plan& p){
        if(!initialized){engines[0].set(prepare(Config{},p.rate,254));initialized=true;}
        engines[1-current].set(p);total=std::max<uint32_t>(1u,p.rate/50u);remaining=total;transition=true;
    }
    bool busy()const{return transition;}
    bool active()const{return initialized&&(transition||engines[current].plan.config.v[0]);}
    uint32_t process(int16_t l,int16_t r,uint32_t dry,unsigned volume,uint32_t rate){
        if(!active())return dry;
        auto value=[&](unsigned index,int channel){
            auto& e=engines[index];
            if(!e.plan.config.v[0]||e.plan.rate!=rate)return float(int16_t(channel?dry:dry>>16))/32768.0f;
            return 0.0f;
        };
        const auto sample=[&](unsigned index,float& left,float& right){
            auto& e=engines[index];
            if(e.plan.config.v[0]&&e.plan.rate==rate)left=right=e.process(l,r,volume);
            else {left=value(index,0);right=value(index,1);}
        };
        float left,right;sample(current,left,right);
        if(transition){
            float nl,nr;sample(1-current,nl,nr);const float mix=float(total-remaining+1)/float(total);
            left+=(nl-left)*mix;right+=(nr-right)*mix;
            if(!--remaining){current=1-current;transition=false;}
        }
        auto pcm=[](float x){return int16_t(std::max(-32768.0f,std::min(32767.0f,x*32768.0f)));};
        return (uint32_t(uint16_t(pcm(left)))<<16)|uint16_t(pcm(right));
    }
};
// Explicit versioned wire record; no padding, pointers, credentials or ABI data.
constexpr unsigned RecordBytes=30;
inline uint32_t checksum(const uint8_t* p,unsigned n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
inline void encode(const Config& c,uint8_t out[RecordBytes]){
    out[0]='M';out[1]='D';out[2]=1;out[3]=Parameters;
    for(unsigned i=0;i<Parameters;++i){const uint16_t n=uint16_t(c.v[i]);out[4+i*2]=uint8_t(n);out[5+i*2]=uint8_t(n>>8);}
    const uint32_t crc=checksum(out,26);for(unsigned i=0;i<4;++i)out[26+i]=uint8_t(crc>>(8*i));
}
inline bool decode(const uint8_t* in,unsigned size,Config& out){
    if(size!=RecordBytes||in[0]!='M'||in[1]!='D'||in[2]!=1||in[3]!=Parameters)return false;
    uint32_t crc=0;for(unsigned i=0;i<4;++i)crc|=uint32_t(in[26+i])<<(8*i);
    if(crc!=checksum(in,26))return false;
    Config c;for(unsigned i=0;i<Parameters;++i)c.v[i]=int16_t(uint16_t(in[4+i*2])|(uint16_t(in[5+i*2])<<8));
    if(!valid(c))return false;out=c;return true;
}
}
