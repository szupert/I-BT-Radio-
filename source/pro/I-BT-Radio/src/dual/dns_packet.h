#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace radiodns {
constexpr size_t length(const char* s){size_t n=0;while(s[n])++n;return n;}
constexpr bool equal(const char* a,const char* b){size_t i=0;while(a[i]&&a[i]==b[i])++i;return a[i]==b[i];}
constexpr uint16_t u16(const uint8_t* p){return uint16_t(p[0])*256+p[1];}
constexpr bool name(const uint8_t* p,size_t n,size_t& at,char* out){
    size_t pos=at,used=0;bool jumped=false;
    for(unsigned hops=0;hops<128;++hops){
        if(pos>=n)return false;unsigned len=p[pos++];
        if((len&192)==192){if(pos>=n)return false;size_t next=((len&63)<<8)|p[pos++];if(!jumped)at=pos;jumped=true;pos=next;continue;}
        if(len&192||len>63||pos+len>n)return false;
        if(!len){if(!jumped)at=pos;out[used]=0;return true;}
        if(used){if(used>=253)return false;out[used++]='.';}
        if(used+len>253)return false;
        for(unsigned i=0;i<len;++i){unsigned c=p[pos++];if(c<33||c>126||c=='.')return false;out[used++]=c>='A'&&c<='Z'?c+32:c;}
    }return false;
}
constexpr size_t query(const char* host,uint16_t id,uint8_t* p,size_t cap){
    const size_t len=length(host);if(!len||len>253||cap<len+18)return 0;
    for(unsigned i=0;i<12;++i)p[i]=0;p[0]=id>>8;p[1]=id;p[2]=1;p[5]=1;size_t at=12;
    const char* start=host;
    for(const char* c=host;;++c){
        if(*c&&*c!='.')continue;size_t count=c-start;if(!count||count>63)return 0;
        p[at++]=count;for(size_t i=0;i<count;++i){unsigned ch=start[i];if(ch<33||ch>126)return 0;p[at++]=ch;}
        if(!*c)break;start=c+1;
    }
    p[at++]=0;p[at++]=0;p[at++]=1;p[at++]=0;p[at++]=1;return at;
}
constexpr bool answer(const uint8_t* p,size_t n,uint16_t id,const char* host,uint8_t ip[4]){
    if(n<12||u16(p)!=id||(u16(p+2)&0xfa0f)!=0x8000||u16(p+4)!=1)return false;
    size_t at=12;char expected[254]={},owner[254]={};
    if(!name(p,n,at,expected)||at+4>n||u16(p+at)!=1||u16(p+at+2)!=1)return false;
    // Case-insensitive exact question match; never trust an unrelated A answer.
    size_t len=length(host);if(length(expected)!=len)return false;
    for(size_t i=0;i<len;++i){unsigned c=host[i];if(c>='A'&&c<='Z')c+=32;if(c!=unsigned(expected[i]))return false;}
    at+=4;const unsigned count=u16(p+6);if(count>64)return false;
    for(unsigned i=0;i<count;++i){
        if(!name(p,n,at,owner)||at+10>n)return false;
        const unsigned type=u16(p+at),klass=u16(p+at+2),bytes=u16(p+at+8);at+=10;if(at+bytes>n)return false;
        if(klass==1&&equal(owner,expected)){
            if(type==5){size_t ptr=at;if(!name(p,n,ptr,expected)||ptr!=at+bytes)return false;}
            if(type==1&&bytes==4){
                if(!p[at]||p[at]==127||p[at]>=224)return false;
                for(unsigned j=0;j<4;++j)ip[j]=p[at+j];return true;
            }
        }at+=bytes;
    }return false;
}
}
