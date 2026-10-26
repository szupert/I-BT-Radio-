#include "../dual/bt_rx_probe.h"
#include "../dual/memory_checkpoint.h"
#include "../dual/resource_policy.h"
#include "../dual/lite_memory.h"
#include "light_payload.h"
#include "../dual/mono_dsp.h"
#include "../dual/net_poll_policy.h"
#include "direct_rx_probe.h"
#include "../dual/stream_connect.h"
#include "../dual/icy_fields.h"
#include "../dual/deferred_audio.h"
#include "../dual/audio_path_probe.h"
#include "../core/options.h"
#if VS1053_CS==255
/*
 * Audio.cpp
 *
 *  Created on: Oct 26.2018
 *
 *  Version 2.0.5k
 *  Updated on: Aug 23.2022
 *      Author: Wolle (schreibfaul1)
 *
 */
#include "AudioEx.h"
#if defined(CONFIG_IDF_TARGET_ESP32)
#include <soc/i2s_struct.h>
#include <soc/gpio_struct.h>
#include <soc/i2s_periph.h>
#endif
#include "resume_seconds.h"
#include "local_read_policy.h"
#include "local_flac_plan.h"
#include "wav_format_policy.h"
#include "wav_pcm.h"
#include "dac_submit.h"
#include "direct_stream_policy.h"
#include "../dual/bt_audio.h"
#include "net_prefetch.h"
#include "startup_buffer.h"
#include "mp3_frame_policy.h"
#include "connection_codec_policy.h"
#include "live_chunk.h"
#include "refill_budget.h"
#include "metadata_budget.h"
#include "../dual/vu_probe.h"
#include "mp3_decoder/mp3_decoder.h"
#include "aac_decoder/aac_decoder.h"
#include "flac_decoder/flac_decoder.h"
#include "../core/config.h"
#include "../core/player.h"
#include "../core/sd_path.h"
#include "../dual/media.h"
#include <esp_heap_caps.h>
#include <new>

#ifdef SDFATFS_USED
fs::SDFATFS SD_SDFAT;
#endif
#ifndef DMA_BUFCOUNT
  #define DMA_BUFCOUNT  8
#endif
#ifndef DMA_BUFLEN
  #define DMA_BUFLEN  512   //  (512)
#endif
#if defined(ESP_ARDUINO_3)
#include "soc/io_mux_reg.h"
#endif
#ifdef ESP_ARDUINO_3
#define dma_buf_count dma_desc_num
#define dma_buf_len dma_frame_num
#endif
//---------------------------------------------------------------------------------------------------------------------
AudioBuffer::AudioBuffer(size_t maxBlockSize) {
    // if maxBlockSize isn't set use defaultspace (1600 bytes) is enough for aac and mp3 player
    if(maxBlockSize) m_resBuffSizeRAM  = maxBlockSize;
    if(maxBlockSize) m_maxBlockSize = maxBlockSize;
}

AudioBuffer::~AudioBuffer() { release(); }
void AudioBuffer::release() {
    if(m_lightRing.attached()){
        free(m_lightRing.page(0));free(m_lightRing.page(1));free(m_lightRing.window());m_lightRing.detach();
    }else free(m_buffer);
    m_buffer=m_readPtr=m_writePtr=m_endPtr=nullptr;
    m_buffSize=0;m_f_init=false;m_f_psram=false;m_f_start=true;
}

void AudioBuffer::setBufsize(int ram, int psram) {
    if (ram > -1) // -1 == default / no change
        m_buffSizeRAM = ram;
    if (psram > -1)
        m_buffSizePSRAM = psram;
}

size_t AudioBuffer::init() {
    release();
    // Bounded PSRAM capacity; never let an EEPROM multiplier size the ring.
    if(psramFound()) {
        const uint32_t caps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
        const uint32_t free=heap_caps_get_free_size(caps);
        const uint32_t largest=heap_caps_get_largest_free_block(caps);
        if(ibtaudio::StreamBufferPolicy::admitLarge(free,largest,m_resBuffSizePSRAM)){
            m_buffer=(uint8_t*)heap_caps_calloc(ibtaudio::StreamBufferPolicy::Large+m_resBuffSizePSRAM,1,caps);
            if(m_buffer)m_buffSize=ibtaudio::StreamBufferPolicy::Large;
        }
        if(!m_buffer){
            m_buffer=(uint8_t*)heap_caps_calloc(ibtaudio::StreamBufferPolicy::Fallback+m_resBuffSizePSRAM,1,caps);
            if(m_buffer)m_buffSize=ibtaudio::StreamBufferPolicy::Fallback;
        }
        if(m_buffer)m_f_psram=true;
        Serial.printf("[STREAM-BUFFER] capacity=%lu psram=%u free_before=%lu largest_before=%lu reserve=%lu\n",
            (unsigned long)m_buffSize,unsigned(m_f_psram),(unsigned long)free,
            (unsigned long)largest,(unsigned long)ibtaudio::StreamBufferPolicy::Reserve);
    }
    if(!m_buffer) {
#if IBT_NO_PSRAM && defined(IBT_LIGHT4_DNS_AUTO)
        if(!m_compactSdBt&&(config.getMode()==PM_WEB||!btaudio::outputSelected())){
            using P=ibtaudio::StreamBufferPolicy;
            constexpr unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
            const uint32_t window=config.getMode()==PM_WEB?CONFIG_LWIP_TCP_WND_DEFAULT:0;
            const uint32_t receiveBudget=P::receiveCost(window);
            lightmem::checkpoint("INPUT_ALLOC_BEFORE");
            uint8_t* pages[2]{};uint8_t* windowBytes=nullptr;
            if(P::admitSegmented(heap_caps_get_free_size(caps),heap_caps_get_largest_free_block(caps),
                m_resBuffSizeRAM,ibtlimit::Policy::reserve,window)){
                for(unsigned i=0;i<2;++i){
                    if(heap_caps_get_largest_free_block(caps)<ibtlimit::Policy::reserve)break;
                    pages[i]=static_cast<uint8_t*>(lightpayload::zero(ibtaudio::LightInputRing::Page));
                    if(!pages[i]||uint64_t(heap_caps_get_free_size(caps))<uint64_t(ibtlimit::Policy::reserve)+receiveBudget)break;
                }
                if(pages[0]&&pages[1]&&heap_caps_get_largest_free_block(caps)>=ibtlimit::Policy::reserve)
                    windowBytes=static_cast<uint8_t*>(lightpayload::zero(m_resBuffSizeRAM));
                if(pages[0]&&pages[1]&&windowBytes&&uint64_t(heap_caps_get_free_size(caps))>=uint64_t(ibtlimit::Policy::reserve)+receiveBudget){
                    m_lightRing.attach(pages[0],pages[1],windowBytes,m_resBuffSizeRAM);
                    m_buffer=pages[0];m_buffSize=P::LightBase;
                }else {free(pages[0]);free(pages[1]);free(windowBytes);}
            }
            lightmem::checkpoint("INPUT_ALLOC_AFTER");
            Serial.printf("[STREAM-BUFFER-LIGHT] capacity=%u requested=16384 policy=FIXED16 layout=PAGED_2X8192 page=8192 tail=%u receive_budget=%u reserve=%u largest_floor=%u source=%s\n",
                unsigned(m_buffSize),unsigned(m_resBuffSizeRAM),unsigned(receiveBudget),unsigned(ibtlimit::Policy::reserve),unsigned(ibtlimit::Policy::reserve),config.getMode()==PM_WEB?"WIFI":"SD");
        }else
#endif
        {
        const size_t base=IBT_NO_PSRAM?(config.getMode()==PM_SDCARD&&btaudio::outputSelected()?2048u:ibtaudio::StreamBufferPolicy::LightBase):32u*1024u;
        size_t capacity=m_compactCapacity?m_compactCapacity:base;
        if(IBT_NO_PSRAM&&config.getMode()==PM_WEB){
            const unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
            if(ibtaudio::StreamBufferPolicy::admitLight(heap_caps_get_free_size(caps),
                heap_caps_get_largest_free_block(caps),m_resBuffSizeRAM,ibtlimit::Policy::reserve))
                capacity=ibtaudio::StreamBufferPolicy::LightPreferred;
        }
        lightmem::checkpoint("INPUT_ALLOC_BEFORE");
        m_buffer = (uint8_t*)(IBT_NO_PSRAM?lightpayload::zero(capacity+(m_compactSdBt?0:m_resBuffSizeRAM)):calloc(capacity + m_resBuffSizeRAM,1));
        // A racing allocation or fragmentation must not prevent the old16KiB path.
        if(!m_buffer&&capacity!=base&&!m_compactCapacity){capacity=base;m_buffer=(uint8_t*)(IBT_NO_PSRAM?lightpayload::zero(base+m_resBuffSizeRAM):calloc(base+m_resBuffSizeRAM,1));}
        lightmem::checkpoint("INPUT_ALLOC_AFTER");
        if(m_buffer)m_buffSize=capacity;
        if(IBT_NO_PSRAM&&config.getMode()==PM_WEB)
            Serial.printf("[STREAM-BUFFER-LIGHT] capacity=%u base=16384 preferred=18432 reserve=%u\n",
                unsigned(m_buffSize),unsigned(ibtlimit::Policy::reserve));
        }
    }
    if(!m_buffer)return 0;
    m_maxBlockSize=1600;
    m_f_init = true;
    resetBuffer();
    return m_buffSize;
}

void AudioBuffer::changeMaxBlockSize(uint16_t mbs){
    if(m_compactSdBt&&m_f_init){m_maxBlockSize=std::min<size_t>(mbs,m_buffSize-1);return;}
    // Reserve is allocated for the largest supported frame, not merely the last codec.
    size_t reserve=m_f_psram?m_resBuffSizePSRAM:m_resBuffSizeRAM;
    m_maxBlockSize = mbs<=reserve?mbs:reserve;
    return;
}

uint16_t AudioBuffer::getMaxBlockSize(){
    return m_maxBlockSize;
}

size_t AudioBuffer::freeSpace() {
    if(m_compactSdBt&&m_f_init){return m_buffSize-1-size_t(m_writePtr-m_readPtr);}
    if(m_lightRing.attached())return m_lightRing.free();
    if(!m_f_init)return 0;
    if(m_readPtr > m_writePtr) {
        m_freeSpace = (m_readPtr - m_writePtr);
    } else {
        m_freeSpace = (m_endPtr - m_writePtr) + (m_readPtr - m_buffer);
    }
    if(m_f_start)
        m_freeSpace = m_buffSize;
    return m_freeSpace - 1;
}

size_t AudioBuffer::writeSpace() {
    if(m_compactSdBt&&m_f_init){
        const size_t used=size_t(m_writePtr-m_readPtr);
        if(m_readPtr!=m_buffer){memmove(m_buffer,m_readPtr,used);m_readPtr=m_buffer;m_writePtr=m_buffer+used;}
        return m_buffSize-1-used;
    }
    if(m_lightRing.attached())return m_lightRing.writeSpace();
    if(!m_f_init)return 0;
    if(m_readPtr > m_writePtr) {
        m_writeSpace = (m_readPtr - m_writePtr - 1); // readPtr must not be overtaken
    } else {
        if(getReadPos() == 0)
            m_writeSpace = (m_endPtr - m_writePtr - 1);
        else
            m_writeSpace = (m_endPtr - m_writePtr);
    }
    if(m_f_start)
        m_writeSpace = m_buffSize - 1;
    return m_writeSpace;
}

size_t AudioBuffer::bufferFilled() {
    if(m_compactSdBt&&m_f_init){return size_t(m_writePtr-m_readPtr);}
    if(m_lightRing.attached())return m_lightRing.filled();
    if(!m_f_init)return 0;
    if(m_writePtr >= m_readPtr) {
        m_dataLength = (m_writePtr - m_readPtr);
    } else {
        m_dataLength = (m_endPtr - m_readPtr) + (m_writePtr - m_buffer);
    }
    return m_dataLength;
}

void AudioBuffer::bytesWritten(size_t bw) {
    if(m_compactSdBt&&m_f_init){m_writePtr+=std::min(bw,size_t(m_endPtr-m_writePtr-1));return;}
    if(m_lightRing.attached()){m_lightRing.written(bw);return;}
    if(!m_f_init)return;
    m_writePtr += bw;
    if(m_writePtr == m_endPtr) {
        m_writePtr = m_buffer;
    }
    if(bw && m_f_start)
        m_f_start = false;
}

void AudioBuffer::bytesWasRead(size_t br) {
    if(m_compactSdBt&&m_f_init){m_readPtr+=std::min(br,size_t(m_writePtr-m_readPtr));return;}
    if(m_lightRing.attached()){m_lightRing.consumed(br);return;}
    if(!m_f_init)return;
    m_readPtr += br;
    if(m_readPtr >= m_endPtr) {
        size_t tmp = m_readPtr - m_endPtr;
        m_readPtr = m_buffer + tmp;
    }
}

uint8_t* AudioBuffer::getWritePtr() {
    if(m_lightRing.attached())return m_lightRing.writePtr();
    return m_writePtr;
}

uint8_t* AudioBuffer::getReadPtr() {
    if(m_compactSdBt&&m_f_init){return m_readPtr;}
    if(m_lightRing.attached())return m_lightRing.readPtr(m_maxBlockSize);
    if(!m_f_init)return NULL;
    size_t len = m_endPtr - m_readPtr;
    if(len < m_maxBlockSize) { // be sure the last frame is completed
        memcpy(m_endPtr, m_buffer, m_maxBlockSize - len);  // cpy from m_buffer to m_endPtr with len
    }
    return m_readPtr;
}

void AudioBuffer::resetBuffer() {
    if(m_lightRing.attached()){m_lightRing.reset();m_f_start=true;return;}
    if(!m_f_init || !m_buffer){m_readPtr=m_writePtr=m_endPtr=NULL;return;}
    m_writePtr = m_buffer;
    m_readPtr = m_buffer;
    m_endPtr = m_buffer + m_buffSize;
    m_f_start = true;
    // memset(m_buffer, 0, m_buffSize); //Clear Inputbuffer
}

uint32_t AudioBuffer::getWritePos() {
    if(m_lightRing.attached())return m_lightRing.writePos();
    if(!m_f_init)return 0;
    return m_writePtr - m_buffer;
}

