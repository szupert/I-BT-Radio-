#pragma once
#include <HTTPClient.h>
#include "stream_secure.h"
#include "dns_auto.h"
#include "light_album_policy.h"
namespace nativealbum {
class Client:public radiodns::SecureClient {
public:
 bool dnsFailure=false;
 Client(){backgroundRole();_use_insecure=false;_use_ca_bundle=true;attach_ssl_certificate_bundle(sslclient.get(),true);setHandshakeTimeout(8);}
 int connect(const char* host,uint16_t port) override{return connect(host,port,4000);}
 int connect(const char* host,uint16_t port,int32_t timeout) override{
  IPAddress ip;dnsFailure=false;
  if(!Network.hostByName(host,ip)||ip.type()!=IPv4){dnsFailure=true;Serial.printf("[ALBUM-DNS] host=%s result=FAILED\n",host);return 0;}
  Serial.printf("[ALBUM-DNS] host=%s result=IPV4\n",host);
  const int ok=resolvedConnect(ip,port,host,timeout);if(ok)radiodnsauto::succeeded();return ok;
 }
};
class Http {
 Client client;HTTPClient http;int remaining=-1;bool complete=false;
public:
 bool dnsFailure=false;int status=0;
 ~Http(){http.end();client.stop();}
 bool open(const char* url,size_t cap){
  if(!onlineart::lightalbum::appleHttpsUrl(url))return false;
  http.useHTTP10(true);http.setReuse(false);http.setConnectTimeout(4000);http.setTimeout(1000);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setUserAgent("I-BT-Radio-Light/211 (https://github.com/szupert/I-BT-Radio-)");
  const char* keys[]={"Transfer-Encoding"};http.collectHeaders(keys,1);
  if(!http.begin(client,url))return false;
  status=http.GET();dnsFailure=client.dnsFailure;
  Serial.printf("[ALBUM-HTTP] provider=ITUNES status=%d bytes=%d dns_failure=%u\n",status,http.getSize(),unsigned(dnsFailure));
  if(status!=200||http.getSize()>int(cap)||http.header("Transfer-Encoding").length())return false;
  remaining=http.getSize();complete=remaining==0;return true;
 }
 int read(uint8_t* out,size_t bytes){
  if(complete||!bytes)return 0;auto* stream=http.getStreamPtr();if(!stream)return 0;
  if(remaining>=0&&bytes>size_t(remaining))bytes=remaining;
  const size_t got=stream->readBytes(out,bytes);
  if(remaining>=0){remaining-=int(got);complete=remaining==0;}else if(!got&&!http.connected())complete=true;
  return int(got);
 }
 bool done(){return complete;}
};
}
