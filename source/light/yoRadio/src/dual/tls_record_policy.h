#pragma once
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <stdint.h>
namespace lighttls {
constexpr unsigned compactCode=MBEDTLS_SSL_MAX_FRAG_LEN_512;
inline int configure(mbedtls_ssl_config* conf,bool compact){return mbedtls_ssl_conf_max_frag_len(conf,compact?compactCode:MBEDTLS_SSL_MAX_FRAG_LEN_NONE);}
// Observe only the bounded plaintext TLS1.2 ServerHello. No extra socket reads
// or transcript changes. MbedTLS still verifies the actual handshake.
// Its input-size getter reflects the request, and client session.mfl_code stays0.
struct ServerHello {
 uint8_t record[5]{},hello[1024]{};unsigned rh=0,left=0,used=0,wanted=0;bool done=false,bad=false;
 void feed(const unsigned char* bytes,size_t n){
  for(size_t i=0;i<n&&!done&&!bad;++i){uint8_t b=bytes[i];
   if(rh<5){record[rh++]=b;if(rh==5){left=(unsigned(record[3])<<8)|record[4];if(record[0]!=22||record[1]!=3||!left||left>18432)bad=true;}continue;}
   if(used>=sizeof(hello)){bad=true;break;}hello[used++]=b;
   if(used==4){wanted=4+(unsigned(hello[1])<<16)+(unsigned(hello[2])<<8)+hello[3];if(hello[0]!=2||wanted>sizeof(hello)||wanted<42)bad=true;}
   if(wanted&&used==wanted)done=true;if(!--left)rh=0;
  }
 }
 bool acknowledged(unsigned code)const{
  if(!done||bad||hello[4]!=3||hello[5]!=3)return false;size_t p=38;
  size_t session=hello[p++];if(session>32||p+session+5>wanted)return false;p+=session+3;
  size_t exts=(size_t(hello[p])<<8)|hello[p+1];p+=2;if(p+exts!=wanted)return false;bool found=false;
  while(p<wanted){if(p+4>wanted)return false;unsigned type=(unsigned(hello[p])<<8)|hello[p+1];size_t len=(size_t(hello[p+2])<<8)|hello[p+3];p+=4;if(p+len>wanted)return false;
   if(type==1){if(found||len!=1||hello[p]!=code)return false;found=true;}p+=len;
  }return found;
 }
};
struct ObservedBio {
 void* socket;ServerHello hello;
 static int send(void* p,const unsigned char* b,size_t n){return mbedtls_net_send(static_cast<ObservedBio*>(p)->socket,b,n);}
 static int receive(void* p,unsigned char* b,size_t n){auto*s=static_cast<ObservedBio*>(p);int got=mbedtls_net_recv(s->socket,b,n);if(got>0)s->hello.feed(b,size_t(got));return got;}
 static int timed(void* p,unsigned char* b,size_t n,uint32_t ms){auto*s=static_cast<ObservedBio*>(p);int got=mbedtls_net_recv_timeout(s->socket,b,n,ms);if(got>0)s->hello.feed(b,size_t(got));return got;}
};
inline bool fallback(bool asked,bool echoed){return asked&&!echoed;}
}
