# Ellenőrizhető fejlesztési blokkok

2026.10.22. Egy PR/commit egy logikai blokkot módosít: audio/PCM/DAC; hálózat/puffer; Bluetooth; BLE; kijelző/menü; WebUI; telepítő. Előbb meglévő forrás és bizonyíték, új kutatás csak konkrét nyitott kérdésre. A teljes projekt újragenerálása és független hibák összevonása nem elfogadott munkamódszer.

Minden blokk tartalmazza: baseline és hardverprofil; konkrét hiba/hipotézis; érintett fájlok; célzott teszt és valós PASS/FAIL/NOT_RUN; rollback commit/hash; következő egyetlen lépés. A fizikai tesztet nem helyettesíti a build vagy a számláló. A lezárt változatlan teszteket nem ismételjük indokolatlanul. Két sikertelen kör után gyökérokelemzés.

Elsődleges a4MB/noPSRAM Light stabilizálása. BT-IN és SD→BT-OUT, majd CLOCK/WEATHER BLE és a módváltási memória-életciklus jön az artwork előtt. Nincs WEB→BT vagy EQ-DSP. A nem használt szolgáltatások ne induljanak el; az SDK kötelező statikus BT/BLE-tartaléka nem nevezhető felszabadított heapnek.

Aktuális Light felhasználói döntés: reserve0, nincs külön indulási RAM-tartalék. BT32kHz/250ms, általános500/2000ms védelem,12288B hangverem,TX32768/RX8192/PCM4608,Wi-Fi16KiB75%. A korábbi10000/5000B és BLE-tartalékok történeti profilokra vonatkoznak; nem újraértelmezendő aktuális korlátok. A működő ébresztési mag és FLAC-DAC megmarad, SD-BTFLAC javítása DEFERRED.

Automatikus flash/NVS-törlés vagy hardvermódosítás nincs. A telefonos és Garmin Fénix7X alkalmazás DEFERRED, nem a stabil rádió kiadásának feltétele. A Dropbox egyetlen központi roadmapjét ne váltsa fel második master; a nyilvános GitHub-dokumentumok feladatleírások és bizonyítékok.
