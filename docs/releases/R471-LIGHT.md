# R471 LIGHT · tesztkiadás

A borítóletöltő akkor dolgozik, amikor az internetes rádió van kiválasztva, a lejátszás áll, a parancssor üres és a Bluetooth-hangút szabad. Wi-Fi → DAC lejátszáskor az MP3 és az AAC hangpuffer is elkérheti a borítóletöltő 7 KiB-os megosztott memóriáját. Az átadás a használható pufferelrendezéstől és a szabad helytől függ.

A csomag a klasszikus ESP32-WROOM-os, 4 MB-os LIGHT rádióhoz készült. A fordítást és a bináris azonosítóját ellenőriztem. Az R471 készülékes hangpróbája még hátravan.

## Telepítés meglévő LIGHT R470-re

Ez az alkalmazásfrissítés a gyári LIGHT R470 partíciókiosztását igényli. Ellenőrizd a kijelzőn a verziót, bontsd ki a ZIP-et, majd indítsd a meglévő LIGHT feltöltő környezetében a mellékelt leírás szerint. Az alkalmazás helye 0x10000. A csomag kizárólag az alkalmazást írja. Üres panel első telepítéséhez teljes LIGHT telepítő szükséges.
