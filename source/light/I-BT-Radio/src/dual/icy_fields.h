#pragma once
#include <stddef.h>
namespace icyfields {
constexpr size_t length(const char* text){size_t n=0;while(text[n])++n;return n;}
constexpr bool equal(const char* text,const char* key){for(size_t n=0;key[n];++n)if(text[n]!=key[n])return false;return true;}
constexpr bool get(const char* text,const char* key,char* out,size_t capacity){
 if(!text||!out||!capacity)return false;
 out[0]=0;size_t found=0;
 while(text[found]&&!((found==0||text[found-1]==';')&&equal(text+found,key)))++found;
 if(!text[found])return false;
 size_t value=found+length(key),end=value;char quote=0;
 if(text[value]==39||text[value]==34){quote=text[value];++value;end=value;while(text[end]&&text[end]!=quote)++end;
  if(!text[end])return false;
 }else while(text[end]&&text[end]!=';')++end;
 if(end-value>=capacity)return false;
 if(end-value>=3&&(unsigned char)text[value]==0xef&&(unsigned char)text[value+1]==0xbb&&(unsigned char)text[value+2]==0xbf)value+=3;
 for(size_t n=0;n<end-value;++n)out[n]=text[value+n];out[end-value]=0;
 return true;
}
}
