# I-BT-Radio Light 4 MB — R174 BÉTA

2026. október 20. · **4 MB flash, PSRAM nélkül** · V166-R174

[**Telepítő letöltése**](I-BT-Radio_Light4MB_R174_BETA.zip?raw=1)

A ZIP-et teljesen ki kell csomagolni, majd az `1_FELTOLTES.bat` indítható Windows alatt. A telepítő automatikusan felismeri a chipet, a flash méretét és a PSRAM-ot, és kiválasztja az ellenőrzött feltöltési sebességet. Más memóriaprofilra ez a csomag nem ír. A COM-port és a valódi feltöltési százalék végig látható.

Azonos partíció esetén a frissítés adatmegőrző. Üres vagy eltérő flash teljes telepítésénél a telepítő külön jelzi az adattörlést és IGEN megerősítést kér. A Wi-Fi, az állomáslista és a beállítások ilyenkor törlődnek; az SD-kártyát nem érinti. Hiba esetén nincs automatikus újraflashelés. Feltöltés után stresszteszt nem indul.

## Mit javítottam?

- Az MP3 védett munkaterületét az I/O-pufferek előtt foglalom le; leállításkor visszaadom a fölösleges input/PCM és TLS erőforrásokat.
- BT-IN módban az eredeti32 KiB vételi puffer mellé nem foglalok kihasználatlan64 KiB adási puffert. A BT-OUT500 ms-os előtöltése változatlan.
- A Bluetooth profil-visszaigazolás és az utolsó pufferhasználó után a Light SDK-erőforrásait is szabályosan leállítom. Bizonytalan leálláskor nem indítok rá DAC-lejátszást.
- A WebUI bal oldalán hangjegy nyitja az állomásszerkesztőt, importot és exportot; jobb oldalon fogaskerék nyitja a beállításokat. Bekerült a Winamp-előnézet is.
- A böngészőben1–10 CSV/M3U lista egyesíthető és a duplikációk kiszűrhetők. Először csak az előnézet változik; a rádióra mentés külön gomb. Importhoz leállított internet-rádió/DAC mód szükséges.
- Főverzió-, R-release- vagy WebUI-buildeltérés esetén a következő megnyitáskor egyszeri frissítés történik, periodikus verziófigyelés nélkül.

## Ellenőrzött állapot

A teljes lite4 fordítás, a célzott memória-/erőforráskezelési hosttesztek,8 MP3 PCM-egyezési próba,40 WebUI-eset,17 verzióőr-eset és18 tényleges telepítőlogikát vizsgáló mock eset PASS. A csomag és a kicsomagolt fájlok hashét visszaellenőriztem.

**Az R174 készülékes hang-, BT- és memóriaellenőrzése még NOT_RUN.** Ez tesztelhető béta, nem igazolt stabil kiadás. A korábbi R173-on az MP3 memóriahiánya és a BT indulási hibája fizikailag reprodukálódott; ezek javításjelöltjét adom ki itt.

A500 ms-os hangvédelem,64 KiB codec-DRAM,4 KiB EXEC és az eredeti RAM/BT/pinout/partícióhatárok megmaradtak. Max5 FPS; a kis VU-régió100 ms. MP3/WAV alap, AAC-LC/FLAC erőforrásfüggő. Wi-Fi→DAC, SD→DAC, BT-IN→DAC és SD→BT-OUT útvonalak; WEB→BT és BLE nincs ebben a profilban. Az infra kikapcsolva marad.

SHA-256: [SHA256SUMS.txt](SHA256SUMS.txt). A csomag saját további HASHES.json ellenőrzéseket tartalmaz.

Korábbi béta kiadásokhoz visszatérés csak azonos hardverprofil és ellenőrzött partíció mellett történjen. Az új revíziókat külön mappában tartom meg.
