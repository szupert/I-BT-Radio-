#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
namespace onlineart {
// No heap allocation, bounded input and nesting. Untrusted search results never
// supply a URL: only validated release UUIDs enter the fixed CAA URL template.
struct JsonView {
 const char* p; const char* end;
 void ws(){while(p<end&&(*p==' '||*p=='\n'||*p=='\r'||*p=='\t'))++p;}
 bool string(char* out=nullptr,size_t cap=0) {
  ws();if(p==end||*p++!='"')return false;size_t n=0;
  while(p<end){unsigned char c=*p++;if(c=='"'){if(out){if(n>=cap)return false;out[n]=0;}return true;}
   if(c<32)return false;
   if(c=='\\'){if(p==end)return false;c=*p++;if(c=='u'){if(end-p<4)return false;for(int i=0;i<4;i++){char h=*p++;if(!((h>='0'&&h<='9')||(h>='a'&&h<='f')||(h>='A'&&h<='F')))return false;}if(out)return false;continue;}
    if(c!='"'&&c!='\\'&&c!='/'&&c!='b'&&c!='f'&&c!='n'&&c!='r'&&c!='t')return false;
    if(c=='b'||c=='f'||c=='n'||c=='r'||c=='t'){if(out)return false;continue;}}
   if(out){if(n+1>=cap)return false;out[n++]=c;}
  }return false;
 }
 bool skip(unsigned depth=0) {
  ws();if(p==end||depth>16)return false;if(*p=='"')return string();
  if(*p=='{'||*p=='['){char close=*p++=='{'?'}':']';bool object=close=='}';ws();if(p<end&&*p==close){++p;return true;}
   for(;;){if(object){if(!string())return false;ws();if(p==end||*p++!=':')return false;}if(!skip(depth+1))return false;ws();if(p==end)return false;if(*p==close){++p;return true;}if(*p++!=',')return false;}
  }
  const char* start=p;while(p<end&&*p!=','&&*p!=']'&&*p!='}'&&*p!=' '&&*p!='\r'&&*p!='\n'&&*p!='\t')++p;
  size_t n=p-start;if(!n)return false;
  if((n==4&&!memcmp(start,"true",4))||(n==5&&!memcmp(start,"false",5))||(n==4&&!memcmp(start,"null",4)))return true;
  const char* q=start;if(*q=='-')++q;if(q==p)return false;
  if(*q=='0')++q;else {if(*q<'1'||*q>'9')return false;while(q<p&&*q>='0'&&*q<='9')++q;}
  if(q<p&&*q=='.'){++q;const char* first=q;while(q<p&&*q>='0'&&*q<='9')++q;if(q==first)return false;}
  if(q<p&&(*q=='e'||*q=='E')){++q;if(q<p&&(*q=='+'||*q=='-'))++q;const char* first=q;while(q<p&&*q>='0'&&*q<='9')++q;if(q==first)return false;}
  return q==p;
 }
 bool field(const char* key,JsonView& value) const {
  JsonView r=*this;r.ws();if(r.p==r.end||*r.p++!='{')return false;r.ws();
  while(r.p<r.end&&*r.p!='}'){char k[64];if(!r.string(k,sizeof(k)))return false;r.ws();if(r.p==r.end||*r.p++!=':')return false;r.ws();const char* begin=r.p;if(!r.skip())return false;
   if(!strcmp(k,key)){value={begin,r.p};return true;}r.ws();if(r.p==r.end||*r.p++!=',')break;}
  return false;
 }
};
inline bool coverQuery(const char* metadata,char* url,size_t capacity) {
 if(!metadata||strlen(metadata)>170)return false;const char* split=strstr(metadata," - ");if(!split)return false;
 size_t a=split-metadata,t=strlen(split+3);while(a&&metadata[a-1]==' ')--a;while(t&&split[3+t-1]==' ')--t;
 if(!a||!t||a>80||t>80)return false;
 char query[350];size_t n=0;
 auto text=[&](const char* s,size_t len){for(size_t i=0;i<len;i++){unsigned char c=s[i];if(c<32||c==127)return false;if(c=='"'||c=='\\')query[n++]='\\';query[n++]=c;}return true;};
 memcpy(query,"artist:\"",8);n=8;if(!text(metadata,a))return false;memcpy(query+n,"\" AND recording:\"",17);n+=17;if(!text(split+3,t))return false;query[n++]='"';
 const char* prefix="https://musicbrainz.org/ws/2/recording/?fmt=json&limit=1&query=";size_t u=strlen(prefix);if(capacity<=u)return false;memcpy(url,prefix,u);
 static const char hex[]="0123456789ABCDEF";
 for(size_t i=0;i<n;i++){unsigned char c=query[i];bool plain=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~';size_t add=plain?1:3;if(u+add>=capacity)return false;if(plain)url[u++]=c;else{url[u++]='%';url[u++]=hex[c>>4];url[u++]=hex[c&15];}}
 url[u]=0;return true;
}
inline bool releaseUuid(const uint8_t* data,size_t length,char (&uuid)[37]) {
 if(!data||!length||length>16384)return false;JsonView root{(const char*)data,(const char*)data+length},valid=root;
 if(!valid.skip())return false;valid.ws();if(valid.p!=valid.end)return false;
 JsonView recordings{},score{},releases{},id{};if(!root.field("recordings",recordings))return false;recordings.ws();if(recordings.p==recordings.end||*recordings.p++!='[')return false;recordings.ws();
 JsonView record=recordings;if(!record.skip())return false;record={recordings.p,record.p};
 if(!record.field("score",score))return false;char value[8];score.ws();
 if(score.p<score.end&&*score.p=='"'){if(!score.string(value,sizeof(value))||strcmp(value,"100"))return false;}
 else if(score.end-score.p!=3||memcmp(score.p,"100",3))return false;
 if(!record.field("releases",releases))return false;releases.ws();if(releases.p==releases.end||*releases.p++!='[')return false;releases.ws();JsonView release=releases;if(!release.skip())return false;release={releases.p,release.p};
 if(!release.field("id",id)||!id.string(uuid,sizeof(uuid))||strlen(uuid)!=36)return false;
 for(unsigned i=0;i<36;i++){char c=uuid[i];if(i==8||i==13||i==18||i==23){if(c!='-')return false;}else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}
 return true;
}
}
