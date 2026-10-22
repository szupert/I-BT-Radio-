#pragma once
#include "../dual/core.h"
#include "../dual/bt_audio.h"
#include <stdio.h>
#include "../dual/vu_probe.h"
namespace content {
inline const char* pick(dual::Language l,const char* hu,const char* en,const char* de){
    return unsigned(l)==0?hu:unsigned(l)==1?en:de;
}
inline unsigned itemCount(unsigned page){
    switch(page){
      case 1:return 5;   // root, including clock home
      case 3:return 6;   // Bluetooth input, output, DAC, status, rate, back
      case 45:return 8;
      case 46:return 6;
      case 43:return 5;  // four standard SBC rates and back
      case 6:return 5;   // sound
      case 7:return 11;   // settings, alarm, back
      case 59:return 7;  // alarm groups, mode and back
      case 60:return 12;
      case 61:return 4;
      case 62:case 63:return 6;
      case 64:return 3;
      case 8:return 6;   // stations
      case 9:return 6;
      case 53:return 7;
      case 54:return 6;   // wifi
      case 47:return 12;
      case 56:return 10; // nine retro illumination colors and back
      case 49:return 3;
      case 50:return 6;
      case 52:return 4;
      case 10:return 11;  // display
      case 11:return 5;  // controls
      case 12:return 5;  // storage
      case 13:return 6;  // diagnostics
      case 19:return 6;  // system
      case 14:return 5;  // volume/stream
      case 15:return 5;  // eq
      case 57:return 13; // ten bands, AUTO, enable, back
      case 58:return eq10::PresetCount+1;
      case 16:return 5;  // outputs
      case 37:return 5;  // SD data backup / restore
      default:return 0;
    }
}
inline bool isMenuPage(unsigned page){return itemCount(page)!=0;}
inline const char* pageTitle(dual::Language l,unsigned page){
    switch(page){
      case 1:return pick(l,"Menü","Menu","Menü");
      case 2:return pick(l,"Memória","Memory","Speicher");
      case 3:return "Bluetooth";
      case 46:return pick(l,"VU frissítés","VU refresh","VU Aktualisierung");
      case 45:return pick(l,"Tétlenség","Inactivity","Inaktivität");
      case 44:return pick(l,"Bluetooth-kimenet","Bluetooth output","Bluetooth-Ausgang");
      case 43:return pick(l,"BT kimeneti frekvencia","BT output rate","BT-Ausgangsrate");
      case 4:return pick(l,"Állomások","Stations","Sender");
      case 5:return "Rotary debug";
      case 6:return pick(l,"Hang","Sound","Klang");
      case 7:return pick(l,"Beállítások","Settings","Einstellungen");
      case 8:return pick(l,"Állomások","Stations","Sender");
      case 9:return "Wi-Fi";
      case 53:return pick(l,"Radio DNS","Radio DNS","Radio DNS");
      case 54:return pick(l,"DNS IPv4-cím","DNS IPv4 address","DNS IPv4-Adresse");
      case 55:return pick(l,"DNS címrész","DNS octet","DNS Adressteil");
      case 47:return pick(l,"Felület","Appearance","Oberfläche");
      case 56:return pick(l,"Retro megvilágítás","Retro illumination","Retro Beleuchtung");
      case 52:return pick(l,"Akkumulátor","Battery","Akku");
      case 49:return pick(l,"Hardver","Hardware","Hardware");
      case 50:return pick(l,"Speciális / Akkumérés","Special / Battery","Spezial / Akku");
      case 10:return pick(l,"Kijelző","Display","Anzeige");
      case 11:return pick(l,"Vezérlés","Controls","Steuerung");
      case 12:return pick(l,"Tárhely","Storage","Speicher");
      case 13:return pick(l,"Diagnosztika","Diagnostics","Diagnose");
      case 20:return pick(l,"Firmware információ","Firmware info","Firmware Info");
      case 19:return pick(l,"Rendszer","System","System");
      case 14:return pick(l,"Hangerő / Stream","Volume / Stream","Lautstärke / Stream");
      case 15:return "EQ";
      case 57:return pick(l,"10 sávos EQ","10-band EQ","10-Band-EQ");
      case 58:return pick(l,"EQ profilok","EQ presets","EQ-Profile");
      case 59:return pick(l,"Ébresztőóra","Alarm clock","Wecker");
      case 60:return pick(l,"Időzítés és napok","Schedule and days","Zeit und Tage");
      case 61:return pick(l,"Ébresztő hangforrás","Alarm source","Weckquelle");
      case 62:return pick(l,"Hangerőgörbe","Volume envelope","Lautstärkeverlauf");
      case 63:return pick(l,"Fényerőgörbe","Light envelope","Helligkeitsverlauf");
      case 64:return pick(l,"Óra megjelenítése","Clock display","Uhrenanzeige");
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
    static constexpr uint8_t schedule[]={1,2,3,4,17,18,19,20,21,22,23};
    static constexpr uint8_t source[]={25,5,26};
    if(page==60)return item<11?schedule[item]:27;
    if(page==61)return item<3?source[item]:27;
    if(page==62)return item<5?6+item:27;
    if(page==63)return item<5?11+item:27;
    if(page==64)return item<2?(item?16:24):27;
    return 27;
}
inline bool menuItem(char *out,size_t cap,const dual::Snapshot &s,unsigned page,unsigned item){
    if(page>=60&&page<=64){item=alarmField(page,item);page=66;}
    const auto l=s.settings.language; const char* label="?"; char value[48]={}; bool enabled=false;
    switch(page){
    case 1:
        {static const char* hu[]={"Állomások","Hang","Bluetooth","Beállítások"};
         static const char* en[]={"Stations","Sound","Bluetooth","Settings"};
         static const char* de[]={"Sender","Klang","Bluetooth","Einstellungen"};
         if(item<4){label=unsigned(l)==0?hu[item]:unsigned(l)==1?en[item]:de[item];enabled=true;}
         else if(item==4){label=pick(l,s.settings.alarm.clockHome?"Rádió megjelenítése":"Óra megjelenítése",s.settings.alarm.clockHome?"Show radio":"Show clock",s.settings.alarm.clockHome?"Radio anzeigen":"Uhr anzeigen");enabled=true;}}
        break;
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
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 6:
        switch(item){
          case 0:label=pick(l,"Hangerő / Stream","Volume / Stream","Lautstärke / Stream");enabled=true;break;
          case 1:label=pick(l,"Hangszín / EQ","Tone / EQ","Klang / EQ");enabled=true;break;
          case 2:label=pick(l,"Kimenetek","Outputs","Ausgänge");enabled=true;break;
          case 3:label=pick(l,"Bluetooth-hang","Bluetooth audio","Bluetooth Audio");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
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
          case 3:label=pick(l,"10 sávos EQ","10-band EQ","10-Band-EQ");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 57:
        if(item<eq10::Bands){
            snprintf(out,cap,"%s %u Hz  %+d dB",item<5?" ":"",unsigned(eq10::Hz[item]),
                     int(s.settings.graphicEq.db[item]));return true;
        }
        if(item==10){label="AUTO";snprintf(value,sizeof(value),"%s",eq10::name(s.settings.graphicEq.preset,unsigned(l)));}
        else if(item==11){label=pick(l,"10 sávos EQ","10-band EQ","10-Band-EQ");
            snprintf(value,sizeof(value),"%s",s.settings.graphicEq.enabled?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));}
        else label=dual::tr(l,dual::Text::Back);
        enabled=true;break;
    case 58:
        if(item<eq10::PresetCount){label=eq10::name(eq10::Preset(item),unsigned(l));enabled=true;}
        else {label=dual::tr(l,dual::Text::Back);enabled=true;}
        break;
    case 16:
        switch(item){
          case 0:label="PCM5102A";enabled=true;break;
          case 1:label=pick(l,"Belső DAC","Internal DAC","Interner DAC");break;
          case 2:label=pick(l,"Bluetooth-kimenet","Bluetooth output","Bluetooth-Ausgang");enabled=true;break;
          case 3:label=pick(l,"Bluetooth-bemenet","Bluetooth input","Bluetooth-Eingang");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 3:
        switch(item){
          case 0:label=pick(l,"Bluetooth-bemenet","Bluetooth input","Bluetooth-Eingang");enabled=true;break;
          case 1:label=pick(l,"Bluetooth-kimenet","Bluetooth output","Bluetooth-Ausgang");enabled=true;break;
          case 2:label=pick(l,"Vissza a DAC-ra","Return to DAC","Zurück zum DAC");enabled=true;break;
          case 3:label=pick(l,"Állapot","Status","Status");enabled=true;break;
          case 4:label=pick(l,"BT kimenet kHz","BT output kHz","BT-Ausgang kHz");snprintf(value,sizeof(value),"%u%s",btaudio::outputRate()==44100?44:btaudio::outputRate()/1000,btaudio::outputRate()==44100?",1":"");enabled=true;break;
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
        static const char* labels[]={"10 mp","20 mp","30 mp","1 perc","2 perc","5 perc","Végtelen"};
        if(item<7){label=labels[item];if(s.settings.menuIdleSeconds==seconds[item])snprintf(value,sizeof(value),"*");}
        else label=dual::tr(l,dual::Text::Back);
        enabled=true;break;
    }
    case 7:
        switch(item){
          case 0:label="Wi-Fi";enabled=true;break;
          case 1:label=pick(l,"Kijelző","Display","Anzeige");enabled=true;break;
          case 2:label=pick(l,"Vezérlés","Controls","Steuerung");enabled=true;break;
          case 3:label=pick(l,"Tárhely","Storage","Speicher");enabled=true;break;
          case 4:label=pick(l,"Rendszer","System","System");enabled=true;break;
          case 5:label=pick(l,"Diagnosztika","Diagnostics","Diagnose");enabled=true;break;
          case 6:label=pick(l,"Tétlenség","Inactivity","Inaktivität");enabled=true;break;
          case 7:label=pick(l,"VU frissítés","VU refresh","VU Aktualisierung");enabled=true;break;
          case 8:label=pick(l,"Hardver","Hardware","Hardware");enabled=true;break;
          case 9:label=pick(l,"Ébresztőóra","Alarm clock","Wecker");enabled=true;break;
          default:label=dual::tr(l,dual::Text::Back);enabled=true;break;
        } break;
    case 59:
        if(item==0)return menuItem(out,cap,s,66,0);
        if(item>=1&&item<=5){snprintf(out,cap,"%s",pageTitle(l,59+item));return true;}
        snprintf(out,cap,"%s",dual::tr(l,dual::Text::Back));return true;
    case 66:
        switch(item){
          case 0:label=pick(l,"Ébresztés","Alarm mode","Weckmodus");
              snprintf(value,sizeof(value),"%s",s.settings.alarm.mode==0?pick(l,"Ki","Off","Aus"):
                  !s.settings.alarm.daysMask?pick(l,"Nincs nap","No days","Keine Tage"):
                  !alarmclock::validWindow(s.settings.alarm)?pick(l,"Időhiba","Invalid time","Zeitfehler"):
                  s.settings.alarm.mode==1?pick(l,"Fix","Fixed","Fest"):pick(l,"Felfut/lefut","Rise/fall","Auf/ab"));break;
          case 1:label=pick(l,"Indul óra","Start hour","Start Stunde");snprintf(value,sizeof(value),"%02u",unsigned(s.settings.alarm.onHour));break;
          case 2:label=pick(l,"Indul perc","Start minute","Start Minute");snprintf(value,sizeof(value),"%02u",unsigned(s.settings.alarm.onMinute));break;
          case 3:label=pick(l,"Leáll óra","Stop hour","Ende Stunde");snprintf(value,sizeof(value),"%02u",unsigned(s.settings.alarm.offHour));break;
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
        const unsigned active=s.settings.vintageSkin?5+unsigned(s.settings.vintageSkin):s.settings.playerEnabled?(s.settings.kajonSkin?4:s.settings.mediaPlayerSkin?3:2):(s.settings.cassetteEnabled?1:0);
        if(item==0)label=pick(l,"Egyszerű","Simple","Einfach");
        else if(item==1)label=pick(l,"Kazetta","Cassette","Kassette");
        else if(item==2)label=pick(l,"Klasszikus","Classic","Klassisch");
        else if(item==3)label="Media Player";
        else if(item==4)label="KAJON VU";
        else if(item==5)label=pick(l,"Service / jelgenerátor","Service / signal generator","Service / Signalgenerator");
        else if(item==6)label=pick(l,"Retro neon","Retro neon","Retro Neon");
        else if(item==7)label=pick(l,"Vintage Hi-Fi","Vintage Hi-Fi","Vintage Hi-Fi");
        else if(item==8)label=pick(l,"Klasszikus fa","Classic wood","Klassisches Holz");
        else if(item==9)label=pick(l,"Retro szín","Retro color","Retro Farbe");
        else if(item==10){label=pick(l,"Sepréses nézetváltás","Swipe to change skin","Wischen: Ansicht wechseln");snprintf(value,sizeof(value),"%s",s.settings.swipeSkins?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));}
        else label=dual::tr(l,dual::Text::Back);
        if(item!=5&&item==active)snprintf(value,sizeof(value),"%s",pick(l,"Aktív","Active","Aktiv"));
        enabled=true;
        }break;
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
          case 4:label=pick(l,"Felület","Appearance","Oberfläche");enabled=true;break;
          case 5:label=pick(l,"Pufferjelző","Buffer bar","Pufferanzeige");snprintf(value,sizeof(value),"%s",s.settings.bufferIndicator?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 6:label=pick(l,"Orsóforgás","Reel animation","Spulenanimation");snprintf(value,sizeof(value),"%s",s.settings.reelAnimation?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 7:label=pick(l,"Térerő (Wi-Fi / BT)","Signal (Wi-Fi / BT)","Signal (WLAN / BT)");snprintf(value,sizeof(value),"%s",s.settings.wifiIndicator?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 8:label=pick(l,"BT gyorskereső","BT quick scan","BT-Schnellsuche");snprintf(value,sizeof(value),"%s",s.settings.btQuickScan?pick(l,"Be","On","Ein"):pick(l,"Ki","Off","Aus"));enabled=true;break;
          case 9:label=pick(l,"Akkumulátor","Battery","Akku");enabled=true;break;
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
          case 1:label=pick(l,"Speciális beállítások","Special settings","Spezielle Einstellungen");enabled=true;break;
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
          case 3:label="IR / Remote";break;
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
