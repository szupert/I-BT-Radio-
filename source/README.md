# I-BT-Radio — nyilvános firmware-forrás

2026. október 22. Az alkalmazás, menü, hangút, WebUI, bootképernyők és skinek forrása itt érhető el.

| Könyvtár | Alapállapot | Státusz |
|---|---|---|
| [Light](light/) | R270, commit 96c5287521ca29dcb5c7925cae1558aecf141e47 | Aktuális Light pillanatkép; készülékes R270 próba még szükséges |
| [Pro](pro/) | A meglévő R176 fejlesztési fa ellenőrzőösszeggel rögzített másolata | Korábbi forrásbaseline; új készülékteszt nem történt |

A jelenlegi Lightban Wi-Fi/SD→DAC, SD→BT, telefon→DAC, óra, API-időjárás/BLE és egyszerű WebUI van. A felhasználó által kért reserve0 mérési politika érvényes; a BT250ms és általános500/2000ms védelem megmarad. A részletes változásokat és nyitott hibákat a [R270 tesztútmutató](../docs/LIGHT_R270_TEST_GUIDE.md) tartalmazza.

A LIGHT_R270_MANIFEST.json és a frissített SOURCE_MANIFEST.json pontos SHA256 fájllistát ad. A korábbi manifesztek történeti pillanatképek. Nem tartalmazunk NVS-t, Wi-Fi-jelszavakat vagy személyes naplókat. A Pro R176 és8048 forrása változatlan.

## Fordítás és függőségek

PlatformIO/pioarduino55.03.311, ArduinoESP323.3.11 és ESP-IDF5.5.5 az alkalmazott környezet. A `dependencies.lock`, az SDK-konfigurációk, CMake- és PlatformIO-fájlok megmaradtak. A jelenlegi fejlesztési környezet C:/esp32 alatti helyi útvonalakat és előre letöltött managed_components könyvtárat használ. Ez a publikált forráspillanatkép **még nem igazolt, tiszta gépen önmagában reprodukálható buildcsomag**. Ne tekints egy eltérő SDK-val készülő buildet az eredeti BIN hiteles reprodukciójának.

Az eredetre és licencekre vonatkozó információ: [licencleírás](https://github.com/szupert/I-BT-Radio-/blob/main/THIRD_PARTY_NOTICES.md).

A bináris telepítők a [beta](../beta/) négy hardverprofilja alatt maradnak. A forrás közzététele nem indít automatikus flashelést.


##8048S070C application
The exact MENU8048C-R4 application source is in [8048](8048/README.md). S3/16MB/8MB/800x480 capacitive menu and skin demo; radio audio backend is not integrated. The approved binary is unchanged. The portable S3 installer is separate from the four WROOM hardware profiles.
