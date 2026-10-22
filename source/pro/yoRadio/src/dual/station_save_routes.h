#pragma once
#include <string.h>
namespace stationsave {
inline bool controlRoute(bool post,const char* path){
    if(!path)return false;
    return post?(!strcmp(path,"/api/station-save/prepare")||!strcmp(path,"/api/station-save/finish")||!strcmp(path,"/api/station-save/cancel")):
                (!strcmp(path,"/api/station-save/status")||!strcmp(path,"/")||
                 !strcmp(path,"/api/version")||!strcmp(path,"/api/player")||
                 !strcmp(path,"/api/skin.png")||!strcmp(path,"/api/radio-stations.csv"));
}
inline bool bootstrapQuery(unsigned count,const char* name,size_t length){
    return count==0||(count==1&&name&&!strcmp(name,"ibt-ui")&&length<=96);
}
}
