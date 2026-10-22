# I-BT-Radio — nyilvános firmware-forrás

2026. október 22. Az alkalmazás, menü, hangút, WebUI, bootképernyők és skinek forrása itt érhető el.

| Könyvtár | Alapállapot | Státusz |
|---|---|---|
| [Light](light/) | R211, helyi baseline d8bcbfc, firmwareforrás a13c7b4 | Lezárt diagnosztikai változat; nem stabil kiadás |
| [Pro](pro/) | A meglévő R176 fejlesztési fa ellenőrzőösszeggel rögzített másolata | Korábbi forrásbaseline; új készülékteszt nem történt |

Az R211 célzott stream/artwork diagnosztika: WebUI, BT/BLE, SD-lejátszás, teljes menü, VU, EQ és időjárás kikapcsolva; alap Play/Szünet és hangerő kezelhető. A BT/BLE-visszaállítás külön következő munkablokk, még nincs kész kiadása. Az R211 közölt 5000 bájtos határ diagnosztikai kivétel; a normál forrás alapértéke10000 bájt, a500ms hangvédelem változatlan.

A `SOURCE_MANIFEST.json` az átadott fájlok SHA-256 listája. A folyamatban lévő helyi BT/BLE-módosításokat nem kevertem a baseline-ba. Nincsenek Wi-Fi-jelszavak, eszközmentések, NVS-tartalmak vagy személyes naplók. A myoptions.h általános panel- és bekötési konfigurációt tartalmaz.

## Fordítás és függőségek

PlatformIO/pioarduino55.03.311, ArduinoESP323.3.11 és ESP-IDF5.5.5 az alkalmazott környezet. A `dependencies.lock`, az SDK-konfigurációk, CMake- és PlatformIO-fájlok megmaradtak. A jelenlegi fejlesztési környezet C:/esp32 alatti helyi útvonalakat és előre letöltött managed_components könyvtárat használ. Ez a publikált forráspillanatkép **még nem igazolt, tiszta gépen önmagában reprodukálható buildcsomag**; a környezet hordozhatóvá tétele külön, visszavonható fejlesztési blokk. Ne tekints egy eltérő SDK-val készülő buildet az eredeti BIN hiteles reprodukciójának.

Az eredetre és licencekre vonatkozó információ: [licencleírás](https://github.com/szupert/I-BT-Radio-/blob/main/THIRD_PARTY_NOTICES.md).

## Közreműködés

Egy PR egy ellenőrizhető munkablokk: hálózat, PCM/DAC, BT-életciklus, BLE-olvasás, menü/WebUI vagy telepítő. Kötelező baseline/hash, ok, érintett fájlok, célzott teszt, PASS/FAIL/NOT_RUN, visszaállítási pont. Teljes fákat újrageneráló és egymástól független javításokat összekeverő PR nem kérhető. A meglévő működő kód az alap; lezárt teszt ismétlése csak új változás vagy regressziógyanú miatt.

A bináris telepítők a [beta](../beta/) négy hardverprofilja alatt maradnak. A forrás közzététele nem indít automatikus flashelést.


##8048S070C application
The exact MENU8048C-R4 application source is in [8048](8048/README.md). S3/16MB/8MB/800x480 capacitive menu and skin demo; radio audio backend is not integrated. The approved binary is unchanged. The portable S3 installer is separate from the four WROOM hardware profiles.
