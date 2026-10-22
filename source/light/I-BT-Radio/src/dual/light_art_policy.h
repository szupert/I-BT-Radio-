#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace lightart {
constexpr size_t maxJpeg=4096;
inline bool source(const char* u){
 if(!u||!u[0]||strlen(u)>=384||strchr(u,'\r')||strchr(u,'\n')||strstr(u,"light-art-cache."))return false;
 return (u[0]=='/'&&u[1]!='/'&&!strstr(u,".."))||(strncmp(u,"http://",7)==0&&u[7]&&u[7]!='/');
}
inline uint8_t pack(uint8_t r,uint8_t g,uint8_t b){return uint8_t((r&0xe0)|((g>>3)&0x1c)|(b>>6));}
inline uint16_t expand(uint8_t v){
 const uint32_t r=uint32_t(v>>5)*255/7,g=uint32_t((v>>2)&7)*255/7,b=uint32_t(v&3)*255/3;
 return uint16_t(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
inline int scale(unsigned w,unsigned h){
 if(!w||!h||w>512||h>512)return -1;
 for(int s=0;s<4;++s)if((w>>s)<=64&&(h>>s)<=64&&(w>>s)&&(h>>s))return s;
 return -1;
}
}
