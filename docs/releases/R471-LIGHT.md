# R471 LIGHT · tesztkiadás

A borítóletöltő akkor dolgozik, amikor az internetes rádió van kiválasztva, a lejátszás áll, a parancssor üres és a Bluetooth-hangút szabad. Wi-Fi → DAC lejátszáskor az MP3 és az AAC hangpuffer is elkérheti a borítóletöltő 7 KiB-os megosztott memóriáját. Az átadás a használható pufferelrendezéstől és a szabad helytől függ.

A csomag a klasszikus ESP32-WROOM-os, 4 MB-os LIGHT rádióhoz készült. A fordítást és a bináris azonosítóját ellenőriztem. A három rádióadó mérési eredménye lent olvasható.

## Telepítés meglévő LIGHT R470-re

Ez az alkalmazásfrissítés a gyári LIGHT R470 partíciókiosztását igényli. Ellenőrizd a kijelzőn a verziót, bontsd ki a ZIP-et, majd indítsd a meglévő LIGHT feltöltő környezetében a mellékelt leírás szerint. Az alkalmazás helye 0x10000. A csomag kizárólag az alkalmazást írja. Üres panel első telepítéséhez teljes LIGHT telepítő szükséges.

## Háromperces készülékpróba · 2026. november 22.

LIGHT R471, PSRAM nélkül, Wi-Fi → DAC. A 180 másodpercet minden adónál a dekódolt lejátszás elindulásától mértem. A RAM-oszlop a lejátszás közbeni minták legkisebb és legnagyobb szabad belső memóriája.

| Adó | Hangpuffer (maximum) | Szabad RAM (bájt) | Újrapufferelés | AAC-dekódolási hiba |
|---|---:|---:|---:|---:|
| Budapest Rock | 24 KiB | 13784–22712 | 5 | — |
| Delite AAC | 23 KiB | 16816–22640 | 0 | 26 |
| Megadance | 24 KiB | 13484–22708 | 1 | — |

Mindhárom adónál 7168 bájt került át a borítóletöltőtől a hangpufferhez. A három lejátszási ablakban a naplóban nem indult új AW hálózati kérés. A Delite első sikertelen kapcsolódása és az automatikus újrapróbálás között viszont az AW elindult: ilyenkor a lejátszó átmenetileg leállított állapotot jelez. A lejátszási szándék tehát nincs végig védve.

A Delite 24 bemeneti adathiányt és 2 érvénytelen csatornaszámot jelzett. A próbák hangfolytonosságát a rádió puffer-, dekóder- és DAC-számlálói alapján vizsgáltam; hangfelvétel nem készült.

A végső kézi leállítás után új borítókérés is indult.
