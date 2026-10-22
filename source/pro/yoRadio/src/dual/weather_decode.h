#pragma once
#include "weather_model.h"
#include <cJSON.h>
namespace weather {
inline bool number(const cJSON* o,const char* name,double lo,double hi,double& out){
 const auto*v=cJSON_GetObjectItemCaseSensitive(o,name);
 if(!cJSON_IsNumber(v)||!std::isfinite(v->valuedouble)||v->valuedouble<lo||v->valuedouble>hi)return false;
 out=v->valuedouble;return true;
}
inline bool decode(const char* json,size_t length,Provider provider,Reading& out){
 const char* end=nullptr;cJSON*root=cJSON_ParseWithLengthOpts(json,length,&end,0);if(!root)return false;
 const char*limit=json+length;while(end<limit&&(*end==' '||*end=='\t'||*end=='\r'||*end=='\n'))++end;
 if(end!=limit){cJSON_Delete(root);return false;}
 bool ok=false;Reading next{};double t,h,w,p,code;
 if(provider==Provider::OpenMeteo){
  const auto*c=cJSON_GetObjectItemCaseSensitive(root,"current");
  ok=number(c,"temperature_2m",-90,65,t)&&number(c,"relative_humidity_2m",0,100,h)&&number(c,"wind_speed_10m",0,500,w)&&number(c,"pressure_msl",800,1200,p)&&number(c,"weather_code",0,99,code);
 }else{
  const auto*m=cJSON_GetObjectItemCaseSensitive(root,"main");const auto*wind=cJSON_GetObjectItemCaseSensitive(root,"wind");const auto*conditions=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"weather"),0);
  ok=number(m,"temp",-90,65,t)&&number(m,"humidity",0,100,h)&&number(wind,"speed",0,138,w)&&number(m,"pressure",800,1200,p)&&number(conditions,"id",200,804,code);
  if(ok){w*=3.6;const int n=int(code);code=n==800?0:n==801?1:n==802?2:n>=803?3:n>=700?45:n>=600?71:n>=500?61:n>=300?51:95;}
 }
 if(ok&&floor(code)!=code)ok=false;
 if(ok){next.temperatureTenths=int16_t(lround(t*10));next.humidity=uint16_t(lround(h));next.windTenths=uint16_t(lround(w*10));next.pressure=uint16_t(lround(p));next.code=uint16_t(code);next.valid=true;out=next;}
 cJSON_Delete(root);return ok;
}
}
