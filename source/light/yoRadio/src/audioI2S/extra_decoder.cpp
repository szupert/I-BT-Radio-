#include "extra_decoder.h"
#include "light_wav.h"
#include "../dual/resource_policy.h"
#include <esp_heap_caps.h>
#include <new>
namespace extracodec {
bool Decoder::open(Kind kind){
 close();if(kind!=Kind::Wav)return false;
 constexpr unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
 constexpr size_t cost=sizeof(lightwav::Decoder),reserve=ibtlimit::Policy::reserve;
 if(heap_caps_get_free_size(caps)<cost+reserve+64||heap_caps_get_largest_free_block(caps)<cost+64)return false;
 void*p=heap_caps_malloc(cost,caps);if(!p)return false;
 if(heap_caps_get_free_size(caps)<reserve){heap_caps_free(p);return false;}
 handle=reinterpret_cast<esp_audio_simple_dec_handle_t>(new(p)lightwav::Decoder());type=kind;return true;
}
void Decoder::close(){if(handle){auto p=reinterpret_cast<lightwav::Decoder*>(handle);p->~Decoder();heap_caps_free(p);}handle=nullptr;pcm=nullptr;capacity=valid=offset=0;format={};type=Kind::None;}
Result Decoder::decode(uint8_t*data,size_t bytes,bool eos,int16_t*out,size_t samples){
 if(!handle)return {0,0,-1};auto&p=*reinterpret_cast<lightwav::Decoder*>(handle);size_t used=0,frames=0;
 const bool ok=p.decode(data,bytes,eos,out,samples,used,frames);
 format.sample_rate=p.rate;format.channel=p.channels;format.bits_per_sample=4;format.bitrate=p.bitrate;
 return {used,frames,ok?0:-2};
}
}
