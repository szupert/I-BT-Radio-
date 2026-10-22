#pragma once
#include <stddef.h>
#include <string.h>
namespace onlineart { namespace lightalbum {
// Public artwork only. No credentials, arbitrary hosts, non-HTTP redirects or
// automatic downgrade of an HTTPS URL. The stream's TLS policy is untouched.
inline bool publicUrl(const char* url,bool tls=false){
 const size_t prefix=tls?8:7;
 if(!url||strnlen(url,700)==700||strncmp(url,tls?"https://":"http://",prefix))return false;
 const char* host=url+prefix;const char* end=strchr(host,'/');if(!end)return false;size_t n=end-host;
 if(n>3&&!memcmp(host+n-3,":80",3))n-=3;
 for(size_t i=0;i<n;++i)if(!((host[i]>='a'&&host[i]<='z')||(host[i]>='0'&&host[i]<='9')||host[i]=='-'||host[i]=='.'))return false;
 return (n==15&&!memcmp(host,"musicbrainz.org",15))||(n==19&&!memcmp(host,"coverartarchive.org",19))||
        (n==11&&!memcmp(host,"archive.org",11))||(n>12&&!memcmp(host+n-12,".archive.org",12));
}
inline bool publicHttpUrl(const char* url){return publicUrl(url);}
} }
