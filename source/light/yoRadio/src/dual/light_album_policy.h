#pragma once
#include "cover_lookup.h"
namespace onlineart { namespace lightalbum {
struct Pair {const char* artist;size_t artistBytes;const char* title;size_t titleBytes;};
inline bool pair(const char* text,Pair& p){
 if(!text||strlen(text)>170)return false;const char* split=strstr(text," - ");if(!split)return false;
 p={text,size_t(split-text),split+3,strlen(split+3)};
 while(p.artistBytes&&p.artist[p.artistBytes-1]==' ')--p.artistBytes;
 while(p.titleBytes&&p.title[p.titleBytes-1]==' ')--p.titleBytes;
 return p.artistBytes&&p.titleBytes&&p.artistBytes<=80&&p.titleBytes<=80;
}
inline uint32_t letter(const char*& p,const char* end){
 while(p<end){uint32_t c=uint8_t(*p++);
  if(c>=0xc2&&c<=0xdf){if(p==end||(uint8_t(*p)&0xc0)!=0x80)return 0xffffffff;c=((c&31)<<6)|(uint8_t(*p++)&63);}
  else if(c>=128)return 0xffffffff;
  if(c>='A'&&c<='Z')c+=32;
  switch(c){case 0xc0:case 0xc1:case 0xc2:case 0xc3:case 0xc4:case 0xc5:case 0xe0:case 0xe1:case 0xe2:case 0xe3:case 0xe4:case 0xe5:c='a';break;
   case 0xc9:case 0xc8:case 0xca:case 0xcb:case 0xe9:case 0xe8:case 0xea:case 0xeb:c='e';break;
   case 0xcd:case 0xcc:case 0xce:case 0xcf:case 0xed:case 0xec:case 0xee:case 0xef:c='i';break;
   case 0xd3:case 0xd2:case 0xd4:case 0xd5:case 0xd6:case 0xf3:case 0xf2:case 0xf4:case 0xf5:case 0xf6:case 0x150:case 0x151:c='o';break;
   case 0xda:case 0xd9:case 0xdb:case 0xdc:case 0xfa:case 0xf9:case 0xfb:case 0xfc:case 0x170:case 0x171:c='u';break;}
  if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c>=128)return c;
 }return 0;
}
inline bool equal(const char* a,size_t an,const char* b,size_t bn){
 const char* ae=a+an;const char* be=b+bn;bool seen=false;
 for(;;){uint32_t x=letter(a,ae),y=letter(b,be);if(x==0xffffffff||y==0xffffffff||x!=y)return false;if(!x)return seen;seen=true;}
}
inline bool matchingRelease(const char* text,const uint8_t* json,size_t size,char (&uuid)[37]){
 Pair p{};if(!pair(text,p)||!releaseUuid(json,size,uuid))return false;
 JsonView root{reinterpret_cast<const char*>(json),reinterpret_cast<const char*>(json)+size},records{},record{},value{},credit{},artist{};
 if(!root.field("recordings",records))return false;records.ws();if(records.p==records.end||*records.p++!='[')return false;records.ws();record=records;if(!record.skip())return false;record={records.p,record.p};
 char title[171],name[171];if(!record.field("title",value)||!value.string(title,sizeof(title))||!equal(p.title,p.titleBytes,title,strlen(title)))return false;
 if(!record.field("artist-credit",credit))return false;credit.ws();if(credit.p==credit.end||*credit.p++!='[')return false;credit.ws();artist=credit;if(!artist.skip())return false;artist={credit.p,artist.p};
 if(!artist.field("name",value)||!value.string(name,sizeof(name))||!equal(p.artist,p.artistBytes,name,strlen(name)))return false;
 // Multiple credited artists must not be mistaken for a single exact match.
 credit.p=artist.end;credit.ws();return credit.p<credit.end&&*credit.p==']';
}

inline bool appleHttpsUrl(const char* url){
 if(!url||strnlen(url,700)==700||strncmp(url,"https://",8))return false;
 for(const unsigned char* p=reinterpret_cast<const unsigned char*>(url);*p;++p)if(*p<=32||*p>=127)return false;
 const char* h=url+8;const char* slash=strchr(h,'/');if(!slash)return false;size_t n=slash-h;
 for(size_t i=0;i<n;++i)if(!((h[i]>='a'&&h[i]<='z')||(h[i]>='0'&&h[i]<='9')||h[i]=='-'||h[i]=='.'))return false;
 return (n==16&&!memcmp(h,"itunes.apple.com",16))||(n==12&&!memcmp(h,"mzstatic.com",12))||(n>13&&!memcmp(h+n-13,".mzstatic.com",13));
}
inline bool itunesQuery(const char* metadata,char* out,size_t capacity){
 Pair p{};if(!out||!pair(metadata,p))return false;
 const char* prefix="https://itunes.apple.com/search?media=music&entity=song&limit=1&country=HU&term=";
 size_t n=strlen(prefix);if(n>=capacity)return false;memcpy(out,prefix,n);const char hex[]="0123456789ABCDEF";
 auto append=[&](unsigned char c){const bool plain=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~';if(n+(plain?1u:3u)>=capacity)return false;if(plain)out[n++]=c;else{out[n++]='%';out[n++]=hex[c>>4];out[n++]=hex[c&15];}return true;};
 for(size_t i=0;i<p.artistBytes;++i)if(!append(p.artist[i]))return false;if(!append(' '))return false;
 for(size_t i=0;i<p.titleBytes;++i)if(!append(p.title[i]))return false;out[n]=0;return true;
}
inline bool matchingItunes(const char* text,const uint8_t* json,size_t size,char* url,size_t capacity){
 Pair pairValue{};if(!pair(text,pairValue)||!json||!size||size>4096)return false;
 JsonView root{reinterpret_cast<const char*>(json),reinterpret_cast<const char*>(json)+size},valid=root,results{},record{},field{};
 if(!valid.skip())return false;valid.ws();if(valid.p!=valid.end||!root.field("results",results))return false;
 results.ws();if(results.p==results.end||*results.p++!='[')return false;results.ws();record=results;if(!record.skip())return false;record={results.p,record.p};
 char artist[171],title[171];
 if(!record.field("artistName",field)||!field.string(artist,sizeof(artist))||!equal(pairValue.artist,pairValue.artistBytes,artist,strlen(artist)))return false;
 if(!record.field("trackName",field)||!field.string(title,sizeof(title))||!equal(pairValue.title,pairValue.titleBytes,title,strlen(title)))return false;
 return record.field("artworkUrl100",field)&&field.string(url,capacity)&&appleHttpsUrl(url);
}
} }