uint32_t AudioBuffer::getReadPos() {
    if(m_lightRing.attached())return m_lightRing.readPos();
    if(!m_f_init)return 0;
    return m_readPtr - m_buffer;
}
//---------------------------------------------------------------------------------------------------------------------
Audio::Audio(bool internalDAC /* = false */, uint8_t channelEnabled /* = I2S_DAC_CHANNEL_BOTH_EN */, uint8_t i2sPort) {

    //    build-in-DAC works only with ESP32 (ESP32-S3 has no build-in-DAC)
    //    build-in-DAC last working Arduino Version: 2.0.0-RC2
    //    possible values for channelEnabled are:
    //    I2S_DAC_CHANNEL_DISABLE  = 0,     Disable I2S built-in DAC signals
    //    I2S_DAC_CHANNEL_RIGHT_EN = 1,     Enable I2S built-in DAC right channel, maps to DAC channel 1 on GPIO25
    //    I2S_DAC_CHANNEL_LEFT_EN  = 2,     Enable I2S built-in DAC left  channel, maps to DAC channel 2 on GPIO26
    //    I2S_DAC_CHANNEL_BOTH_EN  = 0x3,   Enable both of the I2S built-in DAC channels.
    //    I2S_DAC_CHANNEL_MAX      = 0x4,   I2S built-in DAC mode max index
#ifdef AUDIO_LOG
    m_f_Log = true;
#endif
    clientsecure.setInsecure();  // if that can't be resolved update to ESP32 Arduino version 1.0.5-rc05 or higher
    m_f_channelEnabled = channelEnabled;
    m_f_internalDAC = internalDAC;
    //i2s configuration
    m_i2s_num = i2sPort; // i2s port number
    m_i2s_config.sample_rate          = 16000;
    m_i2s_config.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    m_i2s_config.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    m_i2s_config.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1; // interrupt priority
#ifdef OLD_DMABUF_PARAMS
    m_i2s_config.dma_buf_count        = 16;    // 4×512×16=32768
#else
    m_i2s_config.dma_buf_count        = DMA_BUFCOUNT;
#endif
    m_i2s_config.dma_buf_len          = DMA_BUFLEN;
    m_i2s_config.use_apll             = APLL_DISABLE; // must be disabled in V2.0.1-RC1
    m_i2s_config.tx_desc_auto_clear   = true;   // new in V1.0.1
    m_i2s_config.fixed_mclk           = I2S_PIN_NO_CHANGE;


    if (internalDAC)  {

        #ifdef CONFIG_IDF_TARGET_ESP32  // ESP32S3 has no DAC

            log_i("internal DAC");
            m_i2s_config.mode             = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN );

            #if ESP_ARDUINO_VERSION_MAJOR >= 2
                m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_MSB); // vers >= 2.0.0
            #else
                m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S_MSB);
            #endif

            m_i2sReady = i2s_driver_install((i2s_port_t)m_i2s_num, &m_i2s_config, 0, NULL)==ESP_OK;
            i2s_set_dac_mode((i2s_dac_mode_t)m_f_channelEnabled);
            if(m_f_channelEnabled != I2S_DAC_CHANNEL_BOTH_EN) {
                m_f_forceMono = true;
            }

        #endif

    }
    else {
        m_i2s_config.mode             = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);

        #if ESP_ARDUINO_VERSION_MAJOR >= 2
            m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S); // Arduino vers. > 2.0.0
        #else
            m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
        #endif

        m_i2sReady = i2s_driver_install((i2s_port_t)m_i2s_num, &m_i2s_config, 0, NULL)==ESP_OK;
        m_f_forceMono = false;
    }

    if(m_i2sReady)i2s_zero_dma_buffer((i2s_port_t) m_i2s_num);

    for(int i = 0; i <3; i++) {
        m_filter[i].a0  = 1;
        m_filter[i].a1  = 0;
        m_filter[i].a2  = 0;
        m_filter[i].b1  = 0;
        m_filter[i].b2  = 0;
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::setBufsize(int rambuf_sz, int psrambuf_sz) {
    if(InBuff.isInitialized()) {
        log_e("Audio::setBufsize must not be called after audio is initialized");
        return;
    }
    InBuff.setBufsize(rambuf_sz, psrambuf_sz);
};

// RADIO_FIRST_AUDIO_DIAG_V52_20260911
void Audio::initInBuff(bool compactSdBt,size_t compactCapacity) {
    lightmem::checkpoint("PCM_ALLOC_BEFORE");
    const bool newOutput=!m_outBuff;
    if(!m_outBuff)m_outBuff=static_cast<int16_t*>(IBT_NO_PSRAM?lightpayload::zero(OutputBytes):heap_caps_calloc(OutputSamples,sizeof(int16_t),litememory::payloadCaps));
    lightmem::checkpoint("PCM_ALLOC_AFTER");
    if(!m_outBuff){Serial.println("[PCM-MEM] result=DENIED reason=STAGING_ALLOCATION");return;}
    if(!InBuff.isInitialized()) {
        if(compactCapacity)InBuff.compactLocal(compactCapacity);else InBuff.compactSdBt(compactSdBt);
        const size_t beforeFree=heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        const size_t beforeLargest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        size_t size = InBuff.init();
        if(!size&&newOutput){heap_caps_free(m_outBuff);m_outBuff=nullptr;}
        Serial.printf("[RADIO-MEM] input-init size=%u psram=%d before=%u/%u after=%u/%u i2s=%d\n",
                      (unsigned)size, InBuff.havePSRAM()?1:0,
                      (unsigned)beforeFree,(unsigned)beforeLargest,
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                      m_i2sReady?1:0);
        if (size > 0) {
            AUDIO_INFO("PSRAM %sfound, inputBufferSize: %u bytes", InBuff.havePSRAM()?"":"not ", size - 1);
        }
    }
    InBuff.changeMaxBlockSize(1600); // reset the actual input buffer, not the unused base
}

//---------------------------------------------------------------------------------------------------------------------
esp_err_t Audio::I2Sstart(uint8_t i2s_num) {
    // It is not necessary to call this function after i2s_driver_install() (it is started automatically),
    // however it is necessary to call it after i2s_stop()
    return i2s_start((i2s_port_t) i2s_num);
}

esp_err_t Audio::I2Sstop(uint8_t i2s_num) {
    return i2s_stop((i2s_port_t) i2s_num);
}
bool Audio::suspendDacForBtSearch(){
#if IBT_NO_PSRAM
    const bool localBt=btaudio::outputSelected()&&btaudio::producerReady()&&config.getMode()==PM_SDCARD;
    if(!player.onAudioTask()||m_f_running||m_f_internalDAC||(!localBt&&(!btaudio::idle()||btaudio::wanted())))return false;
    if(m_dacDormant)return true;
    if(!m_i2sReady)return false;
    const unsigned before=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    const esp_err_t error=i2s_driver_uninstall((i2s_port_t)m_i2s_num);
    if(error!=ESP_OK){Serial.printf("[DAC-LEASE] suspend=FAIL error=%d\n",int(error));return false;}
    m_i2sReady=false;m_dacDormant=true;
    const unsigned after=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    Serial.printf("[DAC-LEASE] suspend=PASS before=%u after=%u reclaimed=%u dma=%ux%u restore=REQUIRED\n",before,after,after>=before?after-before:0,unsigned(m_i2s_config.dma_buf_count),unsigned(m_i2s_config.dma_buf_len));
#endif
    return true;
}
bool Audio::outputReady() const {
    if(m_i2sReady)return true;
#if IBT_NO_PSRAM
    return m_dacDormant&&!m_f_internalDAC&&config.getMode()==PM_SDCARD&&
        btaudio::outputSelected()&&btaudio::producerReady();
#else
    return false;
#endif
}
bool Audio::restoreDacAfterBtSearch(){
#if IBT_NO_PSRAM
    if(!m_dacDormant)return m_i2sReady;
    if(btaudio::outputSelected())return false; // SD->BT never leases the physical DAC.
    if(!player.onAudioTask()||m_f_running)return false;
    const unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    const unsigned dmaBytes=unsigned(m_i2s_config.dma_buf_count)*unsigned(m_i2s_config.dma_buf_len)*4u;
    const unsigned reserve=ibtlimit::Policy::reserve;
    // Retain the full original DMA capacity and reserve plus descriptor/queue budget.
    const unsigned plan=dmaBytes+4096u;
    if(heap_caps_get_free_size(caps)<plan+reserve||heap_caps_get_largest_free_block(caps)<8192u){
        Serial.printf("[DAC-LEASE] restore=DENIED plan=%u reserve=%u free=%u largest=%u\n",plan,reserve,unsigned(heap_caps_get_free_size(caps)),unsigned(heap_caps_get_largest_free_block(caps)));return false;
    }
    auto cfg=m_i2s_config;cfg.sample_rate=m_sampleRate?m_sampleRate:16000;
    esp_err_t error=i2s_driver_install((i2s_port_t)m_i2s_num,&cfg,0,nullptr);
    if(error!=ESP_OK){Serial.printf("[DAC-LEASE] restore=FAIL stage=install error=%d\n",int(error));return false;}
    error=i2s_set_pin((i2s_port_t)m_i2s_num,&m_pin_config);
    if(error==ESP_OK)error=i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
    if(error!=ESP_OK||heap_caps_get_free_size(caps)<reserve){
        // Keep ownership if cleanup itself fails; do not install another driver.
        const esp_err_t cleanup=i2s_driver_uninstall((i2s_port_t)m_i2s_num);
        if(cleanup!=ESP_OK){m_i2sReady=false;m_dacDormant=false;}
        Serial.printf("[DAC-LEASE] restore=FAIL stage=configure error=%d cleanup=%d\n",int(error),int(cleanup));return false;
    }
    m_i2sReady=true;m_dacDormant=false;m_startFadePending=true;
    Serial.printf("[DAC-LEASE] restore=PASS dma=%ux%u free=%u reserve=%u\n",unsigned(cfg.dma_buf_count),unsigned(cfg.dma_buf_len),unsigned(heap_caps_get_free_size(caps)),reserve);
#endif
    return m_i2sReady;
}
//---------------------------------------------------------------------------------------------------------------------
esp_err_t Audio::i2s_mclk_pin_select(const uint8_t pin) {
    // IDF >= 4.4 use setPinout(BCLK, LRC, DOUT, DIN, MCK) only, i2s_mclk_pin_select() is no longer needed

    if(pin != 0 && pin != 1 && pin != 3) {
        log_e("Only support GPIO0/GPIO1/GPIO3, gpio_num:%d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    #ifdef CONFIG_IDF_TARGET_ESP32
        switch(pin){
            case 0:
                PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
                WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
                break;
            case 1:
                PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
                WRITE_PERI_REG(PIN_CTRL, 0xF0F0);
                break;
            case 3:
                PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
                WRITE_PERI_REG(PIN_CTRL, 0xFF00);
                break;
            default:
                break;
        }
    #endif

    return ESP_OK;
}
//---------------------------------------------------------------------------------------------------------------------
Audio::~Audio() {
    //I2Sstop(m_i2s_num);
    //InBuff.~AudioBuffer(); #215 the AudioBuffer is automatically destroyed by the destructor
    setDefaults();
    if(m_outBuff){heap_caps_free(m_outBuff);m_outBuff=nullptr;}
    if(m_playlistBuff) {free(m_playlistBuff); m_playlistBuff = NULL;}
    if(m_i2sReady)i2s_driver_uninstall((i2s_port_t)m_i2s_num); // #215 free I2S buffer
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setDefaults(bool prepareMp3, bool buffers) {
    audiod::newStream();
    if(!netprefetch::end()){m_f_running=false;return false;}
    stopSong();
    // The audio owner has joined the optional network producer. Close both
    // clients even after switching to SD; the old source may have been HTTPS.
    client.stop();clientsecure.stop();_client=static_cast<WiFiClient*>(&client);
    MP3Decoder_FreeBuffers();
    FLACDecoder_FreeBuffers();
    AACDecoder_FreeBuffers();
    if(m_playlistBuff)   {free(m_playlistBuff);     m_playlistBuff = NULL;} // free if stream is not m3u8
    vector_clear_and_shrink(m_playlistURL);
    vector_clear_and_shrink(m_playlistContent);
    m_hashQueue.clear(); m_hashQueue.shrink_to_fit(); // uint32_t vector
    // stopSong() has already cleared both the output buffer and the DMA ring.
    // SD->DAC reset must not queue another synthetic silence block at the old
    // file's rate (512 frames are 64ms at 8kHz, repeated at a file boundary).
    // Preserve the existing WEB/BT drain path and all recovery/headroom gates.
    if(config.getMode()==PM_SDCARD && !btaudio::wanted()) {
        m_validSamples=0;m_curSample=0;
    } else playI2Sremains();
    ts_parsePacket(0, 0, 0); // reset ts routine

    AUDIO_INFO("buffers freed, free Heap: %lu bytes", ESP.getFreeHeap());

    m_streamTransportClosed = false;
    m_streamBuffering = false;
    m_headerWaiting = false;
    m_f_chunked = false;                                    // Assume not chunked
    m_f_firstmetabyte = false;
    m_f_playing = false;
    m_f_ssl = false;
    m_f_metadata = false;
    m_f_tts = false;
    m_f_firstCall = true;                                   // InitSequence for processWebstream and processLokalFile
    m_f_running = false;
    m_f_loop = false;                                       // Set if audio file should loop
    m_f_unsync = false;                                     // set within ID3 tag but not used
    m_f_exthdr = false;                                     // ID3 extended header
    m_f_rtsp = false;                                       // RTSP (m3u8)stream
    m_f_m3u8data = false;                                   // set again in processM3U8entries() if necessary
    m_f_continue = false;
    m_f_ts = false;

    m_streamType = ST_NONE;
    m_codec = CODEC_NONE;
    m_playlistFormat = FORMAT_NONE;
    m_datamode = AUDIO_NONE;
    m_audioCurrentTime = 0;                                 // Reset playtimer
    m_resumeFilePos=0; // A local resume offset must never survive a new WEB source.
    extraReadWaiting=false;extraReadWaitSince=0;
    m_audioFileDuration = 0;
    m_audioDataStart = 0;
    m_audioDataSize = 0;
    m_avr_bitrate = 0;                                      // the same as m_bitrate if CBR, median if VBR
    m_bitRate = 0;                                          // Bitrate still unknown
    m_bytesNotDecoded = 0;                                  // counts all not decodable bytes
    m_chunkcount = 0;                                       // for chunked streams
    m_contentlength = 0;                                    // If Content-Length is known, count it
    m_curSample = 0;
    m_metaint = 0;                                          // No metaint yet
    m_LFcount = 0;                                          // For end of header detection
    m_controlCounter = 0;                                   // Status within readID3data() and readWaveHeader()
    m_channels = 2;                                         // assume stereo #209
    m_streamTitleHash = 0;
    m_file_size = 0;
    m_ID3Size = 0;
#if IBT_NO_PSRAM
    // Do not retain/recreate a 17984B input +4608B PCM ring while stopped or
    // starting BT. Prepare decoder workspaces before these optional I/O buffers:
    // the shared10000B DRAM/unchanged4KiB EXEC guards still see the real available heap.
    InBuff.release();heap_caps_free(m_outBuff);m_outBuff=nullptr;m_validSamples=0;
    if(prepareMp3 && !MP3Decoder_AllocateBuffers()){
        Serial.println("[MP3-PREPARE] result=DENIED phase=BEFORE_IO reserve=10000");return false;
    }
    if(prepareMp3)Serial.println("[MP3-PREPARE] result=READY phase=BEFORE_IO reserve=10000");
#endif
    if(buffers){initInBuff();InBuff.resetBuffer();}
    const bool ok=!buffers || (InBuff.isInitialized()&&m_outBuff&&outputReady());
#if IBT_NO_PSRAM
    if(!ok){MP3Decoder_FreeBuffers();InBuff.release();heap_caps_free(m_outBuff);m_outBuff=nullptr;}
#endif
    return ok;
}

//---------------------------------------------------------------------------------------------------------------------
void Audio::setConnectionTimeout(uint16_t timeout_ms, uint16_t timeout_ms_ssl){
    if(timeout_ms)     m_timeout_ms     = timeout_ms;
    if(timeout_ms_ssl) m_timeout_ms_ssl = timeout_ms_ssl;
}

void Audio::connectTask(void* pvParams) {
  ConnectParams* params = static_cast<ConnectParams*>(pvParams);
  Audio* self = params->instance;
  if(self->_client){
    self->_connectionResult = self->_client->connect(params->hostwoext, params->port/*, self->m_f_ssl ? self->m_timeout_ms_ssl : self->m_timeout_ms*/);
  }else{
    self->_connectionResult = false;
  }
  free((void*)params->hostwoext);
  delete params;
  self->_connectTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

//---------------------------------------------------------------------------------------------------------------------
bool Audio::connecttohost(const char* host, const char* user, const char* pwd) {

    if(ibtdirect::hlsUrl(host)&&!ibtdirect::hlsEnabled()){AUDIO_ERROR("HLS disabled in settings");return false;}

    // user and pwd for authentification only, can be empty

    if(!netprefetch::end())return false;
    // Light allocates I/O only after TLS, never before the reset below.
#if !IBT_NO_PSRAM
    initInBuff();
    if(!InBuff.isInitialized() || !m_i2sReady || !m_outBuff){Serial.printf("[RADIO-MEM] admission FAIL buff=%d i2s=%d heap=%u largest=%u\n",InBuff.isInitialized()?1:0,m_i2sReady?1:0,(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));AUDIO_ERROR("Audio buffer/I2S unavailable");return false;}
#endif
    if(host == NULL) {
        AUDIO_INFO("Hostaddress is empty");
        if(audio_error) audio_error("Hostaddress is empty");
        return false;
    }

    uint16_t lenHost = strlen(host);

    if(lenHost >= 512 - 10) {
        AUDIO_INFO("Hostaddress is too long");
        if(audio_error) audio_error("Hostaddress is too long");
        return false;
    }

    int idx = indexOf(host, "http");
    char* l_host = (char*)malloc(lenHost + 10);
    if(!l_host){AUDIO_ERROR("URL allocation failed");return false;}
    if(idx < 0){strcpy(l_host, "http://"); strcat(l_host, host); } // amend "http;//" if not found
    else       {strcpy(l_host, (host + idx));}                     // trim left if necessary

    char* h_host = NULL; // pointer of l_host without http:// or https://
    lightmem::checkpoint("DECODER_READY");
    if(startsWith(l_host, "https")) h_host = strdup(l_host + 8);
    else                            h_host = strdup(l_host + 7);
    if(!h_host){free(l_host);AUDIO_ERROR("Host allocation failed");return false;}

    // initializationsequence
    int16_t pos_slash;                                        // position of "/" in hostname
    int16_t pos_colon;                                        // position of ":" in hostname
    int16_t pos_ampersand;                                    // position of "&" in hostname
    uint16_t port = 80;                                       // port number

    // In the URL there may be an extension, like noisefm.ru:8000/play.m3u&t=.m3u
    pos_slash     = indexOf(h_host, "/", 0);
    pos_colon     = indexOf(h_host, ":", 0);
        if(isalpha(h_host[pos_colon + 1])) pos_colon = -1; // no portnumber follows
    pos_ampersand = indexOf(h_host, "&", 0);

    char *hostwoext = NULL;                                  // "skonto.ls.lv:8002" in "skonto.ls.lv:8002/mp3"
    char *extension = NULL;                                  // "/mp3" in "skonto.ls.lv:8002/mp3"

    if(pos_slash > 1) {
        hostwoext = (char*)malloc(pos_slash + 2);
        if(!hostwoext){free(l_host);free(h_host);return false;}
        memcpy(hostwoext, h_host, pos_slash);
        hostwoext[pos_slash] = '\0';
        uint16_t extLen =  urlencode_expected_len(h_host + pos_slash);
        extension = (char *)malloc(extLen + 20);
        if(!extension){free(hostwoext);free(l_host);free(h_host);return false;}
        strcpy(extension, h_host + pos_slash);
        urlencode(extension, extLen, true);
    }
    else{  // url has no extension
        hostwoext = strdup(h_host);
        extension = strdup("/");
    }
    if(!hostwoext || !extension){free(hostwoext);free(extension);free(l_host);free(h_host);return false;}

    if((pos_colon >= 0) && ((pos_ampersand == -1) or (pos_ampersand > pos_colon))){
        port = atoi(h_host + pos_colon + 1);// Get portnumber as integer
        hostwoext[pos_colon] = '\0';// Host without portnumber
    }

    AUDIO_INFO("Connect to new host: \"%s\"", l_host);
    if(!setDefaults(IBT_NO_PSRAM && ibtaudio::prepareMp3First(extension), !IBT_NO_PSRAM)){
        free(hostwoext);free(extension);free(l_host);free(h_host);
        AUDIO_ERROR("MP3 inditas: nincs vedett munkaterulet");return false;
    }

    if(startsWith(l_host, "https")) m_f_ssl = true;
    else                            m_f_ssl = false;

    // authentification
    size_t auth = strlen(user) + strlen(pwd);
    if(auth>512){free(hostwoext);free(extension);free(l_host);free(h_host);return false;}
    char toEncode[auth + 4];
    toEncode[0] = '\0';
    strcat(toEncode, user);
    strcat(toEncode, ":");
    strcat(toEncode, pwd);
    char authorization[base64_encode_expected_len(strlen(toEncode)) + 1];
    authorization[0] = '\0';
    b64encode((const char*)toEncode, strlen(toEncode), authorization);

    //  AUDIO_INFO("Connect to \"%s\" on port %d, extension \"%s\"", hostwoext, port, extension);

    char rqh[strlen(hostwoext) + strlen(extension) + strlen(authorization) + 200]; // encoded URL included
    rqh[0] = '\0';

    strcat(rqh, "GET ");
    strcat(rqh, extension);
    strcat(rqh, " HTTP/1.1\r\n");
    strcat(rqh, "Host: ");
    strcat(rqh, hostwoext);
    strcat(rqh, "\r\n");
    strcat(rqh, "Icy-MetaData:1\r\n");
    if (auth > 0) {
      strcat(rqh, "Authorization: Basic ");
      strcat(rqh, authorization);
      strcat(rqh, "\r\n");
    }
    strcat(rqh, "Accept-Encoding: identity;q=1,*;q=0\r\n");
    strcat(rqh, "User-Agent: Mozilla/5.0\r\n");
    strcat(rqh, "Connection: keep-alive\r\n\r\n");

    if(ESP_ARDUINO_VERSION_MAJOR == 2 && ESP_ARDUINO_VERSION_MINOR == 0 && ESP_ARDUINO_VERSION_PATCH >= 3 && MAX_AUDIO_SOCKET_TIMEOUT){
        m_timeout_ms_ssl = UINT16_MAX;  // bug in v2.0.3 if hostwoext is a IPaddr not a name
        m_timeout_ms = UINT16_MAX;  // [WiFiClient.cpp:253] connect(): select returned due to timeout 250 ms for fd 48
    }
    bool res = true; // no need to reconnect if connection exists

    if(m_f_ssl){ _client = static_cast<WiFiClient*>(&clientsecure); if(port == 80) port = 443;}
    else       { _client = static_cast<WiFiClient*>(&client);}

    lightmem::checkpoint("BEFORE_CONNECT");
    uint32_t t = millis();
    if(m_f_Log) AUDIO_INFO("connect to %s on port %d path %s", hostwoext, port, extension);
    if(!config.store.watchdog || YO_DUAL){
      res = radiodns::connect(_client, clientsecure, hostwoext, port, m_f_ssl ? m_timeout_ms_ssl : m_timeout_ms);
    }else{
      ConnectParams* params = new ConnectParams{ strdup(hostwoext), port, this }; _connectionResult = false;
      xTaskCreatePinnedToCore(connectTask, "ConnectTask", WATCHDOG_TASK_SIZE, params, WATCHDOG_TASK_PRIORITY, &_connectTaskHandle, WATCHDOG_TASK_CORE_ID);
      for(;;){
        if(millis()-t>(m_f_ssl ? m_timeout_ms_ssl : m_timeout_ms) || _connectionResult) break;
        vTaskDelay(10);
      }
      res = _connectionResult;
      if (_connectTaskHandle!=nullptr) {
        vTaskDelete(_connectTaskHandle);
        _connectTaskHandle = nullptr;
        AUDIO_INFO("WATCH DOG HAS FINISHED A WORK, BYE!");
      }
    }
#if IBT_NO_PSRAM
    lightmem::checkpoint("AFTER_CONNECT");
#if IBT_NO_PSRAM && defined(IBT_LIGHT4_DNS_AUTO)
    static_assert(CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN==16384,"Retain full TLS input record capacity");
    static_assert(CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN==16384,"Full fallback record capacity; confirmed MFL shrinks both directions");
    static_assert(CONFIG_MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH,"SDK-managed negotiated TLS buffer sizing");
#ifdef CONFIG_MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#error Light4 must release the verified peer certificate after handshake
#endif
    Serial.printf("[TLS-MEM-CONFIG] rx_max=%u tx_max=%u keep_peer=0 reserve=%u source=COMPILED_SDK_CONFIG\n",
        unsigned(CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN),unsigned(CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN),unsigned(ibtlimit::Policy::reserve));
#endif
    // The handshake's temporary workspace must not overlap the input/PCM rings.
    // Keep the existing decoder-first admission and the unchanged ring policy.
    if(res){
        Serial.printf("[STREAM-MEM] phase=AFTER_HANDSHAKE free=%u largest=%u\n",
            unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
            unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
        initInBuff();
        lightmem::checkpoint("AFTER_IO");
        res=InBuff.isInitialized() && m_outBuff && outputReady();
        Serial.printf("[STREAM-MEM] phase=AFTER_IO ok=%u free=%u largest=%u\n",unsigned(res),
            unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
            unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
        if(!res)setDefaults(false,false); // Close TLS and release partial allocation.
    }
#endif
    if(res){
        uint32_t dt = millis() - t;
        strcpy(m_lastHost, l_host);
        AUDIO_INFO("%s has been established in %lu ms, free Heap: %lu bytes",
                    m_f_ssl?"SSL":"Connection", dt, ESP.getFreeHeap());
        m_f_running = true;
    }

    m_expectedCodec = CODEC_NONE;
    m_expectedPlsFmt = FORMAT_NONE;

    if(res){
        _client->print(rqh);
        if(endsWith(extension, ".mp3"))   m_expectedCodec = CODEC_MP3;
        if(endsWith(extension, ".aac"))   m_expectedCodec = CODEC_AAC;
        if(endsWith(extension, ".wav"))   m_expectedCodec = CODEC_WAV;
        if(endsWith(extension, ".m4a"))   m_expectedCodec = CODEC_M4A;
        if(endsWith(extension, ".flac"))  m_expectedCodec = CODEC_FLAC;
        if(endsWith(extension,".mp2"))m_expectedCodec=CODEC_MPEG;
        if(endsWith(extension,".mp1"))m_expectedCodec=CODEC_MPEG;
        if(endsWith(extension,".ts"))m_expectedCodec=CODEC_TS;
        if(endsWith(extension,".awb"))m_expectedCodec=CODEC_AMRWB;
        if(endsWith(extension,".amr"))m_expectedCodec=CODEC_AMR;
        if(endsWith(extension,".opus"))m_expectedCodec=CODEC_OGG;
        if(endsWith(extension,".oga"))m_expectedCodec=CODEC_OGG;
        if(endsWith(extension,".ogg"))m_expectedCodec=CODEC_OGG;
        if(endsWith(extension, ".asx"))  m_expectedPlsFmt = FORMAT_ASX;
        if(endsWith(extension, ".m3u"))  m_expectedPlsFmt = FORMAT_M3U;
        if(endsWith(extension, ".m3u8")) m_expectedPlsFmt = FORMAT_M3U8;
        if(endsWith(extension, ".pls"))  m_expectedPlsFmt = FORMAT_PLS;

        setDatamode(HTTP_RESPONSE_HEADER);   // Handle header
        m_streamType = ST_WEBSTREAM;
    }
    else{
        AUDIO_INFO("Request %s failed!", l_host);
        AUDIO_ERROR("Request %s failed!", l_host);
        if(audio_showstation) audio_showstation("");
        if(audio_showstreamtitle) audio_showstreamtitle("");
        if(audio_icydescription) audio_icydescription("");
        if(audio_icyurl) audio_icyurl("");
        m_lastHost[0] = 0;
    }
    if(hostwoext) {free(hostwoext); hostwoext = NULL;}
    if(extension) {free(extension); extension = NULL;}
    if(l_host   ) {free(l_host);    l_host    = NULL;}
    if(h_host   ) {free(h_host);    h_host    = NULL;}
    return res;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::httpPrint(const char* host) {
    // user and pwd for authentification only, can be empty

    if(host == NULL) {
        AUDIO_INFO("Hostaddress is empty");
        return false;
    }

    char* h_host = NULL; // pointer of l_host without http:// or https://
    if(m_f_ssl) h_host = strdup(host + 8);
    else        h_host = strdup(host + 7);

    int16_t pos_slash;                                        // position of "/" in hostname
    int16_t pos_colon;                                        // position of ":" in hostname
    int16_t pos_ampersand;                                    // position of "&" in hostname
    uint16_t port = 80;                                       // port number

    // In the URL there may be an extension, like noisefm.ru:8000/play.m3u&t=.m3u
    pos_slash     = indexOf(h_host, "/", 0);
    pos_colon     = indexOf(h_host, ":", 0);
        if(isalpha(h_host[pos_colon + 1])) pos_colon = -1; // no portnumber follows
    pos_ampersand = indexOf(h_host, "&", 0);

    char *hostwoext = NULL;                                  // "skonto.ls.lv:8002" in "skonto.ls.lv:8002/mp3"
    char *extension = NULL;                                  // "/mp3" in "skonto.ls.lv:8002/mp3"

    if(pos_slash > 1) {
        hostwoext = (char*)malloc(pos_slash + 2);
        memcpy(hostwoext, h_host, pos_slash);
        hostwoext[pos_slash] = '\0';
        uint16_t extLen =  urlencode_expected_len(h_host + pos_slash);
        extension = (char *)malloc(extLen + 20);
        memcpy(extension, h_host + pos_slash, extLen);
        urlencode(extension, extLen, true);
    }
    else{  // url has no extension
        hostwoext = strdup(h_host);
        extension = strdup("/");
    }

    if((pos_colon >= 0) && ((pos_ampersand == -1) or (pos_ampersand > pos_colon))){
        port = atoi(h_host + pos_colon + 1);// Get portnumber as integer
        hostwoext[pos_colon] = '\0';// Host without portnumber
    }

    AUDIO_INFO("new request: \"%s\"", host);

    char rqh[strlen(h_host) + 200]; // http request header
    rqh[0] = '\0';

    strcat(rqh, "GET ");
    strcat(rqh, extension);
    strcat(rqh, " HTTP/1.1\r\n");
    strcat(rqh, "Host: ");
    strcat(rqh, hostwoext);
    strcat(rqh, "\r\n");
    strcat(rqh, "Accept-Encoding: identity;q=1,*;q=0\r\n");
    strcat(rqh, "User-Agent: Mozilla/5.0\r\n");
    strcat(rqh, "Connection: keep-alive\r\n\r\n");

    if(m_f_ssl){ _client = static_cast<WiFiClient*>(&clientsecure); if(port == 80) port = 443;}
    else       { _client = static_cast<WiFiClient*>(&client);}

    if(!_client->connected()){
        AUDIO_INFO("The host has disconnected, reconnecting");
        if(!radiodns::connect(_client, clientsecure, hostwoext, port, m_f_ssl ? m_timeout_ms_ssl : m_timeout_ms)){
            log_e("connection lost");
            stopSong();
            return false;
        }
    }
    _client->print(rqh);

    if(endsWith(extension, ".mp3"))   m_expectedCodec = CODEC_MP3;
    if(endsWith(extension, ".aac"))   m_expectedCodec = CODEC_AAC;
    if(endsWith(extension, ".wav"))   m_expectedCodec = CODEC_WAV;
    if(endsWith(extension, ".m4a"))   m_expectedCodec = CODEC_M4A;
    if(endsWith(extension, ".flac"))  m_expectedCodec = CODEC_FLAC;
        if(endsWith(extension,".mp2"))m_expectedCodec=CODEC_MPEG;
        if(endsWith(extension,".mp1"))m_expectedCodec=CODEC_MPEG;
        if(endsWith(extension,".ts"))m_expectedCodec=CODEC_TS;
        if(endsWith(extension,".awb"))m_expectedCodec=CODEC_AMRWB;
        if(endsWith(extension,".amr"))m_expectedCodec=CODEC_AMR;
        if(endsWith(extension,".opus"))m_expectedCodec=CODEC_OGG;
        if(endsWith(extension,".oga"))m_expectedCodec=CODEC_OGG;
        if(endsWith(extension,".ogg"))m_expectedCodec=CODEC_OGG;
    if(endsWith(extension, ".asx"))  m_expectedPlsFmt = FORMAT_ASX;
    if(endsWith(extension, ".m3u"))  m_expectedPlsFmt = FORMAT_M3U;
    if(endsWith(extension, ".m3u8")) m_expectedPlsFmt = FORMAT_M3U8;
    if(endsWith(extension, ".pls"))  m_expectedPlsFmt = FORMAT_PLS;

    setDatamode(HTTP_RESPONSE_HEADER);   // Handle header
    m_streamType = ST_WEBSTREAM;
    m_contentlength = 0;
    m_f_chunked = false;

    if(hostwoext) {free(hostwoext); hostwoext = NULL;}
    if(extension) {free(extension); extension = NULL;}
    if(h_host   ) {free(h_host);    h_host    = NULL;}
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setFileLoop(bool input){
    m_f_loop = input;
    return input;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::UTF8toASCII(char* str){

#ifdef SDFATFS_USED
    //UTF8->UTF16 (lowbyte)
    const uint8_t ascii[60] = {
    //129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148  // UTF8(C3)
    //                Ä    Å    Æ    Ç         É                                       Ñ                  // CHAR
      000, 000, 000, 0xC4, 143, 0xC6,0xC7, 000,0xC9,000, 000, 000, 000, 000, 000, 000, 0xD1, 000, 000, 000, // ASCII (Latin1)
    //149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168
    //      Ö                             Ü              ß    à                   ä    å    æ         è
      000, 0xD6,000, 000, 000, 000, 000, 0xDC, 000, 000, 0xDF,0xE0, 000, 000, 000,0xE4,0xE5,0xE6, 000,0xE8,
    //169, 170, 171, 172. 173. 174. 175, 176, 177, 179, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188
    //      ê    ë    ì         î    ï         ñ    ò         ô         ö              ù         û    ü
      000, 0xEA, 0xEB,0xEC, 000,0xEE,0xEB, 000,0xF1,0xF2, 000,0xF4, 000,0xF6, 000, 000,0xF9, 000,0xFB,0xFC};
#else
    const uint8_t ascii[60] = {
    //129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148  // UTF8(C3)
    //                Ä    Å    Æ    Ç         É                                       Ñ                  // CHAR
      000, 000, 000, 142, 143, 146, 128, 000, 144, 000, 000, 000, 000, 000, 000, 000, 165, 000, 000, 000, // ASCII
    //149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168
    //      Ö                             Ü              ß    à                   ä    å    æ         è
      000, 153, 000, 000, 000, 000, 000, 154, 000, 000, 225, 133, 000, 000, 000, 132, 134, 145, 000, 138,
    //169, 170, 171, 172. 173. 174. 175, 176, 177, 179, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188
    //      ê    ë    ì         î    ï         ñ    ò         ô         ö              ù         û    ü
      000, 136, 137, 141, 000, 140, 139, 000, 164, 149, 000, 147, 000, 148, 000, 000, 151, 000, 150, 129};
#endif

    uint16_t i = 0, j=0, s = 0;
    bool f_C3_seen = false;

    while(str[i] != 0) {                                     // convert UTF8 to ASCII
        if(str[i] == 195){                                   // C3
            i++;
            f_C3_seen = true;
            continue;
        }
        str[j] = str[i];
        if(str[j] > 128 && str[j] < 189 && f_C3_seen == true) {
            s = ascii[str[j] - 129];
            if(s != 0) str[j] = s;                         // found a related ASCII sign
            f_C3_seen = false;
        }
        i++; j++;
    }
    str[j] = 0;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::connecttoSD(const char* path, uint32_t resumeFilePos) {
    return connecttoFS(SD, path, resumeFilePos);
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::connecttoFS(fs::FS &fs, const char* path, uint32_t resumeFilePos) {
    return connecttoFSImpl(fs,path,resumeFilePos,true);
}

bool Audio::connecttoFSPrepared(fs::FS &fs, const char* path, uint32_t resumeFilePos) {
    // Only the audio owner after a completed setDefaults(). Never reuse a live decoder.
    if(m_f_running||m_codec!=CODEC_NONE||m_datamode!=AUDIO_NONE||audiofile)return false;
    return connecttoFSImpl(fs,path,resumeFilePos,false);
}

bool Audio::connecttoFSImpl(fs::FS &fs, const char* path, uint32_t resumeFilePos,bool reset) {
    const uint32_t connectStarted=micros();

    if(!path||!path[0]||strlen(path)>=ibtsd::PathCapacity-1)return false;
    if(!netprefetch::end())return false;
    const bool localFlac=IBT_NO_PSRAM&&dual::endsWithIgnoreCase(path,".flac");
    const bool earlyFile=IBT_NO_PSRAM&&!reset;
    if(reset){
        if(!setDefaults(!localFlac&&IBT_NO_PSRAM&&ibtaudio::prepareMp3First(path),!localFlac))return false;
    }
    struct FileGuard {fs::File& file;bool keep=false;~FileGuard(){if(!keep)file.close();}} guard{audiofile};
    auto fail=[&](const char* phase){Serial.printf("[SD-PREPARE-FAIL] phase=%s free=%u largest=%u input=%u pcm=%u output=%u\n",phase,unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(InBuff.isInitialized()),unsigned(m_outBuff!=nullptr),unsigned(outputReady()));return false;};
    String relative;const char* audioName=path;
    if(path[0]!='/'){if(!relative.concat("/")||!relative.concat(path))return fail("PATH");audioName=relative.c_str();}
    uint32_t openDone=0,openDuration=0;
    size_t flacInputCapacity=16384;
    const char* filePhase="FILE_OPEN";
    auto openFile=[&](){
        const uint32_t fileStarted=micros();
        if(audio_beginSDread)audio_beginSDread();
        audiofile=fs.open(audioName);
        if((earlyFile||localFlac)&&audiofile){
            // Configure stdio before its first I/O; don't let a late 4KiB cache
            // compete with the admitted codec. Warm file/cache resources now.
            filePhase="FILE_CACHE";
            if(!audiofile.setBufferSize(512))return false;
            filePhase="FILE_PROBE";
            uint8_t probe[42]{};
            const size_t count=audiofile.read(probe,localFlac?sizeof(probe):1);
            if(!audiofile.size()||!count)return false;
            // STREAMINFO max frame size is an optional 24-bit byte count.
            // Keep a bounded default when unknown; never truncate a known frame.
            if(localFlac&&count==sizeof(probe)&&!memcmp(probe,"fLaC",4)&&
               (probe[4]&0x7f)==0&&probe[5]==0&&probe[6]==0&&probe[7]==34){
                const uint32_t frame=(uint32_t(probe[15])<<16)|(uint32_t(probe[16])<<8)|probe[17];
                const auto plan=localflac::input(frame);
                if(!plan.supported){filePhase="FLAC_FRAME_LIMIT";return false;}
                flacInputCapacity=plan.capacity;
            }
            if(!audiofile.seek(0))return false;
        }
        openDone=micros();openDuration=openDone-fileStarted;
        Serial.printf("[SD-FILE-ADMIT] ok=%u early=%u stdio_bytes=%u flac_input=%u free=%u largest=%u\n",unsigned(bool(audiofile)),unsigned(earlyFile||localFlac),(earlyFile||localFlac)?512u:0u,localFlac?unsigned(flacInputCapacity):0u,unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
        return bool(audiofile);
    };
    if((earlyFile||localFlac)&&!openFile())return fail(filePhase);
    if(!reset||localFlac){
#if IBT_NO_PSRAM
        if(btaudio::outputSelected()&&ibtaudio::prepareMp3First(path)){
            // Allocate byte-addressed output/input before integer codec workspaces.
            // The unused final PCM granule is temporary reorder scratch, consumed
            // before synthesis writes that granule; no PCM survives into next decode.
            initInBuff(true);
            if(!m_outBuff||!InBuff.isInitialized())return fail("PCM_INPUT");
            static_assert(OutputBytes==4608,"Borrowed reorder layout requires full stereo PCM frame");
            if(!MP3Decoder_BorrowReorder(m_outBuff+1152,OutputBytes-2304))return fail("REORDER_BIND");
            if(!MP3Decoder_AllocateBuffers())return fail("MP3_WORKSPACE");
            Serial.printf("[SD-BT-MEM] input_bytes=2048 wrap_tail=0 pcm_bytes=4608 borrowed_reorder=792 saving=2392 free=%u largest=%u\n",
              unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
        }else if(ibtaudio::prepareMp3First(path)&&!MP3Decoder_AllocateBuffers())return fail("MP3_WORKSPACE");
#endif
        initInBuff(false,localFlac?flacInputCapacity:0);
    }
    m_resumeFilePos=resumeFilePos;
    const uint32_t defaultsDone=micros();
    if(!InBuff.isInitialized()||!outputReady()||!m_outBuff)return fail("IO_READY");
    if(!earlyFile&&!localFlac&&!openFile())return fail(filePhase);
    setDatamode(AUDIO_LOCALFILE);
    m_file_size = audiofile.size();//TEST loop
    const char* afn=audiofile.name();
    if(dual::endsWithIgnoreCase(afn,".mp3"))m_codec=CODEC_MP3;
    else if(dual::endsWithIgnoreCase(afn,".m4a"))m_codec=CODEC_M4A;
    else if(dual::endsWithIgnoreCase(afn,".aac"))m_codec=CODEC_AAC;
    else if(dual::endsWithIgnoreCase(afn,".wav"))m_codec=CODEC_WAV;
    else if(dual::endsWithIgnoreCase(afn,".flac"))m_codec=CODEC_FLAC;
    else if(dual::endsWithIgnoreCase(afn,".ogg")||dual::endsWithIgnoreCase(afn,".oga")||dual::endsWithIgnoreCase(afn,".opus"))m_codec=CODEC_OGG;
    else if(dual::endsWithIgnoreCase(afn,".amr"))m_codec=CODEC_AMR;
    else if(dual::endsWithIgnoreCase(afn,".awb"))m_codec=CODEC_AMRWB;
    else if(dual::endsWithIgnoreCase(afn,".ts"))m_codec=CODEC_TS;
    else if(dual::endsWithIgnoreCase(afn,".mp1")||dual::endsWithIgnoreCase(afn,".mp2"))m_codec=CODEC_MPEG;
    if(m_codec==CODEC_WAV&&extraWavFile())m_codec=CODEC_EXTRA_WAV;
    if(m_codec==CODEC_M4A&&!extraM4aFile()){audiofile.close();return false;}
    if(m_codec==CODEC_AMR){uint8_t h[9];const int n=audiofile.read(h,sizeof(h));audiofile.seek(0);if(n==9&&!memcmp(h,"#!AMR-WB\n",9))m_codec=CODEC_AMRWB;}
    if(m_codec==CODEC_NONE){audiofile.close();return false;}
    const uint32_t decoderStarted=micros();
    bool ret = initializeDecoder();
    // The compact local input has no wrap tail. Expose its actual bounded
    // decoder window; the Wi-Fi/PSRAM FLAC maximum remains unchanged.
    if(ret&&localFlac)InBuff.changeMaxBlockSize(uint16_t(flacInputCapacity-1));
    Serial.printf("[SD-OPEN] ok=%u defaults_us=%lu open_us=%lu probe_decoder_us=%lu total_us=%lu\n",ret?1:0,
        (unsigned long)(defaultsDone-connectStarted),(unsigned long)openDuration,
        (unsigned long)(micros()-decoderStarted),(unsigned long)(micros()-connectStarted));
    if(ret) m_f_running = true;
    else {
      audiofile.close();
    }
    guard.keep=ret;
    return ret;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::connecttospeech(const char* speech, const char* lang){

    if(!netprefetch::end())return false;
    setDefaults();
    if(!InBuff.isInitialized() || !outputReady() || !m_outBuff)return false;
    char host[] = "translate.google.com.vn";
    char path[] = "/translate_tts";

    uint16_t speechLen = strlen(speech);
    uint16_t speechBuffLen = speechLen + 300;
    memcpy(m_lastHost, speech, 256);
    char* speechBuff = (char*)malloc(speechBuffLen);
    if(!speechBuff) {log_e("out of memory"); return false;}
    memcpy(speechBuff, speech, speechLen);
    speechBuff[speechLen] = '\0';
    urlencode(speechBuff, speechBuffLen);

    char resp[strlen(speechBuff) + 200] = "";
    strcat(resp, "GET ");
    strcat(resp, path);
    strcat(resp, "?ie=UTF-8&tl=");
    strcat(resp, lang);
    strcat(resp, "&client=tw-ob&q=");
    strcat(resp, speechBuff);
    strcat(resp, " HTTP/1.1\r\n");
    strcat(resp, "Host: ");
    strcat(resp, host);
    strcat(resp, "\r\n");
    strcat(resp, "User-Agent: Mozilla/5.0 \r\n");
    strcat(resp, "Accept-Encoding: identity\r\n");
    strcat(resp, "Accept: text/html\r\n");
    strcat(resp, "Connection: close\r\n\r\n");

    if(speechBuff){free(speechBuff); speechBuff = NULL;}
    _client = static_cast<WiFiClient*>(&client);
    if(!radiodns::connect(_client, clientsecure, host, 80, m_timeout_ms)) {
        log_e("Connection failed");
        return false;
    }
    _client->print(resp);

    m_streamType = ST_WEBSTREAM;
    m_f_running = true;
    m_f_ssl = false;
    m_f_tts = true;
    setDatamode(HTTP_RESPONSE_HEADER);

    return true;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::connecttomarytts(const char* speech, const char* lang, const char* voice){

    //lang:     fr, te, ru, en_US, en_GB, sv, lb, tr, de, it

    //voice:    upmc-pierre-hsmm             fr male hmm
    //          upmc-pierre                  fr male unitselection general
    //          upmc-jessica-hsmm            fr female hmm
    //          upmc-jessica                 fr female unitselection general
    //          marylux                      lb female unitselection general
    //          istc-lucia-hsmm              it female hmm
    //          enst-dennys-hsmm             fr male hmm
    //          enst-camille-hsmm            fr female hmm
    //          enst-camille                 fr female unitselection general
    //          dfki-spike-hsmm              en_GB male hmm
    //          dfki-spike                   en_GB male unitselection general
    //          dfki-prudence-hsmm           en_GB female hmm
    //          dfki-prudence                en_GB female unitselection general
    //          dfki-poppy-hsmm              en_GB female hmm
    //          dfki-poppy                   en_GB female unitselection general
    //          dfki-pavoque-styles          de male unitselection general
    //          dfki-pavoque-neutral-hsmm    de male hmm
    //          dfki-pavoque-neutral         de male unitselection general
    //          dfki-ot-hsmm                 tr male hmm
    //          dfki-ot                      tr male unitselection general
    //          dfki-obadiah-hsmm            en_GB male hmm
    //          dfki-obadiah                 en_GB male unitselection general
    //          cmu-slt-hsmm                 en_US female hmm
    //          cmu-slt                      en_US female unitselection general
    //          cmu-rms-hsmm                 en_US male hmm
    //          cmu-rms                      en_US male unitselection general
    //          cmu-nk-hsmm                  te female hmm
    //          cmu-bdl-hsmm                 en_US male hmm
    //          cmu-bdl                      en_US male unitselection general
    //          bits4                        de female unitselection general
    //          bits3-hsmm                   de male hmm
    //          bits3                        de male unitselection general
    //          bits2                        de male unitselection general
    //          bits1-hsmm                   de female hmm
    //          bits1                        de female unitselection general

    if(!netprefetch::end())return false;
    setDefaults();
    if(!InBuff.isInitialized() || !outputReady() || !m_outBuff)return false;
    char host[] = "mary.dfki.de";
    char path[] = "/process";
    int port = 59125;

    uint16_t speechLen = strlen(speech);
    uint16_t speechBuffLen = speechLen + 300;
    memcpy(m_lastHost, speech, 256);
    char* speechBuff = (char*)malloc(speechBuffLen);
    if(!speechBuff) {log_e("out of memory"); return false;}
    memcpy(speechBuff, speech, speechLen);
    speechBuff[speechLen] = '\0';
    urlencode(speechBuff, speechBuffLen);

    char resp[strlen(speechBuff) + 200] = "";
    strcat(resp, "GET ");
    strcat(resp, path);
    strcat(resp, "?INPUT_TEXT=");
    strcat(resp, speechBuff);
    strcat(resp, "&INPUT_TYPE=TEXT");
    strcat(resp, "&OUTPUT_TYPE=AUDIO");
    strcat(resp, "&AUDIO=WAVE_FILE");
    strcat(resp, "&LOCALE=");
    strcat(resp, lang);
    strcat(resp, "&VOICE=");
    strcat(resp, voice);
    strcat(resp, " HTTP/1.1\r\n");
    strcat(resp, "Host: ");
    strcat(resp, host);
    strcat(resp, "\r\n");
    strcat(resp, "User-Agent: Mozilla/5.0 \r\n");
    strcat(resp, "Accept-Encoding: identity\r\n");
    strcat(resp, "Accept: text/html\r\n");
    strcat(resp, "Connection: close\r\n\r\n");

    if(speechBuff){free(speechBuff); speechBuff = NULL;}
    _client = static_cast<WiFiClient*>(&client);
    if(!radiodns::connect(_client, clientsecure, host, port, m_f_ssl ? m_timeout_ms_ssl : m_timeout_ms)) {
        log_e("Connection failed");
        return false;
    }
    _client->print(resp);

    m_streamType = ST_WEBSTREAM;
    m_f_running = true;
    m_f_ssl = false;
    m_f_tts = true;
    setDatamode(HTTP_RESPONSE_HEADER);

    return true;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::urlencode(char* buff, uint16_t buffLen, bool spacesOnly) {

    uint16_t len = strlen(buff);
    uint8_t* tmpbuff = (uint8_t*)malloc(buffLen);
    if(!tmpbuff) {log_e("out of memory"); return;}
    char c;
    char code0;
    char code1;
    uint16_t j = 0;
    for(int i = 0; i < len; i++) {
        c = buff[i];
        if(isalnum(c)) tmpbuff[j++] = c;
        else if(spacesOnly){
            if(c == ' '){
                tmpbuff[j++] = '%';
                tmpbuff[j++] = '2';
                tmpbuff[j++] = '0';
            }
            else{
                tmpbuff[j++] = c;
            }
        }
        else {
            code1 = (c & 0xf) + '0';
            if((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if(c > 9) code0 = c - 10 + 'A';
            tmpbuff[j++] = '%';
            tmpbuff[j++] = code0;
            tmpbuff[j++] = code1;
        }
        if(j == buffLen - 1){
            log_e("out of memory");
            break;
        }
    }
    memcpy(buff, tmpbuff, j);
    buff[j] ='\0';
    free(tmpbuff);
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::showID3Tag(const char* tag, const char* value){

    chbuf[0] = 0;
    // V2.2
    if(!strcmp(tag, "CNT")) sprintf(chbuf, "Play counter: %s", value);
    // if(!strcmp(tag, "COM")) sprintf(chbuf, "Comments: %s", value);
    if(!strcmp(tag, "CRA")) sprintf(chbuf, "Audio encryption: %s", value);
    if(!strcmp(tag, "CRM")) sprintf(chbuf, "Encrypted meta frame: %s", value);
    if(!strcmp(tag, "ETC")) sprintf(chbuf, "Event timing codes: %s", value);
    if(!strcmp(tag, "EQU")) sprintf(chbuf, "Equalization: %s", value);
    if(!strcmp(tag, "IPL")) sprintf(chbuf, "Involved people list: %s", value);
    if(!strcmp(tag, "PIC")) sprintf(chbuf, "Attached picture: %s", value);
    if(!strcmp(tag, "SLT")) sprintf(chbuf, "Synchronized lyric/text: %s", value);
    // if(!strcmp(tag, "TAL")) sprintf(chbuf, "Album/Movie/Show title: %s", value);
    if(!strcmp(tag, "TBP")) sprintf(chbuf, "BPM (Beats Per Minute): %s", value);
    if(!strcmp(tag, "TCM")) sprintf(chbuf, "Composer: %s", value);
    if(!strcmp(tag, "TCO")) sprintf(chbuf, "Content type: %s", value);
    if(!strcmp(tag, "TCR")) sprintf(chbuf, "Copyright message: %s", value);
    if(!strcmp(tag, "TDA")) sprintf(chbuf, "Date: %s", value);
    if(!strcmp(tag, "TDY")) sprintf(chbuf, "Playlist delay: %s", value);
    if(!strcmp(tag, "TEN")) sprintf(chbuf, "Encoded by: %s", value);
    if(!strcmp(tag, "TFT")) sprintf(chbuf, "File type: %s", value);
    if(!strcmp(tag, "TIM")) sprintf(chbuf, "Time: %s", value);
    if(!strcmp(tag, "TKE")) sprintf(chbuf, "Initial key: %s", value);
    if(!strcmp(tag, "TLA")) sprintf(chbuf, "Language(s): %s", value);
    if(!strcmp(tag, "TLE")) sprintf(chbuf, "Length: %s", value);
    if(!strcmp(tag, "TMT")) sprintf(chbuf, "Media type: %s", value);
    if(!strcmp(tag, "TOA")) sprintf(chbuf, "Original artist(s)/performer(s): %s", value);
    if(!strcmp(tag, "TOF")) sprintf(chbuf, "Original filename: %s", value);
    if(!strcmp(tag, "TOL")) sprintf(chbuf, "Original Lyricist(s)/text writer(s): %s", value);
    if(!strcmp(tag, "TOR")) sprintf(chbuf, "Original release year: %s", value);
    if(!strcmp(tag, "TOT")) sprintf(chbuf, "Original album/Movie/Show title: %s", value);
    if(!strcmp(tag, "TP1")) sprintf(chbuf, "Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group: %s", value);
    if(!strcmp(tag, "TP2")) sprintf(chbuf, "Band/Orchestra/Accompaniment: %s", value);
    if(!strcmp(tag, "TP3")) sprintf(chbuf, "Conductor/Performer refinement: %s", value);
    if(!strcmp(tag, "TP4")) sprintf(chbuf, "Interpreted, remixed, or otherwise modified by: %s", value);
    if(!strcmp(tag, "TPA")) sprintf(chbuf, "Part of a set: %s", value);
    if(!strcmp(tag, "TPB")) sprintf(chbuf, "Publisher: %s", value);
    if(!strcmp(tag, "TRC")) sprintf(chbuf, "ISRC (International Standard Recording Code): %s", value);
    if(!strcmp(tag, "TRD")) sprintf(chbuf, "Recording dates: %s", value);
    if(!strcmp(tag, "TRK")) sprintf(chbuf, "Track number/Position in set: %s", value);
    if(!strcmp(tag, "TSI")) sprintf(chbuf, "Size: %s", value);
    if(!strcmp(tag, "TSS")) sprintf(chbuf, "Software/hardware and settings used for encoding: %s", value);
    if(!strcmp(tag, "TT1")) sprintf(chbuf, "Content group description: %s", value);
    if(!strcmp(tag, "TT2")) sprintf(chbuf, "Title/Songname/Content description: %s", value);
    if(!strcmp(tag, "TT3")) sprintf(chbuf, "Subtitle/Description refinement: %s", value);
    if(!strcmp(tag, "TXT")) sprintf(chbuf, "Lyricist/text writer: %s", value);
    if(!strcmp(tag, "TXX")) sprintf(chbuf, "User defined text information frame: %s", value);
    if(!strcmp(tag, "TYE")) sprintf(chbuf, "Year: %s", value);
    if(!strcmp(tag, "UFI")) sprintf(chbuf, "Unique file identifier: %s", value);
    if(!strcmp(tag, "ULT")) sprintf(chbuf, "Unsychronized lyric/text transcription: %s", value);
    if(!strcmp(tag, "WAF")) sprintf(chbuf, "Official audio file webpage: %s", value);
    if(!strcmp(tag, "WAR")) sprintf(chbuf, "Official artist/performer webpage: %s", value);
    if(!strcmp(tag, "WAS")) sprintf(chbuf, "Official audio source webpage: %s", value);
    if(!strcmp(tag, "WCM")) sprintf(chbuf, "Commercial information: %s", value);
    if(!strcmp(tag, "WCP")) sprintf(chbuf, "Copyright/Legal information: %s", value);
    if(!strcmp(tag, "WPB")) sprintf(chbuf, "Publishers official webpage: %s", value);
    if(!strcmp(tag, "WXX")) sprintf(chbuf, "User defined URL link frame: %s", value);

    // V2.3 V2.4 tags
    // if(!strcmp(tag, "COMM")) sprintf(chbuf, "Comment: %s", value);
    if(!strcmp(tag, "OWNE")) sprintf(chbuf, "Ownership: %s", value);
    // if(!strcmp(tag, "PRIV")) sprintf(chbuf, "Private: %s", value);
    if(!strcmp(tag, "SYLT")) sprintf(chbuf, "SynLyrics: %s", value);
    if(!strcmp(tag, "TALB")) { sprintf(chbuf, "Album: %s", value); if(audio_id3album) audio_id3album(value); }
    if(!strcmp(tag, "TBPM")) sprintf(chbuf, "BeatsPerMinute: %s", value);
    if(!strcmp(tag, "TCMP")) sprintf(chbuf, "Compilation: %s", value);
    if(!strcmp(tag, "TCOM")) sprintf(chbuf, "Composer: %s", value);
    if(!strcmp(tag, "TCON")) sprintf(chbuf, "ContentType: %s", value);
    if(!strcmp(tag, "TCOP")) sprintf(chbuf, "Copyright: %s", value);
    if(!strcmp(tag, "TDAT")) sprintf(chbuf, "Date: %s", value);
    if(!strcmp(tag, "TEXT")) sprintf(chbuf, "Lyricist: %s", value);
    if(!strcmp(tag, "TIME")) sprintf(chbuf, "Time: %s", value);
    if(!strcmp(tag, "TIT1")) sprintf(chbuf, "Grouping: %s", value);
    if(!strcmp(tag, "TIT2")) { sprintf(chbuf, "Title: %s", value); if(audio_id3title) audio_id3title(value); }
    if(!strcmp(tag, "TIT3")) sprintf(chbuf, "Subtitle: %s", value);
    if(!strcmp(tag, "TLAN")) sprintf(chbuf, "Language: %s", value);
    if(!strcmp(tag, "TLEN")) sprintf(chbuf, "Length (ms): %s", value);
    if(!strcmp(tag, "TMED")) sprintf(chbuf, "Media: %s", value);
    if(!strcmp(tag, "TOAL")) sprintf(chbuf, "OriginalAlbum: %s", value);
    if(!strcmp(tag, "TOPE")) sprintf(chbuf, "OriginalArtist: %s", value);
    if(!strcmp(tag, "TORY")) sprintf(chbuf, "OriginalReleaseYear: %s", value);
    if(!strcmp(tag, "TPE1")) { sprintf(chbuf, "Artist: %s", value); if(audio_id3artist) audio_id3artist(value); }
    if(!strcmp(tag, "TPE2")) sprintf(chbuf, "Band: %s", value);
    if(!strcmp(tag, "TPE3")) sprintf(chbuf, "Conductor: %s", value);
    if(!strcmp(tag, "TPE4")) sprintf(chbuf, "InterpretedBy: %s", value);
    if(!strcmp(tag, "TPOS")) sprintf(chbuf, "PartOfSet: %s", value);
    if(!strcmp(tag, "TPUB")) sprintf(chbuf, "Publisher: %s", value);
    if(!strcmp(tag, "TRCK")) sprintf(chbuf, "Track: %s", value);
    if(!strcmp(tag, "TSSE")) sprintf(chbuf, "SettingsForEncoding: %s", value);
    if(!strcmp(tag, "TRDA")) sprintf(chbuf, "RecordingDates: %s", value);
    if(!strcmp(tag, "TXXX")) sprintf(chbuf, "UserDefinedText: %s", value);
    if(!strcmp(tag, "TYER")) sprintf(chbuf, "Year: %s", value);
    if(!strcmp(tag, "USER")) sprintf(chbuf, "TermsOfUse: %s", value);
    if(!strcmp(tag, "USLT")) sprintf(chbuf, "Lyrics: %s", value);
    if(!strcmp(tag, "WOAR")) sprintf(chbuf, "OfficialArtistWebpage: %s", value);
    if(!strcmp(tag, "XDOR")) sprintf(chbuf, "OriginalReleaseTime: %s", value);

    latinToUTF8(chbuf, sizeof(chbuf));
    if(chbuf[0] != 0) if(audio_id3data) audio_id3data(chbuf);
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::unicode2utf8(char* buff, uint32_t len){
    // converts unicode in UTF-8, buff contains the string to be converted up to len
    // range U+1 ... U+FFFF
    uint8_t* tmpbuff = (uint8_t*)malloc(len * 2);
    if(!tmpbuff) {log_e("out of memory"); return;}
    bool bitorder = false;
    uint16_t j = 0;
    uint16_t k = 0;
    uint16_t m = 0;
    uint8_t uni_h = 0;
    uint8_t uni_l = 0;

    while(m < len - 1) {
        if((buff[m] == 0xFE) && (buff[m + 1] == 0xFF)) {
            bitorder = true;
            j = m + 2;
        }  // LSB/MSB
        if((buff[m] == 0xFF) && (buff[m + 1] == 0xFE)) {
            bitorder = false;
            j = m + 2;
        }  // MSB/LSB
        m++;
    } // seek for last bitorder
    m = 0;
    if(j > 0) {
        for(k = j; k < len; k += 2) {
            if(bitorder == true) {
                uni_h = (uint8_t)buff[k];
                uni_l = (uint8_t)buff[k + 1];
            }
            else {
                uni_l = (uint8_t)buff[k];
                uni_h = (uint8_t)buff[k + 1];
            }

            uint16_t uni_hl = ((uni_h << 8) | uni_l);

            if (uni_hl < 0X80){
                tmpbuff[m] = uni_l;
                m++;
            }
            else if (uni_hl < 0X800) {
                tmpbuff[m]= ((uni_hl >> 6) | 0XC0);
                m++;
                tmpbuff[m] =((uni_hl & 0X3F) | 0X80);
                m++;
            }
            else {
                tmpbuff[m] = ((uni_hl >> 12) | 0XE0);
                m++;
                tmpbuff[m] = (((uni_hl >> 6) & 0X3F) | 0X80);
                m++;
                tmpbuff[m] = ((uni_hl & 0X3F) | 0X80);
                m++;
            }
        }
    }
    buff[m] = 0;
    memcpy(buff, tmpbuff, m);
    if(tmpbuff){free(tmpbuff); tmpbuff = NULL;}
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::latinToUTF8(char* buff, size_t bufflen){
    // most stations send  strings in UTF-8 but a few sends in latin. To standardize this, all latin strings are
    // converted to UTF-8. If UTF-8 is already present, nothing is done and true is returned.
    // A conversion to UTF-8 extends the string. Therefore it is necessary to know the buffer size. If the converted
    // string does not fit into the buffer, false is returned
    // utf8 bytelength: >=0xF0 3 bytes, >=0xE0 2 bytes, >=0xC0 1 byte, e.g. e293ab is ⓫

    uint16_t pos = 0;
    uint8_t  ext_bytes = 0;
    uint16_t len = strlen(buff);
    uint8_t  c;

    while(pos < len){
        c = buff[pos];
        if(c >= 0xC2) {    // is UTF8 char
            pos++;
            if(c >= 0xC0 && buff[pos] < 0x80) {ext_bytes++; pos++;}
            if(c >= 0xE0 && buff[pos] < 0x80) {ext_bytes++; pos++;}
            if(c >= 0xF0 && buff[pos] < 0x80) {ext_bytes++; pos++;}
        }
        else pos++;
    }
    if(!ext_bytes) return true; // is UTF-8, do nothing

    pos = 0;

    while(buff[pos] != 0){
        len = strlen(buff);
        if(buff[pos] >= 0x80 && buff[pos+1] < 0x80){       // is not UTF8, is latin?
            for(int i = len+1; i > pos; i--){
                buff[i+1] = buff[i];
            }
            uint8_t c = buff[pos];
            buff[pos++] = 0xc0 | ((c >> 6) & 0x1f);      // 2+1+5 bits
            buff[pos++] = 0x80 | ((char)c & 0x3f);       // 1+1+6 bits
        }
        pos++;
        if(pos > bufflen -3){
            buff[bufflen -1] = '\0';
            return false; // do not overwrite
        }
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
size_t Audio::readAudioHeader(uint32_t bytes){
    if(InBuff.segmentedInput())bytes=min(bytes,uint32_t(InBuff.contiguousReadSpace()));
    if(extraDecoder.active()){m_controlCounter=100;eofHeader=true;return 0;}
    size_t bytesReaded = 0;
    if(m_codec == CODEC_WAV){
        int res = read_WAV_Header(InBuff.getReadPtr(), bytes);
        if(res >= 0) bytesReaded = res;
        else{ // error, skip header
            m_controlCounter = 100;
            eofHeader = true;
        }
    }
    if(m_codec == CODEC_MP3){
        int res = read_ID3_Header(InBuff.getReadPtr(), bytes);
        if(res >= 0) bytesReaded = res;
        else{ // error, skip header
            m_controlCounter = 100;
            eofHeader = true;
        }
    }
    if(m_codec == CODEC_M4A){
        int res = read_M4A_Header(InBuff.getReadPtr(), bytes);
        if(res >= 0) bytesReaded = res;
        else{ // error, skip header
            m_controlCounter = 100;
            eofHeader = true;
        }
    }
    if(m_codec == CODEC_AAC){
        // stream only, no header
        m_audioDataSize = getFileSize();
        m_controlCounter = 100;
        eofHeader = true;
    }
    if(m_codec == CODEC_FLAC){
        int res = read_FLAC_Header(InBuff.getReadPtr(), bytes);
        if(res >= 0) bytesReaded = res;
        else{ // error, skip header
            stopSong();
            m_controlCounter = 100;
            eofHeader = true;
        }
    }
    if(!isRunning()){
        log_e("Processing stopped due to invalid audio header");
        return 0;
    }
    return bytesReaded;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::read_WAV_Header(uint8_t* data, size_t len) {
    if(len<4)return 0;
    static size_t headerSize;
    static uint32_t cs = 0;
    static uint8_t bts = 0;

    if(m_controlCounter == 0){
        m_controlCounter ++;
        if((*data != 'R') || (*(data + 1) != 'I') || (*(data + 2) != 'F') || (*(data + 3) != 'F')) {
            AUDIO_INFO("file has no RIFF tag");
            headerSize = 0;
            return -1; //false;
        }
        else{
            headerSize = 4;
            return 4; // ok
        }
    }

    if(m_controlCounter == 1){
        m_controlCounter ++;
        cs = (uint32_t) (*data + (*(data + 1) << 8) + (*(data + 2) << 16) + (*(data + 3) << 24) - 8);
        headerSize += 4;
        return 4; // ok
    }

    if(m_controlCounter == 2){
        m_controlCounter ++;
        if((*data  != 'W') || (*(data + 1) != 'A') || (*(data + 2) != 'V') || (*(data + 3) != 'E')) {
            AUDIO_INFO("format tag is not WAVE");
            return -1;//false;
        }
        else {
            headerSize += 4;
            return 4;
        }
    }

    if(m_controlCounter == 3){
        if((*data  == 'f') && (*(data + 1) == 'm') && (*(data + 2) == 't')) {
            m_controlCounter ++;
            headerSize += 4;
            return 4;
        }
        else{
            headerSize += 4;
            return 4;
        }
    }

    if(m_controlCounter == 4){
        m_controlCounter ++;
        cs = uint32_t(data[0]) | (uint32_t(data[1]) << 8) |
             (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
        if(cs < 16 || cs > 40) return -1; // Reject before subtracting or reading fmt fields.
        bts = cs - 16; // bytes to skip if fmt chunk is >16
        headerSize += 4;
        return 4;
    }

    if(m_controlCounter == 5){
        if(len < cs) return 0; // Wait for the entire fmt chunk, including extensions.
        m_controlCounter ++;
        uint16_t fc  = (uint16_t) (*(data + 0)  + (*(data + 1)  << 8));         // Format code
        uint16_t nic = (uint16_t) (*(data + 2)  + (*(data + 3)  << 8));         // Number of interleaved channels
        uint32_t sr  = (uint32_t) (*(data + 4)  + (*(data + 5)  << 8) +
                                  (*(data + 6)  << 16) + (*(data + 7)  << 24)); // Samplerate
        uint32_t dr  = (uint32_t) (*(data + 8)  + (*(data + 9)  << 8) +
                                  (*(data + 10) << 16) + (*(data + 11) << 24)); // Datarate
        uint16_t dbs = (uint16_t) (*(data + 12) + (*(data + 13) << 8));         // Data block size
        uint16_t bps = (uint16_t) (*(data + 14) + (*(data + 15) << 8));         // Bits per sample

        AUDIO_INFO("FormatCode: %u", fc);
        // AUDIO_INFO("Channel: %u", nic);
        // AUDIO_INFO("SampleRate: %u", sr);
        AUDIO_INFO("DataRate: %lu", dr);
        AUDIO_INFO("DataBlockSize: %u", dbs);
        AUDIO_INFO("BitsPerSample: %u", bps);

        uint16_t format=fc;
        if(fc==0xfffe)format=wavfmt::subformat(data,cs,bps);
        const bool floating=format==3;
        if((floating?(bps!=32&&bps!=64):(bps!=8&&bps!=16&&bps!=24&&bps!=32))){
            Serial.printf("[SD-WAV] rejected=unsupported_bits format=%u bits=%u channels=%u\n",fc,bps,nic);
            AUDIO_INFO("BitsPerSample is %u,  must be 8 or 16" , bps);
            stopSong();
            return -1;
        }
        if((nic != 1) && (nic != 2)){
            AUDIO_INFO("num channels is %u,  must be 1 or 2" , nic); audio_info(chbuf);
            stopSong();
            return -1;
        }
        // WAVE_FORMAT_EXTENSIBLE is PCM only with the exact PCM subformat GUID.
        if((format!=1 && format!=3) || dbs!=(bps/8)*nic || !sr || uint64_t(sr)*dbs!=dr) {
            Serial.printf("[SD-WAV] rejected=unsupported_format format=%u bits=%u channels=%u fmt_bytes=%lu\n",fc,bps,nic,(unsigned long)cs);
            AUDIO_INFO("format code is not 1 (PCM)");
            stopSong();
            return -1 ; //false;
        }
        m_wavSourceBits=bps;m_wavFloat=floating;
        setBitsPerSample(16); // DAC and native A2DP consume normalized signed PCM16.
        Serial.printf("[SD-WAV] accepted=format%u source_bits=%u output_bits=16 channels=%u rate=%lu\n",format,bps,nic,(unsigned long)sr);
        setChannels(nic);
        setSampleRate(sr);
        setBitrate(nic * sr * bps);
    //    AUDIO_INFO("BitRate: %u", m_bitRate);
        headerSize += 16;
        return 16; // ok
    }

    if(m_controlCounter == 6){
        m_controlCounter ++;
        headerSize += bts;
        return bts; // skip to data
    }

    if(m_controlCounter == 7){
        if((*(data + 0) == 'd') && (*(data + 1) == 'a') && (*(data + 2) == 't') && (*(data + 3) == 'a')){
            m_controlCounter ++;
            vTaskDelay(30);
            headerSize += 4;
            return 4;
        }
        else{
            headerSize ++;
            return 1;
        }
    }

    if(m_controlCounter == 8){
        m_controlCounter ++;
        size_t cs =  *(data + 0) + (*(data + 1) << 8) + (*(data + 2) << 16) + (*(data + 3) << 24); //read chunkSize
        headerSize += 4;
        if(getDatamode() == AUDIO_LOCALFILE) m_contentlength = getFileSize();
        if(cs){
            m_audioDataSize = cs; // RIFF data chunk length excludes its header already.
        }
        else { // sometimes there is nothing here
            if(getDatamode() == AUDIO_LOCALFILE) m_audioDataSize = getFileSize() - headerSize;
            if(m_streamType == ST_WEBFILE) m_audioDataSize = m_contentlength - headerSize;
        }
        const unsigned frameBytes=(m_wavSourceBits/8)*getChannels();
        if(!frameBytes || m_audioDataSize%frameBytes){
            Serial.println("[SD-WAV] rejected=partial_frame");stopSong();return -1;
        }
        AUDIO_INFO("Audio-Length: %u", m_audioDataSize);
        if(audio_progress) audio_progress(headerSize, m_audioDataSize);
        return 4;
    }
    m_controlCounter = 100; // header succesfully read
    eofHeader = true;
    m_audioDataStart = headerSize;
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::read_FLAC_Header(uint8_t *data, size_t len) {
    static size_t headerSize;
    static size_t retvalue;
    static bool   f_lastMetaBlock;

    if(retvalue) {
        if(retvalue > len) { // if returnvalue > bufferfillsize
            if(len > InBuff.getMaxBlockSize()) len = InBuff.getMaxBlockSize();
            retvalue -= len; // and wait for more bufferdata
            return len;
        }
        else {
            size_t tmp = retvalue;
            retvalue = 0;
            return tmp;
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_BEGIN) {  // init
        headerSize = 0;
        retvalue = 0;
        m_audioDataStart = 0;
        f_lastMetaBlock = false;
        m_controlCounter = FLAC_MAGIC;
        if(getDatamode() == AUDIO_LOCALFILE){
            m_contentlength = getFileSize();
            AUDIO_INFO("Content-Length: %lu", m_contentlength);
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_MAGIC) { /* check MAGIC STRING */
        if(specialIndexOf(data, "fLaC", 10) != 0) {
            log_e("Magic String 'fLaC' not found in header");
            stopSong();
            return -1;
        }
        m_controlCounter = FLAC_MBH;
        headerSize = 4;
        retvalue = 4;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_MBH) { /* METADATA_BLOCK_HEADER */
        uint8_t blockType = *data;
        if(!f_lastMetaBlock){
            if(blockType & 128) {f_lastMetaBlock = true;}
            blockType &= 127;
            if(blockType == 0) m_controlCounter = FLAC_SINFO;
            if(blockType == 1) m_controlCounter = FLAC_PADDING;
            if(blockType == 2) m_controlCounter = FLAC_APP;
            if(blockType == 3) m_controlCounter = FLAC_SEEK;
            if(blockType == 4) m_controlCounter = FLAC_VORBIS;
            if(blockType == 5) m_controlCounter = FLAC_CUESHEET;
            if(blockType == 6) m_controlCounter = FLAC_PICTURE;
            headerSize += 1;
            retvalue = 1;
            return 0;
        }
        m_controlCounter = FLAC_OKAY;
        eofHeader = true;
        m_audioDataStart = headerSize;
        m_audioDataSize = m_contentlength - m_audioDataStart;
        AUDIO_INFO("Audio-Length: %u", m_audioDataSize);
        if(audio_progress) audio_progress(m_audioDataStart, m_audioDataSize);
        retvalue = 0;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_SINFO) { /* Stream info block */
        size_t l = bigEndian(data, 3);
        vTaskDelay(2);
        m_flacMaxBlockSize = bigEndian(data + 5, 2);
        AUDIO_INFO("FLAC maxBlockSize: %u", m_flacMaxBlockSize);
        vTaskDelay(2);
        m_flacMaxFrameSize = bigEndian(data + 10, 3);
        if(m_flacMaxFrameSize){
            AUDIO_INFO("FLAC maxFrameSize: %u", m_flacMaxFrameSize);
        }
        else {
            AUDIO_INFO("FLAC maxFrameSize: N/A");
        }
        if(m_flacMaxFrameSize > InBuff.getMaxBlockSize()) {
            log_e("FLAC maxFrameSize too large!");
            stopSong();
            return -1;
        }
//        InBuff.changeMaxBlockSize(m_flacMaxFrameSize);
        vTaskDelay(2);
        uint32_t nextval = bigEndian(data + 13, 3);
        m_flacSampleRate = nextval >> 4;
        AUDIO_INFO("FLAC sampleRate: %lu", m_flacSampleRate);
        vTaskDelay(2);
        m_flacNumChannels = ((nextval & 0x06) >> 1) + 1;
        AUDIO_INFO("FLAC numChannels: %u", m_flacNumChannels);
        vTaskDelay(2);
        uint8_t bps = (nextval & 0x01) << 4;
        bps += (*(data +16) >> 4) + 1;
        m_flacBitsPerSample = bps;
        if(bps<8 || bps>16){
            log_e("bits per sample must be8..16, is %i", bps);
            stopSong();
            return -1;
        }
        AUDIO_INFO("FLAC bitsPerSample: %u", m_flacBitsPerSample);
        m_flacTotalSamplesInStream = bigEndian(data + 17, 4);
        if(m_flacTotalSamplesInStream){
            AUDIO_INFO("total samples in stream: %lu", m_flacTotalSamplesInStream);
        }
        else{
            AUDIO_INFO("total samples in stream: N/A");
        }
        if(bps != 0 && m_flacTotalSamplesInStream) {
            AUDIO_INFO("audio file duration: %lu seconds", m_flacTotalSamplesInStream / m_flacSampleRate);
        }
        m_controlCounter = FLAC_MBH; // METADATA_BLOCK_HEADER
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_PADDING) { /* PADDING */
        size_t l = bigEndian(data, 3);
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_APP) { /* APPLICATION */
        size_t l = bigEndian(data, 3);
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_SEEK) { /* SEEKTABLE */
        size_t l = bigEndian(data, 3);
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_VORBIS) { /* VORBIS COMMENT */                          // field names
        const char fn[7][12] = {"TITLE", "VERSION", "ALBUM", "TRACKNUMBER", "ARTIST", "PERFORMER", "GENRE"};
        int offset;
        size_t l = bigEndian(data, 3);

        for(int i = 0; i < 7; i++){
            offset = specialIndexOf(data, fn[i], len);
            if(offset >= 0){
                sprintf(chbuf, "%s: %s", fn[i], data + offset + strlen(fn[i]) + 1);
                chbuf[strlen(chbuf) - 1] = 0;
                for(int i=0; i<strlen(chbuf);i++){
                    if(chbuf[i] == 255) chbuf[i] = 0;
                }
                if(audio_id3data) audio_id3data(chbuf);
            }
        }
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_CUESHEET) { /* CUESHEET */
        size_t l = bigEndian(data, 3);
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == FLAC_PICTURE) { /* PICTURE */
        size_t l = bigEndian(data, 3);
        m_controlCounter = FLAC_MBH;
        retvalue = l + 3;
        headerSize += retvalue;
        return 0;
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::read_ID3_Header(uint8_t *data, size_t len) {

    static size_t id3Size;
    static size_t headerSize;
    static uint8_t ID3version;
    static int ehsz = 0;
    static char tag[5];
    static char frameid[5];
    static size_t framesize = 0;
    static bool compressed = false;
    static bool APIC_seen = false;
    static size_t APIC_size = 0;
    static uint32_t APIC_pos = 0;
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 0){      /* read ID3 tag and ID3 header size */
        if(getDatamode() == AUDIO_LOCALFILE){
            ID3version = 0;
            m_contentlength = getFileSize();
            AUDIO_INFO("Content-Length: %lu", m_contentlength);
        }
        m_controlCounter ++;
        APIC_seen = false;
        headerSize = 0;
        ehsz = 0;
        if(specialIndexOf(data, "ID3", 4) != 0) { // ID3 not found
            AUDIO_INFO("file has no mp3 tag, skip metadata");
            m_audioDataSize = m_contentlength;
            AUDIO_INFO("Audio-Length: %u", m_audioDataSize);
            //if(audio_progress) audio_progress(295903, m_audioDataSize);
            if(audio_progress) audio_progress(0, m_audioDataSize);
            return -1; // error, no ID3 signature found
        }
        ID3version = *(data + 3);
        switch(ID3version){
            case 2:
                m_f_unsync = (*(data + 5) & 0x80);
                m_f_exthdr = false;
                break;
            case 3:
            case 4:
                m_f_unsync = (*(data + 5) & 0x80); // bit7
                m_f_exthdr = (*(data + 5) & 0x40); // bit6 extended header
                break;
        };
        id3Size = bigEndian(data + 6, 4, 7); //  ID3v2 size  4 * %0xxxxxxx (shift left seven times!!)
        id3Size += 10;

        // Every read from now may be unsync'd
        AUDIO_INFO("ID3 framesSize: %i", id3Size);

        AUDIO_INFO("ID3 version: 2.%i", ID3version);

        if(ID3version == 2){
            m_controlCounter = 10;
        }
        headerSize = id3Size;
        m_ID3Size = id3Size;
        headerSize -= 10;

        return 10;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 1){      // compute extended header size if exists
        m_controlCounter ++;
        if(m_f_exthdr) {
            AUDIO_INFO("ID3 extended header");
            ehsz =  bigEndian(data, 4);
            headerSize -= 4;
            ehsz -= 4;
            return 4;
        }
        else{
            AUDIO_INFO("ID3 normal frames");
            return 0;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 2){      // skip extended header if exists
        if(ehsz > 256) {
            ehsz -=256;
            headerSize -= 256;
            return 256;} // Throw it away
        else           {
            m_controlCounter ++;
            headerSize -= ehsz;
            return ehsz;} // Throw it away
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 3){      // read a ID3 frame, get the tag
        if(headerSize == 0){
            m_controlCounter = 99;
            return 0;
        }
        m_controlCounter ++;
        frameid[0] = *(data + 0);
        frameid[1] = *(data + 1);
        frameid[2] = *(data + 2);
        frameid[3] = *(data + 3);
        frameid[4] = 0;
        for(uint8_t i = 0; i < 4; i++) tag[i] = frameid[i]; // tag = frameid

        headerSize -= 4;
        if(frameid[0] == 0 && frameid[1] == 0 && frameid[2] == 0 && frameid[3] == 0) {
            // We're in padding
            m_controlCounter = 98;  // all ID3 metadata processed
        }
        return 4;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 4){  // get the frame size
        m_controlCounter = 6;

        if(ID3version == 4){
            framesize = bigEndian(data, 4, 7); // << 7
        }
        else {
            framesize = bigEndian(data, 4);  // << 8
        }
        headerSize -= 4;
        uint8_t flag = *(data + 4); // skip 1st flag
        (void) flag;
        headerSize--;
        compressed = (*(data + 5)) & 0x80; // Frame is compressed using [#ZLIB zlib] with 4 bytes for 'decompressed
                                           // size' appended to the frame header.
        headerSize--;
        uint32_t decompsize = 0;
        if(compressed){
            if(m_f_Log) log_i("iscompressed");
            decompsize = bigEndian(data + 6, 4);
            headerSize -= 4;
            (void) decompsize;
            if(m_f_Log) log_i("decompsize=%u", decompsize);
            return 6 + 4;
        }
        return 6;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 5){      // If the frame is larger than 256 bytes, skip the rest
        if(framesize > 256){
            framesize -= 256;
            headerSize -= 256;
            return 256;
        }
        else {
            m_controlCounter = 3; // check next frame
            headerSize -= framesize;
            return framesize;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 6){      // Read the value
        m_controlCounter = 5;       // only read 256 bytes
        char value[256];
        char ch = *(data + 0);
        // $00 – ISO-8859-1 (LATIN-1, Identical to ASCII for values smaller than 0x80).
        // $01 – UCS-2 encoded Unicode with BOM, in ID3v2.2 and ID3v2.3.
        // $02 – UTF-16BE encoded Unicode without BOM, in ID3v2.4.
        // $03 – UTF-8 encoded Unicode, in ID3v2.4.
        bool isUnicode = (ch==1) ? true : false;

        if(startsWith(tag, "APIC")) { // a image embedded in file, passing it to external function
            isUnicode = false;
            if(getDatamode() == AUDIO_LOCALFILE){
                APIC_seen = true;
                APIC_pos = id3Size - headerSize;
                APIC_size = framesize;
            }
            return 0;
        }

        size_t fs = framesize;
        if(fs >255) fs = 255;
        for(int i=0; i<fs; i++){
            value[i] = *(data + i);
        }
        framesize -= fs;
        headerSize -= fs;
        value[fs] = 0;
        if(isUnicode && fs > 1) {
            unicode2utf8(value, fs);   // convert unicode to utf-8 U+0020...U+07FF
        }
        if(!isUnicode){
            uint16_t j = 0, k = 0;
            j = 0;
            k = 0;
            while(j < fs) {
                if(value[j] == 0x0A) value[j] = 0x20; // replace LF by space
                if((uint8_t)value[j] > 0x1F) {
                    value[k] = value[j];
                    k++;
                }
                j++;
            } //remove non printables
            if(k>0) value[k] = 0; else value[0] = 0; // new termination
        }
        showID3Tag(tag, value);
        return fs;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    // -- section V2.2 only , greater Vers above ----
    if(m_controlCounter == 10){ // frames in V2.2, 3bytes identifier, 3bytes size descriptor
        frameid[0] = *(data + 0);
        frameid[1] = *(data + 1);
        frameid[2] = *(data + 2);
        frameid[3] = 0;
        for(uint8_t i = 0; i < 4; i++) tag[i] = frameid[i]; // tag = frameid
        headerSize -= 3;
        size_t len = bigEndian(data + 3, 3);
        headerSize -= 3;
        headerSize -= len;
        char value[256];
        size_t tmp = len;
        if(tmp > 254) tmp = 254;
        memcpy(value, (data + 7), tmp);
        value[tmp+1] = 0;
        chbuf[0] = 0;

        showID3Tag(tag, value);
        if(len == 0) m_controlCounter = 98;

        return 3 + 3 + len;
    }
    // -- end section V2.2 -----------

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 98){ // skip all ID3 metadata (mostly spaces)
        if(headerSize > 256) {
            headerSize -=256;
            return 256;
        } // Throw it away
        else           {
            m_controlCounter = 99;
            return headerSize;
        } // Throw it away
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 99){ //  exist another ID3tag?
        m_audioDataStart += id3Size;
        vTaskDelay(30);
        if((*(data + 0) == 'I') && (*(data + 1) == 'D') && (*(data + 2) == '3')) {
            m_controlCounter = 0;
            return 0;
        }
        else {
            m_controlCounter = 100; // ok
            eofHeader = true;
            m_audioDataSize = m_contentlength - m_audioDataStart;
            AUDIO_INFO("Audio-Length: %u", m_audioDataSize);
            if(audio_progress) audio_progress(m_audioDataStart, m_audioDataSize);
            if(APIC_seen && audio_id3image){
                size_t pos = audiofile.position();
                audio_id3image(audiofile, APIC_pos, APIC_size);
                audiofile.seek(pos); // the filepointer could have been changed by the user, set it back
            }
            return 0;
        }
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::read_M4A_Header(uint8_t *data, size_t len) {
/*
       ftyp
         | - moov  -> trak -> ... -> mp4a contains raw block parameters
         |    L... -> ilst  contains artist, composer ....
       free (optional)
         |
       mdat contains the audio data                                                      */


    static size_t headerSize = 0;
    static size_t retvalue = 0;
    static size_t atomsize = 0;
    static size_t audioDataPos = 0;

    if(retvalue) {
        if(retvalue > len) { // if returnvalue > bufferfillsize
            if(len > InBuff.getMaxBlockSize()) len = InBuff.getMaxBlockSize();
            retvalue -= len; // and wait for more bufferdata
            return len;
        }
        else {
            size_t tmp = retvalue;
            retvalue = 0;
            return tmp;
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_BEGIN) {  // init
        headerSize = 0;
        retvalue = 0;
        atomsize = 0;
        audioDataPos = 0;
        m_controlCounter = M4A_FTYP;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_FTYP) { /* check_m4a_file */
        atomsize = bigEndian(data, 4); // length of first atom
        if(specialIndexOf(data, "ftyp", 10) != 4) {
            log_e("atom 'type' not found in header");
            stopSong();
            return -1;
        }
        int m4a  = specialIndexOf(data, "M4A ", 20);
        int isom = specialIndexOf(data, "isom", 20);

        if((m4a !=8) && (isom != 8)){
            log_e("subtype 'MA4 ' or 'isom' expected, but found '%s '", (data + 8));
            stopSong();
            return -1;
        }

        m_controlCounter = M4A_CHK;
        retvalue = atomsize;
        headerSize = atomsize;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_CHK) { /* check  Tag */
        atomsize = bigEndian(data, 4); // length of this atom
        if(specialIndexOf(data, "moov", 10) == 4) {
            m_controlCounter = M4A_MOOV;
            return 0;
        }
        else if(specialIndexOf(data, "free", 10) == 4) {
            retvalue = atomsize;
            headerSize += atomsize;
            return 0;
        }
        else if(specialIndexOf(data, "mdat", 10) == 4) {
            m_controlCounter = M4A_MDAT;
            return 0;
        }
        else {
            char atomName[5];
            (void)atomName;
            atomName[0] = *data;
            atomName[1] = *(data + 1);
            atomName[2] = *(data + 2);
            atomName[3] = *(data + 3);
            atomName[4] = 0;

            if(m_f_Log) log_i("atom %s found", atomName);

            retvalue = atomsize;
            headerSize += atomsize;
            return 0;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_MOOV) {  // moov
        // we are looking for track and ilst
        if(specialIndexOf(data, "trak", len) > 0){
            int offset = specialIndexOf(data, "trak", len);
            retvalue = offset;
            atomsize -= offset;
            headerSize += offset;
            m_controlCounter = M4A_TRAK;
            return 0;
        }
        if(specialIndexOf(data, "ilst", len) > 0){
            int offset = specialIndexOf(data, "ilst", len);
            retvalue = offset;
            atomsize -= offset;
            headerSize += offset;
            m_controlCounter = M4A_ILST;
            return 0;

        }
        if (atomsize > len -10){atomsize -= (len -10); headerSize += (len -10); retvalue = (len -10);}
        else {m_controlCounter = M4A_CHK; retvalue = atomsize; headerSize += atomsize;}
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_TRAK) {  // trak
        if(specialIndexOf(data, "esds", len) > 0){
            int esds = specialIndexOf(data, "esds", len); // Packaging/Encapsulation And Setup Data
            uint8_t *pos = data + esds;
            uint8_t len_of_OD  = *(pos + 12); // length of this OD (which includes the next 2 tags)
            (void)len_of_OD;
            uint8_t len_of_ESD = *(pos + 20); // length of this Elementary Stream Descriptor
            (void)len_of_ESD;
            uint8_t audioType  = *(pos + 21);

            if     (audioType == 0x40) {AUDIO_INFO("AudioType: MPEG4 / Audio");} // ObjectTypeIndication
            else if(audioType == 0x66) {AUDIO_INFO("AudioType: MPEG2 / Audio");}
            else if(audioType == 0x69) {AUDIO_INFO("AudioType: MPEG2 / Audio Part 3");} // Backward Compatible Audio
            else if(audioType == 0x6B) {AUDIO_INFO("AudioType: MPEG1 / Audio");}
            else                       {AUDIO_INFO("unknown Audio Type %x", audioType);}

            uint8_t streamType = *(pos + 22);
            streamType = streamType >> 2;  // 6 bits
            if(streamType!= 5) { log_e("Streamtype is not audio!"); }

            uint32_t maxBr = bigEndian(pos + 26, 4); // max bitrate
            AUDIO_INFO("max bitrate: %lu", maxBr);

            uint32_t avrBr = bigEndian(pos + 30, 4); // avg bitrate
            AUDIO_INFO("avr bitrate: %lu", avrBr);

            uint16_t ASC   = bigEndian(pos + 39, 2);

            uint8_t objectType = ASC >> 11; // first 5 bits

            if     (objectType == 1) {AUDIO_INFO("AudioObjectType: AAC Main");} // Audio Object Types
            else if(objectType == 2) {AUDIO_INFO("AudioObjectType: AAC Low Complexity");}
            else if(objectType == 3) {AUDIO_INFO("AudioObjectType: AAC Scalable Sample Rate");}
            else if(objectType == 4) {AUDIO_INFO("AudioObjectType: AAC Long Term Prediction");}
            else if(objectType == 5) {AUDIO_INFO("AudioObjectType: AAC Spectral Band Replication");}
            else if(objectType == 6) {AUDIO_INFO("AudioObjectType: AAC Scalable");}
            else                     {AUDIO_INFO("unknown Audio Type %x", audioType);}

            const uint32_t samplingFrequencies[13] = {
                    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350
            };
            uint8_t sRate = (ASC & 0x0600) >> 7; // next 4 bits Sampling Frequencies
            AUDIO_INFO("Sampling Frequency: %lu",samplingFrequencies[sRate]);

            uint8_t chConfig = (ASC & 0x78) >> 3;  // next 4 bits
            if(chConfig == 0) AUDIO_INFO("Channel Configurations: AOT Specifc Config");
            if(chConfig == 1) AUDIO_INFO("Channel Configurations: front-center");
            if(chConfig == 2) AUDIO_INFO("Channel Configurations: front-left, front-right");
            if(chConfig >  2) { log_e("Channel Configurations with more than 2 channels is not allowed!"); }

            uint8_t frameLengthFlag     = (ASC & 0x04);
            uint8_t dependsOnCoreCoder  = (ASC & 0x02);
            (void)dependsOnCoreCoder;
            uint8_t extensionFlag       = (ASC & 0x01);
            (void)extensionFlag;

            if(frameLengthFlag == 0) AUDIO_INFO("AAC FrameLength: 1024 bytes");
            if(frameLengthFlag == 1) AUDIO_INFO("AAC FrameLength: 960 bytes");
        }
        if(specialIndexOf(data, "mp4a", len) > 0){
            int offset = specialIndexOf(data, "mp4a", len);
            int channel = bigEndian(data + offset + 20, 2); // audio parameter must be set before starting
            int bps     = bigEndian(data + offset + 22, 2); // the aac decoder. There are RAW blocks only in m4a
            int srate   = bigEndian(data + offset + 26, 4); //
            setBitsPerSample(bps);
            setChannels(channel);
            setSampleRate(srate);
            setBitrate(bps * channel * srate);
            AUDIO_INFO("ch; %i, bps: %i, sr: %i", channel, bps, srate);
            if(audioDataPos && getDatamode() == AUDIO_LOCALFILE) {
                m_controlCounter = M4A_AMRDY;
                setFilePos(audioDataPos);
                return 0;
            }
        }
        m_controlCounter = M4A_MOOV;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_ILST) {  // ilst
        const char info[12][6] = { "nam\0", "ART\0", "alb\0", "too\0",  "cmt\0",  "wrt\0",
                                   "tmpo\0", "trkn\0","day\0", "cpil\0", "aART\0", "gen\0"};
        int offset;
        for(int i=0; i < 12; i++){
            offset = specialIndexOf(data, info[i], len, true);  // seek info[] with '\0'
            if(offset>0) {
                offset += 19; if(*(data + offset) == 0) offset ++;
                char value[256];
                size_t tmp = strlen((const char*)data + offset);
                if(tmp > 254) tmp = 254;
                memcpy(value, (data + offset), tmp);
                value[tmp] = 0;
                chbuf[0] = 0;
                if(i == 0)  sprintf(chbuf, "Title: %s", value);
                if(i == 1)  sprintf(chbuf, "Artist: %s", value);
                if(i == 2)  sprintf(chbuf, "Album: %s", value);
                if(i == 3)  sprintf(chbuf, "Encoder: %s", value);
                if(i == 4)  sprintf(chbuf, "Comment: %s", value);
                if(i == 5)  sprintf(chbuf, "Composer: %s", value);
                if(i == 6)  sprintf(chbuf, "BPM: %s", value);
                if(i == 7)  sprintf(chbuf, "Track Number: %s", value);
                if(i == 8)  sprintf(chbuf, "Year: %s", value);
                if(i == 9)  sprintf(chbuf, "Compile: %s", value);
                if(i == 10) sprintf(chbuf, "Album Artist: %s", value);
                if(i == 11) sprintf(chbuf, "Types of: %s", value);
                if(chbuf[0] != 0) {
                    if(audio_id3data) audio_id3data(chbuf);
                }
            }
        }
        m_controlCounter = M4A_MOOV;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == M4A_MDAT) {  // mdat
        m_audioDataSize = bigEndian(data, 4) -8; // length of this atom - strlen(M4A_MDAT)
        AUDIO_INFO( "Audio-Length: %u",m_audioDataSize);
        if(audio_progress) audio_progress(m_audioDataStart, m_audioDataSize);
        retvalue = 8;
        headerSize += 8;
        m_controlCounter = M4A_AMRDY;  // last step before starting the audio
        return 0;
    }

    if(m_controlCounter == M4A_AMRDY){ // almost ready
        m_audioDataStart = headerSize;
//        m_contentlength = headerSize + m_audioDataSize; // after this mdat atom there may be other atoms
        if(getDatamode() == AUDIO_LOCALFILE){
            AUDIO_INFO("Content-Length: %lu", m_contentlength);
            if(audio_progress) audio_progress(m_audioDataStart, m_audioDataSize);
        }
        m_controlCounter = M4A_OKAY; // that's all
        eofHeader = true;
        return 0;
    }
    // this section should never be reached
    log_e("error");
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::read_OGG_Header(uint8_t *data, size_t len){
    static size_t retvalue = 0;
    static size_t pageLen = 0;
    static bool   f_firstPacket = false;

    if(retvalue) {
        if(retvalue > len) { // if returnvalue > bufferfillsize
            if(len > InBuff.getMaxBlockSize()) len = InBuff.getMaxBlockSize();
            retvalue -= len; // and wait for more bufferdata
            return len;
        }
        else {
            size_t tmp = retvalue;
            retvalue = 0;
            return tmp;
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == OGG_BEGIN) {  // init
        retvalue = 0;
        m_audioDataStart = 0;
        f_firstPacket = true;
        m_controlCounter = OGG_MAGIC;
        if(getDatamode() == AUDIO_LOCALFILE){
            m_contentlength = getFileSize();
            AUDIO_INFO("Content-Length: %lu", m_contentlength);
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == OGG_MAGIC) { /* check MAGIC STRING */
        if(specialIndexOf(data, "OggS", 10) != 0) {
            log_e("Magic String 'OggS' not found in header");
            stopSong();
            return -1;
        }
        m_controlCounter = OGG_HEADER;
        retvalue = 4;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == OGG_HEADER) { /* check OGG PAGE HEADER */
        uint8_t i = 0;
        uint8_t ssv = *(data + i);                  // stream_structure_version
        (void)ssv;
        i++;
        uint8_t htf = *(data + i);                  // header_type_flag
        (void)htf;
        i++;
        uint32_t tmp = bigEndian(data + i, 4);      // absolute granule position
        uint64_t agp = (uint64_t) tmp << 32;
        i += 4;
        agp += bigEndian(data + i, 4);
        i += 4;
        uint32_t ssnr = bigEndian(data + i, 4);     // stream serial number
        (void)ssnr;
        i += 4;
        uint32_t psnr = bigEndian(data + i, 4);     // page sequence no
        (void)psnr;
        i += 4;
        uint32_t pchk = bigEndian(data + i, 4);     // page checksum
        (void)pchk;
        i += 4;
        uint8_t psegm = *(data + i);
        i++;
        uint8_t psegmBuff[256];
        pageLen = 0;
        for(uint8_t j = 0; j < psegm; j++){
            psegmBuff[j] = *(data + i);
            pageLen += psegmBuff[j];
            i++;
        }
        retvalue = i;
        if(agp == 0){
            if(f_firstPacket == true){
                f_firstPacket = false;
                m_controlCounter = OGG_FIRST; // ogg first pages
            }
            else{
                retvalue += pageLen;
                m_controlCounter = OGG_MAGIC;
            }
        }
        else{
            if(m_codec == CODEC_OGG_FLAC){
                m_controlCounter = OGG_AMRDY;
            }
            else {
                AUDIO_INFO("unknown format");
                stopSong();
                return -1;
            }
        }
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == OGG_FIRST) { /* check OGG FIRST PAGES (has no streaming content) */
        uint8_t i = 0;
        uint8_t obp = *(data + i);                  // oneBytePacket shold be 0x7F
        (void)obp;
        i++;
        if(specialIndexOf(data + i, "FLAC", 10) == 0){
        }
        else{
            log_e("ogg/flac support only"); // ogg/vorbis or ogg//opus not supported yet
            stopSong();
            return -1;
        }
        i += 4;
        uint8_t major_vers = *(data + i);
        (void)major_vers;
        i++;
        uint8_t minor_vers = *(data + i);
        (void)minor_vers;
        i++;
        uint16_t nonah = bigEndian(data + i, 2); // number of non audio headers (0x00 = unknown)
        (void)nonah;
        i += 2;
        if(specialIndexOf(data + i, "fLaC", 10) == 0){
            m_codec = CODEC_OGG_FLAC;
        }
        i += 4;
        // STREAMINFO metadata block begins
        uint32_t mblen = bigEndian(data + i, 4);
        (void)mblen;
        i += 4; // skip metadata block header + length
        i += 2; // skip minimun block size
        m_flacMaxBlockSize = bigEndian(data + i, 2);
        i += 2;
        vTaskDelay(2);
        AUDIO_INFO("FLAC maxBlockSize: %u", m_flacMaxBlockSize);
        i += 3; // skip minimun frame size
        vTaskDelay(2);
        m_flacMaxFrameSize = bigEndian(data + i, 3);
        i += 3;
        if(m_flacMaxFrameSize){
            AUDIO_INFO("FLAC maxFrameSize: %u", m_flacMaxFrameSize);
        }
        else {
            AUDIO_INFO("FLAC maxFrameSize: N/A");
        }
        if(m_flacMaxFrameSize > InBuff.getMaxBlockSize()) {
            log_e("FLAC maxFrameSize too large!");
            stopSong();
            return -1;
        }
        vTaskDelay(2);
        uint32_t nextval = bigEndian(data + i, 3);
        i += 3;
        m_flacSampleRate = nextval >> 4;
        AUDIO_INFO("FLAC sampleRate: %lu", m_flacSampleRate);
        vTaskDelay(2);
        m_flacNumChannels = ((nextval & 0x06) >> 1) + 1;
        AUDIO_INFO("FLAC numChannels: %u", m_flacNumChannels);
        if(m_flacNumChannels != 1 && m_flacNumChannels != 2){
            vTaskDelay(2);
            AUDIO_INFO("numChannels must be 1 or 2");
            stopSong();
            return -1;
        }
        vTaskDelay(2);
        uint8_t bps = (nextval & 0x01) << 4;
        bps += (*(data +i) >> 4) + 1;
        i++;
        m_flacBitsPerSample = bps;
        if((bps != 8) && (bps != 16)){
            log_e("bits per sample must be 8 or 16, is %i", bps);
            stopSong();
            return -1;
        }
        AUDIO_INFO("FLAC bitsPerSample: %u", m_flacBitsPerSample);
        m_flacTotalSamplesInStream = bigEndian(data + i, 4);
        i++;
        if(m_flacTotalSamplesInStream) {
            AUDIO_INFO("total samples in stream: %lu", m_flacTotalSamplesInStream);
        }
        else {
            AUDIO_INFO("total samples in stream: N/A");
        }
        if(bps != 0 && m_flacTotalSamplesInStream) {
            AUDIO_INFO("audio file duration: %lu seconds", m_flacTotalSamplesInStream / m_flacSampleRate);
        }
        m_controlCounter = OGG_MAGIC;
        retvalue = pageLen;
        return 0;
    }
    if(m_controlCounter == OGG_AMRDY){ // ogg almost ready
        if(!FLACDecoder_AllocateBuffers()) {m_f_running = false; stopSong(); return -1;}
        InBuff.changeMaxBlockSize(m_frameSizeFLAC);
        AUDIO_INFO("FLACDecoder has been initialized, free Heap: %lu bytes", ESP.getFreeHeap());

        m_controlCounter = OGG_OKAY; // 100
        eofHeader = true;
        retvalue = 0;
        return 0;
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
size_t Audio::process_m3u8_ID3_Header(uint8_t* packet){
    uint8_t         ID3version;
    size_t          id3Size;
    bool            m_f_unsync = false, m_f_exthdr = false;
    static uint64_t last_timestamp;  // remember the last timestamp
    static uint32_t lastSampleRate;
    uint64_t        current_timestamp = 0;
    uint32_t        newSampleRate = 0;
    (void)m_f_unsync; (void)last_timestamp; (void)lastSampleRate; (void)current_timestamp; (void)newSampleRate;
    if(specialIndexOf(packet, "ID3", 4) != 0) { // ID3 not found
        if(m_f_Log) log_i("m3u8 file has no mp3 tag");
        return 0; // error, no ID3 signature found
    }
    ID3version = *(packet + 3);
    switch(ID3version){
            case 2:
                m_f_unsync = (*(packet + 5) & 0x80);
                m_f_exthdr = false;
                break;
            case 3:
            case 4:
                m_f_unsync = (*(packet + 5) & 0x80); // bit7
                m_f_exthdr = (*(packet + 5) & 0x40); // bit6 extended header
                break;
    };
    id3Size = bigEndian(&packet[6], 4, 7); //  ID3v2 size  4 * %0xxxxxxx (shift left seven times!!)
    id3Size += 10;
    if(m_f_Log) log_i("ID3 framesSize: %i", id3Size);
    if(m_f_Log) log_i("ID3 version: 2.%i", ID3version);

    if(m_f_exthdr) {
        log_e("ID3 extended header in m3u8 files not supported");
        return 0;
    }
    if(m_f_Log) log_i("ID3 normal frames");

    if(specialIndexOf(&packet[10], "PRIV", 5) != 0) { // tag PRIV not found
        log_e("tag PRIV in m3u8 Id3 Header not found");
        return 0;
    }
    // if tag PRIV exists assume content is "com.apple.streaming.transportStreamTimestamp"
    // a time stamp is expected in the header.

    current_timestamp = (double)bigEndian(&packet[69], 4) / 90000; // seconds

    return id3Size;
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::stopSong() {
    m_streamBuffering=false;
    if(cooperativeWebDac()){m_validSamples=0;m_curSample=0;} // Discard the cancelled frame on its owner.
#if IBT_NO_PSRAM
    lightnet::gate().cancel();
#endif
    directrx::dacBreak();
    m_startFadePending=true; // Audio owner: arm the next actual DAC samples, not wall-clock time.
    if(!netprefetch::end()){m_f_running=false;return 0;} // Never reuse a busy socket.
    uint32_t pos = 0;
    if(m_f_running) {
        m_f_running = false;
        if(getDatamode() == AUDIO_LOCALFILE){
            m_streamType = ST_NONE;
            {const uint32_t filePos=getFilePos(),buffered=inBufferFilled();pos=filePos>=buffered?filePos-buffered:0;}
            audiofile.close();
            AUDIO_INFO("Closing audio file");
        }
    }
    if(audiofile){
        // added this before putting 'm_f_localfile = false' in stopSong(); shoulf never occur....
        audiofile.close();
        AUDIO_INFO("Closing audio file");
        log_w("Closing audio file");  // for debug
    }
    extraDecoder.close();
    if(m_outBuff)memset(m_outBuff,0,OutputBytes);     //Clear OutputBuffer
    if(m_i2sReady)i2s_zero_dma_buffer((i2s_port_t) m_i2s_num);
    return pos;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::playI2Sremains() { // returns true if all dma_buffs flushed
    if(!outputReady()||!m_outBuff){m_validSamples=0;return;}
    if(!getSampleRate()) setSampleRate(96000);
    if(!getChannels()) setChannels(2);
    if(getBitsPerSample() > 8) memset(m_outBuff,   0, OutputBytes);     //Clear OutputBuffer (signed)
    else                       memset(m_outBuff, 128, OutputBytes);     //Clear OutputBuffer (unsigned, PCM 8u)

    m_validSamples = m_i2s_config.dma_buf_len;
    while(m_validSamples) {
        if(!playChunk())break;
    }
    return;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::pauseResume() {
    bool retVal = false;
    if(m_outBuff&&(getDatamode() == AUDIO_LOCALFILE || m_streamType == ST_WEBSTREAM)) {
        m_f_running = !m_f_running;
        retVal = true;
        if(!m_f_running) {
            if(cooperativeWebDac()){m_validSamples=0;m_curSample=0;}
            memset(m_outBuff, 0, OutputBytes);               //Clear OutputBuffer
            if(m_i2sReady)i2s_zero_dma_buffer((i2s_port_t) m_i2s_num);
        }
    }
    return retVal;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::submitDacFrames(const uint32_t* frames, size_t count) {
    const auto result=dacsubmit::all(reinterpret_cast<const uint8_t*>(frames),count*4,
        [this](const uint8_t* data,size_t bytes,size_t& accepted){
            const bool measured=directrx::dacBegin();
            const uint32_t started=measured?uint32_t(micros()):0;
            const esp_err_t err=i2s_write((i2s_port_t)m_i2s_num,data,bytes,&accepted,pdMS_TO_TICKS(10));
            const uint32_t finished=measured?uint32_t(micros()):0;
#if defined(CONFIG_IDF_TARGET_ESP32)
            if(IBT_NO_PSRAM){const auto& hw=m_i2s_num==0?I2S0:I2S1;directrx::dacHardwareSample(hw.out_eof_des_addr,hw.conf.tx_start!=0);}
#endif
            if(IBT_NO_PSRAM&&accepted<=bytes)directrx::pcmAccepted(data,accepted);
            if(measured)directrx::dacWrite(started,finished,bytes,accepted,err==ESP_OK,
                getSampleRate(),m_i2s_config.dma_buf_count*m_i2s_config.dma_buf_len);
            if(err==ESP_OK&&accepted==0)vTaskDelay(1);
            return err==ESP_OK;
        },[](){return uint32_t(millis());});
    m_i2s_bytesWritten=result.bytes;
    if(!result.complete){
        log_e("DAC submit failed: accepted=%u requested=%u; stopping",unsigned(result.bytes),unsigned(count*4));
        // Explicit abort, not silent PCM loss or an infinite outer retry.
        m_validSamples=0;m_curSample=0;
        stopSong();
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
// Use the already-shared native64-frame writer. Only the Light32k extra
// stream is adapted to the44.1k mode independently verified with native WAV.
bool Audio::playExtraDacBlock(){
    if(btaudio::outputSelected()||!m_outBuff||m_curSample||m_validSamples<=0||
       (getChannels()!=1&&getChannels()!=2)||unsigned(m_validSamples)>OutputSamples/getChannels())return false;
    monodsp::block(getSampleRate());
    uint32_t frames[64];size_t ready=0;bool ok=true;
    const bool measured=monodsp::active();uint32_t workUs=0;unsigned emitted=0;
    auto flush=[&](){if(!ready||!ok)return;directrx::pcmPacked(reinterpret_cast<const uint8_t*>(frames),ready);ok=submitDacFrames(frames,ready);ready=0;};
    auto emit=[&](int16_t l,int16_t r){
        if(!ok)return;const uint32_t began=measured?micros():0;int16_t sample[2]={l,r};
        if(m_f_forceMono&&!monodsp::active())sample[0]=sample[1]=(int32_t(l)+r)/2;
        if(!playSample(sample,&frames[ready])){ok=false;return;}
        if(measured)workUs+=uint32_t(micros())-began;
        ++ready;++emitted;if(ready==64)flush();
    };
    const unsigned count=unsigned(m_validSamples),ch=getChannels();
    for(unsigned i=0;i<count&&ok;++i){
        const int16_t l=m_outBuff[i*ch],r=m_outBuff[i*ch+ch-1];
        if(m_extraDacSourceRate)m_extraDacResampler.feed(l,r,m_extraDacSourceRate,getSampleRate(),emit);
        else emit(l,r); // Other native rates need no BT-rate admissibility gate.
    }
    flush();if(measured)monodsp::measuredBlock(emitted,workUs);
    if(!ok)return false;m_validSamples=0;m_curSample=0;return true;
}
void Audio::dacHardwareDiagnostics(){
#if defined(CONFIG_IDF_TARGET_ESP32)
    if(!IBT_NO_PSRAM)return;
    const auto& hw=m_i2s_num==0?I2S0:I2S1;
    auto route=[](int pin){return pin>=0&&pin<34?int(GPIO.func_out_sel_cfg[pin].func_sel):-1;};
    const int b=route(m_pin_config.bck_io_num),w=route(m_pin_config.ws_io_num),d=route(m_pin_config.data_out_num);
    const auto& expected=i2s_periph_signal[m_i2s_num];
    const bool pins=b==expected.m_tx_bck_sig&&w==expected.m_tx_ws_sig&&d==expected.data_out_sig;
    Serial.printf("[DAC-HW] installed=%u dormant=%u port=%u sdk_rate=%lu tx_start=%u tx_idle=%u eof_desc=%08lx bits=%u bclk_div=%u pins=%d,%d,%d matrix=%d,%d,%d route_match=%u mute_pin=%u mute_level=%d mute_lock=%u scope=REGISTER_SNAPSHOT_NOT_PIN_OR_ANALOG_MEASUREMENT\n",
       unsigned(m_i2sReady),unsigned(m_dacDormant),unsigned(m_i2s_num),(unsigned long)(m_i2sReady?i2s_get_clk((i2s_port_t)m_i2s_num):0),unsigned(hw.conf.tx_start),unsigned(hw.state.tx_idle),(unsigned long)hw.out_eof_des_addr,unsigned(hw.sample_rate_conf.tx_bits_mod),unsigned(hw.sample_rate_conf.tx_bck_div_num),m_pin_config.bck_io_num,m_pin_config.ws_io_num,m_pin_config.data_out_num,b,w,d,unsigned(pins),unsigned(MUTE_PIN),MUTE_PIN==255?-1:int(digitalRead(MUTE_PIN)),unsigned(MUTE_LOCK));
#endif
}
//---------------------------------------------------------------------------------------------------------------------
// Keep network service between bounded PCM handoffs on the small HTTPS DAC
// path. Pending samples keep their offset and must drain before another decode.
bool Audio::cooperativeWebDac(){
#if IBT_NO_PSRAM && defined(IBT_LIGHT4_DNS_AUTO)
    return m_streamType==ST_WEBSTREAM && m_f_ssl && !m_f_tts && m_codec==CODEC_MP3 &&
           !btaudio::outputSelected() && getBitsPerSample()==16;
#else
    return false;
#endif
}
void Audio::drainDecoded(){
    if(cooperativeWebDac()){if(m_validSamples)playChunk();return;}
    while(m_validSamples){if(!playChunk())break;}
}
bool Audio::playChunk() {
    if(!btaudio::outputSelected())monodsp::block(getSampleRate());
    // If we've got data, try and pump it out..
    int16_t sample[2];
    const bool cooperative=cooperativeWebDac();
    unsigned sentFrames=0;
    if(getBitsPerSample()==16&&!btaudio::outputSelected()&&(getChannels()==1||getChannels()==2)){
        uint32_t frames[64]; // 256 B bounded stack use; no PSRAM allocation.
        while(m_validSamples){
            const size_t count=std::min(size_t(m_validSamples),size_t(64));
            const bool monoMeasured=monodsp::active();
            const uint32_t monoStarted=monoMeasured?micros():0;
            for(size_t i=0;i<count;++i){
                const size_t at=size_t(m_curSample)+i;
                if(getChannels()==1)sample[0]=sample[1]=m_outBuff[at];
                else if(m_f_forceMono&&!monodsp::active())sample[0]=sample[1]=(int32_t(m_outBuff[at*2])+m_outBuff[at*2+1])/2;
                else {sample[0]=m_outBuff[at*2];sample[1]=m_outBuff[at*2+1];}
                if(!playSample(sample,&frames[i]))return false;
            }
            if(monoMeasured)monodsp::measuredBlock(unsigned(count),uint32_t(micros())-monoStarted);
            if(!submitDacFrames(frames,count))return false;
            m_validSamples-=count;m_curSample+=count;
            sentFrames+=unsigned(count);
            if(cooperative && m_validSamples && sentFrames>=512)return true;
        }
        m_curSample=0;
        return true;
    }

    if(getBitsPerSample() == 8) {
        if(getChannels() == 1) {
            while(m_validSamples) {
                uint8_t x =  m_outBuff[m_curSample] & 0x00FF;
                uint8_t y = (m_outBuff[m_curSample] & 0xFF00) >> 8;
                sample[LEFTCHANNEL]  = x;
                sample[RIGHTCHANNEL] = x;
                if(!playSample(sample))return false;
                sample[LEFTCHANNEL]  = y;
                sample[RIGHTCHANNEL] = y;
                if(!playSample(sample))return false;
                m_validSamples--;
                m_curSample++;
            }
        }
        if(getChannels() == 2) {
            while(m_validSamples) {
                uint8_t x =  m_outBuff[m_curSample] & 0x00FF;
                uint8_t y = (m_outBuff[m_curSample] & 0xFF00) >> 8;
                if(!m_f_forceMono) { // stereo mode
                    sample[LEFTCHANNEL]  = x;
                    sample[RIGHTCHANNEL] = y;
                }
                else { // force mono
                    uint8_t xy = (x + y) / 2;
                    sample[LEFTCHANNEL]  = xy;
                    sample[RIGHTCHANNEL] = xy;
                }
                if(!playSample(sample))return false;
                m_validSamples--;
                m_curSample++;
            }
        }
        m_curSample = 0;
        return true;
    }
    if(getBitsPerSample() == 16) {
        if(getChannels() == 1) {
            while(m_validSamples) {
                sample[LEFTCHANNEL]  = m_outBuff[m_curSample];
                sample[RIGHTCHANNEL] = m_outBuff[m_curSample];
                if(!playSample(sample)) {
                    log_e("can't send");
                    return false;
                } // Can't send
                m_validSamples--;
                m_curSample++;
            }
        }
        if(getChannels() == 2) {
            m_curSample = 0;
            while(m_validSamples) {
                if(!m_f_forceMono) { // stereo mode
                    sample[LEFTCHANNEL]  = m_outBuff[m_curSample * 2];
                    sample[RIGHTCHANNEL] = m_outBuff[m_curSample * 2 + 1];
                }
                else { // mono mode, #100
                    int16_t xy = (m_outBuff[m_curSample * 2] + m_outBuff[m_curSample * 2 + 1]) / 2;
                    sample[LEFTCHANNEL] = xy;
                    sample[RIGHTCHANNEL] = xy;
                }
                playSample(sample);
                m_validSamples--;
                m_curSample++;
            }
        }
        m_curSample = 0;
        return true;
    }
    log_e("BitsPer Sample must be 8 or 16!");
    m_validSamples = 0;
    stopSong();
    return false;
}

/*
 * Shamelessly borrowed from @schreibfaul1 https://github.com/schreibfaul1/ESP32-audioI2S/blob/1296374fc513a6d6bfaa3b1ca08f6ba938b18d99/src/Audio.cpp#L5030
 */
void Audio::_computeVUlevel(int16_t sample[2]) {
  const int mode=vuprobe::mode.load(std::memory_order_relaxed);
  if(mode==0||(mode<0&&!config.store.vumeter)) return;
  if(mode!=1){
    static vuprobe::Meter meter;
    if(meter.due()&&meter.sample(sample[LEFTCHANNEL],sample[RIGHTCHANNEL],millis())){
      vuLeft=std::min(255u,meter.left>>7);vuRight=std::min(255u,meter.right>>7);
      config.vuThreshold=std::max(unsigned(config.vuThreshold),std::max(unsigned(vuLeft),unsigned(vuRight)));
    }
    return;
  }
  static uint8_t sampleArray[2][4][8] = {0};
  static uint8_t cnt0 = 0, cnt1 = 0, cnt2 = 0, cnt3 = 0, cnt4 = 0;
  static bool    f_vu = false;

  auto avg = [&](uint8_t* sampArr) { // lambda, inner function, compute the average of 8 samples
    uint16_t av = 0;
    for(int i = 0; i < 8; i++) { av += sampArr[i]; }
    return av >> 3;
  };

  auto largest = [&](uint8_t* sampArr) { // lambda, inner function, compute the largest of 8 samples
    uint16_t maxValue = 0;
    for(int i = 0; i < 8; i++) {
      if(maxValue < sampArr[i]) maxValue = sampArr[i];
    }
    return maxValue;
  };

  if(cnt0 == 64) {
    cnt0 = 0;
    cnt1++;
  }
  if(cnt1 == 8) {
    cnt1 = 0;
    cnt2++;
  }
  if(cnt2 == 8) {
    cnt2 = 0;
    cnt3++;
  }
  if(cnt3 == 8) {
    cnt3 = 0;
    cnt4++;
    f_vu = true;
  }
  if(cnt4 == 8) { cnt4 = 0; }

  if(!cnt0) { // store every 64th sample in the array[0]
    sampleArray[LEFTCHANNEL][0][cnt1] = abs(sample[LEFTCHANNEL] >> 7);
    sampleArray[RIGHTCHANNEL][0][cnt1] = abs(sample[RIGHTCHANNEL] >> 7);
  }
  if(!cnt1) { // store argest from 64 * 8 samples in the array[1]
    sampleArray[LEFTCHANNEL][1][cnt2] = largest(sampleArray[LEFTCHANNEL][0]);
    sampleArray[RIGHTCHANNEL][1][cnt2] = largest(sampleArray[RIGHTCHANNEL][0]);
  }
  if(!cnt2) { // store avg from 64 * 8 * 8 samples in the array[2]
    sampleArray[LEFTCHANNEL][2][cnt3] = largest(sampleArray[LEFTCHANNEL][1]);
    sampleArray[RIGHTCHANNEL][2][cnt3] = largest(sampleArray[RIGHTCHANNEL][1]);
  }
  if(!cnt3) { // store avg from 64 * 8 * 8 * 8 samples in the array[3]
    sampleArray[LEFTCHANNEL][3][cnt4] = avg(sampleArray[LEFTCHANNEL][2]);
    sampleArray[RIGHTCHANNEL][3][cnt4] = avg(sampleArray[RIGHTCHANNEL][2]);
  }
  if(f_vu) {
    f_vu = false;
    vuLeft = avg(sampleArray[LEFTCHANNEL][3]);
    if(vuLeft>config.vuThreshold)  config.vuThreshold = vuLeft;
    vuRight = avg(sampleArray[RIGHTCHANNEL][3]);
    if(vuRight>config.vuThreshold) config.vuThreshold = vuRight;
  }
  cnt1++;
}

uint16_t Audio::get_VUlevel(uint16_t dimension){
  if(!config.store.vumeter || config.vuThreshold==0) return 0;
  uint8_t L = map(vuLeft, config.vuThreshold, 0, 0, dimension);
  uint8_t R = map(vuRight, config.vuThreshold, 0, 0, dimension);
  return (L << 8) | R;
}
//---------------------------------------------------------------------------------------------------------------------

void Audio::loop() {
    if(m_playlistFormat==FORMAT_M3U8&&!ibtdirect::hlsEnabled()){stopSong();AUDIO_ERROR("HLS disabled in settings");return;}

    if(!InBuff.isInitialized() || !outputReady() || !m_outBuff){m_f_running=false;return;}
    if(!m_f_running) {
      vuLeft=0; vuRight=0;
      vTaskDelay(2);
      return;
    }
    if(m_playlistFormat != FORMAT_M3U8){ // normal process
        switch(getDatamode()){
            case AUDIO_LOCALFILE:
                processLocalFile();
                break;
            case HTTP_RESPONSE_HEADER:
                parseHttpResponseHeader();
                break;
            case AUDIO_PLAYLISTINIT:
                readPlayListData();
                break;
            case AUDIO_PLAYLISTDATA:
                if(m_playlistFormat == FORMAT_M3U)  connecttohost(parsePlaylist_M3U());
                if(m_playlistFormat == FORMAT_PLS)  connecttohost(parsePlaylist_PLS());
                if(m_playlistFormat == FORMAT_ASX)  connecttohost(parsePlaylist_ASX());
                break;
            case AUDIO_DATA:
                if(m_streamType == ST_WEBSTREAM) processWebStream();
                if(m_streamType == ST_WEBFILE)   processWebFile();
                break;
        }
    }
    else { // HLS/M3U8 stream; the persistent menu switch is checked above
        static bool f_noNewHost = false;
        static int32_t remaintime, timestamp1, timestamp2;
        const char* host;
        switch(getDatamode()){
            case HTTP_RESPONSE_HEADER:
                playAudioData();
                parseHttpResponseHeader();
                m_codec = CODEC_AAC;
                break;
            case AUDIO_PLAYLISTINIT:
                readPlayListData();
                break;
            case AUDIO_PLAYLISTDATA:
                host = parsePlaylist_M3U8();
                m_f_m3u8data = true;
                if(host){
                    f_noNewHost = false;
                    timestamp1 = millis();
                    httpPrint(host);
                }
                else {
                    f_noNewHost = true;
                    timestamp2 = millis() + remaintime;
                    setDatamode(AUDIO_DATA);
                }
                break;
            case AUDIO_DATA:
                if(m_f_ts) processWebStreamTS();
                else       processWebStreamHLS();
                if(f_noNewHost){
                    m_f_continue = false;
                    if(timestamp2 < millis()){
                        httpPrint(m_lastHost);
                        remaintime = 1000;
                    }
                }
                else if(m_f_continue){
                    remaintime = (int32_t)(m_m3u8_targetDuration * 1000) - (millis() - timestamp1);
                    m_f_continue = false;
                    setDatamode(AUDIO_PLAYLISTDATA);
                }
                break;
            default:
                break;
        }
    }

}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::readPlayListData() {

    if(getDatamode() != AUDIO_PLAYLISTINIT) return false;
    if(_client->available() == 0) return false;

    uint32_t chunksize = 0; uint8_t readedBytes = 0;
    if(m_f_chunked) chunksize = chunkedDataTransfer(&readedBytes);

    // reads the content of the playlist and stores it in the vector m_contentlength
    // m_contentlength is a table of pointers to the lines
    char pl[512]; // playlistLine
    uint32_t ctl  = 0;
    int lines = 0;
    // delete all memory in m_playlistContent
    if(!psramFound()){log_e("m3u8 playlists requires PSRAM enabled!");}
    vector_clear_and_shrink(m_playlistContent);
    while(true){  // outer while

        uint32_t ctime = millis();
        uint32_t timeout = 2000; // ms

        while(true) { // inner while
            uint16_t pos = 0;
            while(_client->available()){ // super inner while :-))
                pl[pos] = _client->read();
                ctl++;
                if(pl[pos] == '\n') {pl[pos] = '\0'; pos++; break;}
            //    if(pl[pos] == '&' ) {pl[pos] = '\0'; pos++; break;}
                if(pl[pos] == '\r') {pl[pos] = '\0'; pos++; continue;;}
                pos++;
                if(pos == 511){ pos--; continue;}
                if(pos == 510) {pl[pos] = '\0';}
                if(ctl == chunksize) {pl[pos] = '\0'; break;}
                if(ctl == m_contentlength) {pl[pos] = '\0'; break;}
            }
            if(ctl == chunksize) break;
            if(ctl == m_contentlength) break;
            if(pos) {pl[pos] = '\0'; break;}

            if(ctime + timeout < millis()) {
                log_e("timeout");
                for(int i = 0; i<m_playlistContent.size(); i++) log_e("pl%i = %s", i, m_playlistContent[i]);
                goto exit;}
        } // inner while

        if(startsWith(pl, "<!DOCTYPE")) {AUDIO_INFO("url is a webpage!"); goto exit;}
        if(startsWith(pl, "<html"))     {AUDIO_INFO("url is a webpage!"); goto exit;}
        if(strlen(pl) > 0) m_playlistContent.push_back(strdup((const char*)pl));
        if(m_playlistContent.size() == 100){
            if(m_f_Log) log_i("the maximum number of lines in the playlist has been reached");
            break;
        }
        // termination conditions
        // 1. The http response header returns a value for contentLength -> read chars until contentLength is reached
        // 2. no contentLength, but Transfer-Encoding:chunked -> compute chunksize and read until chunksize is reached
        // 3. no chunksize and no contentlengt, but Connection: close -> read all available chars
        if(ctl == m_contentlength){while(_client->available()) _client->read(); break;} // read '\n\n' if exists
        if(ctl == chunksize)      {while(_client->available()) _client->read(); break;}
        if(!_client->connected() && _client->available() == 0) break;

    } // outer while
    lines = m_playlistContent.size();
    for (int i = 0; i < lines ; i++) { // print all string in first vector of 'arr'
        if(m_f_Log) log_i("pl=%i \"%s\"", i, m_playlistContent[i]);
    }
    setDatamode(AUDIO_PLAYLISTDATA);
    return true;

    exit:
        vector_clear_and_shrink(m_playlistContent);
        m_f_running = false;
        setDatamode(AUDIO_NONE);
    return false;
}
//----------------------------------------------------------------------------------------------------------------------
const char* Audio::parsePlaylist_M3U(){
    uint8_t lines = m_playlistContent.size();
    int pos = 0;
    char* host = nullptr;

    for(int i= 0; i < lines; i++){
        if(indexOf(m_playlistContent[i], "#EXTINF:") >= 0) {            // Info?
            pos = indexOf(m_playlistContent[i], ",");                   // Comma in this line?
            if(pos > 0) {
                // Show artist and title if present in metadata
                AUDIO_INFO(m_playlistContent[i] + pos + 1);
            }
            continue;
        }
        if(startsWith(m_playlistContent[i], "#")) {                     // Commentline?
            continue;
        }

        pos = indexOf(m_playlistContent[i], "http://:@", 0);            // ":@"??  remove that!
        if(pos >= 0) {
            AUDIO_INFO("Entry in playlist found: %s", (m_playlistContent[i] + pos + 9));
            host = m_playlistContent[i] + pos + 9;
            break;
        }
        // AUDIO_INFO("Entry in playlist found: %s", pl);
        pos = indexOf(m_playlistContent[i], "http", 0);                 // Search for "http"
        if(pos >= 0) {                                                  // Does URL contain "http://"?
    //    log_e("%s pos=%i", m_playlistContent[i], pos);
            host = m_playlistContent[i] + pos;                        // Yes, set new host
            break;
        }
    }
    vector_clear_and_shrink(m_playlistContent);
    return host;
}
//----------------------------------------------------------------------------------------------------------------------
const char* Audio::parsePlaylist_PLS(){
    uint8_t lines = m_playlistContent.size();
    int pos = 0;
    char* host = nullptr;

    for(int i= 0; i < lines; i++){
        if(i == 0){
            if(strlen(m_playlistContent[0]) == 0) goto exit;            // empty line
            if(strcmp(m_playlistContent[0] , "[playlist]") != 0){       // first entry in valid pls
                setDatamode(HTTP_RESPONSE_HEADER);                      // pls is not valid
                AUDIO_INFO("pls is not valid, switch to HTTP_RESPONSE_HEADER");
                goto exit;
            }
            continue;
        }
        if(startsWith(m_playlistContent[i], "File1")) {
            if(host) continue;                                          // we have already a url
            pos = indexOf(m_playlistContent[i], "http", 0);             // File1=http://streamplus30.leonex.de:14840/;
            if(pos >= 0) {                                              // yes, URL contains "http"?
                host = m_playlistContent[i] + pos;                      // Now we have an URL for a stream in host.
            }
            continue;
        }
        if(startsWith(m_playlistContent[i], "Title1")) {                // Title1=Antenne Tirol
            const char* plsStationName = (m_playlistContent[i] + 7);
            if(audio_showstation) audio_showstation(plsStationName);
            AUDIO_INFO("StationName: \"%s\"", plsStationName);
            continue;
        }
        if(startsWith(m_playlistContent[i], "Length1")){
            continue;
        }
        if(indexOf(m_playlistContent[i], "Invalid username") >= 0){     // Unable to access account:
            goto exit;                                                  // Invalid username or password
        }
    }
    return host;

exit:
    m_f_running = false;
    stopSong();
    vector_clear_and_shrink(m_playlistContent);
    setDatamode(AUDIO_NONE);
    return nullptr;
}
//----------------------------------------------------------------------------------------------------------------------
const char* Audio::parsePlaylist_ASX(){                             // Advanced Stream Redirector
    uint8_t lines = m_playlistContent.size();
    bool f_entry = false;
    int pos = 0;
    char* host = nullptr;

    for(int i= 0; i < lines; i++){
        int p1 = indexOf(m_playlistContent[i], "<", 0);
        int p2 = indexOf(m_playlistContent[i], ">", 1);
        if(p1 >= 0 && p2 > p1){                                     // #196 set all between "< ...> to lowercase
            for(uint8_t j = p1; j < p2; j++){
                m_playlistContent[i][j] = toLowerCase(m_playlistContent[i][j]);
            }
        }
        if(indexOf(m_playlistContent[i], "<entry>") >= 0) f_entry = true; // found entry tag (returns -1 if not found)
        if(f_entry) {
            if(indexOf(m_playlistContent[i], "ref href") > 0) {     //  <ref href="http://87.98.217.63:24112/stream" />
                pos = indexOf(m_playlistContent[i], "http", 0);
                if(pos > 0) {
                    host = (m_playlistContent[i] + pos);            // http://87.98.217.63:24112/stream" />
                    int pos1 = indexOf(host, "\"", 0);              // http://87.98.217.63:24112/stream
                    if(pos1 > 0) host[pos1] = '\0';                 // Now we have an URL for a stream in host.
                }
            }
        }
        pos = indexOf(m_playlistContent[i], "<title>", 0);
        if(pos >= 0) {
            char* plsStationName = (m_playlistContent[i] + pos + 7);            // remove <Title>
            pos = indexOf(plsStationName, "</", 0);
            if(pos >= 0){
                *(plsStationName +pos) = 0;                                     // remove </Title>
            }
            if(audio_showstation) audio_showstation(plsStationName);
            AUDIO_INFO("StationName: \"%s\"", plsStationName);
        }

        if(indexOf(m_playlistContent[i], "http") == 0 && !f_entry) {            //url only in asx
            host = m_playlistContent[i];
        }
    }
    return host;
}
//----------------------------------------------------------------------------------------------------------------------
const char* Audio::parsePlaylist_M3U8(){
    uint8_t lines = m_playlistContent.size();
    bool f_begin = false;
    uint8_t occurence = 0;
    if(lines){
        for(int i= 0; i < lines; i++){
            if(strlen(m_playlistContent[i]) == 0) continue;                    // empty line
            if(startsWith(m_playlistContent[i], "#EXTM3U")){                  // what we expected
                f_begin      = true;
                continue;
            }
            if(!f_begin) continue;

            // example: redirection
            // #EXTM3U
            // #EXT-X-STREAM-INF:BANDWIDTH=22050,CODECS="mp4a.40.2"
            // http://ample.revma.ihrhls.com/zc7729/63_sdtszizjcjbz02/playlist.m3u8
            if(startsWith(m_playlistContent[i],"#EXT-X-STREAM-INF:")){
                if(occurence > 0) break; // no more than one #EXT-X-STREAM-INF: (can have different BANDWIDTH)
                occurence++;
                if(!endsWith(m_playlistContent[i+1], "m3u8")){ // we have a new m3u8 playlist, skip to next line
                    int pos = indexOf(m_playlistContent[i], "CODECS=\"mp4a", 18);
                    // 'mp4a.40.01' AAC Main
                    // 'mp4a.40.02' AAC LC (Low Complexity)
                    // 'mp4a.40.03' AAC SSR (Scalable Sampling Rate) ??
                    // 'mp4a.40.03' AAC LTP (Long Term Prediction) ??
                    // 'mp4a.40.03' SBR (Spectral Band Replication)
                    if(pos < 0){ // not found
                        int pos1 = indexOf(m_playlistContent[i], "CODECS=", 18);
                        if(pos1 < 0) pos1 = 0;
                        log_e("codec %s in m3u8 playlist not supported", m_playlistContent[i] + pos1);
                        goto exit;
                    }
                }
                i++;                                                    // next line
                if(i == lines) continue; // and exit for()

                char* tmp = nullptr;
                if(!startsWith(m_playlistContent[i], "http")){
                  //http://livees.com/prog_index.m3u8 and prog_index48347.aac --> http://livees.com/prog_index48347.aac
                  //http://livees.com/prog_index.m3u8 and chunklist022.m3u8   --> http://livees.com/chunklist022.m3u8
                    tmp = (char*)malloc(strlen(m_lastHost)+ strlen(m_playlistContent[i]));
                    strcpy(tmp, m_lastHost);
                    int idx = lastIndexOf(tmp, "/");
                    strcpy(tmp + idx + 1, m_playlistContent[i]);
                }
                else{
                    tmp = strdup(m_playlistContent[i]);
                }
                if(m_playlistContent[i]){free(m_playlistContent[i]); m_playlistContent[i] = NULL;}
                m_playlistContent[i] = strdup(tmp);
                strcpy(m_lastHost, tmp);
                if(tmp){free(tmp); tmp = NULL;}
                if(m_f_Log) log_i("redirect %s", m_playlistContent[i]);
                return m_playlistContent[i];                            // it's a redirection, a new m3u8 playlist
            }

            // example: audio chunks
            // #EXTM3U
            // #EXT-X-TARGETDURATION:10
            // #EXT-X-MEDIA-SEQUENCE:163374040
            // #EXT-X-DISCONTINUITY
            // #EXTINF:10,title="text=\"Spot Block End\" amgTrackId=\"9876543\"",artist=" ",url="length=\"00:00:00\""
            // http://n3fa-e2.revma.ihrhls.com/zc7729/63_sdtszizjcjbz02/main/163374038.aac
            // #EXTINF:10,title="text=\"Spot Block End\" amgTrackId=\"9876543\"",artist=" ",url="length=\"00:00:00\""
            // http://n3fa-e2.revma.ihrhls.com/zc7729/63_sdtszizjcjbz02/main/163374039.aac
            if(startsWith(m_playlistContent[i], "#EXT-X-MEDIA-SEQUENCE:")){
                // do nothing, because MEDIA-SECUENCE is not set sometimes
            }
            static uint16_t targetDuration = 0;
            if(startsWith(m_playlistContent[i], "#EXT-X-TARGETDURATION:")) {
                targetDuration = atoi(m_playlistContent[i] + 22);
            }
            if(targetDuration) m_m3u8_targetDuration = targetDuration;
            if(m_f_Log) log_i("m_m3u8_targetDuration %d", m_m3u8_targetDuration);

            if(startsWith(m_playlistContent[i],"#EXTINF")) {
                if(STfromEXTINF(m_playlistContent[i])) showstreamtitle(chbuf);
                i++;
                if(i == lines) continue; // and exit for()

                char* tmp = nullptr;
                if(!startsWith(m_playlistContent[i], "http")){
                    //http://livees.com/prog_index.m3u8 and prog_index48347.aac --> http://livees.com/prog_index48347.aac
                    tmp = (char*)malloc(strlen(m_lastHost)+ strlen(m_playlistContent[i]));
                    strcpy(tmp, m_lastHost);
                    int idx = lastIndexOf(tmp, "/");
                    strcpy(tmp + idx + 1, m_playlistContent[i]);
                }
                else{
                    tmp = strdup(m_playlistContent[i]);
                }

                uint32_t hash = simpleHash(tmp);
                if(m_hashQueue.size() == 0){
                    m_hashQueue.insert(m_hashQueue.begin(), hash);
                    m_playlistURL.insert(m_playlistURL.begin(), strdup(tmp));
                }
                else{
                    bool known = false;
                    for(int i = 0; i< m_hashQueue.size(); i++){
                        if(hash == m_hashQueue[i]){
                            if(m_f_Log) log_i("file already known %s", tmp);
                            known = true;
                        }
                    }
                    if(!known){
                        m_hashQueue.insert(m_hashQueue.begin(), hash);
                        m_playlistURL.insert(m_playlistURL.begin(), strdup(tmp));
                    }
                }

                if(m_hashQueue.size() > 20)  m_hashQueue.pop_back();

                if(tmp){free(tmp); tmp = NULL;}

                if(m_playlistURL.size() == 20){
                    ESP_LOGD("", "can't stuff anymore");
                    break;
                }
                continue;
            }
        }
        vector_clear_and_shrink(m_playlistContent); //clear after reading everything, m_playlistContent.size is now 0
    }

    if(m_playlistURL.size() > 0){
        if(m_playlistBuff) {free(m_playlistBuff); m_playlistBuff = NULL;}

        if(m_playlistURL[m_playlistURL.size() -1]) {
                m_playlistBuff = strdup(m_playlistURL[m_playlistURL.size() -1]);
                free( m_playlistURL[m_playlistURL.size() -1]);
                m_playlistURL[m_playlistURL.size() -1] = NULL;
                m_playlistURL.pop_back();
                m_playlistURL.shrink_to_fit();
        }
        if(m_f_Log) log_i("now playing %s", m_playlistBuff);
        return m_playlistBuff;
    }
    else{
        return NULL;
    }
exit:
    stopSong();
    return NULL;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::STfromEXTINF(char* str){
    // the result is copied in chbuf!!
    // extraxt StreamTitle from m3u #EXTINF line to icy-format
    // orig: #EXTINF:10,title="text="TitleName",artist="ArtistName"
    // conv: StreamTitle=TitleName - ArtistName
    // orig: #EXTINF:10,title="text=\"Spot Block End\" amgTrackId=\"9876543\"",artist=" ",url="length=\"00:00:00\""
    // conv: StreamTitle=text=\"Spot Block End\" amgTrackId=\"9876543\" -

    int t1, t2, t3, n0 = 0, n1 = 0, n2 = 0;

    t1 = indexOf(str, "title", 0);
    if(t1 > 0){
        strcpy(chbuf, "StreamTitle="); n0 = 12;
        t2 = t1 + 7; // title="
        t3 = indexOf(str, "\"", t2);
        while(str[t3 - 1] == '\\'){
            t3 = indexOf(str, "\"", t3 + 1);
        }
        if(t2 < 0 || t2 > t3) return false;
        n1 = t3 - t2;
        strncpy(chbuf + n0, str + t2, n1);
        chbuf[n1] = '\0';
    }

    t1 = indexOf(str, "artist", 0);
    if(t1 > 0){
        strcpy(chbuf + n0 + n1, " - ");   n1 += 3;
        t2 = indexOf(str, "=\"", t1); t2 += 2;
        t3 = indexOf(str, "\"", t2);
        if(t2 < 0 || t2 > t3) return false;
        n2 = t3 - t2;
        strncpy(chbuf + n0 + n1, str + t2, n2);
        chbuf[n0 + n1 + n2] = '\0';
        chbuf[n2] = '\0';
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::processLocalFile() {
    if(extraDecoder.active()){processExtraLocalFile();return;}

    if(!(audiofile && m_f_running && getDatamode() == AUDIO_LOCALFILE)) return;
    int bytesDecoded = 0;
    uint32_t bytesCanBeWritten = 0;
    uint32_t bytesCanBeRead = 0;
    int32_t bytesAddedToBuffer = 0;
    static bool f_stream;

    if(m_f_firstCall) {  // runs only one time per connection, prepare for start
        m_f_firstCall = false;
        f_stream = false;
        return;
    }

    if(!f_stream && m_controlCounter == 100) {
        f_stream = true;
        AUDIO_INFO("stream ready");
        if(m_resumeFilePos){
            if(m_resumeFilePos < m_audioDataStart) m_resumeFilePos = m_audioDataStart;
            if(m_avr_bitrate) m_audioCurrentTime = ((m_resumeFilePos - m_audioDataStart) / m_avr_bitrate) * 8;
            audiofile.seek(m_resumeFilePos);
            InBuff.resetBuffer();
            if(m_f_Log) log_i("m_resumeFilePos %i", m_resumeFilePos);
        }
    }

    bytesCanBeWritten = InBuff.writeSpace();
    const size_t readPosition=getFilePos();
    size_t readEnd=m_file_size;
    //----------------------------------------------------------------------------------------------------
    // some files contain further data after the audio block (e.g. pictures).
    // In that case, the end of the audio block is not the end of the file. An 'eof' has to be forced.
    if((m_controlCounter == 100) && (m_contentlength > 0)) { // fileheader was read
           if(m_contentlength < readEnd) readEnd=m_contentlength;
    }
    //----------------------------------------------------------------------------------------------------

    // Keep the PSRAM ring; bound each blocking SD operation independently.
    bytesCanBeWritten=localsd::request(bytesCanBeWritten,readPosition,readEnd);
    if(bytesCanBeWritten){
        const uint32_t readStarted=micros();
        {pathprobe::Scope readProbe(pathprobe::Read);
         bytesAddedToBuffer=audiofile.read(InBuff.getWritePtr(),bytesCanBeWritten);
         readProbe.units(bytesAddedToBuffer);}
        if(readPosition==0 && bytesAddedToBuffer>0)Serial.printf("[SD-FIRST-READ] requested=%lu received=%ld elapsed_us=%lu\n",
            (unsigned long)bytesCanBeWritten,(long)bytesAddedToBuffer,(unsigned long)(micros()-readStarted));
    }
    if(bytesAddedToBuffer > 0) {
        InBuff.bytesWritten(bytesAddedToBuffer);
        extraReadWaiting=false;
    }
    const bool fileDone=getFilePos()>=readEnd;
    if(bytesCanBeWritten && bytesAddedToBuffer<=0 && !fileDone){
        // A temporary zero/error before the known end is not EOF.
        const uint32_t now=millis();
        if(!extraReadWaiting){extraReadWaiting=true;extraReadWaitSince=now;}
        if(localsd::stalled(now,extraReadWaitSince)){AUDIO_ERROR("SD read stalled before EOF");stopSong();return;}
        vTaskDelay(1);
    }
    bytesCanBeRead = InBuff.bufferFilled();
    if(bytesCanBeRead > InBuff.getMaxBlockSize()) bytesCanBeRead = InBuff.getMaxBlockSize();
    if(bytesCanBeRead == InBuff.getMaxBlockSize()) { // mp3 or aac frame complete?

        if(m_controlCounter != 100){
            bytesDecoded = readAudioHeader(bytesCanBeRead);
        }
        else {
            bytesDecoded = sendBytes(InBuff.getReadPtr(), bytesCanBeRead);
        }
        if(bytesDecoded > 0) {InBuff.bytesWasRead(bytesDecoded); return;}
        if(bytesDecoded < 0) {  // no syncword found or decode error, try next chunk
            InBuff.bytesWasRead(200); // try next chunk
            m_bytesNotDecoded += 200;
            return;
        }
        return;
    }

    if(fileDone) {  // Physical or header-defined end; never a transient read failure.
        bytesCanBeRead = InBuff.bufferFilled();
        if(bytesCanBeRead > 200){
            if(bytesCanBeRead > InBuff.getMaxBlockSize()) bytesCanBeRead = InBuff.getMaxBlockSize();
            bytesDecoded = sendBytes(InBuff.getReadPtr(), bytesCanBeRead); // play last chunk(s)
            if(bytesDecoded > 0){
                InBuff.bytesWasRead(bytesDecoded);
                return;
            }
            // FLAC emits one frame in several PCM chunks without consuming
            // further compressed bytes. EOF must wait for its final chunk.
            if(m_f_running&&(m_codec==CODEC_FLAC||m_codec==CODEC_OGG_FLAC)&&FLACOutputPending())return;
        }
        InBuff.resetBuffer();
        playI2Sremains();

        if(m_f_loop  && f_stream){  //eof
            AUDIO_INFO("loop from: %lu to: %lu", getFilePos(), m_audioDataStart); //TEST loop
            setFilePos(m_audioDataStart);
            if(m_codec == CODEC_FLAC) FLACDecoderReset();
            /*
                The current time of the loop mode is not reset,
                which will cause the total audio duration to be exceeded.
                For example: current time   ====progress bar====>  total audio duration
                                3:43        ====================>        3:33
            */
            m_audioCurrentTime = 0;
            return;
        } //TEST loop
        f_stream = false;
        m_streamType = ST_NONE;
#ifdef SDFATFS_USED
        audiofile.getName(chbuf, sizeof(chbuf));
        char *afn =strdup(chbuf);
#else
        char *afn =strdup(audiofile.name()); // store temporary the name
#endif
        stopSong();
        if(m_codec == CODEC_MP3)   MP3Decoder_FreeBuffers();
        if(m_codec == CODEC_AAC)   AACDecoder_FreeBuffers();
        if(m_codec == CODEC_M4A)   AACDecoder_FreeBuffers();
        if(m_codec == CODEC_FLAC) FLACDecoder_FreeBuffers();
        AUDIO_INFO("End of file \"%s\"", afn);
        if(audio_eof_mp3) audio_eof_mp3(afn);
        if(afn) {free(afn); afn = NULL;}
    }
}
//----------------------------------------------------------------------------------------------------------------------
void Audio::processWebStream() {

    // TLS available()/read() can decrypt/receive a record. Never put that work
    // between already decoded PCM slices: service pending output on this owner
    // first, without advancing the compressed ring or decoding another frame.
    if(!m_f_firstCall && getDatamode()==AUDIO_DATA && cooperativeWebDac() && m_validSamples){
        drainDecoded();
        return;
    }

    const uint16_t  maxFrameSize = InBuff.getMaxBlockSize();    // every mp3/aac frame is not bigger
    static ibtaudio::StartupBuffer startup;
    static netpoll::Gate pollGate;
    static bool     f_tmr_1s;
    static bool     f_stream;                                   // first audio data received
    static uint8_t  cnt_slow;
    static ibtaudio::LiveChunk chunks;
    static uint32_t tmr_1s;                                     // timer 1 sec
    static uint32_t loopCnt;                                    // count loops if clientbuffer is empty
    // One edge record per stalled BT episode. This does not change decoding.
    static uint32_t lastPlayAttemptAt=0;
    static bool metadataEdgeLogged=false;

    // first call, set some values to default  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_firstCall) { // runs only ont time per connection, prepare for start
        m_f_firstCall = false;
        f_stream = false;
        f_tmr_1s = false;
        directrx::reset();
        pollGate.reset();netpoll::applied().store(0,std::memory_order_relaxed);
        const bool refillFirst=cooperativeWebDac();
        m_streamBuffering=refillFirst;
        startup.begin(millis(),uint32_t(InBuff.bufferFilled()+InBuff.freeSpace()),refillFirst);
        if(!m_f_tts)Serial.printf("[PREBUFFER] WAIT policy=%s target=%lu capacity=%lu\n",refillFirst?"LIGHT_TLS_75":"CAPPED_25",(unsigned long)startup.threshold(),(unsigned long)(InBuff.bufferFilled()+InBuff.freeSpace()));
        if(refillFirst)Serial.println("[WEB-DAC-SERVICE] slice_frames=512 pending_pcm=DRAIN_BEFORE_RX refill=BETWEEN_FRAMES low_input=CONTINUOUS_NO_REBUFFER_GATE heap_added=0");
        cnt_slow = 0;
        chunks.reset();
        loopCnt = 0;
        tmr_1s = millis();
        lastPlayAttemptAt=tmr_1s;
        metadataEdgeLogged=false;
        m_metacount = m_metaint;
        readMetadata(0, true); // reset all static vars
        if(m_f_tts)netprefetch::noteBypass("TTS",m_f_ssl);
        else if(m_f_ssl && btaudio::outputSelected() && btaudio::transmitting()
                && m_codec==CODEC_MP3)netprefetch::begin(_client,true);
        else if(m_f_ssl)netprefetch::noteBypass(!btaudio::outputSelected()?"HTTPS_DAC":!btaudio::transmitting()?"HTTPS_BT_WAIT":"HTTPS_CODEC",true);
        else netprefetch::begin(_client); // Plain live stream only; failure keeps the direct path.
        Serial.printf("[NET-SELECT] ssl=%u codec=%u chunked=%u bt_out=%u bt_tx=%u prefetch=%u\n",unsigned(m_f_ssl),unsigned(m_codec),unsigned(m_f_chunked),unsigned(btaudio::outputSelected()),unsigned(btaudio::transmitting()),unsigned(netprefetch::active()));
    }

    if(getDatamode() != AUDIO_DATA) return;              // guard
    // The75% gate belongs to startup only. During playback, the complete-frame
    // check in playAudioData() waits only for the missing frame bytes; never
    // turn an ordinary partial TCP/TLS arrival into a new startup pause.
    if(!f_stream && !m_f_tts){
        const auto ready=startup.poll(millis(),uint32_t(InBuff.bufferFilled()));
        if(ready==ibtaudio::StartupBuffer::Ready){
            f_stream=true;
            m_streamBuffering=false;
            Serial.printf("[PREBUFFER] READY fill=%lu target=%lu\n",(unsigned long)InBuff.bufferFilled(),(unsigned long)startup.threshold());
            AUDIO_INFO("stream ready");
        }else if(ready!=ibtaudio::StartupBuffer::Waiting){
            Serial.printf("[PREBUFFER] FAIL reason=%s fill=%lu target=%lu\n",ready==ibtaudio::StartupBuffer::Stalled?"NO_PROGRESS":"DEADLINE",(unsigned long)InBuff.bufferFilled(),(unsigned long)startup.threshold());
            netprefetch::diagnostics(); // Preserve the owner snapshot before stopSong resets it.
            stopSong();
            AUDIO_ERROR("Puffer feltoltes sikertelen");
            return; // Existing recovery may reconnect; never bypass the startup gate.
        }
    }
    // Diagnostic TLS polling cadence; skipping I/O MUST NOT skip decoding below.
    // Never reuse an earlier positive available count (chunk/ICY parsers own framing).
    const uint32_t pollMs=netpoll::period(netpoll::requested().load(std::memory_order_relaxed),
        m_f_ssl && !netprefetch::active() && f_stream && !m_f_tts && m_codec==CODEC_MP3 &&
        btaudio::outputSelected() && btaudio::producerReady(),uint32_t(InBuff.bufferFilled()));
    netpoll::applied().store(pollMs,std::memory_order_relaxed);
    bool batch=false;
#if IBT_NO_PSRAM && defined(IBT_LIGHT4_DNS_AUTO)
    batch=m_f_ssl&&!netprefetch::active()&&!m_f_tts&&m_codec==CODEC_MP3&&!btaudio::outputSelected();
#endif
    const uint32_t refillAt=micros();
    ibtaudio::RefillBudget refill(batch,refillAt);
    do {
    refill.enter();
    uint32_t availableBytes=0;
    // A full compressed ring cannot accept a TLS record. Decode first rather
    // than provoking another TLS receive allocation on every audio-owner tick.
    const bool receiveRoom=!IBT_NO_PSRAM || InBuff.writeSpace()>0;
    if(receiveRoom && pollGate.poll(millis(),pollMs)){
        netpoll::polls().fetch_add(1,std::memory_order_relaxed);
        pathprobe::Scope probe(pathprobe::Poll);
        availableBytes=netprefetch::active()?netprefetch::available():directrx::available(_client);
        probe.units(availableBytes);
#if IBT_NO_PSRAM
        if(!availableBytes && !netprefetch::active() && !_client->connected() && !m_streamTransportClosed){
            m_streamTransportClosed=true;
            Serial.printf("[STREAM-CLOSED] buffered=%u free=%u largest=%u\n",unsigned(InBuff.bufferFilled()),
                unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
                unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
        }
#endif
    }else netpoll::skips().fetch_add(1,std::memory_order_relaxed);
    // chunked data tramsfer - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_chunked && availableBytes){
        chunks.poll([&](){return netprefetch::active()?netprefetch::read():_client->read();});
        if(chunks.failed()||chunks.done()){
            stopSong();AUDIO_ERROR("Chunked stream ended or invalid");return;
        }
        availableBytes=chunks.payload(netprefetch::active()?netprefetch::available():directrx::available(_client,true));
    }
    // we have metadata  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_metadata && availableBytes){
        if(m_metacount == 0) {
            const unsigned used=readMetadata(ibtmeta::readBudget(min(availableBytes,refill.remaining())));
            if(m_f_chunked)chunks.consume(used);
            if(!metadataEdgeLogged && f_stream && btaudio::transmitting() &&
               InBuff.bufferFilled()>=245760 && uint32_t(millis()-lastPlayAttemptAt)>=2000){
                metadataEdgeLogged=true;
                Serial.printf("[DECODE-EDGE] reason=METADATA_RETURN no_play_ms=%lu used=%u avail=%lu metacount=%lu metaint=%lu input=%lu prefetch=%u\n",
                    (unsigned long)uint32_t(millis()-lastPlayAttemptAt),used,
                    (unsigned long)availableBytes,(unsigned long)m_metacount,
                    (unsigned long)m_metaint,(unsigned long)InBuff.bufferFilled(),
                    unsigned(netprefetch::active()));
            }
            // ICY can span several HTTP chunks. Its incremental parsing must
            // not withhold already buffered PCM from the DAC between chunks.
            refill.account(used);
            if(!used || getDatamode()!=AUDIO_DATA || !refill.more(micros(),uint32_t(InBuff.bufferFilled()),uint32_t(InBuff.bufferFilled()+InBuff.freeSpace())))break;
            continue;
        }
        availableBytes = min(availableBytes, m_metacount);
    }
    if(!availableBytes)break;
    if(availableBytes) {
        availableBytes = min(availableBytes, (uint32_t)InBuff.writeSpace());
        availableBytes=min(availableBytes,min(uint32_t(16384),refill.remaining())); // Bound both direct and prefetched reads.
        int16_t bytesAddedToBuffer;
        {pathprobe::Scope probe(pathprobe::Read);bytesAddedToBuffer=netprefetch::active()?netprefetch::read(InBuff.getWritePtr(),availableBytes):directrx::read(_client,InBuff.getWritePtr(),availableBytes);probe.units(bytesAddedToBuffer);}

        if(bytesAddedToBuffer > 0) {
            if(m_f_metadata)            m_metacount  -= bytesAddedToBuffer;
            if(m_f_chunked)             chunks.consume(bytesAddedToBuffer);
            InBuff.bytesWritten(bytesAddedToBuffer);
            refill.account(uint32_t(bytesAddedToBuffer));
        }

        if(m_f_tts && InBuff.bufferFilled() > maxFrameSize && !f_stream) {  // waiting for buffer filled
            f_stream = true;  // ready to play the audio data
            AUDIO_INFO("stream ready");
        }
        if(bytesAddedToBuffer<=0)break;
    }

    }while(refill.more(micros(),uint32_t(InBuff.bufferFilled()),uint32_t(InBuff.bufferFilled()+InBuff.freeSpace())));
    if(batch)directrx::refillSample(refill.rounds,refill.bytes,uint32_t(micros()-refillAt));
    // timer, triggers every second  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if((tmr_1s + 1000) < millis()) {
        f_tmr_1s = true;                                        // flag will be set every second for one loop only
        tmr_1s = millis();
    }
    // if the buffer is often almost empty issue a warning - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(InBuff.bufferFilled() > maxFrameSize) {f_tmr_1s = false; cnt_slow = 0; loopCnt = 0;}
    if(f_tmr_1s){
        cnt_slow ++;
        if(cnt_slow > 50){cnt_slow = 0; f_tmr_1s = false; AUDIO_INFO("slow stream, dropouts are possible");}
    }
    // if the buffer can't filled for several seconds try a new connection - - - - - - - - - - - - - - - - - - - - - - -
    // Player owns elapsed-time recovery; do not also reconnect by loop count.
    // play audio data - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(f_stream){
        // Both DAC and BT use their actual sink backpressure. A loop-count
        // gate adds input-poll/yield latency without representing PCM demand.
        lastPlayAttemptAt=millis();metadataEdgeLogged=false;
        lightmem::Phase memoryPhase("WEB_DECODE_DAC");
        playAudioData();
    }
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::processWebFile() {

    const uint32_t  maxFrameSize = InBuff.getMaxBlockSize();    // every mp3/aac frame is not bigger
    static bool     f_stream;                                   // first audio data received
    static bool     f_webFileDataComplete;                      // all file data received
    static uint32_t byteCounter;                                // count received data
    static uint32_t chunkSize;                                  // chunkcount read from stream
    static size_t   audioDataCount;                             // counts the decoded audiodata only

    // first call, set some values to default - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_firstCall) { // runs only ont time per connection, prepare for start
        netprefetch::noteBypass("WEBFILE",m_f_ssl);
        m_f_firstCall = false;
        f_webFileDataComplete = false;
        f_stream = false;
        byteCounter = 0;
        chunkSize = 0;
        audioDataCount = 0;
    }

    if(!m_contentlength) {log_e("webfile without contentlength!"); stopSong(); return;} // guard

    uint32_t availableBytes = _client->available(); // available from stream

    // chunked data tramsfer - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_chunked){
        uint8_t readedBytes = 0;
        if(!chunkSize) chunkSize = chunkedDataTransfer(&readedBytes);
        availableBytes = min(availableBytes, chunkSize);
    }

    // if the buffer is often almost empty issue a warning  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(!f_webFileDataComplete && f_stream){
        slowStreamDetection(InBuff.bufferFilled(), maxFrameSize);
    }

    availableBytes = min((uint32_t)InBuff.writeSpace(), availableBytes);
    availableBytes = min(m_contentlength - byteCounter, availableBytes);
    if(m_audioDataSize) availableBytes = min(m_audioDataSize - (byteCounter - m_audioDataStart), availableBytes);

    int16_t bytesAddedToBuffer = _client->read(InBuff.getWritePtr(), availableBytes);

    if(bytesAddedToBuffer > 0) {
        byteCounter  += bytesAddedToBuffer;  // Pull request #42
        if(m_f_chunked)             m_chunkcount   -= bytesAddedToBuffer;
        if(m_controlCounter == 100) audioDataCount += bytesAddedToBuffer;
        InBuff.bytesWritten(bytesAddedToBuffer);
    }

    if(InBuff.bufferFilled() > maxFrameSize && !f_stream) {  // waiting for buffer filled
        f_stream = true;  // ready to play the audio data
        uint16_t filltime = millis() - m_t0;
        AUDIO_INFO("stream ready\nbuffer filled in %d ms", filltime);
    }

    if(!f_stream) return;

    // we have a webfile, read the file header first - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter != 100){
        InBuff.bytesWasRead(readAudioHeader(availableBytes));
        return;
    }

    // end of webfile reached? - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(f_webFileDataComplete && InBuff.bufferFilled() < InBuff.getMaxBlockSize()){
        if(InBuff.bufferFilled()){
            if(!readID3V1Tag()){
                int bytesDecoded = sendBytes(InBuff.getReadPtr(), InBuff.bufferFilled());
                if(bytesDecoded > 2){InBuff.bytesWasRead(bytesDecoded); return;}
            }
        }
        playI2Sremains();
        stopSong(); // Correct close when play known length sound #74 and before callback #11
        if(m_f_tts){
            AUDIO_INFO("End of speech: \"%s\"", m_lastHost);
            if(audio_eof_speech) audio_eof_speech(m_lastHost);
        }
        else{
            AUDIO_INFO("End of webstream: \"%s\"", m_lastHost);
            if(audio_eof_stream) audio_eof_stream(m_lastHost);
        }
        return;
    }

    if(byteCounter == m_contentlength)                    {f_webFileDataComplete = true;}
    if(byteCounter - m_audioDataStart == m_audioDataSize) {f_webFileDataComplete = true;}

    // play audio data - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(f_stream){
        static uint8_t cnt = 0;
        uint8_t compression;
        if(m_codec == CODEC_WAV)  compression = 1;
        if(m_codec == CODEC_FLAC) compression = 2;
        else compression = 6;
        cnt++;
        if(cnt == compression){playAudioData(); cnt = 0;}
    }
    return;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::processWebStreamTS() {

    const uint16_t  maxFrameSize = InBuff.getMaxBlockSize();    // every mp3/aac frame is not bigger
    uint32_t        availableBytes;                             // available bytes in stream
    static bool     f_tmr_1s;
    static bool     f_stream;                                   // first audio data received
    static bool     f_firstPacket;
    static int      bytesDecoded;
    static uint32_t byteCounter;                                // count received data
    static uint32_t tmr_1s;                                     // timer 1 sec
    static uint32_t loopCnt;                                    // count loops if clientbuffer is empty
    static uint8_t  ts_packet[188];                             // m3u8 transport stream is 188 bytes long
    uint8_t         ts_packetStart = 0;
    uint8_t         ts_packetLength = 0;
    static uint8_t  ts_packetPtr = 0;
    const uint8_t   ts_packetsize = 188;
    static size_t   chunkSize = 0;
    (void)bytesDecoded;
    // first call, set some values to default - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_firstCall) { // runs only ont time per connection, prepare for start
        netprefetch::noteBypass("HLS_TS",m_f_ssl);
        f_stream = false;
        f_firstPacket = true;
        byteCounter = 0;
        bytesDecoded = 0;
        chunkSize = 0;
        loopCnt = 0;
        tmr_1s = millis();
        m_t0 = millis();
        ts_packetPtr = 0;
        m_controlCounter = 0;
        m_f_firstCall = false;
    }

    if(getDatamode() != AUDIO_DATA) return;        // guard

    if(InBuff.freeSpace() < maxFrameSize && f_stream){playAudioData(); return;}

    availableBytes = _client->available();
    if(availableBytes){
        uint8_t readedBytes = 0;
        if(m_f_chunked) chunkSize = chunkedDataTransfer(&readedBytes);
        int res = _client->read(ts_packet + ts_packetPtr, ts_packetsize - ts_packetPtr);
        if(res > 0){
            ts_packetPtr += res;
            byteCounter += res;
            if(ts_packetPtr < ts_packetsize)  return;
            ts_packetPtr = 0;
            if(f_firstPacket){  // search for ID3 Header in the first packet
                f_firstPacket = false;
                uint8_t ID3_HeaderSize = process_m3u8_ID3_Header(ts_packet);
                if(ID3_HeaderSize > ts_packetsize){
                    log_e("ID3 Header is too big");
                    stopSong();
                    return;
                }
                if(ID3_HeaderSize){
                    memcpy(ts_packet, &ts_packet[ID3_HeaderSize], ts_packetsize - ID3_HeaderSize);
                    ts_packetPtr = ts_packetsize - ID3_HeaderSize;
                    return;
                }
            }
            ts_parsePacket(&ts_packet[0], &ts_packetStart, &ts_packetLength);

            if(ts_packetLength) {
                size_t ws = InBuff.writeSpace();
                if(ws >= ts_packetLength){
                    memcpy(InBuff.getWritePtr(), ts_packet + ts_packetStart, ts_packetLength);
                    InBuff.bytesWritten(ts_packetLength);
                }
                else{
                    memcpy(InBuff.getWritePtr(), ts_packet + ts_packetStart, ws);
                    InBuff.bytesWritten(ws);
                    memcpy(InBuff.getWritePtr(), &ts_packet[ws + ts_packetStart], ts_packetLength -ws);
                    InBuff.bytesWritten(ts_packetLength -ws);
                }
            }
            if(byteCounter == m_contentlength  || byteCounter == chunkSize){
                byteCounter = 0;
                m_f_continue = true;
            }
            if(byteCounter > m_contentlength) log_e("byteCounter overflow");
        }

    }

    // timer, triggers every second - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if((tmr_1s + 1000) < millis()) {
        f_tmr_1s = true;                                        // flag will be set every second for one loop only
        tmr_1s = millis();
    }

    // if the buffer is often almost empty issue a warning  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(InBuff.bufferFilled() < maxFrameSize && f_stream){
        static uint8_t cnt_slow = 0;
        cnt_slow ++;
        if(f_tmr_1s) {
            if(cnt_slow > 50 && audio_info) audio_info("slow stream, dropouts are possible");
            f_tmr_1s = false;
            cnt_slow = 0;
        }
    }

    // if the buffer can't filled for several seconds try a new connection  - - - - - - - - - - - - - - - - - - - - - -
    if(f_stream && !availableBytes){
        loopCnt++;
        if(loopCnt > 200000) {              // wait several seconds
            loopCnt = 0;
            AUDIO_INFO("Stream lost -> try new connection");
            httpPrint(m_lastHost);
            return;
        }
    }
    if(availableBytes) loopCnt = 0;

    // buffer fill routine  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(true) { // statement has no effect
        if(InBuff.bufferFilled() > maxFrameSize && !f_stream) {  // waiting for buffer filled
            f_stream = true;  // ready to play the audio data
            uint16_t filltime = millis() - m_t0;
            if(m_f_Log) AUDIO_INFO("stream ready");
            if(m_f_Log) AUDIO_INFO("buffer filled in %d ms", filltime);
        }
        if(!f_stream) return;
    }

    // play audio data - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(f_stream){
        static uint8_t cnt = 0;
        cnt++;
        if(cnt == 6){playAudioData(); cnt = 0;}
    }
    return;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::processWebStreamHLS() {

    const uint16_t  maxFrameSize = InBuff.getMaxBlockSize();    // every mp3/aac frame is not bigger
    uint32_t        availableBytes;                             // available bytes in stream
    static bool     f_tmr_1s;
    static bool     f_stream;                                   // first audio data received
    static int      bytesDecoded;
    static uint32_t byteCounter;                                // count received data
    static size_t   chunkSize = 0;
    static uint32_t tmr_1s;                                     // timer 1 sec
    static uint32_t loopCnt;                                    // count loops if clientbuffer is empty
    (void)bytesDecoded;
    // first call, set some values to default - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_firstCall) { // runs only ont time per connection, prepare for start
        netprefetch::noteBypass("HLS",m_f_ssl);
        f_stream = false;
        byteCounter = 0;
        bytesDecoded = 0;
        chunkSize = 0;
        loopCnt = 0;
        tmr_1s = millis();
        m_t0 = millis();
         m_f_firstCall = false;
    }

    if(getDatamode() != AUDIO_DATA) return;        // guard

    availableBytes = _client->available();
    if(availableBytes){
        uint8_t readedBytes = 0;
        if(m_f_chunked) chunkSize = chunkedDataTransfer(&readedBytes);
        size_t bytesWasWritten = 0;
        if(InBuff.writeSpace() >= availableBytes){
            bytesWasWritten = _client->read(InBuff.getWritePtr(), availableBytes);
        }
        else{
            bytesWasWritten = _client->read(InBuff.getWritePtr(), InBuff.writeSpace());
        }
        InBuff.bytesWritten(bytesWasWritten);
        byteCounter += bytesWasWritten;
        if(byteCounter == m_contentlength || byteCounter == chunkSize){
            byteCounter = 0;
            m_f_continue = true;
        }
    }

    // timer, triggers every second - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if((tmr_1s + 1000) < millis()) {
        f_tmr_1s = true;                                        // flag will be set every second for one loop only
        tmr_1s = millis();
    }

    // if the buffer is often almost empty issue a warning  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(InBuff.bufferFilled() < maxFrameSize && f_stream){
        static uint8_t cnt_slow = 0;
        cnt_slow ++;
        if(f_tmr_1s) {
            if(cnt_slow > 25 && audio_info) audio_info("slow stream, dropouts are possible");
            f_tmr_1s = false;
            cnt_slow = 0;
        }
    }

    // if the buffer can't filled for several seconds try a new connection  - - - - - - - - - - - - - - - - - - - - - -
    if(f_stream && !availableBytes){
        loopCnt++;
        if(loopCnt > 200000) {              // wait several seconds
            loopCnt = 0;
            AUDIO_INFO("Stream lost -> try new connection");
            httpPrint(m_lastHost);
            return;
        }
    }
    if(availableBytes) loopCnt = 0;

    if(InBuff.bufferFilled() > maxFrameSize && !f_stream) {  // waiting for buffer filled
        f_stream = true;  // ready to play the audio data
        uint16_t filltime = millis() - m_t0;
        if(m_f_Log) AUDIO_INFO("stream ready");
        if(m_f_Log) AUDIO_INFO("buffer filled in %d ms", filltime);
    }

    // play audio data - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(f_stream) playAudioData();
    return;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::playAudioData(){
    // Even an empty compressed ring must not strand already decoded PCM.
    // No new decode/read-pointer advance until this frame is fully handed off.
    if(cooperativeWebDac() && m_validSamples){drainDecoded();return;}

    const size_t filled=InBuff.bufferFilled();size_t required=InBuff.getMaxBlockSize();
    uint8_t* data=nullptr;
    if(IBT_NO_PSRAM&&m_streamType==ST_WEBSTREAM&&m_codec==CODEC_MP3&&filled>=4){
        data=InBuff.getReadPtr();required=ibtaudio::mp3ReadBudget(data,filled,required);
    }
    if(filled<required){pathprobe::low();if(m_streamType==ST_WEBSTREAM)directrx::lowInput();return;}
    int bytesDecoded=sendBytes(data?data:InBuff.getReadPtr(),required);

    if(bytesDecoded < 0) {  // no syncword found or decode error, try next chunk
        log_i("err bytesDecoded %i", bytesDecoded);
        uint8_t next = 200;
        if(InBuff.bufferFilled() < next) next = InBuff.bufferFilled();
        InBuff.bytesWasRead(next); if(m_streamType==ST_WEBSTREAM)directrx::consumed(next); // try next chunk
        m_bytesNotDecoded += next;
    }
    else {
        if(bytesDecoded > 0) {InBuff.bytesWasRead(bytesDecoded); if(m_streamType==ST_WEBSTREAM)directrx::consumed(bytesDecoded); return;}
        if(bytesDecoded == 0) return; // syncword at pos0 found
    }

    return;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::parseHttpResponseHeader() { // this is the response to a GET / request
    if(getDatamode() != HTTP_RESPONSE_HEADER) return false;
    if(_client->available() == 0) {
      if (!m_headerWaiting) {m_headerWaitAt=millis();m_headerWaiting=true;}
      if (uint32_t(millis()-m_headerWaitAt) > HEADER_TIMEOUT) {
        m_headerWaiting = false;
        if(audio_showstation) audio_showstation("");
        if(audio_icydescription) audio_icydescription("");
        if(audio_icyurl) audio_icyurl("");
        AUDIO_ERROR("Host not available");
        m_lastHost[0] = '\0';
        setDatamode(AUDIO_NONE);
        stopSong();
      }
      return false;
    }
    m_headerWaiting = false;
    char rhl[512]; // responseHeaderline
    bool ct_seen = false;
    uint32_t ctime = millis();
    uint32_t timeout = 2500; // ms
    while(true){  // outer while
        uint16_t pos = 0;
        if((millis() - ctime) > timeout) {
            log_e("timeout");
            goto exit;
        }
        while(_client->available()){
            uint8_t b = _client->read();
            if(b == '\n') {
                if(!pos){ // empty line received, is the last line of this responseHeader
                    if(ct_seen) goto lastToDo;
                    else goto exit;
                }
                break;
            }
            if(b == '\r') rhl[pos] = 0;
            if(b < 0x20) continue;
            rhl[pos] = b;
            pos++;
            if(pos == 511){pos = 510; continue;}
            if(pos == 510){
                rhl[pos] = '\0';
                if(m_f_Log) log_i("responseHeaderline overflow");
            }
        } // inner while

        if(!pos){vTaskDelay(3); continue;}

        if(m_f_Log) {log_i("httpResponseHeader: %s", rhl);}

        int16_t posColon = indexOf(rhl, ":", 0); // lowercase all letters up to the colon
        if(posColon >= 0) {
            for(int i=0; i< posColon; i++) {
                rhl[i] = toLowerCase(rhl[i]);
            }
        }

        if(startsWith(rhl, "HTTP/")){ // HTTP status error code
            char statusCode[5];
            statusCode[0] = rhl[9];
            statusCode[1] = rhl[10];
            statusCode[2] = rhl[11];
            statusCode[3] = '\0';
            int sc = atoi(statusCode);
            if(sc > 310){ // e.g. HTTP/1.1 301 Moved Permanently
                if(audio_showstreamtitle) audio_showstreamtitle(rhl);
                goto exit;
            }
        }

        else if(startsWith(rhl, "content-type:")){ // content-type: text/html; charset=UTF-8
            int idx = indexOf(rhl + 13, ";");
            if(idx >0) rhl[13 + idx] = '\0';
            if(parseContentType(rhl + 13)) ct_seen = true;
            else goto exit;
        }

        else if(startsWith(rhl, "location:")) {
            int pos = indexOf(rhl, "http", 0);
            if(pos >= 0){
                const char* c_host = (rhl + pos);
                if(strcmp(c_host, m_lastHost) != 0) { // prevent a loop
                    int pos_slash = indexOf(c_host, "/", 9);
                    if(pos_slash > 9){
                        if(!strncmp(c_host, m_lastHost, pos_slash)){
                            AUDIO_INFO("redirect to new extension at existing host \"%s\"", c_host);
                            if(m_playlistFormat == FORMAT_M3U8) {
                                strcpy(m_lastHost, c_host);
                                m_f_m3u8data = true;
                            }
                            httpPrint(c_host);
                            while(_client->available()) _client->read(); // empty client buffer
                            return true;
                        }
                    }
                    AUDIO_INFO("redirect to new host \"%s\"", c_host);
                    connecttohost(c_host);
                    return true;
                }
            }
        }

        else if(startsWith(rhl, "content-encoding:")){
            if(indexOf(rhl, "gzip")){
                AUDIO_INFO("can't extract gzip");
                goto exit;
            }
        }

        else if(startsWith(rhl, "content-disposition:")) {
            int pos1, pos2; // pos3;
            // e.g we have this headerline:  content-disposition: attachment; filename=stream.asx
            // filename is: "stream.asx"
            pos1 = indexOf(rhl, "filename=", 0);
            if(pos1 > 0){
                pos1 += 9;
                if(rhl[pos1] == '\"') pos1++;  // remove '\"' around filename if present
                pos2 = strlen(rhl);
                if(rhl[pos2 - 1] == '\"') rhl[pos2 - 1] = '\0';
            }
            AUDIO_INFO("Filename is %s", rhl + pos1);
        }

        // if(startsWith(rhl, "set-cookie:")         ||
        //         startsWith(rhl, "pragma:")        ||
        //         startsWith(rhl, "expires:")       ||
        //         startsWith(rhl, "cache-control:") ||
        //         startsWith(rhl, "icy-pub:")       ||
        //         startsWith(rhl, "p3p:")           ||
        //         startsWith(rhl, "accept-ranges:") ){
        //     ; // do nothing
        // }

        else if(startsWith(rhl, "connection:")) {
            if(indexOf(rhl, "close", 0) >= 0) {; /* do nothing */}
        }

        else if(startsWith(rhl, "icy-genre:")) {
            ; // do nothing Ambient, Rock, etc
        }

        else if(startsWith(rhl, "icy-br:")) {
            const char* c_bitRate = (rhl + 7);
            int32_t br = atoi(c_bitRate); // Found bitrate tag, read the bitrate in Kbit
            br = br * 1000;
            setBitrate(br);
            sprintf(chbuf, "%lu", getBitRate());
            if(audio_bitrate) audio_bitrate(chbuf);
        }

        else if(startsWith(rhl, "icy-metaint:")) {
            const char* c_metaint = (rhl + 12);
            int32_t i_metaint = atoi(c_metaint);
            m_metaint = i_metaint;
            if(m_metaint) m_f_metadata = true;                            // Multimediastream
        }

        else if(startsWith(rhl, "icy-name:")) {
            char* c_icyname = (rhl + 9); // Get station name
            trim(c_icyname);
            if(strlen(c_icyname) > 0) {
                if(!m_f_Log) AUDIO_INFO("icy-name: %s", c_icyname);
                if(audio_showstation) audio_showstation(c_icyname);
            }
        }

        else if(startsWith(rhl, "content-length:")) {
            const char* c_cl = (rhl + 15);
            int32_t i_cl = atoi(c_cl);
            m_contentlength = i_cl;
            m_streamType = ST_WEBFILE; // Stream comes from a fileserver
            if(m_f_Log) AUDIO_INFO("content-length: %lu", m_contentlength);
        }

        else if(startsWith(rhl, "icy-description:")) {
            const char* c_idesc = (rhl + 16);
            while(c_idesc[0] == ' ') c_idesc++;
            latinToUTF8(rhl, sizeof(rhl)); // if already UTF-0 do nothing, otherwise convert to UTF-8
            if(audio_icydescription) audio_icydescription(c_idesc);
        }

        else if((startsWith(rhl, "transfer-encoding:"))){
            if(endsWith(rhl, "chunked") || endsWith(rhl, "Chunked") ) { // Station provides chunked transfer
                m_f_chunked = true;
                if(!m_f_Log) AUDIO_INFO("chunked data transfer");
                m_chunkcount = 0;                         // Expect chunkcount in DATA
            }
        }

        else if(startsWith(rhl, "icy-url:")) {
            char* icyurl = (rhl + 8);
            trim(icyurl);
            if(audio_icyurl) audio_icyurl(icyurl);
        }

        else if(startsWith(rhl, "www-authenticate:")) {
            AUDIO_INFO("authentification failed, wrong credentials?");
            goto exit;
        }
        else {;}
    } // outer while

    exit:  // termination condition
        if(audio_showstation) audio_showstation("");
        if(audio_icydescription) audio_icydescription("");
        if(audio_icyurl) audio_icyurl("");
        m_lastHost[0] = '\0';
        setDatamode(AUDIO_NONE);
        stopSong();
        return false;

    lastToDo:
        if(m_codec != CODEC_NONE){
            setDatamode(AUDIO_DATA); // Expecting data now
            if(!initializeDecoder()) return false;
            if(m_f_Log) {log_i("Switch to DATA, metaint is %d", m_metaint);}
            if(m_playlistFormat != FORMAT_M3U8 && audio_lasthost) audio_lasthost(m_lastHost);
            m_controlCounter = 0;
            m_f_firstCall = true;
        }
        else if(m_playlistFormat != FORMAT_NONE){
            setDatamode(AUDIO_PLAYLISTINIT); // playlist expected
            if(m_f_Log) {log_i("now parse playlist");}
        }
        else{
            AUDIO_INFO("unknown content found at: %s", m_lastHost);
            AUDIO_ERROR("unknown content found at: %s", m_lastHost);
            goto exit;
        }
        return true;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio:: initializeDecoder(){
#if IBT_NO_PSRAM
    // An extensionless station may identify as AAC/WAV/FLAC in its HTTP header.
    // Return the speculative MP3 workspace before selecting the actual codec.
    if(m_codec!=CODEC_MP3)MP3Decoder_FreeBuffers();
#endif
    extracodec::Kind kind=extracodec::Kind::None;
    if(m_codec==CODEC_EXTRA_M4A)kind=extracodec::Kind::M4a;
    if(m_codec==CODEC_OGG||m_codec==CODEC_OGG_OPUS)kind=extracodec::Kind::Ogg;
    if(m_codec==CODEC_AMR)kind=extracodec::Kind::Amr;
    if(m_codec==CODEC_AMRWB)kind=extracodec::Kind::AmrWb;
    if(m_codec==CODEC_EXTRA_WAV)kind=extracodec::Kind::Wav;
    if(m_codec==CODEC_TS)kind=extracodec::Kind::Ts;
    if(m_codec==CODEC_MPEG)kind=extracodec::Kind::Mpeg;
    if(kind!=extracodec::Kind::None){
        lightmem::checkpoint("EXTRA_OPEN_BEFORE");
        if(!extraDecoder.open(kind)){AUDIO_ERROR("Extra codec refused: PSRAM/header/decoder");return false;}
        lightmem::checkpoint("EXTRA_OPEN_AFTER");
        InBuff.changeMaxBlockSize(4096);m_controlCounter=100;m_audioDataStart=0;m_audioDataSize=m_file_size;
        m_resumeFilePos=0;Serial.printf("[CODEC-EXTRA] opened=%u experimental=1\n",unsigned(m_codec));return true;
    }
    switch(m_codec){
        case CODEC_MP3:
            if(!MP3Decoder_AllocateBuffers()) goto exit;
            AUDIO_INFO("MP3Decoder has been initialized, free Heap: %lu bytes", ESP.getFreeHeap());
            InBuff.changeMaxBlockSize(m_frameSizeMP3);
            break;
        case CODEC_AAC:
            if(!AACDecoder_IsInit()){
                if(!AACDecoder_AllocateBuffers()) goto exit;
                AUDIO_INFO("AACDecoder has been initialized, free Heap: %lu bytes", ESP.getFreeHeap());
                InBuff.changeMaxBlockSize(m_frameSizeAAC);
            }
            break;
        case CODEC_M4A:
            if(!AACDecoder_IsInit()){
                if(!AACDecoder_AllocateBuffers()) goto exit;
                AUDIO_INFO("AACDecoder has been initialized, free Heap: %lu bytes", ESP.getFreeHeap());
                InBuff.changeMaxBlockSize(m_frameSizeAAC);
            }
            break;
        case CODEC_FLAC:
            if(!FLACDecoder_AllocateBuffers()) goto exit;
            InBuff.changeMaxBlockSize(m_frameSizeFLAC);
            AUDIO_INFO("FLACDecoder has been initialized, free Heap: %lu bytes", ESP.getFreeHeap());
            break;
        case CODEC_WAV:
            InBuff.changeMaxBlockSize(m_frameSizeWav);
            break;
        case CODEC_OGG:
            m_codec = CODEC_OGG;
            AUDIO_INFO("ogg not supported");
            AUDIO_ERROR("ogg not supported");
            goto exit;
            break;
        default:
            goto exit;
            break;
    }
    return true;

    exit:
        AUDIO_ERROR("Nincs eleg RAM a dekoderhez: belso8bit=%lu blokk=%lu", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
        stopSong();
        return false;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::parseContentType(char* ct) {
    if(!strcmp(ct,"audio/ogg")||!strcmp(ct,"audio/opus")||!strcmp(ct,"audio/vorbis")){m_codec=CODEC_OGG;return true;}
    if(!strcmp(ct,"audio/amr")){m_codec=CODEC_AMR;return true;}
    if(!strcmp(ct,"audio/amr-wb")){m_codec=CODEC_AMRWB;return true;}


    enum : int {CT_NONE, CT_MP3, CT_AAC, CT_M4A, CT_WAV, CT_OGG, CT_FLAC, CT_PLS, CT_M3U, CT_ASX,
                CT_M3U8, CT_TXT, CT_AACP};

    strlwr(ct);
    trim(ct);

    m_codec = CODEC_NONE;
    int ct_val = CT_NONE;

    if(!strcmp(ct, "audio/mpeg"))            ct_val = CT_MP3;
    else if(!strcmp(ct, "audio/mpeg3"))      ct_val = CT_MP3;
    else if(!strcmp(ct, "audio/x-mpeg"))     ct_val = CT_MP3;
    else if(!strcmp(ct, "audio/x-mpeg-3"))   ct_val = CT_MP3;
    else if(!strcmp(ct, "audio/mp3"))        ct_val = CT_MP3;

    else if(!strcmp(ct, "audio/aac"))        ct_val = CT_AAC;
    else if(!strcmp(ct, "audio/x-aac"))      ct_val = CT_AAC;
    else if(!strcmp(ct, "audio/aacp")){      ct_val = CT_AAC; if(m_playlistFormat == FORMAT_M3U8) m_f_ts = true;}
    else if(!strcmp(ct, "video/mp2t")){      ct_val = CT_AAC; m_f_ts = true;} // assume AAC transport stream
    else if(!strcmp(ct, "audio/mp4"))        ct_val = CT_M4A;
    else if(!strcmp(ct, "audio/m4a"))        ct_val = CT_M4A;
    else if(!strcmp(ct, "audio/x-m4a"))      ct_val = CT_M4A;

    else if(!strcmp(ct, "audio/wav"))        ct_val = CT_WAV;
    else if(!strcmp(ct, "audio/x-wav"))      ct_val = CT_WAV;

    else if(!strcmp(ct, "audio/flac"))       ct_val = CT_FLAC;

    else if(!strcmp(ct, "audio/scpls"))      ct_val = CT_PLS;
    else if(!strcmp(ct, "audio/x-scpls"))    ct_val = CT_PLS;
    else if(!strcmp(ct, "application/pls+xml")) ct_val = CT_PLS;
    else if(!strcmp(ct, "audio/mpegurl"))    ct_val = CT_M3U;
    else if(!strcmp(ct, "audio/x-mpegurl"))  ct_val = CT_M3U;
    else if(!strcmp(ct, "audio/ms-asf"))     ct_val = CT_ASX;
    else if(!strcmp(ct, "video/x-ms-asf"))   ct_val = CT_ASX;

    else if(!strcmp(ct, "application/ogg"))  ct_val = CT_OGG;
    else if(!strcmp(ct, "application/vnd.apple.mpegurl")) ct_val = CT_M3U8;
    else if(!strcmp(ct, "application/x-mpegurl")) ct_val =CT_M3U8;

    else if(!strcmp(ct, "application/octet-stream")) ct_val = CT_TXT; // ??? listen.radionomy.com/1oldies before redirection
    else if(!strcmp(ct, "text/html"))        ct_val = CT_TXT;
    else if(!strcmp(ct, "text/plain"))       ct_val = CT_TXT;

    else if(ct_val == CT_NONE){
        AUDIO_INFO("ContentType %s not supported", ct);
        AUDIO_ERROR("ContentType %s not supported", ct);
        return false; // nothing valid had been seen
    }
    else {;}

    switch(ct_val){
        case CT_MP3:
            m_codec = m_expectedCodec==CODEC_MPEG?CODEC_MPEG:CODEC_MP3;
            if(m_f_Log) { log_i("ContentType %s, format is mp3", ct); } //ok is likely mp3
            if(audio_info) audio_info("format is mp3");
            break;
        case CT_AAC:
            m_codec = CODEC_AAC;
            if(m_f_Log) { log_i("ContentType %s, format is aac", ct); }
            if(audio_info) audio_info("format is aac");
            break;
        case CT_M4A:
            m_codec = CODEC_M4A;
            if(m_f_Log) { log_i("ContentType %s, format is aac", ct); }
            if(audio_info) audio_info("format is aac");
            break;
        case CT_FLAC:
            m_codec = CODEC_FLAC;
            if(m_f_Log) { log_i("ContentType %s, format is flac", ct); }
            if(audio_info) audio_info("format is flac");
            break;
        case CT_WAV:
            m_codec = CODEC_WAV;
            if(m_f_Log) { log_i("ContentType %s, format is wav", ct); }
            if(audio_info) audio_info("format is wav");
            break;
        case CT_OGG:
            m_codec = CODEC_OGG;
            if(m_f_Log) { log_i("ContentType %s found", ct); }
            if(audio_info) audio_info("format is ogg");
            break;

        case CT_PLS:
            m_playlistFormat = FORMAT_PLS;
            break;
        case CT_M3U:
            m_playlistFormat = FORMAT_M3U;
            break;
        case CT_ASX:
            m_playlistFormat = FORMAT_ASX;
            break;
        case CT_M3U8:
            m_playlistFormat = FORMAT_M3U8;
            break;
        case CT_TXT: // overwrite text/plain
            if(m_expectedCodec == CODEC_AAC){ m_codec = CODEC_AAC; if(m_f_Log) log_i("set ct from M3U8 to AAC");}
            if(m_expectedCodec == CODEC_MP3){ m_codec = CODEC_MP3; if(m_f_Log) log_i("set ct from M3U8 to MP3");}
            if(m_expectedCodec>=CODEC_OGG&&m_expectedCodec<=CODEC_MPEG)m_codec=m_expectedCodec;

            if(m_expectedPlsFmt == FORMAT_ASX){ m_playlistFormat = FORMAT_ASX;  if(m_f_Log) log_i("set playlist format to ASX");}
            if(m_expectedPlsFmt == FORMAT_M3U){ m_playlistFormat = FORMAT_M3U;  if(m_f_Log) log_i("set playlist format to M3U");}
            if(m_expectedPlsFmt == FORMAT_M3U8){m_playlistFormat = FORMAT_M3U8; if(m_f_Log) log_i("set playlist format to M3U8");}
            if(m_expectedPlsFmt == FORMAT_PLS){ m_playlistFormat = FORMAT_PLS;  if(m_f_Log) log_i("set playlist format to PLS");}
            break;
        default:
            AUDIO_INFO("%s, unsupported audio format", ct);
            return false;
            break;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::showstreamtitle(const char* ml) {
    // ICY framing stays with the network/audio owner; text parsing is Core0 work.
    if(audiod::defer(audiod::Kind::Icy,ml))return;
    if(audio_showstreamtitle){char title[640];if(icyfields::get(ml,"StreamTitle=",title,sizeof(title)))audio_showstreamtitle(title);}
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::showCodecParams(){
    // print Codec Parameter (mp3, aac) in audio_info()

    AUDIO_INFO("Channels: %i", getChannels());
    AUDIO_INFO("SampleRate: %lu", getSampleRate());
    AUDIO_INFO("BitsPerSample: %i", getBitsPerSample());
    if(getBitRate()) {AUDIO_INFO("BitRate: %lu", getBitRate());}
    else             {AUDIO_INFO("BitRate: N/A");}

    if(m_codec == CODEC_AAC || m_codec == CODEC_M4A){
        uint8_t answ;
        if((answ = AACGetFormat()) < 4){
            const char hf[4][8] = {"unknown", "ADTS", "ADIF", "RAW"};
            sprintf(chbuf, "AAC HeaderFormat: %s", hf[answ]);
            audio_info(chbuf);
        }
        if(answ == 1){ // ADTS Header
            const char co[2][23] = {"MPEG-4", "MPEG-2"};
            sprintf(chbuf, "AAC Codec: %s", co[AACGetID()]);
            audio_info(chbuf);
            if(AACGetProfile() <5){
                const char pr[4][23] = {"Main", "LowComplexity", "Scalable Sampling Rate", "reserved"};
                sprintf(chbuf, "AAC Profile: %s", pr[answ]);
                audio_info(chbuf);
            }
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::findNextSync(uint8_t* data, size_t len){
    // Mp3 and aac audio data are divided into frames. At the beginning of each frame there is a sync word.
    // The sync word is 0xFFF. This is followed by information about the structure of the frame.
    // Wav files have no frames
    // Return: 0 the synchronous word was found at position 0
    //         > 0 is the offset to the next sync word
    //         -1 the sync word was not found within the block with the length len

    int nextSync;
    static uint32_t swnf = 0;
    if(m_codec == CODEC_WAV)  {
        m_f_playing = true; nextSync = 0;
    }
    if(m_codec == CODEC_MP3) {
        nextSync = MP3FindSyncWord(data, len);
    }
    if(m_codec == CODEC_AAC) {
        nextSync = AACFindSyncWord(data, len);
    }
    if(m_codec == CODEC_M4A) {
        AACSetRawBlockParams(0, 2,44100, 1); m_f_playing = true; nextSync = 0;
    }
    if(m_codec == CODEC_FLAC) {
        FLACSetRawBlockParams(m_flacNumChannels,   m_flacSampleRate,
                              m_flacBitsPerSample, m_flacTotalSamplesInStream, m_audioDataSize);
        nextSync = FLACFindSyncWord(data, len);
    }
    if(m_codec == CODEC_OGG_FLAC) {
        FLACSetRawBlockParams(m_flacNumChannels,   m_flacSampleRate,
                              m_flacBitsPerSample, m_flacTotalSamplesInStream, m_audioDataSize);
        nextSync = FLACFindSyncWord(data, len);
    }
    if(nextSync == -1) {
         if(audio_info && swnf == 0) audio_info("syncword not found");
         if(m_codec == CODEC_OGG_FLAC){
             nextSync = len;
         }
         else {
             swnf++; // syncword not found counter, can be multimediadata
         }
     }
     if (nextSync == 0){
         if(audio_info && swnf>0){
             sprintf(chbuf, "syncword not found %lu times", swnf);
             audio_info(chbuf);
             swnf = 0;
         }
         else {
             if(audio_info) audio_info("syncword found at pos 0");
         }
     }
     if(nextSync > 0){
         AUDIO_INFO("syncword found at pos %i", nextSync);
     }
     return nextSync;
}
//---------------------------------------------------------------------------------------------------------------------
int Audio::sendBytes(uint8_t* data, size_t len) {
    if(extraDecoder.active())return sendExtraBytes(data,len,false);
    int bytesLeft;
    static bool f_setDecodeParamsOnce = true;
    int nextSync = 0;
    if(!m_f_playing) {
        f_setDecodeParamsOnce = true;
        nextSync = findNextSync(data, len);
        if(nextSync == 0) { m_f_playing = true;}
        return nextSync;
    }
    // m_f_playing is true at this pos
    bytesLeft = len;
    int ret = 0;
    int bytesDecoded = 0;

    {pathprobe::Scope probe(pathprobe::Decode);
    switch(m_codec){
        case CODEC_WAV: {
            const auto converted=wavpcm::convert(data,len,m_outBuff,
                OutputSamples,m_wavSourceBits,getChannels(),m_wavFloat);
            if(!converted.consumed)return 0; // Keep partial channel/frame bytes in input ring.
            m_validSamples=converted.frames;bytesLeft=len-converted.consumed;break;
        }
        case CODEC_MP3:      ret = MP3Decode(data, &bytesLeft, m_outBuff, 0); break;
        case CODEC_AAC:      ret = AACDecodeBounded(data, &bytesLeft, m_outBuff,OutputSamples);    break;
        case CODEC_M4A:      ret = AACDecodeBounded(data, &bytesLeft, m_outBuff,OutputSamples);    break;
        case CODEC_FLAC:     ret = FLACDecodeBounded(data, &bytesLeft, m_outBuff,OutputSamples);   break;
        case CODEC_OGG_FLAC: ret = FLACDecodeBounded(data, &bytesLeft, m_outBuff,OutputSamples);   break; // FLAC webstream wrapped in OGG
        default: {log_e("no valid codec found codec = %d", m_codec); stopSong();}
    }

    probe.units(ret<0?-1:int(len-bytesLeft));}
    bytesDecoded = len - bytesLeft;
    if(bytesDecoded == 0 && ret == 0){ // unlikely framesize
            if(audio_info) audio_info("framesize is 0, start decoding again");
            m_f_playing = false; // seek for new syncword
        // we're here because there was a wrong sync word
        // so skip two sync bytes and seek for next
        return 1;
    }
    if(ret==ERR_AAC_OUTPUT_TOO_SMALL&&(m_codec==CODEC_AAC||m_codec==CODEC_M4A)){
        AUDIO_ERROR("A hangformátumhoz nagyobb PCM-puffer kell");stopSong();return 0;
    }
    if((m_codec==CODEC_FLAC||m_codec==CODEC_OGG_FLAC)&&ret==ERR_FLAC_MEMORY){
        AUDIO_ERROR("Nincs eleg RAM a FLAC hangblokkhoz");stopSong();return 0;
    }
    if(ret < 0) { // Error, skip the frame...
        if(m_f_Log) if(m_codec == CODEC_M4A){log_i("begin not found"); return 1;}
        if(m_i2sReady)i2s_zero_dma_buffer((i2s_port_t)m_i2s_num);
        if(!getChannels() && (ret == -2)) {
             ; // suppress errorcode MAINDATA_UNDERFLOW
        }
        else {
            printDecodeError(ret);
            m_f_playing = false; // seek for new syncword
        }
        if(!bytesDecoded) bytesDecoded = 2;
        return bytesDecoded;
    }
    else{  // ret>=0
        if(f_setDecodeParamsOnce){
            f_setDecodeParamsOnce = false;
            m_PlayingStartTime = millis();

            if(m_codec == CODEC_MP3){
                setChannels(MP3GetChannels());
                setSampleRate(MP3GetSampRate());
                setBitsPerSample(MP3GetBitsPerSample());
                setBitrate(MP3GetBitrate());
            }
            if(m_codec == CODEC_AAC || m_codec == CODEC_M4A){
                setChannels(AACGetChannels());
                setSampleRate(AACGetSampRate());
                setBitsPerSample(AACGetBitsPerSample());
                setBitrate(AACGetBitrate());
            }
            if(m_codec == CODEC_FLAC || m_codec == CODEC_OGG_FLAC){
                setChannels(FLACGetChannels());
                if(!setSampleRate(FLACGetSampRate())){AUDIO_ERROR("FLAC: I2S clock rejected");stopSong();return 0;}
                setBitsPerSample(16); // Decoder normalizes8..16-bit samples to PCM16.
                setBitrate(FLACGetBitRate());
                m_extraDacSourceRate=0;m_extraDacResampler.reset();
                if(IBT_NO_PSRAM&&!btaudio::outputSelected()){
                    directrx::pcmBegin(getSampleRate(),getChannels());
                    Serial.printf("[FLAC-DAC] rate=%lu source_bits=%u output_bits=16 channels=%u writer=NATIVE64 probe=ADPCM_PCM_SHARED\n",(unsigned long)getSampleRate(),unsigned(FLACGetBitsPerSample()),unsigned(getChannels()));
                }
            }
            showCodecParams();
        }
        if(m_codec == CODEC_MP3){
            m_validSamples = MP3GetOutputSamps() / getChannels();
        }
        if((m_codec == CODEC_AAC) || (m_codec == CODEC_M4A)){
            m_validSamples = AACGetOutputSamps() / getChannels();
        }
        if((m_codec == CODEC_FLAC) || (m_codec == CODEC_OGG_FLAC)){
            m_validSamples = FLACGetOutputSamps() / getChannels();
        }
    }
    pathprobe::frames(m_validSamples);
    const bool nativeFlac=IBT_NO_PSRAM&&(m_codec==CODEC_FLAC||m_codec==CODEC_OGG_FLAC)&&!btaudio::outputSelected();
    if(nativeFlac)directrx::pcmRaw(m_outBuff,m_validSamples,getChannels());
    if(m_streamType==ST_WEBSTREAM)directrx::decoded(m_validSamples,m_codec==CODEC_MP3?MP3GetBitrate():getBitRate(),getSampleRate(),getChannels(),getBitsPerSample());
    compute_audioCurrentTime(bytesDecoded);
    pathprobe::Scope outputProbe(pathprobe::Output);outputProbe.units(m_validSamples);

    if(playBtBlock())return bytesDecoded;
    if(audio_process_extern){
        bool continueI2S = false;
        audio_process_extern(m_outBuff, m_validSamples, &continueI2S);
        if(!continueI2S){
            if(cooperativeWebDac()){m_validSamples=0;m_curSample=0;}
            return bytesDecoded;
        }
    }
    if(nativeFlac&&m_validSamples){
        m_curSample=0;
        // Same native PCM16 handoff used by verified WAV/ADPCM playback;
        // includes gain/fade, bounded writes and packed/accepted PCM probes.
        if(!playExtraDacBlock()){if(m_f_running)stopSong();}
        return bytesDecoded;
    }
    drainDecoded();
    return bytesDecoded;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::compute_audioCurrentTime(int bd) {
    static uint16_t loop_counter = 0;
    static int old_bitrate = 0;
    static uint64_t sum_bitrate = 0;
    static boolean f_CBR = true; // constant bitrate

    if(m_codec == CODEC_MP3) {setBitrate(MP3GetBitrate()) ;} // if not CBR, bitrate can be changed
    if(m_codec == CODEC_M4A) {setBitrate(AACGetBitrate()) ;} // if not CBR, bitrate can be changed
    if(m_codec == CODEC_AAC) {setBitrate(AACGetBitrate()) ;} // if not CBR, bitrate can be changed
    if(m_codec == CODEC_FLAC){setBitrate(FLACGetBitRate());} // if not CBR, bitrate can be changed
    if(!getBitRate()) return;

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_avr_bitrate == 0) { // first time
        loop_counter = 0;
        old_bitrate = 0;
        sum_bitrate = 0;
        f_CBR = true;
        m_avr_bitrate = getBitRate();
        old_bitrate = getBitRate();
    }
    if(!m_avr_bitrate) return;
    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    if(loop_counter < 1000) loop_counter ++;

    if((old_bitrate != getBitRate()) && f_CBR) {
        if(audio_info) audio_info("VBR recognized, audioFileDuration is estimated");
        f_CBR = false; // variable bitrate
    }
    old_bitrate = getBitRate();

    if(!f_CBR) {
        if(loop_counter > 20 && loop_counter < 200) {
            // if VBR: m_avr_bitrate is average of the first values of m_bitrate
            sum_bitrate += getBitRate();
            m_avr_bitrate = sum_bitrate / (loop_counter - 20);
            if(loop_counter == 199 && m_resumeFilePos && getDatamode()==AUDIO_LOCALFILE && audiofile){
                m_audioCurrentTime = resumetime::seconds(getFilePos(),m_audioDataStart,inBufferFilled(),m_avr_bitrate); // #293
            }
        }
    }
    else {
        if(loop_counter == 2){
            m_avr_bitrate = getBitRate();
            if(m_resumeFilePos && getDatamode()==AUDIO_LOCALFILE && audiofile){  // local resume only
                m_audioCurrentTime = resumetime::seconds(getFilePos(),m_audioDataStart,inBufferFilled(),m_avr_bitrate); // #293
            }
        }
    }
    m_audioCurrentTime += ((float)bd / m_avr_bitrate) * 8;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::printDecodeError(int r) {
    const char *e;

    if(m_codec == CODEC_MP3){
        switch(r){
            case ERR_MP3_NONE:                              e = "NONE";                             break;
            case ERR_MP3_INDATA_UNDERFLOW:                  e = "INDATA_UNDERFLOW";                 break;
            case ERR_MP3_MAINDATA_UNDERFLOW:                e = "MAINDATA_UNDERFLOW";               break;
            case ERR_MP3_FREE_BITRATE_SYNC:                 e = "FREE_BITRATE_SYNC";                break;
            case ERR_MP3_OUT_OF_MEMORY:                     e = "OUT_OF_MEMORY";                    break;
            case ERR_MP3_NULL_POINTER:                      e = "NULL_POINTER";                     break;
            case ERR_MP3_INVALID_FRAMEHEADER:               e = "INVALID_FRAMEHEADER";              break;
            case ERR_MP3_INVALID_SIDEINFO:                  e = "INVALID_SIDEINFO";                 break;
            case ERR_MP3_INVALID_SCALEFACT:                 e = "INVALID_SCALEFACT";                break;
            case ERR_MP3_INVALID_HUFFCODES:                 e = "INVALID_HUFFCODES";                break;
            case ERR_MP3_INVALID_DEQUANTIZE:                e = "INVALID_DEQUANTIZE";               break;
            case ERR_MP3_INVALID_IMDCT:                     e = "INVALID_IMDCT";                    break;
            case ERR_MP3_INVALID_SUBBAND:                   e = "INVALID_SUBBAND";                  break;
            default: e = "ERR_UNKNOWN";
        }
        AUDIO_INFO("MP3 decode error %d : %s", r, e);
     }
    if(m_codec == CODEC_AAC){
        switch(r){
            case ERR_AAC_NONE:                              e = "NONE";                             break;
            case ERR_AAC_INDATA_UNDERFLOW:                  e = "INDATA_UNDERFLOW";                 break;
            case ERR_AAC_NULL_POINTER:                      e = "NULL_POINTER";                     break;
            case ERR_AAC_INVALID_ADTS_HEADER:               e = "INVALID_ADTS_HEADER";              break;
            case ERR_AAC_INVALID_ADIF_HEADER:               e = "INVALID_ADIF_HEADER";              break;
            case ERR_AAC_INVALID_FRAME:                     e = "INVALID_FRAME";                    break;
            case ERR_AAC_MPEG4_UNSUPPORTED:                 e = "MPEG4_UNSUPPORTED";                break;
            case ERR_AAC_CHANNEL_MAP:                       e = "CHANNEL_MAP";                      break;
            case ERR_AAC_SYNTAX_ELEMENT:                    e = "SYNTAX_ELEMENT";                   break;
            case ERR_AAC_DEQUANT:                           e = "DEQUANT";                          break;
            case ERR_AAC_STEREO_PROCESS:                    e = "STEREO_PROCESS";                   break;
            case ERR_AAC_PNS:                               e = "PNS";                              break;
            case ERR_AAC_SHORT_BLOCK_DEINT:                 e = "SHORT_BLOCK_DEINT";                break;
            case ERR_AAC_TNS:                               e = "TNS";                              break;
            case ERR_AAC_IMDCT:                             e = "IMDCT";                            break;
            case ERR_AAC_SBR_INIT:                          e = "SBR_INIT";                         break;
            case ERR_AAC_SBR_BITSTREAM:                     e = "SBR_BITSTREAM";                    break;
            case ERR_AAC_SBR_DATA:                          e = "SBR_DATA";                         break;
            case ERR_AAC_SBR_PCM_FORMAT:                    e = "SBR_PCM_FORMAT";                   break;
            case ERR_AAC_SBR_NCHANS_TOO_HIGH:               e = "SBR_NCHANS_TOO_HIGH";              break;
            case ERR_AAC_SBR_SINGLERATE_UNSUPPORTED:        e = "BR_SINGLERATE_UNSUPPORTED";        break;
            case ERR_AAC_NCHANS_TOO_HIGH:                   e = "NCHANS_TOO_HIGH";                  break;
            case ERR_AAC_RAWBLOCK_PARAMS:                   e = "RAWBLOCK_PARAMS";                  break;
            default: e = "ERR_UNKNOWN";
        }
        AUDIO_INFO("AAC decode error %d : %s", r, e);
    }
    if(m_codec == CODEC_FLAC){
        switch(r){
            case ERR_FLAC_NONE:                             e = "NONE";                             break;
            case ERR_FLAC_BLOCKSIZE_TOO_BIG:                e = "BLOCKSIZE TOO BIG";                break;
            case ERR_FLAC_SAMPLE_RANGE: e = "FLAC reconstructed sample out of range"; break;
            case ERR_FLAC_MEMORY: e = "FLAC sample workspace allocation failed"; break;
            case ERR_FLAC_RESERVED_BLOCKSIZE_UNSUPPORTED:   e = "Reserved Blocksize unsupported";   break;
            case ERR_FLAC_SYNC_CODE_NOT_FOUND:              e = "SYNC CODE NOT FOUND";              break;
            case ERR_FLAC_UNKNOWN_CHANNEL_ASSIGNMENT:       e = "UNKNOWN CHANNEL ASSIGNMENT";       break;
            case ERR_FLAC_RESERVED_CHANNEL_ASSIGNMENT:      e = "RESERVED CHANNEL ASSIGNMENT";      break;
            case ERR_FLAC_RESERVED_SUB_TYPE:                e = "RESERVED SUB TYPE";                break;
            case ERR_FLAC_PREORDER_TOO_BIG:                 e = "PREORDER TOO BIG";                 break;
            case ERR_FLAC_RESERVED_RESIDUAL_CODING:         e = "RESERVED RESIDUAL CODING";         break;
            case ERR_FLAC_WRONG_RICE_PARTITION_NR:          e = "WRONG RICE PARTITION NR";          break;
            case ERR_FLAC_BITS_PER_SAMPLE_TOO_BIG:          e = "BITS PER SAMPLE > 16";             break;
            case ERR_FLAG_BITS_PER_SAMPLE_UNKNOWN:          e = "BITS PER SAMPLE UNKNOWN";          break;
            default: e = "ERR_UNKNOWN";
        }
        AUDIO_INFO("FLAC decode error %d : %s", r, e);
    }
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setPinout(uint8_t BCLK, uint8_t LRC, uint8_t DOUT, int8_t DIN, int8_t MCK) {
    if(!m_i2sReady)return false;
    m_pin_config.bck_io_num   = BCLK;
    m_pin_config.ws_io_num    = LRC; //  wclk
    m_pin_config.data_out_num = DOUT;
    m_pin_config.data_in_num  = DIN;
#if(ESP_IDF_VERSION_MAJOR >= 4 && ESP_IDF_VERSION_MINOR >= 4) || defined(ESP_ARDUINO_3)
    m_pin_config.mck_io_num   = MCK;
#endif
    const esp_err_t result = i2s_set_pin((i2s_port_t) m_i2s_num, &m_pin_config);
    return (result == ESP_OK);
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getFileSize() {
    if(!audiofile) return 0;
    uint32_t s = audiofile.size();
    return s;
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getFilePos() {
    if(!audiofile) return 0;
    uint32_t p = audiofile.position();
    return p;
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getAudioDataStartPos() {
    if(!audiofile) return 0;
    return m_audioDataStart;
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getAudioFileDuration() {
    if(getDatamode() == AUDIO_LOCALFILE) {if(!audiofile) return 0;}
    if(m_streamType == ST_WEBFILE)   {if(!m_contentlength) return 0;}

    if     (m_avr_bitrate && m_codec == CODEC_MP3)   m_audioFileDuration = 8 * (m_audioDataSize / m_avr_bitrate); // #289
    else if(m_avr_bitrate && m_codec == CODEC_WAV)   m_audioFileDuration = 8 * (m_audioDataSize / m_avr_bitrate);
    else if(m_avr_bitrate && m_codec == CODEC_M4A)   m_audioFileDuration = 8 * (m_audioDataSize / m_avr_bitrate);
    else if(m_avr_bitrate && m_codec == CODEC_AAC)   m_audioFileDuration = 8 * (m_audioDataSize / m_avr_bitrate);
    else if(                 m_codec == CODEC_FLAC)  m_audioFileDuration = FLACGetAudioFileDuration();
    else return 0;
    return m_audioFileDuration;
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getAudioCurrentTime() {  // return current time in seconds
    return (uint32_t) m_audioCurrentTime;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setAudioPlayPosition(uint16_t sec){
    // Jump to an absolute position in time within an audio file
    // e.g. setAudioPlayPosition(300) sets the pointer at pos 5 min
    // works only with format mp3 or wav
    if(m_codec == CODEC_M4A)  return false;
    if(sec > getAudioFileDuration()) sec = getAudioFileDuration();
    uint32_t filepos = m_audioDataStart + (m_avr_bitrate * sec / 8);

    return setFilePos(filepos);
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::getTotalPlayingTime() {
    // Is set to zero by a connectToXXX() and starts as soon as the first audio data is available,
    // the time counting is not interrupted by a 'pause / resume' and is not reset by a fileloop
    return millis() - m_PlayingStartTime;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setTimeOffset(int sec){
    // fast forward or rewind the current position in seconds
    // audiosource must be a mp3, aac or wav file

    if(!audiofile || !m_avr_bitrate) return false;

    uint32_t oneSec  = m_avr_bitrate / 8;                   // bytes decoded in one sec
    int32_t  offset  = oneSec * sec;                        // bytes to be wind/rewind
    uint32_t startAB = m_audioDataStart;                    // audioblock begin
    uint32_t endAB   = m_audioDataStart + m_audioDataSize;  // audioblock end

    if(m_codec == CODEC_MP3 || m_codec == CODEC_AAC || m_codec == CODEC_WAV || m_codec == CODEC_FLAC){
        int32_t pos = getFilePos();
        pos += offset;
        if(pos <  (int32_t)startAB) pos = startAB;
        if(pos >= (int32_t)endAB)   pos = endAB;
        setFilePos(pos);
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setFilePos(uint32_t pos) {
    if(extraDecoder.active())return false; // Container parsers restart from a header, not an arbitrary byte.
    if(!audiofile) return false;
//    if(!m_avr_bitrate) return false;
    if(m_codec == CODEC_M4A) return false;
    m_f_playing = false;
    if(m_codec == CODEC_MP3) MP3Decoder_ClearBuffer();
    if(m_codec == CODEC_WAV && pos>m_audioDataStart) {
        const unsigned block=(m_wavSourceBits/8)*getChannels();
        if(block)pos-=((pos-m_audioDataStart)%block); // Align relative to data, including 24-bit stereo.
    }
    if(m_codec == CODEC_FLAC) FLACDecoderReset();
    InBuff.resetBuffer();
    if(pos < m_audioDataStart) pos = m_audioDataStart; // issue #96
    if(m_avr_bitrate) m_audioCurrentTime = ((pos-m_audioDataStart) / m_avr_bitrate) * 8; // #96
    uint32_t sk = audiofile.seek(pos);
    return sk;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::audioFileSeek(const float speed) {
    // 0.5 is half speed
    // 1.0 is normal speed
    // 1.5 is one and half speed
    if((speed > 1.5f) || (speed < 0.25f)) return false;

    if(!m_i2sReady)return false; // This legacy control changes the physical DAC clock only.
    uint32_t srate = getSampleRate() * speed;
    i2s_set_sample_rates((i2s_port_t)m_i2s_num, srate);
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setSampleRate(uint32_t sampRate) {
    if(!sampRate) sampRate = 16000; // fuse, if there is no value -> set default #209
    if(!m_i2sReady){
        if(!outputReady())return false;
        m_sampleRate=sampRate;return true; // BT resampler consumes the source clock.
    }
    const esp_err_t result=i2s_set_sample_rates((i2s_port_t)m_i2s_num, sampRate);
    if(result!=ESP_OK){log_e("I2S rate change failed: rate=%lu error=%d",(unsigned long)sampRate,int(result));return false;}
    m_sampleRate = sampRate;
    return true;
}
uint32_t Audio::getSampleRate(){
    return m_sampleRate;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setBitsPerSample(int bits) {
    if((bits != 16) && (bits != 8)) return false;
    m_bitsPerSample = bits;
    return true;
}
uint8_t Audio::getBitsPerSample(){
    return m_bitsPerSample;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setChannels(int ch) {
    if((ch < 1) || (ch > 2)) return false;
    m_channels = ch;
    return true;
}
uint8_t Audio::getChannels(){
    if (m_channels == 0) {    // this should not happen! #209
        m_channels = 2;
    }
    return m_channels;
}
//---------------------------------------------------------------------------------------------------------------------
bool Audio::setBitrate(int br){
    m_bitRate = br;
    if(br)return true;
    return false;
}
uint32_t Audio::getBitRate(bool avg){
    if (avg)
        return m_avr_bitrate;
    return m_bitRate;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::setI2SCommFMT_LSB(bool commFMT) {
    // false: I2S communication format is by default I2S_COMM_FORMAT_I2S_MSB, right->left (AC101, PCM5102A)
    // true:  changed to I2S_COMM_FORMAT_I2S_LSB for some DACs (PT8211)
    //        Japanese or called LSBJ (Least Significant Bit Justified) format

    if (commFMT) {
        if(m_f_Log) log_i("commFMT LSB");

        #if ESP_ARDUINO_VERSION_MAJOR >= 2
            m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_MSB); // v >= 2.0.0
        #else
            m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB);
        #endif

    }
    else {
        if(m_f_Log) log_i("commFMT MSB");

        #if ESP_ARDUINO_VERSION_MAJOR >= 2
            m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S); // vers >= 2.0.0
        #else
            m_i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
        #endif

    }
    AUDIO_INFO("commFMT = %i", m_i2s_config.communication_format);
    if(!m_i2sReady)return; // Save format for the next real DAC lease, no SDK call while dormant.
    i2s_driver_uninstall((i2s_port_t)m_i2s_num);
    i2s_driver_install  ((i2s_port_t)m_i2s_num, &m_i2s_config, 0, NULL);
}
//---------------------------------------------------------------------------------------------------------------------
// Bounded service adapter. Existing music and BT-IN write paths remain unchanged.
bool Audio::prepareServicePcm(uint32_t rate){
    if(!player.onAudioTask()||m_f_running||m_f_internalDAC||
       btaudio::wanted()||!btaudio::idle()||!serviceui::supportedRate(rate))return false;
    if(!restoreDacAfterBtSearch())return false;
    // Unlike setSampleRate(), preserve the actual driver's error result.
    if(i2s_set_clk((i2s_port_t)m_i2s_num,rate,I2S_BITS_PER_SAMPLE_16BIT,I2S_CHANNEL_STEREO)!=ESP_OK)return false;
    m_sampleRate=rate;m_bitsPerSample=16;m_channels=2;
    return true;
}
serviceui::Transfer Audio::writeServicePcm(const int16_t* samples,uint16_t frames,uint32_t rate){
    if(!player.onAudioTask()||m_f_running||!m_i2sReady||m_f_internalDAC||btaudio::wanted()||!btaudio::idle()||
       !samples||frames>256||!serviceui::supportedRate(rate)||m_sampleRate!=rate||
       m_bitsPerSample!=16||m_channels!=2)return {0,true};
    if(!frames)return {0,false};
    uint32_t packed[256];
    for(unsigned i=0;i<frames;++i)packed[i]=serviceui::dacWord(samples[i*2],samples[i*2+1]);
    size_t written=0;
    const auto error=i2s_write((i2s_port_t)m_i2s_num,packed,size_t(frames)*4,&written,1);
    return serviceui::dacTransfer(frames,written,error==ESP_OK,error==ESP_ERR_TIMEOUT);
}
bool Audio::writePcm16(const uint8_t* data,size_t bytes,uint32_t rate){
    if(!m_i2sReady || bytes%4)return false;
    const uint32_t started=micros();unsigned submissions=0;
    const auto finish=[&](bool ok){btrxprobe::stats().write(submissions,uint32_t(micros()-started),ok);return ok;};
    setBitsPerSample(16);setChannels(2);
    if(getSampleRate()!=rate && !setSampleRate(rate))return finish(false);
    static uint32_t lastReportedRate=0;
    if(rate!=lastReportedRate){
        lastReportedRate=rate;
        Serial.printf("[BT-IN-DAC] requested_hz=%lu actual_hz=%.1f bits=16 channels=2\n",
            (unsigned long)rate,double(i2s_get_clk((i2s_port_t)m_i2s_num)));
    }
    monodsp::block(rate);
    uint32_t frames[64];unsigned ready=0;
    for(size_t i=0;i<bytes;i+=4){
        int16_t frame[2];memcpy(frame,data+i,4);
        if(!playSample(frame,&frames[ready]))return finish(false);
        ++ready;
        if(ready==64||i+4==bytes){++submissions;if(!submitDacFrames(frames,ready))return finish(false);ready=0;}
    }
    return finish(true);
}
bool Audio::playBtBlock() {
    if(!btaudio::outputSelected() || getBitsPerSample()!=16)return false;
    const int channels=getChannels();
    if(channels!=1 && channels!=2)return false;
    const unsigned rate=getSampleRate();
    if(rate!=44100 && rate!=48000)return false; // preserve V092 fallback for other formats
    if(!(channels==2&&m_f_forceMono)){
        // feedBlock already divides into bounded256frame queue jobs.
        // Submit a decoded block once, avoiding repeated decoder-side checks.
        btaudio::feedBlock(m_outBuff+unsigned(m_curSample)*channels,m_validSamples,channels,rate,m_vol,m_balance);
        m_validSamples=0;m_curSample=0;return true;
    }
    unsigned consumed=0;
    const unsigned frames=m_validSamples;
    while(consumed<frames){
        const unsigned count=std::min<unsigned>(256,frames-consumed);
        const int16_t* pcm=m_outBuff+(unsigned(m_curSample)+consumed)*channels;
        if(channels==2 && m_f_forceMono){
            int16_t mono[512];
            for(unsigned i=0;i<count;++i){
                const int16_t value=int16_t((int32_t(pcm[i*2])+pcm[i*2+1])/2);
                mono[i*2]=mono[i*2+1]=value;
            }
            btaudio::feedBlock(mono,count,2,rate,m_vol,m_balance);
        }else{
            // V029 route: decoded PCM directly to a bounded BT ring; no DAC sample loop.
            btaudio::feedBlock(pcm,count,channels,rate,m_vol,m_balance);
        }
        consumed+=count;
    }
    m_validSamples=0;
    m_curSample=0;
    return true;
}
inline __attribute__((always_inline)) bool Audio::playSample(int16_t sample[2], uint32_t* prepared) {

    if (getBitsPerSample() == 8) { // Upsample from unsigned 8 bits to signed 16 bits
        sample[LEFTCHANNEL]  = ((sample[LEFTCHANNEL]  & 0xff) -128) << 8;
        sample[RIGHTCHANNEL] = ((sample[RIGHTCHANNEL] & 0xff) -128) << 8;
    }

    // R123: EQ DSP disabled in every route, irrespective of saved settings.
    uint32_t s32 = Gain(sample);
    static vuprobe::Meter meter; // shared single-producer observer across exclusive DAC/BT routes
    if(btaudio::outputSelected()){
        // Optional SD-only VU observation on the single PCM producer, never BT callback.
        static uint16_t localVuDivider=0;
        if((++localVuDivider&63u)==0&&vuprobe::localBtEnabled.load(std::memory_order_relaxed)){
            if(meter.sample(int16_t(s32>>16),int16_t(s32),millis())){
                vuprobe::dacLeft.store(uint8_t(std::min(255u,meter.left>>7)),std::memory_order_relaxed);
                vuprobe::dacRight.store(uint8_t(std::min(255u,meter.right>>7)),std::memory_order_relaxed);
            }
        }
        btaudio::sample(int16_t(s32>>16),int16_t(s32&0xffff),getSampleRate());
        return true; // BT consumer paces PCM; keep the DAC silent.
    }

    if(monodsp::active())s32=monodsp::process(sample[LEFTCHANNEL],sample[RIGHTCHANNEL],s32,m_vol,getSampleRate());
    if(m_startFadePending){m_startFade.begin(getSampleRate());m_startFadePending=false;}
    if(m_startFade.active()){
        int16_t left=int16_t(s32>>16),right=int16_t(s32);
        m_startFade.scale(left,right);
        s32=(uint32_t(uint16_t(left))<<16)|uint16_t(right);
    }
    static uint16_t vuDivider=0;
    const uint16_t vuStep=++vuDivider;
    if((vuStep&7u)==0&&vuprobe::dacEnabled.load(std::memory_order_relaxed)){
        static uint32_t pcmWrite=0;
#if !IBT_NO_PSRAM
        vuprobe::dacPcm[pcmWrite&127u].store(int16_t(s32>>16),std::memory_order_relaxed);
#endif
        vuprobe::dacPcmWrite.store(++pcmWrite,std::memory_order_release);
        if((vuStep&15u)==0){
            if(meter.sample(int16_t(s32>>16),int16_t(s32),millis())){
                vuprobe::dacLeft.store(uint8_t(std::min(255u,meter.left>>7)),std::memory_order_relaxed);
                vuprobe::dacRight.store(uint8_t(std::min(255u,meter.right>>7)),std::memory_order_relaxed);
            }
        }
    }
    if(m_f_internalDAC) {
        s32 += 0x80008000;
    }
    if(prepared){
        *prepared=s32;
        m_startFade.advance(); // Prepared once; retries retain this exact PCM.
        return true;
    }
    if(!submitDacFrames(&s32,1))return false;
    m_startFade.advance();

    return true;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::setTone(int8_t, int8_t, int8_t){
    // Settings may retain old values, but this release never enables EQ DSP.
    m_gain0=m_gain1=m_gain2=0;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::forceMono(bool m) { // #100 mono option
    m_f_forceMono = m; // false stereo, true mono
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::setBalance(int8_t bal){ // bal -16...16
    if(bal < -16) bal = -16;
    if(bal >  16) bal =  16;
    m_balance = bal;
}
//---------------------------------------------------------------------------------------------------------------------
void Audio::setVolume(uint8_t vol) { // vol 22 steps, 0...21
    if(vol > 254) vol = 254;
    m_vol = vol;
/*    if(vol > 21) vol = 21;
    m_vol = volumetable[vol];*/
}
//---------------------------------------------------------------------------------------------------------------------
uint8_t Audio::getVolume() {
    return m_vol;
    /*for(uint8_t i = 0; i < 22; i++) {
        if(volumetable[i] == m_vol) return i;
    }
    m_vol = 12; // if m_vol not found in table
    return m_vol;*/
}
//---------------------------------------------------------------------------------------------------------------------
uint8_t Audio::getI2sPort() {
    return m_i2s_num;
}
//---------------------------------------------------------------------------------------------------------------------
inline __attribute__((always_inline)) int32_t Audio::Gain(int16_t s[2]) {
    return int32_t(m_pcmGain.apply(s[LEFTCHANNEL],s[RIGHTCHANNEL],m_vol,m_balance));
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::inBufferFilled() {
    // current audio input buffer fillsize in bytes
    return InBuff.bufferFilled();
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t Audio::inBufferFree() {
    // current audio input buffer free space in bytes
    return InBuff.freeSpace();
}
//---------------------------------------------------------------------------------------------------------------------
//            ***     D i g i t a l   b i q u a d r a t i c     f i l t e r     ***
//---------------------------------------------------------------------------------------------------------------------
void Audio::IIR_calculateCoefficients(int8_t G0, int8_t G1, int8_t G2){  // Infinite Impulse Response (IIR) filters

    // G1 - gain low shelf   set between -40 ... +6 dB
    // G2 - gain peakEQ      set between -40 ... +6 dB
    // G3 - gain high shelf  set between -40 ... +6 dB
    // https://www.earlevel.com/main/2012/11/26/biquad-c-source-code/

    if(getSampleRate() < 1000) return;  // fuse

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    if(G0 < -40) G0 = -40;      // -40dB -> Vin*0.01
    if(G0 > 6) G0 = 6;          // +6dB -> Vin*2
    if(G1 < -40) G1 = -40;
    if(G1 > 6) G1 = 6;
    if(G2 < -40) G2 = -40;
    if(G2 > 6) G2 = 6;

    const float FcLS   =   80;  // Frequency LowShelf[Hz] //500
    const float FcPKEQ = 3000;  // Frequency PeakEQ[Hz]
    const float FcHS   = 6000;  // Frequency HighShelf[Hz]

    float K, norm, Q, Fc, V ;

    // LOWSHELF
    Fc = (float)FcLS / (float)getSampleRate(); // Cutoff frequency
    K = tanf((float)PI * Fc);
    V = powf(10, fabs(G0) / 20.0);

    if (G0 >= 0) {  // boost
        norm = 1 / (1 + sqrtf(2) * K + K * K);
        m_filter[LOWSHELF].a0 = (1 + sqrtf(2*V) * K + V * K * K) * norm;
        m_filter[LOWSHELF].a1 = 2 * (V * K * K - 1) * norm;
        m_filter[LOWSHELF].a2 = (1 - sqrtf(2*V) * K + V * K * K) * norm;
        m_filter[LOWSHELF].b1 = 2 * (K * K - 1) * norm;
        m_filter[LOWSHELF].b2 = (1 - sqrtf(2) * K + K * K) * norm;
    }
    else {          // cut
        norm = 1 / (1 + sqrtf(2*V) * K + V * K * K);
        m_filter[LOWSHELF].a0 = (1 + sqrtf(2) * K + K * K) * norm;
        m_filter[LOWSHELF].a1 = 2 * (K * K - 1) * norm;
        m_filter[LOWSHELF].a2 = (1 - sqrtf(2) * K + K * K) * norm;
        m_filter[LOWSHELF].b1 = 2 * (V * K * K - 1) * norm;
        m_filter[LOWSHELF].b2 = (1 - sqrtf(2*V) * K + V * K * K) * norm;
    }

    // PEAK EQ
    Fc = (float)FcPKEQ / (float)getSampleRate(); // Cutoff frequency
    K = tanf((float)PI * Fc);
    V = powf(10, fabs(G1) / 20.0);
    Q = 2.5; // Quality factor
    if (G1 >= 0) { // boost
        norm = 1 / (1 + 1/Q * K + K * K);
        m_filter[PEAKEQ].a0 = (1 + V/Q * K + K * K) * norm;
        m_filter[PEAKEQ].a1 = 2 * (K * K - 1) * norm;
        m_filter[PEAKEQ].a2 = (1 - V/Q * K + K * K) * norm;
        m_filter[PEAKEQ].b1 = m_filter[PEAKEQ].a1;
        m_filter[PEAKEQ].b2 = (1 - 1/Q * K + K * K) * norm;
    }
    else {    // cut
        norm = 1 / (1 + V/Q * K + K * K);
        m_filter[PEAKEQ].a0 = (1 + 1/Q * K + K * K) * norm;
        m_filter[PEAKEQ].a1 = 2 * (K * K - 1) * norm;
        m_filter[PEAKEQ].a2 = (1 - 1/Q * K + K * K) * norm;
        m_filter[PEAKEQ].b1 = m_filter[PEAKEQ].a1;
        m_filter[PEAKEQ].b2 = (1 - V/Q * K + K * K) * norm;
    }

    // HIGHSHELF
    Fc = (float)FcHS / (float)getSampleRate(); // Cutoff frequency
    K = tanf((float)PI * Fc);
    V = powf(10, fabs(G2) / 20.0);
    if (G2 >= 0) {  // boost
        norm = 1 / (1 + sqrtf(2) * K + K * K);
        m_filter[HIFGSHELF].a0 = (V + sqrtf(2*V) * K + K * K) * norm;
        m_filter[HIFGSHELF].a1 = 2 * (K * K - V) * norm;
        m_filter[HIFGSHELF].a2 = (V - sqrtf(2*V) * K + K * K) * norm;
        m_filter[HIFGSHELF].b1 = 2 * (K * K - 1) * norm;
        m_filter[HIFGSHELF].b2 = (1 - sqrtf(2) * K + K * K) * norm;
    }
    else {
        norm = 1 / (V + sqrtf(2*V) * K + K * K);
        m_filter[HIFGSHELF].a0 = (1 + sqrtf(2) * K + K * K) * norm;
        m_filter[HIFGSHELF].a1 = 2 * (K * K - 1) * norm;
        m_filter[HIFGSHELF].a2 = (1 - sqrtf(2) * K + K * K) * norm;
        m_filter[HIFGSHELF].b1 = 2 * (K * K - V) * norm;
        m_filter[HIFGSHELF].b2 = (V - sqrtf(2*V) * K + K * K) * norm;
    }

//    log_i("LS a0=%f, a1=%f, a2=%f, b1=%f, b2=%f", m_filter[0].a0, m_filter[0].a1, m_filter[0].a2,
//                                                  m_filter[0].b1, m_filter[0].b2);
//    log_i("EQ a0=%f, a1=%f, a2=%f, b1=%f, b2=%f", m_filter[1].a0, m_filter[1].a1, m_filter[1].a2,
//                                                  m_filter[1].b1, m_filter[1].b2);
//    log_i("HS a0=%f, a1=%f, a2=%f, b1=%f, b2=%f", m_filter[2].a0, m_filter[2].a1, m_filter[2].a2,
//                                                  m_filter[2].b1, m_filter[2].b2);
}
//---------------------------------------------------------------------------------------------------------------------
int16_t* Audio::IIR_filterChain0(int16_t iir_in[2], bool clear){  // Infinite Impulse Response (IIR) filters

    uint8_t z1 = 0, z2 = 1;
    enum: uint8_t {in = 0, out = 1};
    float inSample[2];
    float outSample[2];
    static int16_t iir_out[2];

    if(clear){
        memset(m_filterBuff, 0, sizeof(m_filterBuff));            // zero IIR filterbuffer
        iir_out[0] = 0;
        iir_out[1] = 0;
        iir_in[0]  = 0;
        iir_in[1]  = 0;
    }

    inSample[LEFTCHANNEL]  = (float)(iir_in[LEFTCHANNEL]);
    inSample[RIGHTCHANNEL] = (float)(iir_in[RIGHTCHANNEL]);

    outSample[LEFTCHANNEL] =   m_filter[0].a0  * inSample[LEFTCHANNEL]
                             + m_filter[0].a1  * m_filterBuff[0][z1][in] [LEFTCHANNEL]
                             + m_filter[0].a2  * m_filterBuff[0][z2][in] [LEFTCHANNEL]
                             - m_filter[0].b1  * m_filterBuff[0][z1][out][LEFTCHANNEL]
                             - m_filter[0].b2  * m_filterBuff[0][z2][out][LEFTCHANNEL];

    m_filterBuff[0][z2][in] [LEFTCHANNEL]  = m_filterBuff[0][z1][in][LEFTCHANNEL];
    m_filterBuff[0][z1][in] [LEFTCHANNEL]  = inSample[LEFTCHANNEL];
    m_filterBuff[0][z2][out][LEFTCHANNEL]  = m_filterBuff[0][z1][out][LEFTCHANNEL];
    m_filterBuff[0][z1][out][LEFTCHANNEL]  = outSample[LEFTCHANNEL];
    iir_out[LEFTCHANNEL] = (int16_t)std::max(-32768.0f,std::min(32767.0f,outSample[LEFTCHANNEL]));


    outSample[RIGHTCHANNEL] =  m_filter[0].a0 * inSample[RIGHTCHANNEL]
                             + m_filter[0].a1 * m_filterBuff[0][z1][in] [RIGHTCHANNEL]
                             + m_filter[0].a2 * m_filterBuff[0][z2][in] [RIGHTCHANNEL]
                             - m_filter[0].b1 * m_filterBuff[0][z1][out][RIGHTCHANNEL]
                             - m_filter[0].b2 * m_filterBuff[0][z2][out][RIGHTCHANNEL];

    m_filterBuff[0][z2][in] [RIGHTCHANNEL] = m_filterBuff[0][z1][in][RIGHTCHANNEL];
    m_filterBuff[0][z1][in] [RIGHTCHANNEL] = inSample[RIGHTCHANNEL];
    m_filterBuff[0][z2][out][RIGHTCHANNEL] = m_filterBuff[0][z1][out][RIGHTCHANNEL];
    m_filterBuff[0][z1][out][RIGHTCHANNEL] = outSample[RIGHTCHANNEL];
    iir_out[RIGHTCHANNEL] = (int16_t)std::max(-32768.0f,std::min(32767.0f,outSample[RIGHTCHANNEL]));

    return iir_out;
}
//---------------------------------------------------------------------------------------------------------------------
int16_t* Audio::IIR_filterChain1(int16_t iir_in[2], bool clear){  // Infinite Impulse Response (IIR) filters

    uint8_t z1 = 0, z2 = 1;
    enum: uint8_t {in = 0, out = 1};
    float inSample[2];
    float outSample[2];
    static int16_t iir_out[2];

    if(clear){
        memset(m_filterBuff, 0, sizeof(m_filterBuff));            // zero IIR filterbuffer
        iir_out[0] = 0;
        iir_out[1] = 0;
        iir_in[0]  = 0;
        iir_in[1]  = 0;
    }

    inSample[LEFTCHANNEL]  = (float)(iir_in[LEFTCHANNEL]);
    inSample[RIGHTCHANNEL] = (float)(iir_in[RIGHTCHANNEL]);

    outSample[LEFTCHANNEL] =   m_filter[1].a0  * inSample[LEFTCHANNEL]
                             + m_filter[1].a1  * m_filterBuff[1][z1][in] [LEFTCHANNEL]
                             + m_filter[1].a2  * m_filterBuff[1][z2][in] [LEFTCHANNEL]
                             - m_filter[1].b1  * m_filterBuff[1][z1][out][LEFTCHANNEL]
                             - m_filter[1].b2  * m_filterBuff[1][z2][out][LEFTCHANNEL];

    m_filterBuff[1][z2][in] [LEFTCHANNEL]  = m_filterBuff[1][z1][in][LEFTCHANNEL];
    m_filterBuff[1][z1][in] [LEFTCHANNEL]  = inSample[LEFTCHANNEL];
    m_filterBuff[1][z2][out][LEFTCHANNEL]  = m_filterBuff[1][z1][out][LEFTCHANNEL];
    m_filterBuff[1][z1][out][LEFTCHANNEL]  = outSample[LEFTCHANNEL];
    iir_out[LEFTCHANNEL] = (int16_t)std::max(-32768.0f,std::min(32767.0f,outSample[LEFTCHANNEL]));


    outSample[RIGHTCHANNEL] =  m_filter[1].a0 * inSample[RIGHTCHANNEL]
                             + m_filter[1].a1 * m_filterBuff[1][z1][in] [RIGHTCHANNEL]
                             + m_filter[1].a2 * m_filterBuff[1][z2][in] [RIGHTCHANNEL]
                             - m_filter[1].b1 * m_filterBuff[1][z1][out][RIGHTCHANNEL]
                             - m_filter[1].b2 * m_filterBuff[1][z2][out][RIGHTCHANNEL];

    m_filterBuff[1][z2][in] [RIGHTCHANNEL] = m_filterBuff[1][z1][in][RIGHTCHANNEL];
    m_filterBuff[1][z1][in] [RIGHTCHANNEL] = inSample[RIGHTCHANNEL];
    m_filterBuff[1][z2][out][RIGHTCHANNEL] = m_filterBuff[1][z1][out][RIGHTCHANNEL];
    m_filterBuff[1][z1][out][RIGHTCHANNEL] = outSample[RIGHTCHANNEL];
    iir_out[RIGHTCHANNEL] = (int16_t)std::max(-32768.0f,std::min(32767.0f,outSample[RIGHTCHANNEL]));

    return iir_out;
}
//---------------------------------------------------------------------------------------------------------------------
int16_t* Audio::IIR_filterChain2(int16_t iir_in[2], bool clear){  // Infinite Impulse Response (IIR) filters

    uint8_t z1 = 0, z2 = 1;
    enum: uint8_t {in = 0, out = 1};
    float inSample[2];
    float outSample[2];
    static int16_t iir_out[2];

    if(clear){
        memset(m_filterBuff, 0, sizeof(m_filterBuff));            // zero IIR filterbuffer
        iir_out[0] = 0;
        iir_out[1] = 0;
        iir_in[0]  = 0;
        iir_in[1]  = 0;
    }

    inSample[LEFTCHANNEL]  = (float)(iir_in[LEFTCHANNEL]);
    inSample[RIGHTCHANNEL] = (float)(iir_in[RIGHTCHANNEL]);

    outSample[LEFTCHANNEL] =   m_filter[2].a0  * inSample[LEFTCHANNEL]
                             + m_filter[2].a1  * m_filterBuff[2][z1][in] [LEFTCHANNEL]
                             + m_filter[2].a2  * m_filterBuff[2][z2][in] [LEFTCHANNEL]
                             - m_filter[2].b1  * m_filterBuff[2][z1][out][LEFTCHANNEL]
                             - m_filter[2].b2  * m_filterBuff[2][z2][out][LEFTCHANNEL];

    m_filterBuff[2][z2][in] [LEFTCHANNEL]  = m_filterBuff[2][z1][in][LEFTCHANNEL];
    m_filterBuff[2][z1][in] [LEFTCHANNEL]  = inSample[LEFTCHANNEL];
    m_filterBuff[2][z2][out][LEFTCHANNEL]  = m_filterBuff[2][z1][out][LEFTCHANNEL];
    m_filterBuff[2][z1][out][LEFTCHANNEL]  = outSample[LEFTCHANNEL];
    iir_out[LEFTCHANNEL] = (int16_t)std::max(-32768.0f,std::min(32767.0f,outSample[LEFTCHANNEL]));


    outSample[RIGHTCHANNEL] =  m_filter[2].a0 * inSample[RIGHTCHANNEL]
                             + m_filter[2].a1 * m_filterBuff[2][z1][in] [RIGHTCHANNEL]
                             + m_filter[2].a2 * m_filterBuff[2][z2][in] [RIGHTCHANNEL]
                             - m_filter[2].b1 * m_filterBuff[2][z1][out][RIGHTCHANNEL]
                             - m_filter[2].b2 * m_filterBuff[2][z2][out][RIGHTCHANNEL];

    m_filterBuff[2][z2][in] [RIGHTCHANNEL] = m_filterBuff[2][z1][in][RIGHTCHANNEL];
    m_filterBuff[2][z1][in] [RIGHTCHANNEL] = inSample[RIGHTCHANNEL];
    m_filterBuff[2][z2][out][RIGHTCHANNEL] = m_filterBuff[2][z1][out][RIGHTCHANNEL];
    m_filterBuff[2][z1][out][RIGHTCHANNEL] = outSample[RIGHTCHANNEL];
    iir_out[RIGHTCHANNEL] = (int16_t)std::max(-32768.0f,std::min(32767.0f,outSample[RIGHTCHANNEL]));

    return iir_out;
}
//----------------------------------------------------------------------------------------------------------------------
//    AAC - T R A N S P O R T S T R E A M
//----------------------------------------------------------------------------------------------------------------------
bool Audio::ts_parsePacket(uint8_t* packet, uint8_t* packetStart, uint8_t* packetLength) {

    const uint8_t TS_PACKET_SIZE = 188;
    const uint8_t PAYLOAD_SIZE = 184;
    const uint8_t PID_ARRAY_LEN = 4;
    (void)PAYLOAD_SIZE;
    typedef struct{
        int number= 0;
        int pids[PID_ARRAY_LEN];
    } pid_array;

    static pid_array pidsOfPMT;
    static int PES_DataLength = 0;
    static int pidOfAAC = 0;

    if(packet == NULL){
        if(m_f_Log) log_i("parseTS reset");
        for(int i = 0; i < PID_ARRAY_LEN; i++) pidsOfPMT.pids[i] = 0;
        PES_DataLength = 0;
        pidOfAAC = 0;
        return true;
    }

    // --------------------------------------------------------------------------------------------------------
    // 0. Byte SyncByte  | 0 | 1 | 0 | 0 | 0 | 1 | 1 | 1 | always bit pattern of 0x47
    //---------------------------------------------------------------------------------------------------------
    // 1. Byte           |PUSI|TP|   |PID|PID|PID|PID|PID|
    //---------------------------------------------------------------------------------------------------------
    // 2. Byte           |PID|PID|PID|PID|PID|PID|PID|PID|
    //---------------------------------------------------------------------------------------------------------
    // 3. Byte           |TSC|TSC|AFC|ADC|CC |CC |CC |CC |
    //---------------------------------------------------------------------------------------------------------
    // 4.-187. Byte      |Payload data if AFC==01 or 11  |
    //---------------------------------------------------------------------------------------------------------

    // PUSI Payload unit start indicator, set when this packet contains the first byte of a new payload unit.
    //      The first byte of the payload will indicate where this new payload unit starts.
    // TP   Transport priority, set when the current packet has a higher priority than other packets with the same PID.
    // PID  Packet Identifier, describing the payload data.
    // TSC  Transport scrambling control, '00' = Not scrambled.
    // AFC  Adaptation field control, 01 – no adaptation field, payload only, 10 – adaptation field only, no payload,
    //                                11 – adaptation field followed by payload, 00 – RESERVED for future use
    // CC   Continuity counter, Sequence number of payload packets (0x00 to 0x0F) within each stream (except PID 8191)

    if(packet[0] != 0x47) {
        log_e("ts SyncByte not found, first bytes are %X %X %X %X", packet[0], packet[1], packet[2], packet[3]);
        stopSong();
        return false;
    }
    int PID = (packet[1] & 0x1F) << 8 | (packet[2] & 0xFF);
    if(m_f_Log) log_i("PID: 0x%04X(%d)", PID, PID);
    int PUSI = (packet[1] & 0x40) >> 6;
    if(m_f_Log) log_i("Payload Unit Start Indicator: %d", PUSI);
    int AFC = (packet[3] & 0x30) >> 4;
    if(m_f_Log) log_i("Adaption Field Control: %d", AFC);

    int AFL = -1;
    if((AFC & 0b10) == 0b10) {  // AFC '11' Adaptation Field followed
        AFL = packet[4] & 0xFF; // Adaptation Field Length
        if(m_f_Log) log_i("Adaptation Field Length: %d", AFL);
    }
    int PLS = PUSI ? 5 : 4;     // PayLoadStart, Payload Unit Start Indicator

    if(PID == 0) {
        // Program Association Table (PAT) - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        if(m_f_Log)  log_i("PAT");
        pidsOfPMT.number = 0;
        pidOfAAC = 0;
        int startOfProgramNums = 8;
        int lengthOfPATValue = 4;
        int sectionLength = ((packet[PLS + 1] & 0x0F) << 8) | (packet[PLS + 2] & 0xFF);
        if(m_f_Log) log_i("Section Length: %d", sectionLength);
        int program_number, program_map_PID;
        (void)program_number;
        int indexOfPids = 0;
        for(int i = startOfProgramNums; i <= sectionLength; i += lengthOfPATValue) {
            program_number = ((packet[PLS + i] & 0xFF) << 8) | (packet[PLS + i + 1] & 0xFF);
            program_map_PID = ((packet[PLS + i + 2] & 0x1F) << 8) | (packet[PLS + i + 3] & 0xFF);
            if(m_f_Log) log_i("Program Num: 0x%04X(%d) PMT PID: 0x%04X(%d)", program_number, program_number,
                   program_map_PID, program_map_PID);
            pidsOfPMT.pids[indexOfPids++] = program_map_PID;
        }
        pidsOfPMT.number = indexOfPids;
        *packetStart = 0;
        *packetLength = 0;
        return true;

    }
    else if(PID == pidOfAAC) {
        if(m_f_Log) log_i("AAC");
        uint8_t posOfPacketStart = 4;
        if(AFL >= 0) {posOfPacketStart = 5 + AFL;
        if(m_f_Log) log_i("posOfPacketStart: %d", posOfPacketStart);}
        // Packetized Elementary Stream (PES) - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        if (PES_DataLength > 0) {
            *packetStart    = posOfPacketStart;
            *packetLength   = TS_PACKET_SIZE - posOfPacketStart;
            PES_DataLength -= TS_PACKET_SIZE - posOfPacketStart;
            return true;
        }
        else{
            int firstByte  = packet[posOfPacketStart] & 0xFF;
            int secondByte = packet[posOfPacketStart + 1] & 0xFF;
            int thirdByte  = packet[posOfPacketStart + 2] & 0xFF;
            if(m_f_Log) log_i("First 3 bytes: %02X %02X %02X", firstByte, secondByte, thirdByte);
            if(firstByte == 0x00 && secondByte == 0x00 && thirdByte == 0x01) { // Packet start code prefix
                // PES
                uint8_t StreamID = packet[posOfPacketStart + 3] & 0xFF;
                if(StreamID >= 0xC0 && StreamID <= 0xDF) {;} // okay ist audio stream
                if(StreamID >= 0xE0 && StreamID <= 0xEF) {log_e("video stream!"); return false;}
                const uint8_t posOfPacketLengthLatterHalf = 5;
                int PES_PacketLength =
                    ((packet[posOfPacketStart + 4] & 0xFF) << 8) + (packet[posOfPacketStart + 5] & 0xFF);
                if(m_f_Log) log_i("PES Packet length: %d", PES_PacketLength);
                PES_DataLength = PES_PacketLength;
                int posOfHeaderLength = 8;
                int PESRemainingHeaderLength = packet[posOfPacketStart + posOfHeaderLength] & 0xFF;
                if(m_f_Log) log_i("PES Header length: %d", PESRemainingHeaderLength);
                int startOfData = posOfHeaderLength + PESRemainingHeaderLength + 1;
                if(m_f_Log) log_i("First AAC data byte: %02X", packet[posOfPacketStart + startOfData]);
                *packetStart = posOfPacketStart + startOfData;
                *packetLength = TS_PACKET_SIZE - posOfPacketStart - startOfData;
                PES_DataLength -= (TS_PACKET_SIZE - posOfPacketStart) - (posOfPacketLengthLatterHalf + 1);
                return true;
            }
        }
        *packetStart = 0;
        *packetLength = 0;
        if(m_f_Log) log_e("PES not found");
        return false;
    }
    else if(pidsOfPMT.number) {
        //  Program Map Table (PMT) - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        for(int i = 0; i < pidsOfPMT.number; i++) {
            if(PID == pidsOfPMT.pids[i]) {
                if(m_f_Log) log_i("PMT");
                int staticLengthOfPMT = 12;
                int sectionLength = ((packet[PLS + 1] & 0x0F) << 8) | (packet[PLS + 2] & 0xFF);
                if(m_f_Log) log_i("Section Length: %d", sectionLength);
                int programInfoLength = ((packet[PLS + 10] & 0x0F) << 8) | (packet[PLS + 11] & 0xFF);
                if(m_f_Log) log_i("Program Info Length: %d", programInfoLength);
                int cursor = staticLengthOfPMT + programInfoLength;
                while(cursor < sectionLength - 1) {
                    int streamType = packet[PLS + cursor] & 0xFF;
                    int elementaryPID = ((packet[PLS + cursor + 1] & 0x1F) << 8) | (packet[PLS + cursor + 2] & 0xFF);
                    if(m_f_Log) log_i("Stream Type: 0x%02X Elementary PID: 0x%04X", streamType, elementaryPID);

                    if(streamType == 0x0F || streamType == 0x11) {
                        if(m_f_Log) log_i("AAC PID discover");
                        pidOfAAC= elementaryPID;
                    }
                    int esInfoLength = ((packet[PLS + cursor + 3] & 0x0F) << 8) | (packet[PLS + cursor + 4] & 0xFF);
                    if(m_f_Log) log_i("ES Info Length: 0x%04X", esInfoLength);
                    cursor += 5 + esInfoLength;
                }
            }
        }
        *packetStart = 0;
        *packetLength = 0;
        return true;
    }
    // PES received before PAT and PMT seen
    *packetStart = 0;
    *packetLength = 0;
    return false;
}
//----------------------------------------------------------------------------------------------------------------------
//    W E B S T R E A M  -  H E L P   F U N C T I O N S
//----------------------------------------------------------------------------------------------------------------------
uint16_t Audio::readMetadata(uint16_t maxBytes, bool first) {

    static uint16_t pos_ml = 0;                          // determines the current position in metaline
    static uint16_t metalen = 0;
    uint16_t res = 0;
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(first){
        pos_ml = 0;
        metalen = 0;
        return 0;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(!maxBytes) return 0;  // guard

    if(!metalen) {
        int b = netprefetch::active()?netprefetch::read():_client->read();   // First byte of metadata?
        if(b<0)return 0; // No byte consumed: retain metadata state.
        metalen = b * 16 ;                              // New count for metadata including length byte
        if(metalen > 512){
            AUDIO_INFO("Metadata block to long! Skipping all Metadata from now on.");
            m_f_metadata = false;                        // expect stream without metadata
            return 1;
        }
        pos_ml = 0; chbuf[pos_ml] = 0;                   // Prepare for new line
        res = 1;
    }
    if(!metalen) {m_metacount = m_metaint; return res;} // metalen is 0
    const uint16_t take=min((uint16_t)(metalen-pos_ml),(uint16_t)(maxBytes-res));
    // Read only currently available bytes; Stream::readBytes waits for the
    // entire request and can block PCM service after a short network read.
    const int got=take?(netprefetch::active()?netprefetch::read(reinterpret_cast<uint8_t*>(&chbuf[pos_ml]),take):_client->read(reinterpret_cast<uint8_t*>(&chbuf[pos_ml]),take)):0;
    const uint16_t a=got>0?uint16_t(got):0;
    res += a;
    pos_ml += a;
    if(pos_ml == metalen) {
        chbuf[pos_ml] = '\0';
        if(strlen(chbuf)) {                             // Any info present?
            // metaline contains artist and song name.  For example:
            // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
            // Sometimes it is just other info like:
            // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
            // Isolate the StreamTitle, remove leading and trailing quotes if present.
            if(m_f_Log) log_i("metaline %s", chbuf);
            latinToUTF8(chbuf, sizeof(chbuf)); // convert to UTF-8 if necessary
            int pos = indexOf(chbuf, "song_spot", 0);    // remove some irrelevant infos
            if(pos > 3) {                                // e.g. song_spot="T" MediaBaseId="0" itunesTrackId="0"
                chbuf[pos] = 0;
            }
            showstreamtitle(chbuf);   // Show artist and title if present in metadata
        }
        m_metacount = m_metaint;
        metalen = 0;
        pos_ml = 0;
    }
    return res;
}
//----------------------------------------------------------------------------------------------------------------------
size_t Audio::chunkedDataTransfer(uint8_t* bytes){
    size_t chunksize = 0;
    int b = 0;
    uint32_t ctime = millis();
    uint32_t timeout = 2000; // ms
    while(true){
        if(ctime + timeout < millis()) {
            log_e("timeout");
            stopSong();
            return 0;
        }
        b = netprefetch::active()?netprefetch::read():_client->read();
        *bytes++;
        if(b < 0){if(netprefetch::active())vTaskDelay(1);continue;}  // -1 no data available
        if(b == '\n') break;
        if(b < '0') continue;
        // We have received a hexadecimal character.  Decode it and add to the result.
        b = toupper(b) - '0';                       // Be sure we have uppercase
        if(b > 9) b = b - 7;                        // Translate A..F to 10..15
        chunksize = (chunksize << 4) + b;
    }
    if(m_f_Log) log_i("chunksize %d", chunksize);
    return chunksize;
}
//----------------------------------------------------------------------------------------------------------------------
bool Audio::readID3V1Tag(){
    // this is an V1.x id3tag after an audio block, ID3 v1 tags are ASCII
    // Version 1.x is a fixed size at the end of the file (128 bytes) after a <TAG> keyword.
    if(m_codec != CODEC_MP3) return false;
    if(InBuff.bufferFilled() == 128 && startsWith((const char*)InBuff.getReadPtr(), "TAG")){ // maybe a V1.x TAG
        char title[31];
        memcpy(title,   InBuff.getReadPtr() + 3 +  0,  30);  title[30]  = '\0'; latinToUTF8(title, sizeof(title));
        char artist[31];
        memcpy(artist,  InBuff.getReadPtr() + 3 + 30,  30); artist[30]  = '\0'; latinToUTF8(artist, sizeof(artist));
        char album[31];
        memcpy(album,   InBuff.getReadPtr() + 3 + 60,  30);  album[30]  = '\0'; latinToUTF8(album, sizeof(album));
        char year[5];
        memcpy(year,    InBuff.getReadPtr() + 3 + 90,   4);  year[4]    = '\0'; latinToUTF8(year, sizeof(year));
        char comment[31];
        memcpy(comment, InBuff.getReadPtr() + 3 + 94,  30); comment[30] = '\0'; latinToUTF8(comment, sizeof(comment));
        uint8_t zeroByte = *(InBuff.getReadPtr() + 125);
        uint8_t track    = *(InBuff.getReadPtr() + 126);
        uint8_t genre    = *(InBuff.getReadPtr() + 127);
        if(zeroByte) {AUDIO_INFO("ID3 version: 1");} //[2]
        else         {AUDIO_INFO("ID3 Version 1.1");}
        if(strlen(title))  {sprintf(chbuf, "Title: %s",        title);   if(audio_id3data) audio_id3data(chbuf);}
        if(strlen(artist)) {sprintf(chbuf, "Artist: %s",       artist);  if(audio_id3data) audio_id3data(chbuf);}
        if(strlen(album))  {sprintf(chbuf, "Album: %s",        album);   if(audio_id3data) audio_id3data(chbuf);}
        if(strlen(year))   {sprintf(chbuf, "Year: %s",         year);    if(audio_id3data) audio_id3data(chbuf);}
        if(strlen(comment)){sprintf(chbuf, "Comment: %s",      comment); if(audio_id3data) audio_id3data(chbuf);}
        if(zeroByte == 0)  {sprintf(chbuf, "Track Number: %d", track);   if(audio_id3data) audio_id3data(chbuf);}
        if(genre < 192)    {sprintf(chbuf, "Genre: %d",        genre);   if(audio_id3data) audio_id3data(chbuf);} //[1]
        return true;
    }
    if(InBuff.bufferFilled() == 227 && startsWith((const char*)InBuff.getReadPtr(), "TAG+")){ // ID3V1EnhancedTAG
        AUDIO_INFO("ID3 version: 1 - Enhanced TAG");
        char title[61];
        memcpy(title,   InBuff.getReadPtr() + 4 +   0,  60);  title[60] = '\0'; latinToUTF8(title, sizeof(title));
        char artist[61];
        memcpy(artist,  InBuff.getReadPtr() + 4 +  60,  60); artist[60] = '\0'; latinToUTF8(artist, sizeof(artist));
        char album[61];
        memcpy(album,   InBuff.getReadPtr() + 4 + 120,  60);  album[60] = '\0'; latinToUTF8(album, sizeof(album));
        // one byte "speed" 0=unset, 1=slow, 2= medium, 3=fast, 4=hardcore
        char genre[31];
        memcpy(genre,   InBuff.getReadPtr() + 5 + 180,  30);  genre[30] = '\0'; latinToUTF8(genre, sizeof(genre));
        // six bytes "start-time", the start of the music as mmm:ss
        // six bytes "end-time",   the end of the music as mmm:ss
        if(strlen(title))  {sprintf(chbuf, "Title: %s",  title);  if(audio_id3data) audio_id3data(chbuf);}
        if(strlen(artist)) {sprintf(chbuf, "Artist: %s", artist); if(audio_id3data) audio_id3data(chbuf);}
        if(strlen(album))  {sprintf(chbuf, "Album: %s",  album);  if(audio_id3data) audio_id3data(chbuf);}
        if(strlen(genre))  {sprintf(chbuf, "Genre: %s",  genre);  if(audio_id3data) audio_id3data(chbuf);}
        return true;
    }
    return false;
    // [1] https://en.wikipedia.org/wiki/List_of_ID3v1_Genres
    // [2] https://en.wikipedia.org/wiki/ID3#ID3v1_and_ID3v1.1[5]
}
//----------------------------------------------------------------------------------------------------------------------
void Audio::slowStreamDetection(uint32_t inBuffFilled, uint32_t maxFrameSize){
    static uint32_t tmr_1s   = millis(); // timer 1 sec
    static bool     f_tmr_1s = false;
    static uint8_t  cnt_slow = 0;
    if(tmr_1s + 1000 < millis()) {f_tmr_1s = true; tmr_1s = millis();}
    if(m_codec == CODEC_WAV)  maxFrameSize /= 4;
    if(m_codec == CODEC_FLAC) maxFrameSize /= 2;
    if(inBuffFilled < maxFrameSize){
        cnt_slow ++;
        if(f_tmr_1s) {
            if(cnt_slow > 50) AUDIO_INFO("slow stream, dropouts are possible");
            f_tmr_1s = false;
            cnt_slow = 0;
        }
    }
    else cnt_slow = 0;
}
#endif  //  if VS1053_CS==255



#include "container_probe.h"
bool Audio::extraM4aFile(){
    auto read=[this](uint64_t p,uint8_t* dst,size_t n){return p<=UINT32_MAX&&audiofile.seek(uint32_t(p))&&audiofile.read(dst,n)==int(n);};
    const auto kind=containerprobe::m4a(read,m_file_size);audiofile.seek(0);
    if(kind==containerprobe::M4a::AlacLate){AUDIO_ERROR("ALAC requires MOOV before MDAT");return false;}
    if(kind==containerprobe::M4a::Alac)m_codec=CODEC_EXTRA_M4A;
    // AAC and unknown layouts retain the existing M4A/AAC path.
    return true;
}
bool Audio::extraWavFile(){
    if(!audiofile)return false;uint8_t riff[12];bool result=false;
    const uint32_t size=audiofile.size();
    if(audiofile.read(riff,12)==12&&!memcmp(riff,"RIFF",4)&&!memcmp(riff+8,"WAVE",4)){
        uint32_t position=12;
        while(position<=65536&&position<=size&&size-position>=8){
            uint8_t h[8];if(!audiofile.seek(position)||audiofile.read(h,8)!=8)break;
            uint32_t n=uint32_t(h[4])|(uint32_t(h[5])<<8)|(uint32_t(h[6])<<16)|(uint32_t(h[7])<<24);
            if(n>size-position-8)break;
            if(!memcmp(h,"fmt ",4)){uint8_t fmt[2];if(n>=16&&audiofile.read(fmt,2)==2){unsigned code=unsigned(fmt[0])|(unsigned(fmt[1])<<8);result=code==6||code==7||code==17;}break;}
            const uint64_t next=uint64_t(position)+8+n+(n&1);if(next>size)break;position=uint32_t(next);
        }
    }
    audiofile.seek(0);return result;
}
int Audio::sendExtraBytes(uint8_t* data,size_t bytes,bool eos){
    extraOutput=false;
    // The PCM owner must finish the previous block before decoding can reuse it.
    if(m_validSamples){
        if(!playBtBlock()){if(IBT_NO_PSRAM&&!btaudio::outputSelected()&&getBitsPerSample()==16&&m_curSample==0&&size_t(m_validSamples)<=OutputSamples/2)playExtraDacBlock();else while(m_validSamples&&m_f_running){if(!playChunk())break;}}
        return 0;
    }
    pathprobe::Scope decodeProbe(pathprobe::Decode);
    // Keep the allocated PCM capacity; bound only this owner service quantum.
    const size_t quantum=IBT_NO_PSRAM?std::min(size_t(OutputSamples),size_t(512)):OutputSamples;
    const auto r=extraDecoder.decode(data,bytes,eos,m_outBuff,quantum);
    decodeProbe.units(r.error?-1:int(r.consumed));
    if(r.error){AUDIO_ERROR("Extra decoder error %d",r.error);stopSong();return -1;}
    if(!r.frames)return int(r.consumed);
    const auto& info=extraDecoder.info();
    const bool firstPcm=!m_f_playing;

    setChannels(info.channel);
    // Keep source timing/decoder rate. Adapt only this Light DAC format to the
    // native-WAV44.1k output verified on the same physical device; BT is unchanged.
    const uint32_t dacRate=IBT_NO_PSRAM&&!btaudio::outputSelected()&&info.sample_rate==32000?44100:info.sample_rate;
    if(firstPcm){m_extraDacResampler.reset();m_extraDacSourceRate=dacRate==info.sample_rate?0:info.sample_rate;}
    // Reapplying an unchanged rate stops/restarts the legacy I2S driver.
    // A new file/restart must still configure its first PCM block, even when
    // the cached rate matches; subsequent blocks only change a different rate.
    if((firstPcm||getSampleRate()!=dacRate)&&!setSampleRate(dacRate)){AUDIO_ERROR("Extra decoder: I2S clock rejected");stopSong();return -1;}
    if(firstPcm){m_f_playing=true;m_PlayingStartTime=millis();Serial.printf("[CODEC-EXTRA] ready=%u rate=%lu channels=%u source_bits=%u output_bits=16\n",unsigned(m_codec),(unsigned long)info.sample_rate,unsigned(info.channel),unsigned(info.bits_per_sample));}
    setBitsPerSample(16);
    if(IBT_NO_PSRAM&&firstPcm&&!btaudio::outputSelected())directrx::pcmBegin(dacRate,info.channel);
    if(IBT_NO_PSRAM&&!btaudio::outputSelected())directrx::pcmRaw(m_outBuff,r.frames,info.channel);
    if(firstPcm&&IBT_NO_PSRAM)Serial.printf("[EXTRA-DAC] source_hz=%lu output_hz=%lu source_channels=%u resampler=%s writer=NATIVE64\n",(unsigned long)info.sample_rate,(unsigned long)dacRate,unsigned(info.channel),m_extraDacSourceRate?"EXISTING_LINEAR":"BYPASS");
    if(info.bitrate){setBitrate(info.bitrate);m_avr_bitrate=info.bitrate;}
    m_validSamples=r.frames;m_curSample=0;extraOutput=true;
    m_audioCurrentTime+=float(r.frames)/float(info.sample_rate);
    pathprobe::frames(m_validSamples);
    if(playBtBlock())return int(r.consumed);
    if(audio_process_extern){bool proceed=false;audio_process_extern(m_outBuff,m_validSamples,&proceed);if(!proceed){m_validSamples=0;m_curSample=0;return int(r.consumed);}}
    if(IBT_NO_PSRAM&&!btaudio::outputSelected()){if(!playExtraDacBlock()){if(m_f_running)stopSong();return -1;}}
    else while(m_validSamples){if(!playChunk())break;}
    return int(r.consumed);
}
void Audio::processExtraLocalFile(){
    if(!audiofile||!m_f_running||!m_outBuff)return;
    if(m_validSamples){
        if(!playBtBlock()){if(IBT_NO_PSRAM&&!btaudio::outputSelected()&&getBitsPerSample()==16&&m_curSample==0&&size_t(m_validSamples)<=OutputSamples/2)playExtraDacBlock();else while(m_validSamples&&m_f_running){if(!playChunk())break;}}
        return; // No SD read, decode, loop restart or EOF while PCM is pending.
    }
    size_t room=InBuff.writeSpace();if(room>8192)room=8192;
    if(IBT_NO_PSRAM&&room>1024)room=1024; // Bound blocking card work between DAC submissions.
    if(room&&audiofile.position()<m_file_size){
        int read=audiofile.read(InBuff.getWritePtr(),room);
        if(read>0){InBuff.bytesWritten(size_t(read));extraReadWaiting=false;}
        else{
            const uint32_t now=millis();
            if(!extraReadWaiting){extraReadWaiting=true;extraReadWaitSince=now;}
            if(uint32_t(now-extraReadWaitSince)>=5000){AUDIO_ERROR("Extra decoder: SD read stalled");stopSong();return;}
            vTaskDelay(1);
        }
    }
    const size_t buffered=InBuff.bufferFilled();
    const size_t contiguous=std::min(size_t(4096),size_t(InBuff.getMaxBlockSize()));
    const size_t bytes=buffered>contiguous?contiguous:buffered;
    const bool fileDone=audiofile.position()>=m_file_size;
    const bool eos=containerprobe::finalChunk(fileDone,buffered,bytes);
    if(!bytes&&!fileDone&&!extraDecoder.pending())return;
    const int used=sendExtraBytes(InBuff.getReadPtr(),bytes,eos);
    if(used<0||!m_f_running)return;
    if(used>0)InBuff.bytesWasRead(size_t(used));
    if(used>0||extraOutput||extraDecoder.pending()||!fileDone)return;
    if(bytes){AUDIO_ERROR("Extra decoder: truncated or unconsumed EOF");stopSong();return;}
    if(m_f_loop){if(audiofile.seek(0)&&extraDecoder.restart()){InBuff.resetBuffer();m_audioCurrentTime=0;m_f_playing=false;return;}stopSong();return;}
    char name[256];strlcpy(name,audiofile.name(),sizeof(name));
    stopSong();InBuff.resetBuffer();if(audio_eof_mp3)audio_eof_mp3(name);
}
