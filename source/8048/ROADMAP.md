# ESP32-8048S070C – elszigetelt menüpróba

A felhasználó kapacitív 8048S070C panelt nevezett meg. A külön menüteszt engedélyezett; a SMALL rádió és WEB→BT tiltása nem változik. Forrás SYSTEM, reprodukálható feltöltő USER.

1. Panel/pinout és könyvtári RGB konfiguráció összevetése: PASS, hivatalos Arduino_GFX eszközpélda.
2. Önálló S3/16MB/OPI-PSRAM célprojekt: PASS, külön buildkönyvtár.
3. Magyar font és 16 bites színek, feliratméretek: PASS host; valódi színek NOT_RUN.
4. Rádió/hang/BT/beállítások/óra és ébresztő csoportok: PASS, 23 oldal, 104 sor.
5. Szülőmenü, főmenü, görgetés modellhatárai: PASS host; touch NOT_RUN.
6. GT911 0x5D/0x14 reset és polling: célbuild PASS; paneles ellenőrzés NOT_RUN.
7. Statikus menü és változó sor rajzolása: célbuild PASS; villanás/érintés NOT_RUN.
8. S3 firmware célbuild: PASS, hang/SD/Wi-Fi nincs inicializálva.
9. Teljes telepítő, chip/flash/partíció/hash/timeout/élő CR százalék: host PASS, feltöltés NOT_RUN.
10. USER újraépítés SYSTEM-ből és csomagleltár: PASS a kiadási jelentésben.
11. Következő: felhasználói feltöltés a panel saját USB-portjáról, GT911/PSRAM státusz.
12. Kézi mind az 5 főcsoport, EQ hosszú listája, ébresztő napok, szülő/főmenü és söprés próbája.
13. 2_MENU_SELFTEST.bat futtatása, RETURN_TO_EVA eredmény visszaadása; csak render-bejárásra adható PASS.
14. Képek/koordináták alapján esetleges orientáció és időzítés javítása.
15. Valódi rádiófunkció csak új, külön jóváhagyott portolási lépés után; a demoértékek nem jelentenek működő EQ/BT/hangot.

R2 feltöltőjavítás: a valós COM3 panel 16MB fizikai flash mellett gyári 4MB partíciót tartalmazott. R1 túl szigorú kapuja emiatt nem írt. R2 a pontosan azonosított gyári táblát megtartja, a 3MB apphelybe ellenőrzötten belefér a menü. Írás előtti 64KB mentés, COM-onként kizáró lock, 13 hostteszt PASS. Ismeretlen tábla továbbra tiltott. Feltöltés és fizikai megjelenítés NOT_RUN.

R3: R2 valós feltöltése után ROM reset-loop, ets_loader.c78, QIO. Gyári és build bootloader header2 DIO; installer --flash-mode qio átírta0 QIO-ra. Esptool merge-bin offline reprodukció PASS: QIO=0, keep=2. R3 keep mód/frekvencia és explicit DIO bootloaderkapu; 15 hostteszt/build PASS, fizikai boot/kép NOT_RUN. Következő a javított telepítő felhasználói futtatása.


## R4 teljes navigációs demó
55 oldal, 287 sor, 9 felületválasztás. Eredeti content.h változatlan, tiszta fejlécfüggőségek, backendolvasások demóstub. LovyanGFX1.2.33 Suntonprofil, működő GT911 megtartva. PWM96/255 alap, brightness10–100% ténylegesen állítható. Static RGB565 háttér2x, Service/Óra RAM-demó. Ez nem működő rádióport.
1. R3 valós boot/PSRAM/GT911 PASS; idle kép villogott a felhasználó szerint.
2. R3 touch a felhasználó szerint működik; teljes koordinátaaudit nincs.
3. Új LCD-profil célbuild PASS, fizikai villogás NOT_RUN.
4. 55 oldal teljes gráfja/eredeti címkék host PASS.
5. Visszaút history-stack, scroll build PASS, fizikai NOT_RUN.
6. 9 felület és pontos RGB565 másolat host PASS.
7. Service/Óra demó build PASS, hang/ébresztés nem aktív.
8. Fényerő hardvervezérlés build PASS; táp ok nem bizonyított.
9. Feltöltő/policy regresszió17 hostteszt PASS.
10. Célbuild/csomag PASS; 55 oldalas selftest parser frissítve.
11. Következő: felhasználói R4 feltöltés, idle villogás38% mellett.
12. Főmenü és mind a csoportok, EQ/DNS/ébresztő hosszú listák.
13. 9 skin, swipe, vissza és brightness fizikai próba.
14. 2_MENU_SELFTEST.bat, 287 sor és RETURN_TO_EVA; csak render-PASS.
15. Valódi S3 rádiófunkció csak későbbi külön portolásban.
