#pragma once
#include <cstring>
#include <cctype>
#include <atomic>
namespace ibtdirect {
inline std::atomic<bool>& hlsSwitch(){static std::atomic<bool> enabled{false};return enabled;}
inline bool hlsEnabled(){return hlsSwitch().load(std::memory_order_relaxed);}
inline void setHlsEnabled(bool enabled){hlsSwitch().store(enabled,std::memory_order_relaxed);}
inline bool equalIgnoreCase(const char* a,const char* b,unsigned n){
  for(unsigned i=0;i<n;++i){if(!a[i]||std::tolower((unsigned char)a[i])!=b[i])return false;}return true;
}
inline bool tlsUrl(const char* url){
  if(!url)return false;
  // Existing connecttohost normalizes leading text by its first http occurrence.
  for(const char* p=url;*p;++p)if(equalIgnoreCase(p,"https://",8))return true;
  return false;
}
inline bool hlsUrl(const char* url){
  if(!url)return false;
  const char* end=url;while(*end&&*end!='?'&&*end!='#')++end;
  return end-url>=5&&equalIgnoreCase(end-5,".m3u8",5);
}
}
