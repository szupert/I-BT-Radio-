#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
// Streaming RIFF/IMA-WAV decoder. No PCM cache or whole-file allocation.
// Standard IMA step/index constants; low nibble first, stereo 4-byte groups.
namespace lightwav {
struct Decoder {
 uint8_t header[20]{};unsigned have=0,need=12,state=0,channels=0,align=0,left=0;
 uint32_t remaining=0,dataLeft=0,rate=0,bitrate=0,fact=0,frames=0;
 int predictor[2]{},index[2]{};bool fmt=false,pad=false;
 static unsigned u16(const uint8_t*p){return unsigned(p[0])|(unsigned(p[1])<<8);}
 static uint32_t u32(const uint8_t*p){return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);}
 static int clip(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
 int sample(unsigned ch,unsigned nib){
  static constexpr int steps[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
  static constexpr int change[8]={-1,-1,-1,-1,2,4,6,8};
  const int step=steps[index[ch]];const int delta=((2*int(nib&7)+1)*step)>>3;
  predictor[ch]=clip(predictor[ch]+((nib&8)?-delta:delta),-32768,32767);
  index[ch]=clip(index[ch]+change[nib&7],0,88);return predictor[ch];
 }
 // Returns consumed bytes and frames. Errors leave no unchecked predictor/index.
 bool decode(const uint8_t*data,size_t size,bool eos,int16_t*out,size_t cap,size_t&used,size_t&made){
  used=made=0;if(!out||(!data&&size))return false;
  while(used<size){
   if(state==6){used=size;break;}
   if(state==4){size_t n=size-used;if(n>remaining)n=remaining;used+=n;remaining-=n;
    if(!remaining){state=1;need=8;have=0;}continue;}
   if(state!=5){
    size_t n=need-have;if(n>size-used)n=size-used;memcpy(header+have,data+used,n);used+=n;have+=n;if(have<need)break;
    if(state==0){if(memcmp(header,"RIFF",4)||memcmp(header+8,"WAVE",4))return false;state=1;need=8;have=0;continue;}
    if(state==1){remaining=u32(header+4);pad=remaining&1;
     if(!memcmp(header,"fmt ",4)){if(fmt||remaining<20||remaining>65536)return false;state=2;need=20;have=0;continue;}
     if(!memcmp(header,"fact",4)&&remaining>=4){state=3;need=4;have=0;continue;}
     if(!memcmp(header,"data",4)){if(!fmt)return false;dataLeft=remaining;state=5;left=0;if(!remaining)state=6;continue;}
     if(remaining==UINT32_MAX)return false;remaining+=pad;state=4;if(!remaining){state=1;need=8;have=0;}continue;
    }
    if(state==2){channels=u16(header+2);rate=u32(header+4);align=u16(header+12);
     if(u16(header)!=17||(channels!=1&&channels!=2)||rate<8000||rate>96000||u16(header+14)!=4||u16(header+16)<2||align<4*channels||align>4096||(channels==2&&(align-8)%8)||u16(header+18)!=1+(align-4*channels)*2/channels)return false;
     const uint32_t byteRate=u32(header+8);if(byteRate>UINT32_MAX/8)return false;bitrate=byteRate*8;fmt=true;
    }else if(state==3)fact=u32(header);
    remaining-=need;remaining+=pad;state=4;if(!remaining){state=1;need=8;have=0;}continue;
   }
   if(!dataLeft){state=6;continue;}
   if(!left){
    const unsigned n=4*channels;if(dataLeft<n)return false;if(size-used<n||cap-made*channels<channels)break;
    for(unsigned ch=0;ch<channels;++ch){auto p=data+used+4*ch;predictor[ch]=int16_t(u16(p));index[ch]=p[2];if(index[ch]>88||p[3])return false;}
    used+=n;dataLeft-=n;left=align-n;
    if(!fact||frames<fact){for(unsigned ch=0;ch<channels;++ch)out[made*channels+ch]=predictor[ch];++made;++frames;}
    continue;
   }
   const unsigned n=channels==2?8:1,produce=channels==2?8:2;
   if(left<n||dataLeft<n)return false;if(size-used<n||cap-made*channels<produce*channels)break;
   for(unsigned frame=0;frame<produce;++frame){
    for(unsigned ch=0;ch<channels;++ch){const unsigned at=channels==2?4*ch+frame/2:0;int value=sample(ch,(data[used+at]>>((frame&1)*4))&15);if(!fact||frames<fact)out[made*channels+ch]=int16_t(value);}
    if(!fact||frames<fact){++made;++frames;}
   }
   used+=n;left-=n;dataLeft-=n;
  }
  if(eos&&used==size&&state!=6&&!(state==5&&!dataLeft&&!left))return false;
  return true;
 }
};
}
