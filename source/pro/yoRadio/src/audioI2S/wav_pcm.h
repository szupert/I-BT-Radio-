#pragma once
#include <stdint.h>
#include <stddef.h>
namespace wavpcm {
constexpr uint64_t little(const uint8_t* p,unsigned bytes){
    uint64_t v=0;for(unsigned i=0;i<bytes;++i)v|=uint64_t(p[i])<<(8*i);return v;
}
// IEEE conversion uses integer bit fields: no unaligned loads, float casts or
// software-double operations. NaN is silent; infinities and overload saturate.
constexpr int16_t floating(uint64_t v,unsigned bits){
    const unsigned fraction=bits==32?23:52,bias=bits==32?127:1023;
    const unsigned exponent=unsigned((v>>fraction)&(bits==32?255:2047));
    const uint64_t mantissa=v&((uint64_t(1)<<fraction)-1);
    const bool negative=(v>>(bits-1))!=0;
    if(exponent==(bits==32?255:2047)&&mantissa)return 0;
    if(exponent>=bias)return negative?-32768:32767;
    if(exponent+15<bias)return 0;
    const unsigned shift=fraction+bias-15-exponent;
    const int magnitude=int(((uint64_t(1)<<fraction)|mantissa)>>shift);
    return int16_t(negative?-magnitude:magnitude);
}
constexpr int16_t sample(const uint8_t* p,unsigned bits,bool isFloat){
    const uint64_t v=little(p,bits/8);
    if(isFloat)return floating(v,bits);
    if(bits==8)return int16_t((int(v)-128)*256);
    const unsigned top=unsigned(v>>(bits-16));
    return int16_t(top>=32768?int(top)-65536:int(top));
}
struct Result {size_t consumed;size_t frames;};
constexpr Result convert(const uint8_t* input,size_t bytes,int16_t* out,
                         size_t capacity,unsigned bits,unsigned channels,bool isFloat){
    if(channels<1||channels>2||
       (isFloat?(bits!=32&&bits!=64):(bits!=8&&bits!=16&&bits!=24&&bits!=32)))return {0,0};
    const unsigned block=(bits/8)*channels;
    size_t frames=bytes/block;if(frames>capacity/channels)frames=capacity/channels;
    for(size_t i=0;i<frames*channels;++i)out[i]=sample(input+i*(bits/8),bits,isFloat);
    return {frames*block,frames};
}
}
