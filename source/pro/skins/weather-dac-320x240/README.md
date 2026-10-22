# Időjárás · Fotó — 320×240

V166-R130 SMALL. A rádió Felület menüjében választható, menüből vagy engedélyezett touch-lapozással. Rádió→DAC mellett a korábban lekért adatot mutatja. WEB→BT tiltva marad.

Az elfogadott WEATHER_DAC_PANEL_PREVIEW elrendezését követi: óra balra, külső időjárás jobbra, középen páratartalom/szél/nyomás és tárolt beltéri adat; alul adó, számcím, BT-választás, előző/lejátszás/következő, hangerő. A player.png a tényleges C++ rajzoló hostképe, nem fizikai fotó. A fotóháttér Budapest illusztrációja, más város választásakor nem a város fényképére vált.

Napszak: négy fotó (hajnal/nappal/alkony/éjszaka), UTC + helykoordináta szerinti NOAA-közelítés. A nap helyzete szemléltető képernyőpálya, nem azimutműszer. A háttér legfeljebb ötpercenként frissül. Időszinkron nélkül éjszakai háttér és --:--, nem kitalált idő. A fotó RLE/palettás RGB565 flash-adat; a rajzolási cache kizárólag PSRAM. Nincs belső RAM-fallback. A változatlan képernyő hosttesztben nulla képküldés.

WebUI → Időjárás · Fotó skin: szolgáltató, városkeresés és találatválasztás, kézi koordináta/helynév, 15/30/60 perc, opcionális OpenWeather kulcs. Alapállapot KI. A városkeresés külön gombnyomásra a böngészőn fut. Nincs gépelésenkénti rádiólekérdezés. A rádió órájának Magyarország/CET-CEST időzónáját a városválasztás nem módosítja.

Open-Meteo kulcs nélkül; OpenWeather saját API-kulccsal, HTTPS és tanúsítványellenőrzéssel. Kulcs csak POST-törzsben, nem URL-ben a helyi WebUI felé, nem GET-válaszban és nem localStorage-ban. A szolgáltató által megkövetelt kulcs a rádióból kifelé titkosított HTTPS-kérésben szerepel. Üres mező a korábbi kulcsot megtartja; külön kapcsoló törli. NVS-mentés 15 s nyugalmi főképernyő után és visszaolvasással; queued nem tartós mentési PASS. GET állapot kézzel.

Időjárás hálózati frissítés csak STOP, nyugalmi főképernyő, megfelelő belső RAM és szabad háttérkeret esetén. Hangindítás letiltja/megszakítja. Átmeneti hiba után a beállított ritka periódusban próbálkozik újra. Egy óránál régebbi időjárás nem friss adat. A BLE ettől külön, csak nyugalmi óra- vagy időjárás-felületen működik, minden hang- és memóriavétóval.

Az ébresztőóra külön felület: alapból Bent/Kint piktogramos értékek, egyetlen beállított érzékelő középen; interakcióra Menü/Ébresztő ugyanott. Kor és hiba csak az ébresztett kezelőnézetben. Négy külön betűcsalád (Sans, Serif, Orbit, keskeny), külön óraszín és szövegszín. Az alsó ébresztőikon pixeladata és pozíciója változatlan.

TESZT_10PERC.bat: párhuzamos menübejárás a hangút-próbák mellett, kapcsolati újrakeresés, hibás kör után folytatás; belső RAM/largest/PSRAM trend. Sikertelen/kimaradt funkció nem PASS. BLE-vétel, hang hallgatása, touch/rotary/infra és négy font valós kijelzője külön fizikai ellenőrzést igényel. Fizikai R130 kijelző/API/hang: NOT_RUN.

Források: https://open-meteo.com/en/docs ; https://open-meteo.com/en/docs/geocoding-api ; https://openweathermap.org/api/current ; https://gml.noaa.gov/grad/solcalc/solareqns.PDF . Ellenőrzés: 2026-10-16. Weather data by Open-Meteo / OpenWeather. A szolgáltatói felhasználási feltételek érvényesek.

A fotókat és a transzparens felhőt a beépített image_gen készítette. A generálási leírás és az eredeti képek itt találhatók; a végső szövegek és gombok kódból készülnek. RGB565 konvertálás 32 bites csatornákból, paletta- és RLE-visszaellenőrzéssel. Fontlicencek: Bitstream Vera, SIL OFL; a mellékelt fájlokban. Saját kulcs/cím nem szerepel az anyagban.

R130: külön színes hőmérő/vízcsepp, korfelirat nélkül; hosszú UTF8-város és számcím hárompontos rövidítése; összetartozó hőfok/°C; statikus csapadék- és borultsághangulat a PSRAM-háttérben. Az eredeti fotók nem változtak. A browser.png ugyanennek a C++rajzolónak a képe. Változatlan képernyőn nulla képküldés hoston; a csapadék nem plusz folyamatos animáció.
