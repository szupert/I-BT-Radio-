#pragma once
#include <stddef.h>
namespace ibtaudio {
inline bool prepareMp3First(const char* path){
 if(!path)return false;
 size_t n=0;while(path[n]&&path[n]!='?'&&path[n]!='#')++n;
 const char* other[]={".aac",".m4a",".wav",".flac",".ogg",".opus",".oga",".mp1",".mp2",".m3u",".m3u8",".pls",".asx",".ts",".amr",".awb"};
 for(const char* ext:other){size_t k=0;while(ext[k])++k;if(n<k)continue;bool match=true;for(size_t i=0;i<k;++i){char c=path[n-k+i];if(c>='A'&&c<='Z')c=char(c+'a'-'A');if(c!=ext[i])match=false;}if(match)return false;}
 return true; // MP3 or extensionless stream; HTTP codec selection can return it.
}
}
