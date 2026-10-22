#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace ibtplaylist {
// Config's station.name, station.url, tmpBuf and WebUI buffers have BUFLEN
// bytes. Never use a length taken from uploaded data as a destination size.
inline bool copyField(char* out,size_t cap,const char* begin,const char* end,bool empty=false){
  if(!out||!begin||!end||end<begin)return false;
  size_t n=size_t(end-begin);
  if(n>=cap||(!empty&&!n))return false;
  memcpy(out,begin,n);out[n]=0;return true;
}
inline bool volume(const char* begin,const char* end,int& result){
  char text[9];
  if(!copyField(text,sizeof(text),begin,end))return false;
  char* last=nullptr;long value=strtol(text,&last,10);
  if(last==text||*last||value< -80||value>80)return false;
  result=int(value);return true;
}
inline bool csv(const char* line,char* name,char* url,size_t cap,int& ovol){
  if(!line||!name||!url||cap<2)return false;
  const char* first=strchr(line,'\t');if(!first)return false;
  const char* second=strchr(first+1,'\t');if(!second)return false;
  const char* value=second+1;
  const char* end=value+strcspn(value,"\t\r\n");
  if(!copyField(name,cap,line,first)||!copyField(url,cap,first+1,second))return false;
  return volume(value,end,ovol);
}
inline bool jsonField(const char*& cursor,char* out,size_t cap,bool terminal=false,bool empty=false){
  const char* begin=strstr(cursor,"\":\"");if(!begin)return false;begin+=3;
  const char* end=strstr(begin,terminal?"\"}":"\",\"");if(!end)return false;
  if(!copyField(out,cap,begin,end,empty))return false;
  cursor=end+(terminal?2:3);return true;
}
inline bool json(const char* line,char* name,char* url,size_t cap,int& ovol){
  if(!line||!name||!url||cap<2)return false;
  const char* cursor=line;
  char host[246],path[254],port[8],vol[9],address[254];
  if(!jsonField(cursor,name,cap)||!jsonField(cursor,host,sizeof(host))||
     !jsonField(cursor,path,sizeof(path))||!jsonField(cursor,port,sizeof(port),false,true)||
     !jsonField(cursor,vol,sizeof(vol),true,true))return false;
  if(vol[0]){if(!volume(vol,vol+strlen(vol),ovol))return false;}
  else ovol=0;
  char* last=nullptr;long portNumber=0;
  if(port[0]){
    portNumber=strtol(port,&last,10);
    if(last==port||*last||portNumber<0||portNumber>65535)return false;
  }
  int n=snprintf(address,sizeof(address),"%s%s",(strncmp(host,"http://",7)==0||strncmp(host,"https://",8)==0)?"":"http://",host);
  if(n<0||size_t(n)>=sizeof(address))return false;
  if(portNumber)n=snprintf(url,cap,"%s:%ld%s",address,portNumber,path);
  else n=snprintf(url,cap,"%s%s",address,path);
  return n>=0&&size_t(n)<cap;
}
}
