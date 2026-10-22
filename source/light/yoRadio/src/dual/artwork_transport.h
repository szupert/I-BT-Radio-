#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace onlineart {
inline bool remoteUrl(const char* u) {
  if(!u || strlen(u)>=384) return false;
  const char* h=!strncmp(u,"https://",8)?u+8:(!strncmp(u,"http://",7)?u+7:nullptr);
  if(!h || !*h || *h=='/' || *h=='?' || *h=='#') return false;
  const char* end=h+strcspn(h,"/?#");
  for(const char* p=u;*p;p++) if((unsigned char)*p<=32 || (unsigned char)*p==127 || *p=='\\') return false;
  for(const char* p=h;p<end;p++) if(*p=='@') return false;
  return true;
}
struct BoundedBody {
  uint8_t* bytes; size_t capacity,used=0;
  bool append(const void* p,size_t n) {
    if(!bytes || !p || used>capacity || n>capacity-used) return false;
    memcpy(bytes+used,p,n);used+=n;return true;
  }
};
}
