#pragma once
#include <NetworkClientSecure.h>
#include "light_network_gate.h"
#include "stream_cancel.h"
#include "memory_checkpoint.h"
#if IBT_NO_PSRAM && defined(IBT_LIGHT4_DNS_AUTO)
#include "tls_record_policy.h"
#include "ram_reserve.h"
#include <esp_heap_caps.h>
#endif
namespace radiodns {
class SecureClient:public NetworkClientSecure {
    bool setupDeadline=false;
#if IBT_STREAM_ART_DIAG
    bool ownsStream=true;
#endif
public:
#if IBT_STREAM_ART_DIAG
    void backgroundRole(){ownsStream=false;}
#endif
    void stop() override {
        NetworkClientSecure::stop();
#if IBT_NO_PSRAM
#if IBT_STREAM_ART_DIAG
        if(ownsStream)
#endif
        lightnet::gate().streamClosed();
#endif
    }
    bool preHandshakeTimeout()const{return setupDeadline;}
    int resolvedConnect(IPAddress ip,uint16_t port,const char* host,int32_t timeout){
#if IBT_NO_PSRAM && defined(IBT_LIGHT4_DNS_AUTO)
        for(unsigned pass=0;pass<2;++pass){
#else
        {
#endif
        if(streamcancel::state().cancelled())return 0;
        _timeout=timeout;setupDeadline=false;
        const uint32_t start=millis();
        // Split at the SDK's supported STARTTLS boundary; no plaintext request
        // is sent, and the existing SNI/certificate/insecure policy is retained.
        _stillinPlainStart=true;
        const int ready=NetworkClientSecure::connect(ip,port,host,_CA_cert,_cert,_private_key);
        const uint32_t elapsed=millis()-start;
        char message[48];const int code=lastError(message,sizeof(message));
        Serial.printf("[STREAM-TRANSPORT] phase=TCP_TLS_INIT ok=%u code=%d elapsed_ms=%lu timeout_ms=%ld\n",unsigned(ready!=0),code,(unsigned long)elapsed,(long)timeout);
        if(streamcancel::state().cancelled()){_stillinPlainStart=false;stop();return 0;}
        if(!ready){_stillinPlainStart=false;setupDeadline=code==-1&&timeout>0&&elapsed>=uint32_t(timeout);return 0;}
#if IBT_NO_PSRAM && defined(IBT_LIGHT4_DNS_AUTO)
        const bool compact=pass==0;
        if(lighttls::configure(&sslclient->ssl_conf,compact)!=0){_stillinPlainStart=false;stop();return 0;}
        lighttls::ObservedBio observed{&sslclient->socket};
        mbedtls_ssl_set_bio(&sslclient->ssl_ctx,&observed,lighttls::ObservedBio::send,lighttls::ObservedBio::receive,nullptr);
#endif
        const uint32_t handshake=millis();
        // Same SDK handshake as startTLS(), but retain its actual error code:
        // this SDK's startTLS() returns only0/1 and leaves last_error stale.
        int handshakeCode;{lightmem::Phase memoryPhase("STREAM_TLS_HANDSHAKE");handshakeCode=ssl_starttls_handshake(sslclient.get());}
#if IBT_NO_PSRAM && defined(IBT_LIGHT4_DNS_AUTO)
        mbedtls_ssl_set_bio(&sslclient->ssl_ctx,&sslclient->socket,mbedtls_net_send,mbedtls_net_recv,nullptr);
        const bool echoed=observed.hello.acknowledged(lighttls::compactCode);
        Serial.printf("[TLS-RECORD] offered=%u echoed=%u rx_buffer=%u tx_buffer=%u free=%u largest=%u reserve=%u\n",compact?512u:0u,unsigned(echoed),unsigned(sslclient->ssl_ctx.MBEDTLS_PRIVATE(in_buf_len)),unsigned(sslclient->ssl_ctx.MBEDTLS_PRIVATE(out_buf_len)),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(ibtram::reserve));
        if(handshakeCode>=0&&lighttls::fallback(compact,echoed)){
            // No application byte is sent on an unconfirmed small-record context.
            // Recreate at the full default, never reinterpret a too-small buffer.
            Serial.println("[TLS-RECORD] fallback=FULL_CONTEXT reason=no_confirmed_echo");_stillinPlainStart=false;stop();continue;
        }
        if(handshakeCode>=0&&heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<ibtram::reserve){_stillinPlainStart=false;stop();return 0;}
#endif
        sslclient->last_error=handshakeCode;
        const int result=handshakeCode>=0;
        if(!result)stop();
        _stillinPlainStart=false;
        const int tlsCode=lastError(message,sizeof(message));
        Serial.printf("[STREAM-TRANSPORT] phase=TLS_HANDSHAKE ok=%u code=%d elapsed_ms=%lu\n",unsigned(result!=0),tlsCode,(unsigned long)(millis()-handshake));
        return result;
        }
        return 0;
    }
};
}
