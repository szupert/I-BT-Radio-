# I-BT-Radio — V166-R146 BETA · 2026. október 19.

[English](README.en.md) · [Deutsch](README.de.md)

Az I-BT-Radio a saját, yoRadio-alapú ESP32-WROOM + PSRAM rádióprojektem. A SMALL változaton dolgozom.

**[Funkciólista](docs/Features.md) · [Menü és kezelés](docs/menu/README.md) · [Bekötés](hardware/README.md)**

Az R146 új Service és BT-Speaker látványtervét és a 320×240-es rajzoló előnézetét tettem a galériába. Az előadó végén maradó ajánlóelválasztót és a VU terhelési küszöbének mértékegységét javítottam. A fizikai ellenőrzés még folyamatban van; a VU memória- és hangvédelme megmaradt. Az induláskori állomáslista-indexelés watchdog-jelzése továbbra is nyitott. Ez képes dokumentációfrissítés, nem nyilvános firmware-kiadás.

Az Alapnézet / Basic player galériabejegyzését eltávolítottam. A skin és a régi szürke Bluetooth-tartaléknézet firmware-ből való kivételén, illetve a húzásból keletkező téves playlist-nyitás javításán az R147-ben dolgozom; a készüléken ezek még nem ellenőrzött változtatások.

Korábbi dekóderméréseim (R136–R137):

Az R136-tal ugyanazon 15 mintafájl DAC-próbájában 98,44–99,60%-os PCM-előállítási ütemet mértem a valós időhöz képest. Az Opus Core1-terhelése még 94,1% volt. Az R137-ben a kész 16 bites PCM átadásából vettem ki a fölösleges mintánkénti átalakítást; a fordítás és a hosttesztek sikeresek. Az azonos 15 fájlos R137 DAC-próbában az Opus Core1 87,5%-ra csökkent, a PCM-előállítás 99,40% maradt. Ez fájlonként körülbelül 9 másodperces tiszta CPU-ablakból származó rövid mérés; hallható minőséget és hosszú stabilitást nem igazol. ALAC24-nél 91,3% Core1 és 97,33% PCM adódott, ezt tovább vizsgálom. A biztonsági határok változatlanok.

A rádió → DAC, SD → DAC és korábban a telefon → BT-IN → DAC már működött a készüléken. Az SD → BT a JBL-lel is működött, de a pufferkihagyások és a nagy terhelés javítása még nyitott. **A Wi-Fi-hang → BT tiltott, az EQ hangfeldolgozás kikapcsolt.** Az óra, az ébresztés, a BLE és az időjárás további ellenőrzést igényel. A 800×480-as ESP32-8048S070C panelen az érintéses menüdemót is elindítottam; ez még nem teljes rádióport.

## Skinek

Frissítve: 2026. október 19. · V166-R146 fejlesztői állapot

A galériában 13 megmaradó felület előnézetét mutatom, köztük a Videoton lejátszót és bootképet, a Service és az új BT-Speaker képernyőt. A képek középre rendezve, 320 pixel szélességgel jelennek meg. Az Alapnézet / Basic player képeit kérésre eltávolítottam. A látványterveket, statikus háttereket és host-rajzokat külön jelölöm.

Ebben a frissítésben kizárólag előnézeti képeket és leírásokat publikálok; forráskódot és firmware-t nem töltök fel.

<h3 align="center">Winamp / Classic</h3>

<p align="center"><img src="skins/classic-player-320x240/player_eq.png" width="320" alt="Winamp / Classic" /></p>

<p align="center"><em>Korábbi grafikai előnézet.</em></p>

<h3 align="center">Media Player</h3>

<p align="center"><img src="skins/media-player-320x240/player.png" width="320" alt="Media Player" /></p>

<p align="center"><em>Korábbi grafikai előnézet.<br>Megrendelői kérésre bizonytalan ideig a forráskódot privátként kezelem. Külön kérésre, a tulajdonos engedélyével publikálom.</em></p>

<h3 align="center">Cassette</h3>

<p align="center"><img src="skins/cassette-320x213/cassette.png" width="320" alt="Cassette" /></p>

<p align="center"><em>Statikus háttér; a feliratok lejátszáskor kerülnek rá.<br>Megrendelői kérésre bizonytalan ideig a forráskódot privátként kezelem. Külön kérésre, a tulajdonos engedélyével publikálom.</em></p>

