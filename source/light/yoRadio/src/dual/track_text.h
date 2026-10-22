#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Presentation only. No allocation, I/O, locks, decoder, AVRCP or PCM work.
namespace tracktext {
constexpr size_t inputLimit=1024;
inline bool space(unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
inline uint32_t next(const char*& p){
 const auto* b=reinterpret_cast<const unsigned char*>(p);uint32_t c=*b;
 if(!c)return 0;
 if(c<128){++p;return c;}
 unsigned n=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
 if(!n){++p;return 0xfffd;}
 for(unsigned i=1;i<n;++i)if(!b[i]||(b[i]&0xc0)!=0x80){++p;return 0xfffd;}
 uint32_t v=c&((1u<<(7-n))-1);for(unsigned i=1;i<n;++i)v=(v<<6)|(b[i]&63);
 if((n==2&&v<0x80)||(n==3&&v<0x800)||(n==4&&v<0x10000)||v>0x10ffff||(v>=0xd800&&v<=0xdfff)){++p;return 0xfffd;}
 p+=n;return v;
}
inline bool put(char* out,size_t cap,size_t& used,uint32_t c){
 const unsigned n=c<0x80?1:c<0x800?2:c<0x10000?3:4;
 if(used+n>=cap)return false;
 if(n==1)out[used++]=char(c);
 else{out[used++]=char((n==2?0xc0:n==3?0xe0:0xf0)|(c>>(6*(n-1))));for(unsigned i=n-1;i;--i)out[used++]=char(0x80|((c>>(6*(i-1)))&63));}
 return true;
}
inline void copy(char* out,size_t cap,const char* in){
 if(!cap)return;out[0]=0;if(!in)return;
 size_t used=0;const char* end=in+strnlen(in,inputLimit);
 while(in<end){const char* before=in;uint32_t c=next(in);if(in>end||!c)break;if(c==0xfffd){in=before+1;continue;}if(!put(out,cap,used,c))break;}out[used]=0;
}
inline char fold(uint32_t c){
 if(c>='A'&&c<='Z')return char(c+32);
 switch(c){case 0xc1:case 0xe1:case 0xc4:case 0xe4:return 'a';case 0xc9:case 0xe9:return 'e';case 0xcd:case 0xed:return 'i';case 0xd3:case 0xf3:case 0xd6:case 0xf6:case 0x150:case 0x151:return 'o';case 0xda:case 0xfa:case 0xdc:case 0xfc:case 0x170:case 0x171:return 'u';default:return c<128?char(c):'?';}
}
inline bool matches(const char* text,const char* marker){
 while(*marker){if(*marker==' '){if(!space(*text))return false;while(space(*text))++text;++marker;continue;}if(!*text||fold(next(text))!=*marker++)return false;}
 return !*text||space(*text)||strchr(":;|/[]()!",*text);
}
inline uint32_t entity(const char*& p){
 static const struct{const char* name;uint32_t cp;} named[]={{"amp;",'&'},{"quot;",'"'},{"apos;",'\''},{"lt;",'<'},{"gt;",'>'},{"nbsp;",' '},{"ndash;",0x2013},{"mdash;",0x2014}};
 for(const auto& n:named)if(!strncmp(p,n.name,strlen(n.name))){p+=strlen(n.name);return n.cp;}
 if(*p=='#'){const char* q=p+1;unsigned base=10;if(*q=='x'||*q=='X'){base=16;++q;}uint32_t v=0;unsigned digits=0;while(digits<7&&*q&&*q!=';'){unsigned d=*q>='0'&&*q<='9'?unsigned(*q-'0'):*q>='a'&&*q<='f'?unsigned(*q-'a'+10):*q>='A'&&*q<='F'?unsigned(*q-'A'+10):99;if(d>=base)return 0;v=v*base+d;++q;++digits;}if(digits&&*q==';'&&v>0&&v<=0x10ffff&&!(v>=0xd800&&v<=0xdfff)){p=q+1;return v;}}
 return 0;
}
inline void trim(char* s){size_t n=strlen(s),a=0;while(a<n&&space(s[a]))++a;while(n>a&&space(s[n-1]))--n;if(a)memmove(s,s+a,n-a);s[n-a]=0;}
// These formatting codepoints have no visible glyph. A font's '?' fallback
// must not turn a direction mark or variation selector into artist punctuation.
inline bool invisible(uint32_t c){return c==0xad||c==0x34f||c==0x61c||(c>=0x200b&&c<=0x200f)||(c>=0x202a&&c<=0x202e)||(c>=0x2060&&c<=0x206f)||(c>=0xfe00&&c<=0xfe0f)||c==0xfeff||(c>=0xe0100&&c<=0xe01ef);}
inline uint32_t compose(uint32_t c,uint32_t mark){
 if(mark==0x301){switch(c){case 'A':return 0xc1;case 'a':return 0xe1;case 'E':return 0xc9;case 'e':return 0xe9;case 'I':return 0xcd;case 'i':return 0xed;case 'O':return 0xd3;case 'o':return 0xf3;case 'U':return 0xda;case 'u':return 0xfa;}}
 if(mark==0x308){switch(c){case 'A':return 0xc4;case 'a':return 0xe4;case 'O':return 0xd6;case 'o':return 0xf6;case 'U':return 0xdc;case 'u':return 0xfc;}}
 if(mark==0x30b){switch(c){case 'O':return 0x150;case 'o':return 0x151;case 'U':return 0x170;case 'u':return 0x171;}}
 return 0;
}
inline void clean(char* s,size_t cap){
 if(!cap)return;s[cap-1]=0;const char* p=s;size_t used=0;
 while(*p){uint32_t c;
  if(*p=='<'){const char* end=strchr(p,'>');if(end){const bool br=!strncmp(p,"<br",3)||!strncmp(p,"<BR",3)||!strncmp(p,"</p",3)||!strncmp(p,"</div",5);p=end+1;if(br&&used&&s[used-1]!='\n')put(s,cap,used,'\n');continue;}}
  if(*p=='&'){const char* q=p+1;c=entity(q);if(c)p=q;else c=next(p);}else c=next(p);
  if(c==0xfffd||c==0x7f||invisible(c)||(c<32&&c!='\n'&&c!='\r'&&c!='\t'))continue;
  if(c==0xa0||c==0x1680||(c>=0x2000&&c<=0x200a)||c==0x202f||c==0x205f||c==0x3000||c=='\t'||c=='\r')c=' ';
  if(c==0x2018||c==0x2019||c==0x201a||c==0x201b)c='\'';
  if(c==0x201c||c==0x201d||c==0x201e||c==0x201f)c='"';
  if(c>=0x2010&&c<=0x2015)c='-';
  // Compose supported Hungarian decomposed accents without expanding past
  // the consumed input. Preserve real question marks and unknown visible text.
  if(used&&static_cast<unsigned char>(s[used-1])<128){const uint32_t v=compose(static_cast<unsigned char>(s[used-1]),c);if(v){--used;c=v;}}
  if((c==' '||c=='\n')&&(!used||s[used-1]==char(c)))continue;
  if(!put(s,cap,used,c))break;
 }s[used]=0;trim(s);
}
inline bool promo(const char* p){
 static const char* const strong[]={"neked ajanljuk","ajanljuk neked","ajanlott tartalom","recommended for you","you might also like","sponsored by","gesponsert von"};
 for(auto m:strong)if(matches(p,m))return true;
 static const char* const labels[]={"reklam","hirdetes","advertisement","ad break","commercial break","werbung","szponzoralt tartalom","album","streamurl","website"};
 for(auto m:labels){const char* q=p;const char* k=m;while(*k&&*q&&fold(next(q))==*k)++k;if(!*k){while(*q==' ')++q;if(!strcmp(m,"album")&&*q!=':')continue;if(!*q||*q==':'||*q=='|'||*q==']'||*q==')'||!strncmp(q,"- ",2))return true;}}
 return !strncmp(p,"http://",7)||!strncmp(p,"https://",8)||!strncmp(p,"www.",4);
}
inline void strip(char* text){
 // Explicit recommendation phrases and labelled trailing segments only;
 // a word inside a real title (e.g. Advertising Space) is not a blacklist hit.
 for(char* p=text;*p;++p){const bool boundary=p==text||space(p[-1])||strchr("|;[()",p[-1]);if(boundary&&promo(p)){char* cut=p;
    // Trim only separators immediately before a confirmed recommendation.
    // Keep real punctuation in artist/title content, including a literal '?'.
    while(cut>text){
      if(space(cut[-1])||strchr("|;[-(:,",cut[-1])){--cut;continue;}
      const char* previous=cut-1;
      while(previous>text&&(static_cast<unsigned char>(*previous)&0xc0)==0x80)--previous;
      const char* end=previous;const uint32_t cp=next(end);
      if(end!=cut||(cp!=0x2022&&cp!=0xb7&&cp!=0x2219&&cp!=0x25cf))break;
      cut=const_cast<char*>(previous);
    }*cut=0;break;}}
 // Additional rows are not music metadata. Never display a third line.
 if(char* p=strpbrk(text,"\r\n"))*p=0;
 trim(text);
}
inline bool filename(const char* s){const char* dot=strrchr(s,'.');if(!dot)return false;static const char* const ext[]={"mp3","wav","flac","aac","m4a","ogg","opus","alac","aif","aiff","wma","mp4"};for(auto x:ext){const char* p=dot+1;while(*p&&*x&&fold(next(p))==*x)++x;if(!*p&&!*x)return true;}return false;}
inline void normalize(char* artist,size_t artistCap,char* title,size_t titleCap,bool splitCombined=false,bool sd=false){
 clean(artist,artistCap);clean(title,titleCap);const bool hadTitle=title[0];
 const char* lead=title;while(*lead&&(space(*lead)||strchr("[(\"'",*lead)))++lead;
 if(promo(lead)){artist[0]=title[0]=0;return;}
 if(splitCombined&&!artist[0]){
  char* separator=strstr(title," - ");unsigned width=3;
  if(!separator){separator=strstr(title," – ");width=5;}if(!separator){separator=strstr(title," — ");width=5;}
  if(separator){*separator=0;trim(title);copy(artist,artistCap,title);memmove(title,separator+width,strlen(separator+width)+1);trim(title);if(!title[0]||!artist[0]){artist[0]=title[0]=0;return;}}
 }
 strip(artist);strip(title);
 if(!title[0]&&hadTitle){artist[0]=0;return;}
 if(sd&&filename(title))title[0]=0;
}
inline void line(char* out,size_t cap,const char* artist,const char* title){
 if(!cap)return;copy(out,cap,artist);size_t n=strlen(out);
 if(n&&title[0])for(const char* p=" - ";*p;++p)if(!put(out,cap,n,*p)){out[n]=0;return;}
 const char* p=title;while(*p){uint32_t c=next(p);if(!put(out,cap,n,c))break;}out[n]=0;
}
}
