#if IBT_COMPACT_UI
#include "../dual/ir_receiver.h"
#endif
#include "../dual/mono_dsp.h"
#pragma once
#include "../dual/core.h"
#include "../dual/weather.h"
#include "../dual/bt_audio.h"
#include "../dual/sensors/receiver.h"
#include <stdio.h>
#include "menu_structure.h"
#include "light_navigation.h"
#include "../dual/vu_probe.h"
#include "light_ui_policy.h"
namespace content {
inline const char* pick(dual::Language l,const char* hu,const char* en,const char* de){
    return unsigned(l)==0?hu:unsigned(l)==1?en:de;
}
inline unsigned itemCount(unsigned page){
    switch(page){
      case 75:return 5;
#if IBT_COMPACT_UI
      case 74:return 17;
#endif
      case 1:return IBT_NO_PSRAM?lightnav::mainCount:5;
      case 76:return IBT_NO_PSRAM?3:0;
      case 77:return IBT_NO_PSRAM?8:0;
#if IBT_NO_PSRAM
      case 81:return menustructure::advancedCount+1;
#endif
      case 79:case 80:return IBT_NO_PSRAM?10:0;
      case 3:return 8;   // Bluetooth input, output, DAC, status, rate, back
      case 45:return 8;
      case 46:return IBT_NO_PSRAM?8:6;
      case 43:return 5;  // four standard SBC rates and back
      case 6:return IBT_NO_PSRAM?3:6;
      case 73:return IBT_NO_PSRAM?0:13;   // sound
      case 7:return menustructure::settingsCount+1;   // settings, alarm, skin swipe toggle, back
      case 59:return 7;  // alarm groups, mode and back
      case 60:return 10;
      case 61:return 4;
      case 62:return 7;
      case 63:return 6;
      case 64:return IBT_NO_PSRAM?7:11;
      case 70:return 5;
      case 71:return 9;
      case 68:return 5;
      case 8:return 8;   // sources, Bluetooth input, outputs and back
      case 9:return 9;
      case 53:return 7;
      case 54:return 6;   // wifi
      case 47:return IBT_NO_PSRAM?0:IBT_COMPACT_UI?4:15;
      case 72:return 9;
      case 56:return 10; // nine retro illumination colors and back
      case 49:return IBT_NO_PSRAM?2:4;
      case 50:return 6;
      case 52:return 4;
      case 10:return IBT_NO_PSRAM?6:12;  // display
      case 11:return IBT_NO_PSRAM?lightnav::controlsCount:7;  // controls
      case 12:return IBT_NO_PSRAM?4:5;  // storage
      case 13:return 6;  // diagnostics
      case 19:return 6;  // system
      case 14:return 5;  // volume/stream
      case 15:return IBT_NO_PSRAM?0:5;  // eq
      case 57:return IBT_NO_PSRAM?0:13; // ten bands, AUTO, enable, back
      case 58:return IBT_NO_PSRAM?0:eq10::PresetCount+1;
      case 16:return 5;  // outputs
      case 37:return 5;  // SD data backup / restore
      default:return 0;
    }
}
inline bool isMenuPage(unsigned page){return itemCount(page)!=0;}
inline const char* pageTitle(dual::Language l,unsigned page){
    switch(page){
      case 75:return "Forrás / hangút";
      case 76:return "Kapcsolatok";
      case 81:return pick(l,"Egyéb beállítások","More settings","Weitere Einstellungen");
      case 77:return "Időjárás és BLE";case 79:return "Kint · érzékelő";case 80:return "Bent · érzékelő";
#if IBT_COMPACT_UI
      case 74:return "IR taviranyito";
#endif
      case 1:return pick(l,"Menü","Menu","Menü");
      case 2:return pick(l,"Memória","Memory","Speicher");
      case 3:return "Bluetooth";
      case 46:return IBT_NO_PSRAM?"Jelszintmérő":pick(l,"VU frissítés","VU refresh","VU Aktualisierung");
      case 45:return pick(l,"Menü automatikus bezárása","Menu auto-close","Menü automatisch schließen");
      case 44:return pick(l,"Bluetooth-kimenet","Bluetooth output","Bluetooth-Ausgang");
      case 43:return pick(l,"BT kimeneti frekvencia","BT output rate","BT-Ausgangsrate");
      case 4:return pick(l,"Állomások","Stations","Sender");
      case 5:return "Rotary debug";
      case 6:return pick(l,"Hang","Sound","Klang");
      case 73:return "MONO DSP";
      case 7:return pick(l,"Beállítások","Settings","Einstellungen");
      case 8:return pick(l,"Forrás és kimenet","Source and output","Quelle und Ausgang");
      case 9:return "Wi-Fi";
      case 53:return pick(l,"Radio DNS","Radio DNS","Radio DNS");
      case 54:return pick(l,"DNS IPv4-cím","DNS IPv4 address","DNS IPv4-Adresse");
      case 55:return pick(l,"DNS címrész","DNS octet","DNS Adressteil");
      case 72:return pick(l,"Bootképernyő","Boot screen","Startbild");
      case 47:return pick(l,"Skinek","Skins","Skins");
      case 56:return pick(l,"Retro megvilágítás","Retro illumination","Retro Beleuchtung");
      case 52:return pick(l,"Akkumulátor","Battery","Akku");
      case 49:return pick(l,"Érzékelők és hardver","Sensors and hardware","Sensoren und Hardware");
      case 50:return pick(l,"Speciális / Akkumérés","Special / Battery","Spezial / Akku");
      case 10:return IBT_NO_PSRAM?"Kijelző":pick(l,"Kijelző és skinek","Display and skins","Anzeige und Skins");
      case 11:return pick(l,"Kezelés","Controls","Bedienung");
      case 12:return pick(l,"Tárhely","Storage","Speicher");
      case 13:return pick(l,"Diagnosztika","Diagnostics","Diagnose");
      case 20:return pick(l,"Firmware információ","Firmware info","Firmware Info");
      case 19:return pick(l,"Rendszer","System","System");
      case 14:return pick(l,"Hangerő / Stream","Volume / Stream","Lautstärke / Stream");
      case 15:return "EQ";
      case 57:return pick(l,"EQ látvány","EQ visual","EQ-Optik");
      case 58:return pick(l,"Látványprofilok","Visual presets","Optik-Profile");
      case 59:return pick(l,"Ébresztőóra","Alarm clock","Wecker");
      case 60:return pick(l,"Időzítés és napok","Schedule and days","Zeit und Tage");
      case 61:return pick(l,"Ébresztő hangforrás","Alarm source","Weckquelle");
      case 62:return pick(l,"Hangerőgörbe","Volume envelope","Lautstärkeverlauf");
      case 63:return pick(l,"Fényerőgörbe","Light envelope","Helligkeitsverlauf");
      case 64:return pick(l,"Óra beállításai","Clock settings","Uhreinstellungen");
      case 68:return pick(l,"Időszerverek","Time servers","Zeitserver");
      case 70:return pick(l,"BLE hőmérő","BLE thermometer","BLE-Thermometer");
      case 71:return pick(l,"Érzékelőválasztás","Select sensor","Sensor wählen");
      case 16:return pick(l,"Kimenetek","Outputs","Ausgänge");
      case 18:return pick(l,"Hálózat","Network","Netzwerk");
      case 21:return pick(l,"Mentett Wi-Fi","Saved Wi-Fi","Gespeichertes Wi-Fi");
      case 22:return "Bluetooth";
      case 23:return "PCM5102A";
      case 24:return pick(l,"Fájlrendszer","Filesystem","Dateisystem");
      case 25:return "Artwork cache";
      case 26:return pick(l,"Webes frissítés","Web update","Web-Update");
      case 27:return pick(l,"Gombkiosztás","Button mapping","Tastenbelegung");
      case 28:return pick(l,"Hálózatok","Networks","Netzwerke");
      case 29:return pick(l,"Kedvencek","Favorites","Favoriten");
      case 30:return pick(l,"SD kártya","SD card","SD Karte");
      case 31:return pick(l,"Állomáskeresés","Station search","Sendersuche");
      case 32:return pick(l,"Találatok","Results","Treffer");
      case 33:return pick(l,"Playlist kezelés","Playlist editor","Playlist Verwaltung");
      case 34:return pick(l,"PSRAM stresszteszt","PSRAM stress test","PSRAM Stresstest");
      case 37:return pick(l,"Mentés / vissza SD","SD backup / restore","SD Sichern / Zurück");
      case 38:return pick(l,"Adatmentés eredménye","Backup result","Sicherungsergebnis");
      default:return "?";
    }
}
inline unsigned alarmField(unsigned page,unsigned item){
    static constexpr uint8_t schedule[]={1,3,17,18,19,20,21,22,23};
    static constexpr uint8_t source[]={25,5,26};
    if(page==60)return item<9?schedule[item]:27;
    if(page==61)return item<3?source[item]:27;
    if(page==62)return item==0?29:item<6?5+item:27;
    if(page==63)return item<5?11+item:27;
    if(IBT_NO_PSRAM&&page==64){static constexpr uint8_t f[]={16,28,30,31,33,37,27};return item<7?f[item]:27;}
    if(page==64)return item==0?24:item==1?16:item==2?28:item==3?30:item==4?31:item==5?32:item==6?33:item==7?34:item==8?35:item==9?36:27;
    return 27;
}
inline bool menuItem(char *out,size_t cap,const dual::Snapshot &s,unsigned page,unsigned item){
#if IBT_NO_PSRAM
 if(page==10){
  const char* names[]={"Fényerő","Forgatás","Jelszintmérő","Pufferjelző","Térerő","Vissza"};
  char value[32]={};
  if(item==0)snprintf(value,sizeof(value),"%u%%",s.settings.brightness);
  else if(item==1)snprintf(value,sizeof(value),"%u fok",s.settings.rotation*90);
  else if(item==3)snprintf(value,sizeof(value),"%s",s.settings.bufferIndicator?"Be":"Ki");
  else if(item==4)snprintf(value,sizeof(value),"%s",s.settings.wifiIndicator?"Be":"Ki");
  snprintf(out,cap,"%s%s%s",names[item<6?item:5],value[0]?": ":"",value);return true;
 }
 if(page==76){const char* n[]={"Wi-Fi csatlakozás","SD → DAC","Telefon → DAC"};snprintf(out,cap,"%s",n[item<3?item:0]);return true;}
 if(page==6){const char* n[]={"Hangerő / Stream","Kimenetek","Vissza"};snprintf(out,cap,"%s",n[item<3?item:2]);return true;}
 if(page==11){const char* n[]={"Rotary","Érintés kalibrálása (Stop)","Gombkiosztás","Menü bezárási idő","Lapozásos módváltás","Koppintás","Vissza"};snprintf(out,cap,"%s%s%s",n[item<7?item:6],item==4||item==5?": ":"",item==4?(s.settings.swipeSkins?"Be":"Ki"):item==5?(s.settings.tapSelected?"Kijelölt sor":"Érintett sor"):"");return true;}
 if(page==12){const char* n[]={"Memória","SD-kártya","Fájlrendszer","Vissza"};snprintf(out,cap,"%s",n[item<4?item:3]);return true;}
 if(page==49){snprintf(out,cap,"%s",item?"Vissza":"BLE hőmérő");return item||sensors::available();}
 if(page==46){
  const char* names[]={"Kikapcsolva","Függőleges sztereó","Vízszintes sztereó","Magnórádió","Frissítés: 100 ms","Frissítés: 200 ms","Frissítés: 500 ms","Vissza"};
  const bool active=item<4?lightui::style.load()==item:item<7?vuprobe::refreshMs.load()==(item==4?100:item==5?200:500):false;
  snprintf(out,cap,"%s%s",names[item<8?item:7],active?" *":"");return true;
 }
#endif
#if IBT_COMPACT_UI
 if(page==74){irreceiver::label(out,cap,item);return item!=15;}
 if(page==47){const char* names[]={"Ébresztőóra","MediaPlayer","Időjárás","Vissza"};
 snprintf(out,cap,"%s",names[item<4?item:3]);return true;}
#endif

    if(page>=60&&page<=64){item=alarmField(page,item);page=66;}
    const auto l=s.settings.language; const char* label="?"; char value[48]={}; bool enabled=false;
    switch(page){
#if IBT_NO_PSRAM
    case 77:{const char* n[]={"Időjárás megnyitása · hang nélkül","Frissítés most","BLE-keresés · utána időjárás","Kint: érzékelő kiválasztása","Bent: érzékelő kiválasztása","Frissítési idő","Open-Meteo · beállítás weben","Vissza"};
        if(item==5)snprintf(out,cap,"%s: %u perc",n[5],weather::configuration().intervalMinutes);else snprintf(out,cap,"%s",n[item<8?item:7]);return item!=6;}
    case 79:case 80:
        if(item<8){if(sensors::deviceLabel(item,out,cap))return true;snprintf(out,cap,"—");return false;}
        snprintf(out,cap,"%s",item==8?"Hozzárendelés törlése":"Vissza");return true;
#endif
    case 70:
        if(item==0)snprintf(out,cap,"%s: %s",pick(l,"Hőmérő","Sensor","Sensor"),sensors::enabled()?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));
        else if(item==1)snprintf(out,cap,"%s",pick(l,"Keresés · utána vissza az órára","Search · then return to clock","Suche · dann zur Uhr"));
        else if(item==2)snprintf(out,cap,"%s (%u)",pick(l,"Érzékelő kiválasztása","Select sensor","Sensor wählen"),sensors::deviceCount());
        else if(item==3)snprintf(out,cap,"%s",sensors::status());
        else snprintf(out,cap,"%s",dual::tr(l,dual::Text::Back));
        return item==4||(item<3&&sensors::available());
    case 71:
        if(item<8){if(!sensors::deviceLabel(item,out,cap)){snprintf(out,cap,"—");return false;}return true;}
        snprintf(out,cap,"%s",dual::tr(l,dual::Text::Back));return true;
    case 68:
        if(item<4){const auto servers=dual::clockServers();snprintf(out,cap,"NTP %u: %s",item+1,servers.values[item][0]?servers.values[item]:pick(l,"Kikapcsolva","Disabled","Deaktiviert"));}
        else snprintf(out,cap,"%s",dual::tr(l,dual::Text::Back));
        return true;
#if IBT_NO_PSRAM
    case 81:
        snprintf(out,cap,"%s",item<menustructure::advancedCount?pick(l,menustructure::advanced[item].hu,menustructure::advanced[item].en,menustructure::advanced[item].de):dual::tr(l,dual::Text::Back));return true;
#endif
    case 1:
#if IBT_NO_PSRAM
        {const char* hu[]={"Hang","Bluetooth","Beállítások","Óra","Időjárás","Lejátszó"};
         const char* en[]={"Sound","Bluetooth","Settings","Clock","Weather","Player"};
         const char* de[]={"Klang","Bluetooth","Einstellungen","Uhr","Wetter","Player"};
         snprintf(out,cap,"%s",pick(l,hu[item<6?item:5],en[item<6?item:5],de[item<6?item:5]));return true;}
#else
        {static const char* hu[]={"Forrás és lejátszás","Hang","Bluetooth","Beállítások"};
         static const char* en[]={"Sources and playback","Sound","Bluetooth","Settings"};
         static const char* de[]={"Quellen und Wiedergabe","Klang","Bluetooth","Einstellungen"};
         if(item<4){label=unsigned(l)==0?hu[item]:unsigned(l)==1?en[item]:de[item];enabled=true;}
         else if(IBT_NO_PSRAM&&item==5){label="Kapcsolatok";enabled=true;}
         else if(IBT_NO_PSRAM&&item==6){label="Időjárás · hang nélkül";enabled=true;}
         else if(IBT_NO_PSRAM&&item==7){label="Időjárás / Kint / Bent";enabled=true;}
         else if(item==4){label=pick(l,s.settings.alarm.clockHome?"Rádió megjelenítése":"Óra megjelenítése",s.settings.alarm.clockHome?"Show radio":"Show clock",s.settings.alarm.clockHome?"Radio anzeigen":"Uhr anzeigen");enabled=true;}}
        break;
#endif
    case 8:
        switch(item){
          case 0:label=pick(l,"Internet rádió","Internet radio","Internetradio");enabled=true;break;
          case 1:label=pick(l,"Kedvencek","Favorites","Favoriten");enabled=true;break;
          case 2:label=pick(l,"Keresés","Search","Suche");enabled=true;break;
          case 3:label=pick(l,"SD lejátszás","SD playback","SD Wiedergabe");
#if FEATURE_SD
              enabled=true;
#endif
              break;
          case 4:label=pick(l,"Playlist kezelés","Playlist editor","Playlist Verwaltung");enabled=true;break;
          case 5:label=pick(l,"Forrás: Bluetooth-bemenet","Source: Bluetooth input","Quelle: Bluetooth-Eingang");enabled=true;break;
          case 6:label=pick(l,"Kimenet: DAC / Bluetooth","Output: DAC / Bluetooth","Ausgang: DAC / Bluetooth");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 6:
        switch(item){
          case 0:label=pick(l,"Hangerő / Stream","Volume / Stream","Lautstärke / Stream");enabled=true;break;
          case 1:label=pick(l,"Hangszín / EQ","Tone / EQ","Klang / EQ");enabled=true;break;
          case 2:label=pick(l,"Kimenetek","Outputs","Ausgänge");enabled=true;break;
          case 3:label=pick(l,"Bluetooth-hang","Bluetooth audio","Bluetooth Audio");enabled=true;break;
          case 4:label="MONO DSP";enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 73:{
        static const char* labels[]={"MONO DSP","Csatornakeverés","Melyvagas","Melyvagas meredeksege","Basszusfrekvencia","Basszuskiemelés","Dobozhang frekvencia","Dobozhang korrekció","Ének 2,5 kHz","Magasak 6 kHz","Csucshatarolo","Dinamikus basszusvédelem"};
        static const int8_t map[]={0,1,2,-1,3,4,5,6,7,8,9,10};
        if(item>=12){label=dual::tr(l,dual::Text::Back);enabled=true;break;}
        label=labels[item];enabled=item!=3;const auto c=monodsp::configuration();
        if(item==3)snprintf(value,sizeof(value),"12 dB/okt");
        else if(item==0||item>=10)snprintf(value,sizeof(value),"%s",c.v[map[item]]?"BE":"KI");
        else if(item==1)snprintf(value,sizeof(value),"%s",c.v[1]==0?"(L+R)/2":c.v[1]==1?"L":"R");
        else if(item==2||item==4||item==6)snprintf(value,sizeof(value),"%d Hz",c.v[map[item]]);
        else snprintf(value,sizeof(value),"%+.1f dB",float(c.v[map[item]])*0.1f);
        break;
    }
    case 14:
        switch(item){
          case 0:label=pick(l,"Hangerő lépés","Volume step","Lautstärke Schritt");snprintf(value,sizeof(value),"%u",s.settings.volumeStep);enabled=true;break;
          case 1:label=pick(l,"Stream limit","Stream limit","Stream Limit");if(s.settings.maxKbps)snprintf(value,sizeof(value),"%u k",s.settings.maxKbps);else snprintf(value,sizeof(value),"MAX");enabled=true;break;
          case 2:label=pick(l,"HLS / M3U8","HLS / M3U8","HLS / M3U8");snprintf(value,sizeof(value),"%s",s.settings.hlsEnabled?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 3:label="Balance";snprintf(value,sizeof(value),"%+d",int(s.balance));enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 15:
        switch(item){
          case 0:label=pick(l,"Mély","Bass","Bass");snprintf(value,sizeof(value),"%+d dB",int(s.bass));enabled=true;break;
          case 1:label=pick(l,"Közép","Middle","Mitten");snprintf(value,sizeof(value),"%+d dB",int(s.middle));enabled=true;break;
          case 2:label=pick(l,"Magas","Treble","Höhen");snprintf(value,sizeof(value),"%+d dB",int(s.treble));enabled=true;break;
          case 3:label=pick(l,"EQ látvány","EQ visual","EQ-Optik");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
#if !IBT_NO_PSRAM
    case 57:
        if(item<eq10::Bands){
            snprintf(out,cap,"%s %u Hz  %+d dB",item<5?" ":"",unsigned(eq10::Hz[item]),
                     int(s.settings.graphicEq.db[item]));return true;
        }
        if(item==10){label="AUTO";snprintf(value,sizeof(value),"%s",eq10::name(s.settings.graphicEq.preset,unsigned(l)));}
        else if(item==11){label=pick(l,"EQ látvány","EQ visual","EQ-Optik");
            snprintf(value,sizeof(value),"%s",s.settings.graphicEq.enabled?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));}
        else label=dual::tr(l,dual::Text::Back);
        enabled=true;break;
    case 58:
        if(item<eq10::PresetCount){label=eq10::name(eq10::Preset(item),unsigned(l));enabled=true;}
        else {label=dual::tr(l,dual::Text::Back);enabled=true;}
        break;
#endif
    case 16:
        switch(item){
          case 0:label="PCM5102A";enabled=true;break;
          case 1:label=pick(l,"Belső DAC","Internal DAC","Interner DAC");break;
          case 2:label=pick(l,"Bluetooth-kimenet","Bluetooth output","Bluetooth-Ausgang");enabled=true;break;
          case 3:label=pick(l,"Bluetooth-bemenet","Bluetooth input","Bluetooth-Eingang");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 75:{static const char* routes[]={"WIFI-DAC","SD-DAC","SD-BT","BT-DAC","Vissza"};
        if(item<5){label=routes[item];enabled=true;}break;}
    case 3:
        switch(item){
          case 0:label=pick(l,"Telefon csatlakoztatása","Connect phone","Telefon verbinden");enabled=true;break;
          case 1:label=pick(l,"BT-hangszóró keresése","Find BT speaker","BT-Lautsprecher suchen");enabled=true;break;
          case 2:label=pick(l,"Vissza a DAC-ra","Return to DAC","Zurück zum DAC");enabled=true;break;
          case 3:label=pick(l,"Állapot","Status","Status");enabled=true;break;
          case 4:label=pick(l,"BT kimenet kHz","BT output kHz","BT-Ausgang kHz");snprintf(value,sizeof(value),"%u%s",btaudio::outputRate()==44100?44:btaudio::outputRate()/1000,btaudio::outputRate()==44100?",1":"");enabled=true;break;
          case 5:label=pick(l,"BT gyorskereső","BT quick scan","BT-Schnellsuche");snprintf(value,sizeof(value),"%s",s.settings.btQuickScan?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 6:label=pick(l,"Mentett hangszórók","Saved speakers","Gespeicherte Lautsprecher");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 43:
        if(item<4){static const unsigned rates[]={16000,32000,44100,48000};
            const unsigned hz=rates[item];snprintf(value,sizeof(value),"%s",btaudio::outputRate()==hz?"*":"");
            label=hz==16000?"16 kHz":hz==32000?"32 kHz":hz==44100?"44,1 kHz":"48 kHz";enabled=true;
        }else{label=dual::tr(l,dual::Text::Back);enabled=true;}
        break;
    case 46:{
        static const char* labels[]={"100 ms","200 ms","250 ms","500 ms","1000 ms"};
        if(item<5){label=labels[item];if(vuprobe::refreshMs.load(std::memory_order_relaxed)==vuprobe::refreshOptions[item])snprintf(value,sizeof(value),"*");}
        else label=dual::tr(l,dual::Text::Back);
        enabled=true;break;
    }
    case 45:{
        static const uint16_t seconds[]={10,20,30,60,120,300,0};
        const char* labels[]={pick(l,"10 mp","10 sec","10 Sek"),pick(l,"20 mp","20 sec","20 Sek"),pick(l,"30 mp","30 sec","30 Sek"),pick(l,"1 perc","1 min","1 Min"),pick(l,"2 perc","2 min","2 Min"),pick(l,"5 perc","5 min","5 Min"),pick(l,"Soha","Never","Nie")};
        if(item<7){label=labels[item];if(s.settings.menuIdleSeconds==seconds[item])snprintf(value,sizeof(value),"*");}
        else label=dual::tr(l,dual::Text::Back);
        enabled=true;break;
    }
    case 7:
        if(item<menustructure::settingsCount){const auto &g=menustructure::settings[item];label=pick(l,g.hu,g.en,g.de);}
        else label=dual::tr(l,dual::Text::Back);
        enabled=true;break;
    case 59:
        if(item==0)return menuItem(out,cap,s,66,0);
        if(item>=1&&item<=5){snprintf(out,cap,"%s",pageTitle(l,item==3?63:item==4?62:59+item));return true;}
        snprintf(out,cap,"%s",dual::tr(l,dual::Text::Back));return true;
    case 66:
        switch(item){
          case 32:label=pick(l,"Időjárás · hely a weben","Weather · WebUI location","Wetter · WebUI Ort");snprintf(value,sizeof(value),"%s",weather::enabled()?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));break;
          case 34:{label=pick(l,"Óra betűképe","Clock typeface","Uhr-Schrift");const char*names[]={"Modern Sans","Klasszikus Serif","Digital Orbit","Keskeny Modern"};snprintf(value,sizeof(value),"%s",names[s.settings.alarm.clockStyle<4?s.settings.alarm.clockStyle:0]);break;}
          case 35:case 36:{label=item==35?pick(l,"Óraszámok színe","Clock color","Uhrfarbe"):pick(l,"Feliratok színe","Text color","Textfarbe");const char*names[]={"Alap","Fehér","Borostyán","Türkiz","Zöld","Kék","Lila","Piros","Ezüst"};const unsigned color=item==35?s.settings.alarm.clockColor:s.settings.alarm.textColor;snprintf(value,sizeof(value),"%s",names[color<9?color:0]);break;}
          case 33:label=pick(l,"BLE hőmérő","BLE thermometer","BLE-Thermometer");snprintf(value,sizeof(value),"%s",sensors::enabled()?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));break;
          case 30:label=pick(l,"Időszerverek","Time servers","Zeitserver");snprintf(value,sizeof(value),"4 NTP");break;
          case 31:label=pick(l,"Időzóna","Timezone","Zeitzone");snprintf(value,sizeof(value),"%s",pick(l,"Magyarország (auto)","Hungary (auto)","Ungarn (auto)"));break;
          case 29:label=pick(l,"Profil","Profile","Profil");snprintf(value,sizeof(value),"%s",(s.settings.alarm.mode?s.settings.alarm.mode:s.settings.alarm.enabledProfile)==1?pick(l,"Fix","Fixed","Fest"):pick(l,"Felfut/lefut","Rise/fall","Auf/ab"));break;
          case 0:label=pick(l,"Ébresztés","Alarm mode","Weckmodus");
              snprintf(value,sizeof(value),"%s",s.settings.alarm.mode==0?pick(l,"Ki","Off","Aus"):
                  !s.settings.alarm.daysMask?pick(l,"Nincs nap","No days","Keine Tage"):
                  !alarmclock::validWindow(s.settings.alarm)?pick(l,"Időhiba","Invalid time","Zeitfehler"):
                  s.settings.alarm.mode==1?pick(l,"Fix","Fixed","Fest"):pick(l,"Felfut/lefut","Rise/fall","Auf/ab"));break;
          case 1:label=pick(l,"Kezdés","Start","Beginn");snprintf(value,sizeof(value),"%02u:%02u",unsigned(s.settings.alarm.onHour),unsigned(s.settings.alarm.onMinute));break;
          case 2:label=pick(l,"Indul perc","Start minute","Start Minute");snprintf(value,sizeof(value),"%02u",unsigned(s.settings.alarm.onMinute));break;
          case 3:label=pick(l,"Befejezés","End","Ende");snprintf(value,sizeof(value),"%02u:%02u",unsigned(s.settings.alarm.offHour),unsigned(s.settings.alarm.offMinute));break;
          case 4:label=pick(l,"Leáll perc","Stop minute","Ende Minute");snprintf(value,sizeof(value),"%02u",unsigned(s.settings.alarm.offMinute));break;
          case 5:label=pick(l,"Adó átvétele","Use current station","Sender übernehmen");snprintf(value,sizeof(value),"%u",unsigned(s.settings.alarm.station));break;
          case 6:label=pick(l,"Kezdő hangerő","Start volume","Startlautstärke");snprintf(value,sizeof(value),"%u%%",unsigned(s.settings.alarm.volumeStart));break;
          case 7:label=pick(l,"Csúcshangerő","Peak volume","Max. Lautstärke");snprintf(value,sizeof(value),"%u%%",unsigned(s.settings.alarm.volumePeak));break;
          case 8:label=pick(l,"Záró hangerő","End volume","Endlautstärke");snprintf(value,sizeof(value),"%u%%",unsigned(s.settings.alarm.volumeEnd));break;
          case 9:label=pick(l,"Felfutás","Rise time","Anstieg");snprintf(value,sizeof(value),"%u min",unsigned(s.settings.alarm.volumeRise));break;
          case 10:label=pick(l,"Lefutás","Fall time","Abfall");snprintf(value,sizeof(value),"%u min",unsigned(s.settings.alarm.volumeFall));break;
          case 11:label=pick(l,"Kezdő fény","Start light","Starthelligkeit");snprintf(value,sizeof(value),"%u%%",unsigned(s.settings.alarm.lightStart));break;
          case 12:label=pick(l,"Csúcsfény","Peak light","Max. Helligkeit");snprintf(value,sizeof(value),"%u%%",unsigned(s.settings.alarm.lightPeak));break;
          case 13:label=pick(l,"Záró fény","End light","Endhelligkeit");snprintf(value,sizeof(value),"%u%%",unsigned(s.settings.alarm.lightEnd));break;
          case 14:label=pick(l,"Fény felfut","Light rise","Licht auf");snprintf(value,sizeof(value),"%u min",unsigned(s.settings.alarm.lightRise));break;
          case 15:label=pick(l,"Fény lefut","Light fall","Licht ab");snprintf(value,sizeof(value),"%u min",unsigned(s.settings.alarm.lightFall));break;
          case 16:label=pick(l,"Éjszakai óra","Night clock","Nachtuhr");snprintf(value,sizeof(value),"%s",s.settings.alarm.nightClock?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));break;
          case 17:case 18:case 19:case 20:case 21:case 22:case 23:{
              static const char* const hu[]={"Hétfő","Kedd","Szerda","Csütörtök","Péntek","Szombat","Vasárnap"};
              static const char* const en[]={"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};
              static const char* const de[]={"Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag","Sonntag"};
              const unsigned day=item-17;
              label=pick(l,hu[day],en[day],de[day]);
              snprintf(value,sizeof(value),"%s",s.settings.alarm.daysMask&(1u<<day)?
                  pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));break;
          }
#if IBT_NO_PSRAM
          case 37:label="Órastílus";snprintf(value,sizeof(value),"%s",lightui::clockStyle.load()?"Digitális":"Lapozós");break;
#endif
          case 28:label=pick(l,"Kettőspont villogás","Blink colon","Doppelpunkt blinken");
              snprintf(value,sizeof(value),"%s",s.settings.alarm.blinkColon?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));break;
          case 24:label=pick(l,"Óra nézete","Clock view","Uhransicht");
              snprintf(value,sizeof(value),"%s",s.settings.alarm.clockDisplay==0?
                  pick(l,"Pontos idő","Time","Uhrzeit"):s.settings.alarm.clockDisplay==1?
                  pick(l,"Hátralévő","Countdown","Countdown"):
                  pick(l,"Mindkettő","Both","Beide"));break;
          case 25:label=pick(l,"Ébresztő forrás","Alarm source","Weckquelle");
              snprintf(value,sizeof(value),"%s",s.settings.alarm.source?"SD / Ébresztő":pick(l,"Wi-Fi / rádió","Wi-Fi / radio","WLAN / Radio"));break;
          case 26:label=pick(l,"SD lejátszás","SD playback","SD-Wiedergabe");
              snprintf(value,sizeof(value),"%s",s.settings.alarm.sdPlayMode==0?
                  pick(l,"Folyamatos","Continuous","Endlos"):s.settings.alarm.sdPlayMode==1?
                  pick(l,"Véletlenszerű","Random","Zufall"):
                  pick(l,"Egyszeri","Once","Einmal"));break;
          default:label=dual::tr(l,dual::Text::Back);break;
        }
        enabled=true;break;
    case 9:
        switch(item){
          case 0:label=pick(l,"Állapot","Status","Status");snprintf(value,sizeof(value),"%s",s.connected?pick(l,"csatlakozva","connected","verbunden"):pick(l,"nincs kapcsolat","offline","offline"));enabled=true;break;
          case 1:label=pick(l,"Új Wi-Fi beállítása","Set up new Wi-Fi","Neues Wi-Fi einrichten");enabled=true;break;
          case 2:label=pick(l,"Mentett hálózatok","Saved networks","Gespeicherte Netze");enabled=true;break;
          case 3:label=pick(l,"Hálózat keresése","Scan networks","Netze suchen");enabled=true;break;
          case 4:label="Radio DNS";enabled=true;break;
          case 5:label=pick(l,"Online / Wi-Fi","Online / Wi-Fi","Online / WLAN");enabled=true;if(s.settings.networkMode==0)snprintf(value,sizeof(value),"%s",pick(l,"Aktív","Active","Aktiv"));break;
          case 6:label=pick(l,"Offline / SD","Offline / SD","Offline / SD");enabled=true;if(s.settings.networkMode==1)snprintf(value,sizeof(value),"%s",pick(l,"Aktív","Active","Aktiv"));break;
          case 7:label="Halozat nelkul / BT bemenet";enabled=true;if(s.settings.networkMode==2)snprintf(value,sizeof(value),"%s",pick(l,"Aktív","Active","Aktiv"));break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 53:{
        enabled=true;
        if(item==0){label=pick(l,"DNS mód","DNS mode","DNS-Modus");snprintf(value,sizeof(value),"%s",s.settings.dnsManual?pick(l,"Kézi","Manual","Manuell"):pick(l,"Automatikus","Automatic","Automatisch"));}
        else if(item==1||item==2){label=item==1?pick(l,"Elsődleges","Primary","Primär"):pick(l,"Tartalék","Secondary","Sekundär");const auto a=item==1?s.settings.dnsPrimary:s.settings.dnsSecondary;snprintf(value,sizeof(value),"%u.%u.%u.%u",a[0],a[1],a[2],a[3]);}
        else if(item==3)label="Cloudflare 1.1.1.1";
        else if(item==4)label="Google 8.8.8.8";
        else if(item==5)label="Quad9 9.9.9.9";
        else label=dual::tr(l,dual::Text::Back);break;
    }
    case 54:{enabled=true;
        if(item<4){label=pick(l,"Címrész","Octet","Adressteil");snprintf(value,sizeof(value),"%u: %u",item+1,dual::dnsDraftOctet(item));}
        else if(item==4){label=pick(l,"Mentés","Save","Speichern");uint8_t a[4];for(unsigned i=0;i<4;i++)a[i]=dual::dnsDraftOctet(i);enabled=radiodns::validServer(a);}
        else label=pick(l,"Mégse","Cancel","Abbrechen");break;
    }
    case 47:{
        const unsigned active=dual::playerSkinId(s.settings);
        if(item==0)label="Bluetooth hangszoro";
        else if(item==1)label=pick(l,"Kazetta","Cassette","Kassette");
        else if(item==2)label="Winamp";
        else if(item==3)label="Media Player";
        else if(item==4){snprintf(out,cap,"—");return false;}
        else if(item==5)label=pick(l,"Service / jelgenerátor","Service / signal generator","Service / Signalgenerator");
        else if(item==6)label=pick(l,"Retro neon","Retro neon","Retro Neon");
        else if(item==7)label=pick(l,"Vintage Hi-Fi","Vintage Hi-Fi","Vintage Hi-Fi");
        else if(item==8)label=pick(l,"Klasszikus fa","Classic wood","Klassisches Holz");
        else if(item==13)label="Videoton RT 6303 S";
        else if(item==11)label="Neon Cyberwave";
        else if(item==12)label=pick(l,"Időjárás · Fotó","Weather · Photo","Wetter · Foto");
        else if(item==9)label=pick(l,"Retro szín","Retro color","Retro Farbe");
        else if(item==10){label=pick(l,"Sepréses nézetváltás","Swipe to change skin","Wischen: Ansicht wechseln");snprintf(value,sizeof(value),"%s",s.settings.swipeSkins?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));}
        else label=dual::tr(l,dual::Text::Back);
        if(item!=5&&(item==13?active==15:item==active))snprintf(value,sizeof(value),"%s",pick(l,"Aktív","Active","Aktiv"));
        enabled=true;
        }break;
    case 72:{enabled=true;if(item<8){label=item==0?pick(l,"Eredeti","Original","Original"):item==1?"Videoton RT 6303 S":item==2?pick(l,"Kövesse a skint","Follow skin","Wie Oberfläche"):item==3?"Windows XP / Winamp":item==4?"Windows 7 / Media Player":item==5?pick(l,"Bootdallam: KI","Boot sound: OFF","Startton: AUS"):item==6?pick(l,"Bootdallam: beépített","Boot sound: built-in","Startton: intern"):"Boot: SD /boot.wav";if((item<5&&item==s.settings.bootScreen)||(item>=5&&item-5==s.settings.bootSound))snprintf(value,sizeof(value),"%s",pick(l,"Aktív","Active","Aktiv"));}else label=dual::tr(l,dual::Text::Back);break;}
    case 56:{
        static const char* const hu[]={"Zöld","Borostyán","Narancs","Piros","Kék","Jégkék","Türkiz","Lila","Fehér"};
        static const char* const en[]={"Green","Amber","Orange","Red","Blue","Ice blue","Turquoise","Purple","White"};
        static const char* const de[]={"Grün","Bernstein","Orange","Rot","Blau","Eisblau","Türkis","Lila","Weiß"};
        if(item<9){label=unsigned(l)==0?hu[item]:unsigned(l)==1?en[item]:de[item];
            if(item==s.settings.retroColor)snprintf(value,sizeof(value),"%s",pick(l,"Aktív","Active","Aktiv"));}
        else label=dual::tr(l,dual::Text::Back);
        enabled=true;
        }break;
    case 10:
        switch(item){
          case 0:{label=dual::tr(l,dual::Text::Theme);const char* v[3][3]={{"Sötét","Világos","Auto"},{"Dark","Light","Auto"},{"Dunkel","Hell","Auto"}};snprintf(value,sizeof(value),"%s",v[unsigned(l)][unsigned(s.settings.theme)]);enabled=true;break;}
          case 1:label=dual::tr(l,dual::Text::Rotation);snprintf(value,sizeof(value),"%u°",s.settings.rotation*90);enabled=true;break;
          case 2:label=dual::tr(l,dual::Text::Brightness);if(s.settings.brightness==0)snprintf(value,sizeof(value),"Auto");else snprintf(value,sizeof(value),"%u%%",s.settings.brightness);enabled=true;break;
          case 3:label=pick(l,"Borítókép / Artwork","Artwork","Cover");snprintf(value,sizeof(value),"%s",s.settings.artworkEnabled?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 4:label=pick(l,"Skin kiválasztása","Select skin","Skin auswählen");enabled=true;break;
          case 5:label=pick(l,"Pufferjelző","Buffer bar","Pufferanzeige");snprintf(value,sizeof(value),"%s",s.settings.bufferIndicator?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 6:label=pick(l,"Orsóforgás","Reel animation","Spulenanimation");snprintf(value,sizeof(value),"%s",s.settings.reelAnimation?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 7:label=pick(l,"Térerő (Wi-Fi / BT)","Signal (Wi-Fi / BT)","Signal (WLAN / BT)");snprintf(value,sizeof(value),"%s",s.settings.wifiIndicator?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 8:label=pick(l,"Bootképernyő","Boot screen","Startbild");enabled=true;break;
          case 9:label=pick(l,"Akkumulátor","Battery","Akku");enabled=true;break;
          case 10:label=pick(l,"VU és spektrum frissítés","VU and spectrum refresh","VU- und Spektrumaktualisierung");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 52:
        switch(item){
          case 0:label=pick(l,"Kijelzés","Indicator","Anzeige");snprintf(value,sizeof(value),"%s",s.settings.batteryIndicator?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));break;
          case 1:label=pick(l,"Lépték","Step","Schritt");snprintf(value,sizeof(value),"%u%%",s.settings.batteryStep);break;
          case 2:label=pick(l,"Megjelenítés","Appearance","Darstellung");snprintf(value,sizeof(value),"%s",s.settings.batteryAppearance==0?pick(l,"Százalék","Percent","Prozent"):s.settings.batteryAppearance==1?pick(l,"Ikon","Icon","Symbol"):pick(l,"Mindkettő","Both","Beide"));break;
          default:label=dual::tr(l,dual::Text::Back);break;
        }enabled=true;break;
    case 49:
        switch(item){
          case 0:label=pick(l,"Fény / Akku mérés","Light / Battery","Licht / Akku");
#if defined(BOARD_CYD28) && !FEATURE_ADC_BUTTONS
              enabled=true;
#endif
              if(!s.settings.sensorMode)snprintf(value,sizeof(value),"%s",pick(l,"Ki","Off","Aus"));
              else if(!s.sensorValid)snprintf(value,sizeof(value),"%s --",s.settings.sensorMode==2?"Akku":"LDR");
              else if(s.settings.sensorMode==1)snprintf(value,sizeof(value),"LDR %u mV",unsigned(s.sensorMv));
              else {uint32_t cell=uint32_t(s.sensorMv)*2;snprintf(value,sizeof(value),"Akku %u.%02u V",unsigned(cell/1000),unsigned((cell%1000)/10));}
              break;
          case 1:label=pick(l,"BLE hőmérők","BLE thermometers","BLE-Thermometer");enabled=sensors::available();break;
          case 2:label=pick(l,"Akkumulátor kalibrálása","Battery calibration","Akku kalibrieren");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        }break;
    case 50:
        if(item<5){static const char* labels[]={"0% GPIO mV","25% GPIO mV","50% GPIO mV","75% GPIO mV","100% GPIO mV"};label=labels[item];snprintf(value,sizeof(value),"%u",unsigned(s.settings.batteryMv[item]));}
        else {label=dual::tr(l,dual::Text::Back);if(!battery::valid(s.settings.batteryMv))snprintf(value,sizeof(value),"%s",pick(l,"Hibás sorrend","Invalid order","Falsche Reihenfolge"));}
        enabled=true;break;
    case 11:
        switch(item){
          case 0:label="Rotary";snprintf(value,sizeof(value),"%s",s.settings.rotaryMode==1?pick(l,"hangerő","volume","Lautstärke"):pick(l,"állomás","station","Sender"));enabled=true;break;
          case 1:label=pick(l,"Touch kalibráció (stop)","Touch calibration (stop)","Touch Kalibrierung (Stopp)");enabled=true;break;
          case 2:label=pick(l,"Gombkiosztás","Button mapping","Tastenbelegung");enabled=true;break;
          case 3:label=pick(l,"Skin lapozás","Swipe skins","Skin-Wischen");snprintf(value,sizeof(value),"%s",s.settings.swipeSkins?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 4:label=pick(l,"Menü bezárási idő","Menu auto-close","Menü-Schließzeit");enabled=true;break;
          case 5:label=IBT_COMPACT_UI?"IR taviranyito":pick(l,"IR távirányító · előkészítve","IR remote · not enabled","IR-Fernbedienung · vorbereitet");enabled=IBT_COMPACT_UI;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 12:
        switch(item){
          case 0:label="PSRAM / HiMem";enabled=true;break;
          case 1:label=pick(l,"SD kártya","SD card","SD Karte");
#if FEATURE_SD
              enabled=true;
#endif
              break;
          case 2:label=pick(l,"Artwork cache","Artwork cache","Artwork Cache");enabled=true;break;
          case 3:label=pick(l,"Fájlrendszer","Filesystem","Dateisystem");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 19:
        switch(item){
          case 0:label=dual::tr(l,dual::Text::Language);snprintf(value,sizeof(value),"%s",unsigned(l)==0?"Magyar":unsigned(l)==1?"English":"Deutsch");enabled=true;break;
          case 1:label=pick(l,"Firmware információ","Firmware info","Firmware Info");enabled=true;break;
          case 2:label=pick(l,"Webes frissítés","Web update","Web-Update");enabled=true;break;
          case 3:label=pick(l,"Újraindítás (2×, stop)","Restart (twice, stop)","Neustart (2×, Stopp)");enabled=true;break;
          case 4:label=pick(l,"Mentés / vissza SD","SD backup / restore","SD Sichern / Zurück");
#if FEATURE_SD
              enabled=true;
#endif
              break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 13:
        switch(item){
          case 0:label="Debug";snprintf(value,sizeof(value),"%s",s.settings.debug?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 1:label=pick(l,"Memória / PSRAM","Memory / PSRAM","Speicher / PSRAM");enabled=true;break;
          case 2:label=pick(l,"Hálózat","Network","Netzwerk");enabled=true;break;
          case 3:label="Rotary debug";enabled=true;break;
          case 4:label=pick(l,"PSRAM stresszteszt","PSRAM stress test","PSRAM Stresstest");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 37:
        switch(item){
          case 0:label=pick(l,"Állomások mentése","Save stations","Sender sichern");enabled=true;break;
          case 1:label=pick(l,"Állomások vissza (2×)","Restore stations (2x)","Sender zurück (2x)");enabled=true;break;
          case 2:label=pick(l,"Wi-Fi mentése","Save Wi-Fi","Wi-Fi sichern");enabled=true;break;
          case 3:label=pick(l,"Wi-Fi vissza (2×)","Restore Wi-Fi (2x)","Wi-Fi zurück (2x)");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    default: break;
    }
    snprintf(out,cap,"%s%s%s",label,*value?": ":"",value);
    return enabled;
}
inline void progress(char *out,size_t cap,const dual::Snapshot &s) {
    if(s.sd)snprintf(out,cap,"%02lu:%02lu / %02lu:%02lu",(unsigned long)(s.elapsed/60),(unsigned long)(s.elapsed%60),(unsigned long)(s.duration/60),(unsigned long)(s.duration%60));
    else snprintf(out,cap,"%u kbit/s  %lu Hz",s.bitrate,(unsigned long)s.sampleRate);
}
}