<h3 align="center">Kajon-Jon-bt-vu</h3>

<p align="center"><img src="skins/kajon-vu-320x240/player_tapered_preview.png" width="320" alt="Kajon-Jon-bt-vu" /></p>

<p align="center"><em>Korábbi mutatós elrendezési terv.</em></p>

<h3 align="center">Service</h3>

<p align="center"><img src="skins/service-320x240/concept.png" width="320" alt="Service" /></p>

<p align="center"><em>R146 Service látványterv, példaértékekkel; nem készülékfotó vagy kalibrált műszer.</em></p>

<h3 align="center">Classic Wood</h3>

<p align="center"><img src="skins/classic-wood-320x240/background.png" width="320" alt="Classic Wood" /></p>

<p align="center"><em>Statikus háttér; élő feliratok nélkül.<br>Megrendelői kérésre bizonytalan ideig a forráskódot privátként kezelem. Külön kérésre, a tulajdonos engedélyével publikálom.</em></p>

<h3 align="center">Vintage HiFi</h3>

<p align="center"><img src="skins/vintage-hifi-320x240/background.png" width="320" alt="Vintage HiFi" /></p>

<p align="center"><em>Statikus háttér; élő feliratok nélkül.<br>Megrendelői kérésre bizonytalan ideig a forráskódot privátként kezelem. Külön kérésre, a tulajdonos engedélyével publikálom.</em></p>

<h3 align="center">Retro Neon</h3>

<p align="center"><img src="skins/retro-neon-320x240/background.png" width="320" alt="Retro Neon" /></p>

<p align="center"><em>Statikus háttér; ez a külön vintage téma.<br>Megrendelői kérésre bizonytalan ideig a forráskódot privátként kezelem. Külön kérésre, a tulajdonos engedélyével publikálom.</em></p>

<h3 align="center">Neon</h3>

<p align="center"><img src="skins/neon-320x240/player.png" width="320" alt="Neon" /></p>

<p align="center"><em>Az R137 rajzolókódjával készült előnézet, példaadatokkal és szemléltető spektrummal.<br>Megrendelői kérésre bizonytalan ideig a forráskódot privátként kezelem. Külön kérésre, a tulajdonos engedélyével publikálom.</em></p>

<h3 align="center">Ébresztőóra / Alarm clock</h3>

<p align="center"><img src="skins/alarm-clock-320x240/player.png" width="320" alt="Ébresztőóra / Alarm clock" /></p>

<p align="center"><em>R124 óralap-előnézet példaadatokkal, bekapcsolt fényképes háttérrel.<br>Megrendelői kérésre bizonytalan ideig a forráskódot privátként kezelem. Külön kérésre, a tulajdonos engedélyével publikálom.</em></p>

<h3 align="center">Időjárás / Weather</h3>

<p align="center"><img src="skins/weather-dac-320x240/player.png" width="320" alt="Időjárás / Weather" /></p>

<p align="center"><em>R130 éjszakai előnézet példaadatokkal; nem élő időjárásmérés.<br>Megrendelői kérésre bizonytalan ideig a forráskódot privátként kezelem. Külön kérésre, a tulajdonos engedélyével publikálom.</em></p>

<h3 align="center">Bluetooth IN</h3>

<p align="center"><img src="skins/bluetooth-in-320x240/concept.png" width="320" alt="Bluetooth IN" /></p>

<p align="center"><em>R146 BT-Speaker látványterv, példaadatokkal; nem készülékfotó.</em></p>

A Wi-Fi-hang → BT továbbra is tiltott, az EQ hangfeldolgozás kikapcsolt. A Winamp EQ-görbéje látványelem. A galéria nem új nyilvános firmware-kiadás.


<h3 align="center">Videoton RT 6303 S</h3>

<p align="center"><img src="skins/videoton-rt6303s-320x240/player.png" width="320" alt="Videoton RT 6303 S" /></p>

<p align="center"><em>Az elfogadott eredeti Videoton-felületet tartottam meg, felirat → LED → üres gomb sorrenddel és RT 6303 S típusfelirattal. Az R143-ban a bootképből eltűntek a zöld szövegmaradványok, a betöltősáv tisztán rajzolódik. A BT gomb menü nélkül IN módra váltott, a telefonról folyamatosan szólt a hang. A 36 oldalas, 255 menüsort érintő rövid rajzolási próbában nem tért vissza a veremtúlcsordulás. Az előnézeten példaadatok látszanak; a többi funkció teljes ellenőrzése még folyamatban van.<br>Megrendelői kérésre bizonytalan ideig a forráskódot privátként kezelem. Külön kérésre, a tulajdonos engedélyével publikálom.</em></p>

