#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ibtaudio {
// Non-mutating LayerIII header inspection. Free-format/reserved headers retain
// the original1600-byte path. Never pass an incomplete MP3 frame to Helix.
constexpr size_t mp3FrameBytes(const uint8_t* p,size_t n){
 if(!p||n<4||p[0]!=0xff||(p[1]&0xe0)!=0xe0)return 0;
 const unsigned version=(p[1]>>3)&3,layer=(p[1]>>1)&3,index=p[2]>>4,rate=(p[2]>>2)&3;
 if(version==1||layer!=1||!index||index==15||rate==3)return 0;
 constexpr unsigned rates[]={44100,48000,32000};
 constexpr unsigned v1[]={0,32,40,48,56,64,80,96,112,128,160,192,224,256,320};
 constexpr unsigned v2[]={0,8,16,24,32,40,48,56,64,80,96,112,128,144,160};
 const unsigned hz=rates[rate]/(version==3?1:version==2?2:4);
 return (version==3?144000u:72000u)*(version==3?v1[index]:v2[index])/hz+((p[2]>>1)&1);
}
constexpr size_t mp3ReadBudget(const uint8_t* p,size_t filled,size_t original){
 const size_t frame=mp3FrameBytes(p,filled);
 return frame&&frame<=original?frame:original;
}
}
