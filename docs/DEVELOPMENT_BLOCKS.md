# Ellenőrizhető fejlesztési blokkok

2026.10.22. Egy PR/commit egy logikai blokkot módosít: audio/PCM/DAC; hálózat/puffer; Bluetooth; BLE; kijelző/menü; WebUI; telepítő. Előbb meglévő forrás és bizonyíték, új kutatás csak konkrét nyitott kérdésre. A teljes projekt újragenerálása és független hibák összevonása nem elfogadott munkamódszer.

Minden blokk tartalmazza: baseline és hardverprofil; konkrét hiba/hipotézis; érintett fájlok; célzott teszt és valós PASS/FAIL/NOT_RUN; rollback commit/hash; következő egyetlen lépés. A fizikai tesztet nem helyettesíti a build vagy a számláló. A lezárt változatlan teszteket nem ismételjük indokolatlanul. Két sikertelen kör után gyökérokelemzés.

Elsődleges a4MB/noPSRAM Light stabilizálása. BT-IN és SD→BT-OUT, majd CLOCK/WEATHER BLE és a módváltási memória-életciklus jön az artwork előtt. Nincs WEB→BT vagy EQ-DSP. A nem használt szolgáltatások ne induljanak el; az SDK kötelező statikus BT/BLE-tartaléka nem nevezhető felszabadított heapnek.

Az500ms/2000ms hangvédelem, bekötés, flashgeometria és BLE32768/16384/3072B feltételek megmaradnak. A korábbi64KiB általános RAM-előírást a felhasználó későbbi, bájtban rögzített utasítása felülírta: normálforrás10000B; R210/R211 stream/artwork diagnosztika kifejezett5000B kivétel. Ez nem ígéret arra, hogy a futás minden átmeneti allokáció alatt ténylegesen megtartja a határt; azt mérni kell. További változás csak külön felhasználói döntéssel.

Automatikus flash/NVS-törlés vagy hardvermódosítás nincs. A telefonos és Garmin Fénix7X alkalmazás DEFERRED, nem a stabil rádió kiadásának feltétele. A Dropbox egyetlen központi roadmapjét ne váltsa fel második master; a nyilvános GitHub-dokumentumok feladatleírások és bizonyítékok.