<p align="center"><img src="skins/videoton-rt6303s-320x240/boot.png" width="320" alt="Bootképernyő – indulás közbeni előnézet" /></p>

<p align="center"><em>Bootképernyő – indulás közbeni előnézet · 2026-10-18</em></p>

## Hardver és bekötés

Az alábbi kiosztás az ESP32 Small/Yellow Board 320 × 240-es változatához és a V156 firmware GPIO-beállításaihoz készült. A memórialapka tokján `ESP PSRAM64H` olvasható; az Espressif pontos típusszáma **ESP-PSRAM64H**, 64 Mbit (8 MB), SOP-8-150 mil, 3,3 V.

![Eredeti ESP32-WROOM-32 modul pinout](hardware/ESP32-WROOM-32-Pinout-ORIGINAL.png)

Ez az ESP32-WROOM-32 modul lábkiosztása; a Yellow Board csatlakozóit a következő kép mutatja.

### Yellow Board

![Yellow Board referencia és csatlakozófeliratok](hardware/hardware_media/YELLOW_BOARD_USER.png)

**Fontos módosítás:** a GPIO4, GPIO16 és GPIO17 használata előtt az alaplapi RGB LED-et el kell távolítani. A saját rádiómban ezt már elvégeztem. A beépített hang-erősítőt nem használom; a GPIO26-ra kötött erősítőbemenet esetleges elektromos terhelését a saját panelrevízión külön ellenőrizni kell.

### PCM5102A külső DAC

![PCM5102A modul elöl- és hátoldala](hardware/hardware_media/PCM5102A_FRONT_BACK_USER.png)

![A DAC hátoldali átkötései helyes tájolással, vastag piros jelöléssel](hardware/hardware_media/PCM5102A_BACK_JUMPERS_ANNOTATED.png)

A hátoldali részlet 180°-kal el volt forgatva; helyes tájolásra fordítottam. I²S beállítás: **H1L=L, H2L=L, H3L=H, H4L=L**. A keretek a forrasztási átkötések helyét emelik ki. Ha a DAC most szól, ne forraszd át; előbb ellenőrizd a meglévő hidakat.

| ESP32 jel | PCM5102A felirat | Funkció |
|---|---|---|
| GPIO22 | BCK / BCLK | I²S bitóra |
| GPIO17 | DIN / DATA | I²S hangadat |
| GPIO27 | LCK / LRCK / WSEL | bal/jobb csatorna óra |
| GND | GND | közös föld |
| ellenőrzött táp | VIN | a konkrét modul jelölése szerint |
| nincs bekötve | SCK / MCLK | ezen a firmware-úton nincs külső master clock |

A DAC vonalszintű kimenetet ad; passzív hangszórót közvetlenül nem hajt. A `VIN` feszültségét a konkrét modulon ellenőrizd, mert a modulváltozatok eltérhetnek.

### HW-040 rotary

![HW-040 rotary modul](hardware/hardware_media/ROTARY_HW040_USER.png)

| ESP32 jel | Rotary felirat | Funkció |
|---|---|---|
| GPIO26 | CLK / A | forgás A |
| GPIO35 | DT / B | forgás B |
| GPIO4 | SW | nyomógomb |
| 3,3 V | + / VCC | táp, ha a modul igényli |
| GND | GND | közös föld |

GPIO35-ön nincs belső felhúzás. Ha a rotary modulon sincs, tegyél 10 kΩ ellenállást a DT/GPIO35 és 3,3 V közé. A GPIO-ra ne adj 5 V-ot.

### ESP-PSRAM64H — 64 Mbit / 8 MB, SOP-8-150 mil, 3,3 V

![ESP-PSRAM64H memóriachip](hardware/hardware_media/ESP_PSRAM64H_USER.jpg)

![ESP-PSRAM64H lábkiosztás és CE felhúzó](hardware/hardware_media/PSRAM64H_PINOUT.svg)

