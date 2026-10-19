## 2026-10-16 — skinláthatóság pontosítása

## V166-R151 BETA · 2026-10-19

- Nyilvános, egyfájlos Windows x64 telepítő a beta/V166_R151 mappában, firmware- és skinforrás nélkül.
- Saját futtatókörnyezet, élő COM/BOOT/százalék, adatmegőrző frissítés; teljes törlés csak IGEN-nel; hiba után kézi R/Q, automatikus stresszteszt nélkül.
- R148–R150 rendezett menü, webes linkjavítás és késleltetett oldal-újrapróba bekerült a kiadásba.
- R151: egy kijelölt webfeldolgozó, hangfeladat szinkron webhívásainak eltávolítása, közös válaszpufferek csökkentése, önmagára váró válaszsor javítása. Az 500 ms-os hangvédelem és a többi biztonsági határ változatlan.
- Build/host és önálló EXE-ellenőrzés PASS; R151 készülékes feltöltés, hang, hosszú stabilitás NOT_RUN. R150 korábbi fizikai eredménye külön kezelendő.


Minden skin előnézeti képe megjelenik. A Winamp és Service nyilvános marad. A többi kép alatt: „A tulajdonos kérésére a forráskód privát.” A privát RGB565/layout/forrásadatokat és vegyes forráscsomagokat nem teszem vissza.

## 2026-10-16 — V166-R125 dokumentáció

- Funkciólista: készüléken már kipróbált, még tesztelendő és tervezett részek külön.
- Menü és kezelés frissítve, EQ DSP kikapcsolása és Wi-Fi-hang→BT tiltása egyértelmű.
- Csak Winamp / Classic marad a jelenlegi nyilvános galériában; más skinek privát jelölést kapnak. Korábbi nyilvános verziók a Git-előzményekben megmaradhatnak.
- Vegyes skinletöltések visszavonva a jelenlegi ágból; helyi példány megőrizve. Nincs új nyilvános firmware-kiadás.
- R125 fordítás és hostteszt sikeres, készülékes kontroll még hátravan.

# KAJON mutatóterv — 2026-10-11

- Elfogadott hegyes, alul szélesebb és lágy szélű mutató előnézete a főoldalon.
- Firmware, RGB565 háttér és telepítő változatlan; renderer-beépítés még hátravan.

# V166-R33 BETA — 2026-10-11

- Aktuális alkalmazásfrissítő, SHA256 és hozzá tartozó forráspillanatkép.
- MENU10 FIX3: a rövid mérési ablakban gyakoribb mintavétel; 19-es és 20-as tízperces teszt.
- Magyar főoldal, angol/német melléklet, külön bekötés/kezelés/indulás/mappatérkép.
- Offline feltöltő- és runnerellenőrzés PASS; új fizikai teszt NOT_RUN. A Wi-Fi→BT hangkihagyás továbbra ismert hiba.

## 0.3.0-preview — 2026-10-10

- Media Player: a kilógó fekete eszközmező az ezüst gombokhoz igazított keretet kapott.
- Az inaktív USB tab megtartja az eredeti betűit és keretét; a lapos utólagos szürke téglalap megszűnt.
- V166-R32 fejlesztői renderer szerinti előnézet, ellenőrzött RGB565 adatok. A letölthető firmware továbbra V159; fizikai R32 kijelző-/hangteszt még nincs.

# 0.2.0-preview

- Friss Klasszikus BT-OUT/MENU és Kazetta grafika a V166-R29 forrásból.
- Media Player és Service előnézet; magyar, angol és német főoldal frissítve.
- RGB565 visszaellenőrzés PASS. Firmware-kiadás és fizikai stabilitási állítás nincs ebben a frissítésben.

# Skin asset version history

These files contain display artwork and layout metadata only. They do not include firmware.

## 0.1.3-preview — 2026-10-05

- Finalized the 320 × 240 I-BT-Radio player art using the BT-OUT layout.
- Replaced the small emblem below PL with a centered, subdued orange-and-metal Bluetooth button.
- Updated the player and station-list PNG/RGB565LE assets. Firmware integration and physical screen verification remain pending.

## 0.1.2-preview — 2026-10-05

- Cleaned the player and EQ title bars and regenerated the player image data.
- Kept the display layout and transport controls unchanged.

## 0.1.1-preview — 2026-10-02

- Updated the player, EQ and station-list previews and corrected bitrate/sample-rate field coordinates.
- Added current pictures and a conversion helper to the project description.

## 0.1.0-preview — 2026-10-02

- Initial 320 × 240 player/EQ and station-list assets, plus the 320 × 213 cassette asset.
- Added RGB565LE data, coordinate maps and visible previews.
