#pragma once
#include <stdint.h>
// Stable page IDs. One table controls the Settings labels, destinations and return rows.
namespace menustructure {
struct Group { uint8_t page; const char *hu,*en,*de; };
#if IBT_NO_PSRAM
static constexpr Group settings[]={
 {9,"Wi-Fi","Wi-Fi","Wi-Fi"},
 {10,"Kijelző","Display","Anzeige"},
 {59,"Óra és ébresztő","Alarm settings","Wecker einstellen"},
 {11,"Gombok és forgatógomb","Buttons and rotary","Tasten und Drehgeber"},
 {81,"Egyéb beállítások","More settings","Weitere Einstellungen"}
};
static constexpr Group advanced[]={
 {8,"Állomások és fájlok","Stations and files","Sender und Dateien"},
 {12,"Tárhely és SD","Storage and SD","Speicher und SD"},
 {19,"Rendszer","System","System"},
 {13,"Diagnosztika","Diagnostics","Diagnose"}
};
static constexpr unsigned advancedCount=sizeof(advanced)/sizeof(advanced[0]);
#else
static constexpr Group settings[]={
 {9,"Wi-Fi és hálózat","Wi-Fi and network","WLAN und Netzwerk"},
 {10,IBT_NO_PSRAM?"Kijelző":"Kijelző és skinek",IBT_NO_PSRAM?"Display":"Display and skins",IBT_NO_PSRAM?"Anzeige":"Anzeige und Skins"},
 {11,"Kezelés","Controls","Bedienung"},
 {59,"Óra és ébresztő","Clock and alarm","Uhr und Wecker"},
 {49,"Érzékelők és hardver","Sensors and hardware","Sensoren und Hardware"},
 {12,"Tárhely és SD","Storage and SD","Speicher und SD"},
 {19,"Rendszer","System","System"},
 {13,"Diagnosztika","Diagnostics","Diagnose"},
 {6,"Hang","Sound","Klang"}
};
#endif
static constexpr unsigned settingsCount=sizeof(settings)/sizeof(settings[0]);
struct Parent { uint8_t page,row; };
constexpr Parent parent(unsigned page){
#if IBT_NO_PSRAM
 for(unsigned i=0;i<advancedCount;++i)if(advanced[i].page==page)return {81,uint8_t(i)};
#endif
 for(unsigned i=0;i<settingsCount;++i)if(settings[i].page==page)return {7,uint8_t(i)};
 return page==73?Parent{6,4}:page==72?Parent{10,8}:page==46?Parent{10,uint8_t(IBT_NO_PSRAM?2:10)}:page==45?Parent{11,uint8_t(IBT_NO_PSRAM?3:4)}:
        page==50?Parent{49,2}:Parent{255,0};
}
// Read-only WebUI destinations. Settings links come from the TFT group table.
constexpr bool webReadable(unsigned page){
 return (IBT_NO_PSRAM&&(page==75||page==76||page==81))||page==77||page==79||page==80||page==73||page<=64||page==68||page==70||page==71||page==72||(IBT_COMPACT_UI&&page==74);
}
constexpr unsigned webChild(unsigned page,unsigned item){
#if IBT_NO_PSRAM
 if(page==81)return item<advancedCount?advanced[item].page:7;
 if(page==1)return item==0?6:item==1?3:item==2?7:0;
#endif
 if(page==7)return item<settingsCount?settings[item].page:0;
 if(page==10)return IBT_NO_PSRAM?(item==2?46:0):(item==4?47:item==8?72:item==9?52:item==10?46:0);
 if(page==11)return item==(IBT_NO_PSRAM?3:4)?45:(IBT_COMPACT_UI&&item==5?74:0);
 if(page==49)return IBT_NO_PSRAM?(item==0?70:0):item==1?70:item==2?50:0;
 if(page==64)return IBT_NO_PSRAM?(item==2?68:item==4?70:0):item==3?68:item==6?70:0;
 if(page==70)return item==2?71:0;
 if(page==59)return item>=1&&item<=5?(item==3?63:item==4?62:59+item):0;
 if(page==77)return item==3?79:item==4?80:0;
 if(page==1)return item==0?8:item==1?6:item==2?3:item==3?7:0;
 if(IBT_NO_PSRAM&&page==6)return item==0?14:item==1?16:0;
 if(page==6)return item==0?14:item==1?15:item==2?16:item==3?3:item==4?73:0;
 if(page==3)return item==4?43:0;
 if(page==9)return item==4?53:0;
 if(page==53)return item==1||item==2?54:0;
 if(page==47)return item==9?56:0;
 if(page==19)return item==4?37:0;
 return 0;
}

}
