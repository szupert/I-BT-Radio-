#pragma once
#include <stdint.h>
namespace clocksync {
constexpr uint32_t ValidUtc=1704067200;
constexpr uint32_t RetryMs=60000;
constexpr unsigned ServerCount=4;
constexpr unsigned ServerCapacity=64;
struct Servers {char values[ServerCount][ServerCapacity]={"162.159.200.1","162.159.200.123","time.cloudflare.com","pool.ntp.org"};};
// Numeric Cloudflare addresses are published NTP endpoints. They let the first
// request succeed even when the router's DNS cannot resolve an NTP hostname.
constexpr const char* server(unsigned index){
    switch(index%ServerCount){
        case 0:return "162.159.200.1";
        case 1:return "162.159.200.123";
        case 2:return "time.cloudflare.com";
        default:return "pool.ntp.org";
    }
}
// A host or IPv4 address only: no URL, port, spaces or control characters.
constexpr bool validServer(const char* value){
    if(!value)return false;
    if(!*value)return true; // An empty slot is disabled.
    bool numeric=true;
    unsigned length=0;
    for(;value[length];++length){
        if(length>=ServerCapacity-1)return false;
        if((value[length]<'0'||value[length]>'9')&&value[length]!='.')numeric=false;
    }
    unsigned n=0,label=0,part=0,parts=1;
    char previous=0;
    for(;value[n];++n){
        if(n>=ServerCapacity-1)return false;
        const char c=value[n];
        const bool digit=c>='0'&&c<='9';
        if(c=='.'){
            if(!label||previous=='-')return false;
            if(part>255&&numeric)return false;
            ++parts;part=0;label=0;
        }else{
            if(!(digit||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='-'))return false;
            if(!label&&c=='-')return false;
            ++label;if(label>63)return false;
            if(numeric){part=part*10+unsigned(c-'0');if(part>255)return false;}
        }
        previous=c;
    }
    return label&&previous!='-'&&(!numeric||(parts==4&&part<=255));
}
constexpr bool anyServer(const char (&values)[ServerCount][ServerCapacity]){
    for(unsigned i=0;i<ServerCount;++i)if(values[i][0])return true;
    return false;
}
class Retry {
public:
    constexpr bool due(uint32_t now,bool ready,bool valid,const char (&values)[ServerCount][ServerCapacity]){
        if(!ready){armed=false;return false;}
        uint32_t fingerprint=2166136261u;
        for(unsigned i=0;i<ServerCount;++i){
            for(unsigned j=0;j<ServerCapacity&&values[i][j];++j)fingerprint=(fingerprint^uint8_t(values[i][j]))*16777619u;
            fingerprint=(fingerprint^0xffu)*16777619u;
        }
        const bool changed=fingerprint!=settingsHash;
        if(changed){settingsHash=fingerprint;armed=false;next=0;}
        if(valid&&!changed)return false;
        if(armed&&uint32_t(now-last)<RetryMs)return false;
        for(unsigned i=0;i<ServerCount;++i){
            unsigned candidate=(next+i)%ServerCount;
            if(!values[candidate][0])continue;
            last=now;armed=true;active=candidate;next=(candidate+1)%ServerCount;++requests;
            return true;
        }
        return false;
    }
    constexpr unsigned index() const{return active;}
    constexpr uint32_t count() const{return requests;}
private:
    bool armed=false;
    unsigned active=0,next=0;
    uint32_t last=0,requests=0,settingsHash=0;
};
}
