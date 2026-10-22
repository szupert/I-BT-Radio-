#include "esp_audio_dec_default.h"
#include "esp_audio_simple_dec_default.h"
int ibt_extra_codecs_register(void){
    // Called only by the audio owner, on first use. No encoders or radio stack.
    static int ready=0;if(ready)return 1;
    esp_audio_err_t (*registers[])(void)={esp_opus_dec_register,esp_vorbis_dec_register,
        esp_alac_dec_register,esp_aac_dec_register,esp_mp3_dec_register,esp_flac_dec_register,
        esp_adpcm_dec_register,esp_g711a_dec_register,esp_g711u_dec_register,
        esp_pcm_dec_register,esp_amrnb_dec_register,esp_amrwb_dec_register,
        esp_ogg_dec_register,esp_m4a_dec_register,esp_wav_dec_register,esp_ts_dec_register};
    for(unsigned i=0;i<sizeof(registers)/sizeof(registers[0]);++i){
        const esp_audio_err_t r=registers[i]();
        if(r!=ESP_AUDIO_ERR_OK&&r!=ESP_AUDIO_ERR_ALREADY_EXIST)return 0;
    }
    ready=1;return 1;
}
