# Mappatérkép

A dokumentáció a [yoRadio](https://github.com/e2002/yoradio) logikáját követi: hardver, bekötés, függőségek, indulás, frissítés, vezérlés és változások. Saját I-BT-Radio tartalommal.

| GitHub mappa/fájl | Tartalom |
|---|---|
| README.md / README.en.md / README.de.md | főoldal és nyelvek |
| hardware/ | Yellow Board, DAC, rotary, PSRAM, bekötési képek |
| skins/<nézet>/ | előnézet, pixelszám, koordináták, RGB565 adatok |
| beta/V166_R33/ | aktuális telepítő, forrás és SHA256 |
| docs/ | indulás, vezérlés és mappatérkép |
| CHANGELOG.md | kiadástörténet |

Az R33 forrás-ZIP-ben: `yoRadio/src/core` vezérlés, `yoRadio/src/ui` kijelzés, `scripts` build-javítások, `partitions` flashkiosztás, `sdkconfig.small16` célkonfiguráció, `platformio.ini` buildrecept. Az eredeti forrásútvonalak megmaradtak.

Helyi fejlesztés: egy projektgyökérben **SYSTEM + USER**. SYSTEM őrzi a forrást, platformot, tesztsablont, naplót és ellenőrzött kiadást. USER a telepítő/tesztelő használati csomagja. `SYSTEM/UJRAEPITES_USER.bat` visszaépíti az elveszett USER-t; a meglévő RETURN_TO_EVA megmarad. Ez a képesség az I-BT-Radiónál offline ellenőrzött; a régi archivált projektek önálló újraépítése még nincs igazolva.
