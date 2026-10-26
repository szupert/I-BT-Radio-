#pragma once
#include <stddef.h>
#include <string.h>
#include <stdint.h>
namespace ibtsd {
constexpr size_t PathCapacity=4096;
// Close the index before opening the playlist. Light/BT must not need two
// simultaneous Arduino/VFS file objects just to resolve one display row.
template<class Filesystem> bool indexOffset(Filesystem& fs,const char* path,uint16_t number,uint32_t& offset){
 if(!number)return false;
 auto index=fs.open(path,"r");
 if(!index)return false;
 const bool ok=index.seek(uint32_t(number-1)*4)&&index.readBytes(reinterpret_cast<char*>(&offset),4)==4;
 index.close();return ok;
}
inline size_t utf8Prefix(const char* text,size_t n,size_t cap){
 if(!cap)return 0;
 if(n>=cap){n=cap-1;while(n && (static_cast<unsigned char>(text[n])&0xc0)==0x80)--n;}
 return n;
}
inline void displayCopy(char*out,size_t cap,const char*text,size_t n){
 if(!cap)return;size_t count=utf8Prefix(text,n,cap);memcpy(out,text,count);out[count]=0;
}
// Read the indexed label without allocating a potentially4095-byte String.
// Keep enough bytes past the display edge to preserve a whole UTF-8 prefix.
template<size_t Capacity,class Source> bool readDisplayLabel(Source& source,char(&out)[Capacity]){
 char prefix[Capacity+3];size_t used=0;
 for(size_t n=0;n<PathCapacity;++n){
  const int ch=source.read();
  if(ch=='\t'){displayCopy(out,Capacity,prefix,used);return true;}
  if(ch<0||ch==0||ch=='\r'||ch=='\n'){out[0]=0;return false;}
  if(used<sizeof(prefix))prefix[used++]=char(ch);
 }
 out[0]=0;return false;
}
// SD records retain full UTF-8 paths; Web playlist length gates remain unchanged.
inline bool record(const char*line,const char*&name,size_t&nn,const char*&path,size_t&pn){
 if(!line)return false;const char*a=strchr(line,'\t');if(!a)return false;
 const char*b=strchr(a+1,'\t');if(!b||strcmp(b,"\t0"))return false;
 name=line;nn=a-line;path=a+1;pn=b-path;
 return nn&&pn&&pn<PathCapacity&&path[0]=='/'&&!memchr(path,'\r',pn)&&!memchr(path,'\n',pn);
}
}
