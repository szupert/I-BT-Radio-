#pragma once
#include "dns_packet.h"
#include "core.h"
#include "dns_policy.h"
#include "stream_secure.h"
#include "resource_limiter.h"
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <lwip/sockets.h>
#include <esp_random.h>
namespace radiodns {
// Audio-owner only, during opening; no extra task, cache or global DNS mutation.
inline bool direct(const char* host,IPAddress server,IPAddress& result,unsigned index){
    if(!uint32_t(server))return false;
    uint8_t packet[512];const uint16_t id=uint16_t(esp_random());
    const size_t length=query(host,id,packet,sizeof(packet));if(!length)return false;
    int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(fd<0)return false;
    sockaddr_in peer={};peer.sin_family=AF_INET;peer.sin_port=htons(53);peer.sin_addr.s_addr=uint32_t(server);
    bool good=false;unsigned received=0;const uint32_t start=millis();
    const bool sent=lwip_connect(fd,reinterpret_cast<sockaddr*>(&peer),sizeof(peer))==0&&send(fd,packet,length,0)==int(length);
    if(sent){
        while(uint32_t(millis()-start)<1500){
            const unsigned elapsed=uint32_t(millis()-start);if(elapsed>=1500)break;
            const unsigned left=1500-elapsed;timeval wait={0,long(left*1000)};
            fd_set reads;FD_ZERO(&reads);FD_SET(fd,&reads);
            if(select(fd+1,&reads,nullptr,nullptr,&wait)<=0)break;
            const int n=recv(fd,packet,sizeof(packet),0);if(n>0)++received;uint8_t bytes[4];
            if(n>0&&answer(packet,n,id,host,bytes)){result=IPAddress(bytes[0],bytes[1],bytes[2],bytes[3]);good=true;break;}
        }
    }close(fd);
    Serial.printf("[STREAM-DNS-TRY] fallback=%u sent=%u received=%u accepted=%u elapsed_ms=%lu\n",index,unsigned(sent),received,unsigned(good),(unsigned long)(millis()-start));
    return good;
}
inline int connect(NetworkClient* client,SecureClient& secure,const char* host,uint16_t port,int32_t timeout){
    IPAddress address;const uint32_t started=millis();
    const auto settings=dual::snapshot().settings;
    unsigned preferredServer=0;
    bool resolved=preferred(settings.dnsManual,settings.dnsPrimary,settings.dnsSecondary,[&](const uint8_t* a,unsigned index){
        const bool ok=direct(host,IPAddress(a[0],a[1],a[2],a[3]),address,index);if(ok)preferredServer=index;return ok;
    });
    const int status=resolved?1:Network.hostByName(host,address);
    if(!resolved)resolved=status==1&&address.type()==IPv4&&uint32_t(address)!=0;
    unsigned fallback=0;
    Serial.printf("[STREAM-DNS-CONFIG] manual=%u preferred=%u\n",unsigned(settings.dnsManual),preferredServer);
    if(!resolved&&WiFi.status()==WL_CONNECTED){
        IPAddress servers[3]={WiFi.dnsIP(1),IPAddress(1,1,1,1),IPAddress(8,8,8,8)};
        for(unsigned i=0;i<3&&!resolved;++i){
            if(!uint32_t(servers[i])||servers[i]==WiFi.dnsIP(0))continue;
            bool duplicate=false;for(unsigned j=0;j<i;++j)if(servers[j]==servers[i])duplicate=true;
            if(duplicate)continue;if(WiFi.status()!=WL_CONNECTED)break;
            fallback=i+1;resolved=direct(host,servers[i],address,fallback);
        }
    }
    Serial.printf("[STREAM-DNS] sdk=%d resolved=%u fallback=%u elapsed_ms=%lu\n",status,unsigned(resolved),fallback,(unsigned long)(millis()-started));
    if(!resolved)return 0; // A DNS error must never enter TCP/TLS with 0.0.0.0.
    if(client==static_cast<NetworkClient*>(&secure)){
        // Preserve Host/SNI and the existing certificate / insecure policy.
        int result=secure.resolvedConnect(address,port,host,timeout);
        // One bounded retry for the observed pre-handshake deadline only.
        // No retry for DNS, certificate, handshake or low-memory failures.
        const bool retry=!result&&secure.preHandshakeTimeout()&&WiFi.status()==WL_CONNECTED&&ibtlimit::allowAllocation(0);
        Serial.printf("[STREAM-OPEN] ok=%u retry=%u wifi=%u rssi=%d free=%lu largest=%lu\n",unsigned(result!=0),unsigned(retry),unsigned(WiFi.status()==WL_CONNECTED),WiFi.RSSI(),(unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
        if(retry){vTaskDelay(pdMS_TO_TICKS(100));if(WiFi.status()==WL_CONNECTED)result=secure.resolvedConnect(address,port,host,timeout);}
        return result;
    }
    return client->connect(address,port,timeout);
}
}
