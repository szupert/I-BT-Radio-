#include "weather_light_model.h"
#include <cJSON.h>
namespace weather {
static bool number(const cJSON* obj,const char* key,double lo,double hi,double& v){
 const auto p=cJSON_GetObjectItemCaseSensitive(obj,key);if(!cJSON_IsNumber(p)||!std::isfinite(p->valuedouble)||p->valuedouble<lo||p->valuedouble>hi)return false;v=p->valuedouble;return true;
}
static bool item(const cJSON* a,int i,double lo,double hi,double& v){const auto p=cJSON_GetArrayItem(a,i);if(!cJSON_IsNumber(p)||!std::isfinite(p->valuedouble)||p->valuedouble<lo||p->valuedouble>hi)return false;v=p->valuedouble;return true;}
bool decodeForecast(const char* body,size_t size,Forecast& output){
 if(!body||size<2||size>4095||body[size]!=0)return false;
 // Bound nesting before asking cJSON to allocate nodes, even for an error body.
 unsigned depth=0;bool quoted=false,escape=false;
 for(size_t i=0;i<size;++i){char c=body[i];if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;continue;}if(c=='"')quoted=true;else if(c=='{'||c=='['){if(++depth>6)return false;}else if(c=='}'||c==']'){if(!depth--)return false;}}
 if(depth||quoted)return false;
 const char* end=nullptr;cJSON* root=cJSON_ParseWithLengthOpts(body,size+1,&end,true);if(!root)return false;
 Forecast n{};double t,h,w,code,ap,day,epoch,offset;const auto cur=cJSON_GetObjectItemCaseSensitive(root,"current"),daily=cJSON_GetObjectItemCaseSensitive(root,"daily");
 bool ok=cJSON_IsObject(root)&&cJSON_IsObject(cur)&&cJSON_IsObject(daily)&&!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root,"error"))&&
 number(cur,"temperature_2m",-100,70,t)&&number(cur,"relative_humidity_2m",0,100,h)&&number(cur,"wind_speed_10m",0,500,w)&&number(cur,"weather_code",0,99,code)&&number(cur,"apparent_temperature",-150,100,ap)&&number(cur,"is_day",0,1,day)&&number(cur,"time",1600000000,4102444800.,epoch)&&number(root,"utc_offset_seconds",-43200,50400,offset);
 const char* names[]={"time","weather_code","temperature_2m_max","temperature_2m_min"};const cJSON* arrays[4]{};
 for(unsigned j=0;j<4&&ok;++j){arrays[j]=cJSON_GetObjectItemCaseSensitive(daily,names[j]);ok=cJSON_IsArray(arrays[j])&&cJSON_GetArraySize(arrays[j])==5;}
 for(unsigned i=0;i<5&&ok;++i){double e,c,hi,lo;ok=item(arrays[0],i,1600000000,4102444800.,e)&&item(arrays[1],i,0,99,c)&&item(arrays[2],i,-100,70,hi)&&item(arrays[3],i,-100,70,lo)&&lo<=hi&&std::floor(e)==e&&std::floor(c)==c;
  if(ok){n.days[i]={uint32_t(e),int16_t(lround(lo*10)),int16_t(lround(hi*10)),uint16_t(c)};if(i&&n.days[i].epoch<=n.days[i-1].epoch)ok=false;}}
 if(ok&&std::floor(code)==code&&std::floor(day)==day&&std::floor(epoch)==epoch&&std::floor(offset)==offset){n.current.temperatureTenths=lround(t*10);n.current.humidity=lround(h);n.current.windTenths=lround(w*10);n.current.code=code;n.current.valid=true;n.apparent=lround(ap*10);n.daylight=day!=0;n.epoch=epoch;n.utcOffset=offset;output=n;}else ok=false;
 cJSON_Delete(root);return ok;
}
const char* condition(uint16_t c){return c==0?"Derült":c<=2?"Gomolyfelhős":c==3?"Borult":c<=48?"Ködös":c<=57?"Szitálás":c<=67?"Eső":c<=77?"Havazás":c<=82?"Zápor":c<=86?"Hózápor":"Zivatar";}
}
