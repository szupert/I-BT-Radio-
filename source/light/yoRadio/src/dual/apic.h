#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace artwork {
// Parse an APIC payload, not an MP3 stream. Return JPEG offset; reject bounded/malformed headers.
inline bool apicJpegOffset(const uint8_t *p,size_t n,size_t &offset) {
    if(n<4)return false;
    if(p[0]==0xff&&p[1]==0xd8){offset=0;return true;}
    uint8_t enc=p[0];if(enc>3)return false;
    size_t mimeEnd=1;while(mimeEnd<n&&p[mimeEnd])mimeEnd++;
    if(mimeEnd>=n || mimeEnd+2>=n)return false;
    bool jpeg=(mimeEnd==11&&!memcmp(p+1,"image/jpeg",10)) || (mimeEnd==10&&!memcmp(p+1,"image/jpg",9));
    if(!jpeg)return false;
    size_t i=mimeEnd+2; // skip MIME terminator + picture type
    if(enc==1||enc==2) {while(i+1<n&&(p[i]!=0||p[i+1]!=0))i+=2;i+=2;}
    else {while(i<n&&p[i])i++;i++;}
    if(i+1>=n||p[i]!=0xff||p[i+1]!=0xd8)return false;
    offset=i;return true;
}
}
