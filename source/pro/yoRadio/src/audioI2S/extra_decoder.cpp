#include "extra_decoder.h"
#include "wav_pcm.h"
#include <esp_heap_caps.h>
#include <string.h>
extern "C" int ibt_extra_codecs_register(void);
extern "C" void* ibt_codec_malloc(size_t);
extern "C" void* ibt_codec_realloc(void*,size_t);
namespace extracodec {
bool Decoder::open(Kind kind){
    close();if(kind==Kind::None||heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<524288||!ibt_extra_codecs_register())return false;
    esp_audio_simple_dec_type_t code=ESP_AUDIO_SIMPLE_DEC_TYPE_NONE;
    switch(kind){
        case Kind::Ogg:code=ESP_AUDIO_SIMPLE_DEC_TYPE_OGG;break;
        case Kind::M4a:code=ESP_AUDIO_SIMPLE_DEC_TYPE_M4A;break;
        case Kind::Wav:code=ESP_AUDIO_SIMPLE_DEC_TYPE_WAV;break;
        case Kind::Amr:code=ESP_AUDIO_SIMPLE_DEC_TYPE_AMRNB;break;
        case Kind::AmrWb:code=ESP_AUDIO_SIMPLE_DEC_TYPE_AMRWB;break;
        case Kind::Ts:code=ESP_AUDIO_SIMPLE_DEC_TYPE_TS;break;
        case Kind::Mpeg:code=ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;break;
        default:return false;
    }
    esp_audio_simple_dec_cfg_t cfg{};cfg.dec_type=code;cfg.use_frame_dec=false;
    if(esp_audio_simple_dec_open(&cfg,&handle)!=ESP_AUDIO_ERR_OK){close();return false;}
    capacity=8192;pcm=static_cast<uint8_t*>(ibt_codec_malloc(capacity));
    if(!pcm){close();return false;}type=kind;return true;
}
void Decoder::close(){
    if(handle)esp_audio_simple_dec_close(handle);
    if(pcm)heap_caps_free(pcm);
    handle=nullptr;pcm=nullptr;capacity=valid=offset=0;format={};stalled=0;type=Kind::None;
}
Result Decoder::decode(uint8_t* data,size_t bytes,bool eos,int16_t* out,size_t samples){
    Result r;if(!handle||!out||bytes>UINT32_MAX){r.error=-1;return r;}
    if(!pending()){
        valid=offset=0;esp_audio_simple_dec_raw_t raw{};raw.buffer=data;raw.len=bytes;raw.eos=eos;
        esp_audio_simple_dec_out_t frame{};frame.buffer=pcm;frame.len=capacity;
        esp_audio_err_t status=esp_audio_simple_dec_process(handle,&raw,&frame);
        if(status==ESP_AUDIO_ERR_BUFF_NOT_ENOUGH){
            // Upstream's documented retry keeps the same input unconsumed.
            if(frame.needed_size<=capacity||frame.needed_size>65536){r.error=-2;return r;}
            void* larger=ibt_codec_realloc(pcm,frame.needed_size);
            if(!larger){r.error=-3;return r;}
            pcm=static_cast<uint8_t*>(larger);capacity=frame.needed_size;frame={};frame.buffer=pcm;frame.len=capacity;
            status=esp_audio_simple_dec_process(handle,&raw,&frame);
        }
        if(raw.consumed>bytes||frame.decoded_size>capacity){r.error=-4;return r;}
        r.consumed=raw.consumed;
        if(status!=ESP_AUDIO_ERR_OK&&status!=ESP_AUDIO_ERR_DATA_LACK&&status!=ESP_AUDIO_ERR_CONTINUE){r.error=int(status);return r;}
        valid=frame.decoded_size;
        if(valid){
            if(esp_audio_simple_dec_get_info(handle,&format)!=ESP_AUDIO_ERR_OK||
               format.channel<1||format.channel>2||format.sample_rate<8000||format.sample_rate>48000||
               (format.bits_per_sample!=16&&format.bits_per_sample!=24&&format.bits_per_sample!=32)||
               valid%((format.bits_per_sample/8)*format.channel)){r.error=-5;return r;}
        }
        if(!valid&&!r.consumed&&bytes&&!eos){if(++stalled>=128){r.error=-6;return r;}}else stalled=0;
    }
    if(pending()){
        const size_t block=(format.bits_per_sample/8)*format.channel;
        size_t frames=(valid-offset)/block;if(frames>samples/format.channel)frames=samples/format.channel;
        // The pinned decoders already return signed little-endian PCM16.
        // Copy it as a block instead of doing a dynamic 64-bit conversion per
        // sample. Wider PCM retains the existing conversion and clipping.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        if(format.bits_per_sample==16)memcpy(out,pcm+offset,frames*format.channel*sizeof(*out));
        else
#endif
        if(format.bits_per_sample==24){
            // PCM24 already became PCM16 here. Read the same upper two bytes
            // directly; no unaligned load, signed shift or dynamic 64-bit loop.
            const uint8_t* p=pcm+offset;
            for(size_t i=0;i<frames*format.channel;++i,p+=3){
                const unsigned top=unsigned(p[1])|(unsigned(p[2])<<8);
                out[i]=int16_t(int(top^0x8000u)-32768);
            }
        }else
        for(size_t i=0;i<frames*format.channel;++i)out[i]=wavpcm::sample(pcm+offset+i*(format.bits_per_sample/8),format.bits_per_sample,false);
        offset+=frames*block;r.frames=frames;
    }
    return r;
}
}
