#pragma once
#include <stdint.h>
namespace radiodns {
inline constexpr bool validServer(const uint8_t* a){return a[0]!=0&&a[0]!=127&&a[0]<224;}
inline constexpr uint8_t adjustOctet(uint8_t v,int delta){return uint8_t((int(v)+delta+256)%256);}
// User-selected resolvers are tried before SDK DNS. No periodic queries or cache.
template<class Resolver> bool preferred(bool manual,const uint8_t* a,const uint8_t* b,Resolver resolve){
 if(!manual)return false;
 if(validServer(a)&&resolve(a,1))return true;
 bool same=true;for(unsigned i=0;i<4;i++)if(a[i]!=b[i])same=false;
 return !same&&validServer(b)&&resolve(b,2);
}
}
