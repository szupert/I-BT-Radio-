#include "radio/dual/core.h"

namespace dual {
const char *tr(Language l, Text t) {
    static const char *const table[3][unsigned(Text::Count)]={
        {"Beállítások","Rádió","SD-kártya","Nyelv","Téma","Forgatás","Bitrátakorlát","Fényerő","Bluetooth","Kikapcsolva","Rendszeradatok","Vissza","Forrás","Lejátszás","Leállítva","Nincs borító","Mentés leállításkor","Előadó","Album","Cím","Csatlakozás"},
        {"Settings","Radio","SD card","Language","Theme","Rotation","Bitrate limit","Brightness","Bluetooth","Disabled","System info","Back","Source","Playing","Stopped","No cover","Save when stopped","Artist","Album","Title","Connecting"},
        {"Einstellungen","Radio","SD-Karte","Sprache","Thema","Drehung","Bitratenlimit","Helligkeit","Bluetooth","Deaktiviert","Systeminfo","Zurück","Quelle","Wiedergabe","Gestoppt","Kein Cover","Speichern bei Stopp","Interpret","Album","Titel","Verbinden"}
    };
    if(unsigned(l)>2 || unsigned(t)>=unsigned(Text::Count))return "?";
    return table[unsigned(l)][unsigned(t)];
}
}

namespace dual {uint8_t dnsDraftOctet(unsigned i){return i==0?1:0;}}
namespace btaudio {unsigned outputRate(){return 32000;}}
