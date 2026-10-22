#include "dns_auto.h"
#if defined(IBT_LIGHT4_DNS_AUTO)
#include "dns_selection.h"
#include "resource_policy.h"
#include <esp_heap_caps.h>
#include "dns_packet.h"
#include "dns_policy.h"
#include "core.h"
#include <WiFi.h>
#include <esp_random.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <atomic>
#include <algorithm>
namespace radiodnsauto {
namespace {
// Network-service owner only: one nonblocking UDP socket, no worker or flash writes.
IPAddress servers[4],dhcp[2],networkIP,gateway;
Score scores[4];
uint64_t ap=0;
unsigned count=0,index=0,question=0;
int fd=-1;
uint16_t transaction=0;
uint32_t sentAt=0,lastRun=0,nextBudgetAt=0;
bool connected=false,running=false,attempted=false;
std::atomic<bool> retry{false};
std::atomic<unsigned> failures{0};
std::atomic<uint32_t> lastFailure{0};
std::atomic<unsigned> state{0}; // not measured, measuring, selected, failed
const char* hosts[2]={"itunes.apple.com","bdpstrock.hu"};
void finishSocket(){if(fd>=0){close(fd);fd=-1;}}
void add(IPAddress a){if(a.type()!=IPv4||!uint32_t(a))return;uint8_t b[4]={a[0],a[1],a[2],a[3]};if(!radiodns::validServer(b))return;for(unsigned i=0;i<count;++i)if(servers[i]==a)return;if(count<4)servers[count++]=a;}
void advance(bool success,uint32_t elapsed){
 if(success){++scores[index].successes;scores[index].elapsed+=elapsed;}
 Serial.printf("[DNS-MEASURE] resolver=%s question=%u ok=%u elapsed_ms=%lu\n",servers[index].toString().c_str(),question,unsigned(success),(unsigned long)elapsed);
 finishSocket();if(++question==2){question=0;++index;}
}
void begin(){
 count=index=question=0;for(auto& s:scores)s={};
 const auto settings=dual::snapshot().settings;
 if(settings.dnsManual){add(IPAddress(settings.dnsPrimary[0],settings.dnsPrimary[1],settings.dnsPrimary[2],settings.dnsPrimary[3]));add(IPAddress(settings.dnsSecondary[0],settings.dnsSecondary[1],settings.dnsSecondary[2],settings.dnsSecondary[3]));}
 else{add(dhcp[0]);add(dhcp[1]);add(IPAddress(1,1,1,1));add(IPAddress(8,8,8,8));}
 networkIP=WiFi.localIP();gateway=WiFi.gatewayIP();running=true;attempted=true;lastRun=millis();retry.store(false);failures.store(0);state.store(1);
 Serial.printf("[DNS-MEASURE] begin candidates=%u manual=%u nonblocking=1\n",count,unsigned(settings.dnsManual));
}
}
void failed(){
 const uint32_t now=millis();
 const unsigned old=failures.load();
 const unsigned n=(!old||uint32_t(now-lastFailure.load())>600000u)?1u:std::min(old+1u,3u);
 lastFailure.store(now);failures.store(n);if(n>=3)retry.store(true);
}
void artworkFailed(){failed();retry.store(true);Serial.println("[DNS-REPAIR] trigger=ARTWORK_FINAL_DNS_FAILURE nonblocking=1 boot_probe=0");}
void succeeded(){failures.store(0);retry.store(false);}
const char* status(){switch(state.load()){case 1:return "DNS_MEASURING";case 2:return "DNS_MEASURED_SELECTED";case 3:return "DNS_MEASURE_FAILED";default:return "DNS_CONFIGURED_NOT_PROBED";}}
void tick(){
 const uint32_t now=millis();const bool online=WiFi.status()==WL_CONNECTED&&uint32_t(WiFi.localIP());
 if(!online){finishSocket();running=false;connected=false;attempted=false;retry.store(false);failures.store(0);state.store(0);return;}
 uint64_t currentAp=0;const uint8_t* bssid=WiFi.BSSID();if(bssid)for(unsigned i=0;i<6;++i)currentAp|=uint64_t(bssid[i])<<(8*i);
 const bool changed=!connected||networkIP!=WiFi.localIP()||gateway!=WiFi.gatewayIP()||ap!=currentAp;connected=true;ap=currentAp;
 if(changed){
  finishSocket();running=false;attempted=false;retry.store(false);failures.store(0);state.store(0);
  networkIP=WiFi.localIP();gateway=WiFi.gatewayIP();dhcp[0]=WiFi.dnsIP(0);dhcp[1]=WiFi.dnsIP(1);
 }
 // Ordinary boot/AP changes never launch a benchmark. Three final DNS failures
 // within 600s (with no successful resolution) qualify; runs remain >=60s apart.
 if(!running&&retry.load()&&uint32_t(now-lastFailure.load())>600000u){retry.store(false);failures.store(0);}
 if(!running&&retry.load()&&(!attempted||retryDue(now,lastRun)))begin();
 if(!running)return;
 if(index>=count){
  const int selected=winner(scores,count);bool applied=false;
  if(selected>=0){ // The STA setter updates lwIP DNS shared by getaddrinfo/HTTP and streams.
   applied=WiFi.STA.dnsIP(0,servers[selected]);
   int backup=-1;for(unsigned i=0;i<count;++i)if(int(i)!=selected&&scores[i].successes==2){backup=i;break;}
   if(backup>=0)WiFi.STA.dnsIP(1,servers[backup]);
   Serial.printf("[DNS-SELECT] server=%s ok=%u measured_ms=%lu scope=GLOBAL_LWIP\n",servers[selected].toString().c_str(),unsigned(applied),(unsigned long)scores[selected].elapsed);
  }else Serial.println("[DNS-SELECT] result=NO_VERIFIED_RESOLVER configured_dns_retained=1");
  state.store(applied?2:3);running=false;return;
 }
 uint8_t packet[512];
 if(fd<0){
  if(int32_t(now-nextBudgetAt)<0)return;
  if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<ibtlimit::Policy::reserve+4096u||heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<4096u){nextBudgetAt=now+250;return;}
  const size_t n=radiodns::query(hosts[question],transaction=uint16_t(esp_random()),packet,sizeof(packet));
  fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);sentAt=now;
  if(fd<0||!n){advance(false,0);return;}
  if(fcntl(fd,F_SETFL,O_NONBLOCK)<0){advance(false,0);return;}
  sockaddr_in peer{};peer.sin_family=AF_INET;peer.sin_port=htons(53);peer.sin_addr.s_addr=uint32_t(servers[index]);
  if(lwip_connect(fd,reinterpret_cast<sockaddr*>(&peer),sizeof(peer))<0||send(fd,packet,n,0)!=int(n))advance(false,0);
  return;
 }
 const int n=recv(fd,packet,sizeof(packet),MSG_DONTWAIT);uint8_t ip[4];
 if(n>0&&radiodns::answer(packet,n,transaction,hosts[question],ip)){advance(true,uint32_t(now-sentAt));return;}
 if(uint32_t(now-sentAt)>=900)advance(false,uint32_t(now-sentAt));
}
}
#endif
