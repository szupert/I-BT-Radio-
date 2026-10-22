#pragma once
#include <stddef.h>
#include <string.h>
namespace ibtsd {
constexpr size_t PathCapacity=4096;
inline size_t utf8Prefix(const char* text,size_t n,size_t cap){
 if(!cap)return 0;
 if(n>=cap){n=cap-1;while(n && (static_cast<unsigned char>(text[n])&0xc0)==0x80)--n;}
 return n;
}
inline void displayCopy(char*out,size_t cap,const char*text,size_t n){
 if(!cap)return;size_t count=utf8Prefix(text,n,cap);memcpy(out,text,count);out[count]=0;
}
// SD records retain full UTF-8 paths; Web playlist length gates remain unchanged.
inline bool record(const char*line,const char*&name,size_t&nn,const char*&path,size_t&pn){
 if(!line)return false;const char*a=strchr(line,'\t');if(!a)return false;
 const char*b=strchr(a+1,'\t');if(!b||strcmp(b,"\t0"))return false;
 name=line;nn=a-line;path=a+1;pn=b-path;
 return nn&&pn&&pn<PathCapacity&&path[0]=='/'&&!memchr(path,'\r',pn)&&!memchr(path,'\n',pn);
}
}
