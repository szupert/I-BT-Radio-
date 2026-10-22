#pragma once
#include <NetworkClientSecure.h>
namespace radiodns {
class SecureClient:public NetworkClientSecure {
    bool setupDeadline=false;
public:
    bool preHandshakeTimeout()const{return setupDeadline;}
    int resolvedConnect(IPAddress ip,uint16_t port,const char* host,int32_t timeout){
        _timeout=timeout;setupDeadline=false;
        const uint32_t start=millis();
        // Split at the SDK's supported STARTTLS boundary; no plaintext request
        // is sent, and the existing SNI/certificate/insecure policy is retained.
        _stillinPlainStart=true;
        const int ready=NetworkClientSecure::connect(ip,port,host,_CA_cert,_cert,_private_key);
        const uint32_t elapsed=millis()-start;
        char message[48];const int code=lastError(message,sizeof(message));
        Serial.printf("[STREAM-TRANSPORT] phase=TCP_TLS_INIT ok=%u code=%d elapsed_ms=%lu timeout_ms=%ld\n",unsigned(ready!=0),code,(unsigned long)elapsed,(long)timeout);
        if(!ready){_stillinPlainStart=false;setupDeadline=code==-1&&timeout>0&&elapsed>=uint32_t(timeout);return 0;}
        const uint32_t handshake=millis();
        // Same SDK handshake as startTLS(), but retain its actual error code:
        // this SDK's startTLS() returns only0/1 and leaves last_error stale.
        const int handshakeCode=ssl_starttls_handshake(sslclient.get());
        sslclient->last_error=handshakeCode;
        const int result=handshakeCode>=0;
        if(!result)stop();
        _stillinPlainStart=false;
        const int tlsCode=lastError(message,sizeof(message));
        Serial.printf("[STREAM-TRANSPORT] phase=TLS_HANDSHAKE ok=%u code=%d elapsed_ms=%lu\n",unsigned(result!=0),tlsCode,(unsigned long)(millis()-handshake));
        return result;
    }
};
}
