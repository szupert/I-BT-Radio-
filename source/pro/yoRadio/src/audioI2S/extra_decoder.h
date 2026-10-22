#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_audio_simple_dec.h"
namespace extracodec {
enum class Kind:uint8_t {None,Ogg,M4a,Wav,Amr,AmrWb,Ts,Mpeg};
struct Result {size_t consumed=0,frames=0;int error=0;};
class Decoder {
    esp_audio_simple_dec_handle_t handle=nullptr;
    uint8_t* pcm=nullptr;size_t capacity=0,valid=0,offset=0;
    esp_audio_simple_dec_info_t format{};
    unsigned stalled=0;Kind type=Kind::None;
public:
    Decoder()=default;
    Decoder(const Decoder&)=delete;
    Decoder& operator=(const Decoder&)=delete;
    ~Decoder(){close();}
    bool open(Kind kind);
    void close();
    bool restart(){const auto k=type;close();return open(k);}
    bool active()const{return handle!=nullptr;}
    bool pending()const{return offset<valid;}
    const esp_audio_simple_dec_info_t& info()const{return format;}
    Result decode(uint8_t* data,size_t bytes,bool eos,int16_t* out,size_t samples);
};
}