| PSRAM64H SOP-8 láb | Jel | Bekötés |
|---:|---|---|
| 1 | CE# / CS# | GPIO16; 10 kΩ felhúzó 3,3 V-ra |
| 2 | SO / SIO1 | GPIO7 / flash SD_DATA0 |
| 3 | SIO2 | GPIO10 / flash SD_DATA3 |
| 4 | VSS | GND |
| 5 | SI / SIO0 | GPIO8 / flash SD_DATA1 |
| 6 | SCLK | GPIO6 / közös flashóra |
| 7 | SIO3 | GPIO9 / flash SD_DATA2 |
| 8 | VCC | 3,3 V |

Ez a rajz kizárólag 3,3 V-os **ESP-PSRAM64H** chiphez való. Az ESP-PSRAM64 1,8 V-os, ezért nem helyettesíthető vele. A PSRAM és flash közös GPIO6–10 buszjeleit ne használd általános GPIO-ként; a PSRAM külön CS jele GPIO16, a flash CS jelétől elkülönítve. A hagyományos ESP32 a 8 MB-ból legfeljebb 4 MB-ot képez le közvetlenül a szokásos címtartományba; a fennmaradó részhez külön HiMem-kezelés kell.

| PSRAM típus | Kapacitás | Tápfeszültség |
|---|---:|---:|
| ESP-PSRAM32 | 4 MB | 1,8 V |
| ESP-PSRAM32H | 4 MB | 3,3 V |
| ESP-PSRAM64 | 8 MB | 1,8 V |
| ESP-PSRAM64H | 8 MB | 3,3 V |

**Helyettesítő típusok:** azonos kapacitáshoz `ESP-PSRAM64H` vagy `LY68L6400SLI(T)` (64 Mbit, 3,3 V, SOP-8-150 mil); kisebb, 4 MB-os cseréhez `ESP-PSRAM32H`. A Lyontek chipet az ESP-IDF 5.5.5 külön támogatott típusként is felsorolja. A `H` nélküli 1,8 V-os változatok nem közvetlen cserék ezen a 3,3 V-os lapon. Beszerzéskor egyezzen a tokozás és ellenőrizd a feliratot. [Lyontek adatlap](https://www.lyontek.com.tw/pdf/ddr/LY68L6400-1.2.pdf) · [ESP-IDF 5.5.5 támogatás](https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_psram/esp32/Kconfig.spiram).

### További foglalt GPIO-k

| Funkció | GPIO |
|---|---|
| TFT | CS15, DC2, SCK14, MISO12, MOSI13, háttérfény21 |
| érintőpanel | SCK25, MISO39, MOSI32, CS33, IRQ36 |
| SD-kártya | SCK18, MISO19, MOSI23, CS5 |
| fényérzékelő | GPIO34 |

### Menürendszer állapottérképe

![Menürendszer V156 állapot- és működési térképe](hardware/MENU_MAP_V156.jpg)

A zöld jelölés az adott funkcióval kapcsolatban rendelkezésre álló működési visszajelzést, a piros a kikapcsolt, hibás vagy még teljes fizikai tesztet igénylő részt jelöli. A térkép nem jelent teljes stabilitási igazolást. A GPIO-k, a PSRAM-részletek és a panelrevíziókra vonatkozó ellenőrzések a [részletes hardverleírásban](hardware/README.md) találhatók.

A lejátszó EQ-görbéje **csak látványelem**; Bluetooth módban nincs hangszínszabályzás. A képek egyedi 320 × 240-es TFT-megjelenítőben használhatók; önmagukban nem telepítik a kezelőfelületet a gyári [yoRadio](https://github.com/e2002/yoradio) rendszerbe. A yoRadio-hoz illesztőréteg külön készül.

Az I-BT-Radio a yoRadio alapjaira épül. Köszönet a yoRadio készítőinek.

## Támogatás

Ha szívesen támogatnád a projektet, itt megteheted. A támogatásból új ESP-paneleket, az általatok javasolt más paneleket és fejlesztőeszközöket veszek, hogy többféle hardveren is elérhetővé és jobbá tehessem a rádiót. Ha van ilyen paneled, vagy szívesen küldenél egyet, vedd fel velem a kapcsolatot.

- **Ethereum (ETH):** `0xf044db3277a1b880713e81d66b6E507F65450EE6`
- **Bitcoin (BTC):** `3KLgXnbCs1WCD3igiVErxdy2CAH7QWchLs`
- **PayPal:** `LT253250028810624563`

[Menüellenőrzés és ismert hibák](docs/menu/README.md)
