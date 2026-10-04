# I-BT-Radio skins és hardverdokumentáció

**English:** [README.en.md](README.en.md)

Ez a nyilvános tár **képernyőskineket és hardverdokumentációt** tartalmaz. Firmware, telepítő, állomáslista, hozzáférési adat és befejezetlen rádióforrás nincs benne. A V156 menütérkép a működésről látott és még nem teljesen ellenőrzött állapotokat mutatja; nem teljes stabilitási tanúsítvány.

Skinverzió: **0.1.1-preview** · [Változások](CHANGELOG.md). A Winamp ihlette képek a kijelzőn jóváhagyott V120-as látványt követik. Ez grafikai előnézet; a Bluetooth-hang hosszú távú stabilitási próbája még folyamatban van.

## Preview images

Winamp ihlette lejátszó és EQ:

![Winamp-inspired player and EQ](skins/classic-winamp-320x240/player_eq.png)

Lejátszási lista:

![Winamp-inspired playlist](skins/classic-winamp-320x240/playlist.png)

Kazettatest; az állomás neve és borítóképe működés közben kerül rá:

![Translucent cassette skin](skins/cassette-320x213/cassette.png)

## Files

- [Részletes Yellow Board hardverleírás](hardware/README.md): panelváltozatok, foglalt GPIO-k, PSRAM-típusok és beépítés előtti ellenőrzések.
- [English readme](README.en.md): angol nyelvű, önállóan követhető hardver- és bekötési útmutató.
- [Winamp ihlette skin](skins/classic-winamp-320x240/README.md): 320 × 240 lejátszó/EQ és lista képek, RGB565LE adatok, koordináták.
- [Kazettaskin](skins/cassette-320x213/README.md): 320 × 213-as kép, RGB565LE adat és érintési zónák.

## Hardver és bekötés

Az alábbi kiosztás az ESP32 Small/Yellow Board 320 × 240-es változatához és a V156 firmware GPIO-beállításaihoz készült. A saját összeállításomban PCM5102A külső DAC-ot, HW-040 rotaryt és ESP-PSRAM64H memóriát használok.

![Eredeti ESP32-WROOM-32 modul pinout](hardware/ESP32-WROOM-32-Pinout-ORIGINAL.png)

Ez az ESP32-WROOM-32 modul lábkiosztása; a Yellow Board csatlakozóit a következő kép mutatja.

### Yellow Board

![Yellow Board referencia és csatlakozófeliratok](hardware/hardware_media/YELLOW_BOARD_USER.png)

**Fontos módosítás:** a GPIO4, GPIO16 és GPIO17 használata előtt az alaplapi RGB LED-et el kell távolítani. A saját rádiómban ezt már elvégeztem. A beépített hang-erősítőt nem használom; a GPIO26-ra kötött erősítőbemenet esetleges elektromos terhelését a saját panelrevízión külön ellenőrizni kell.

### PCM5102A külső DAC

![PCM5102A modul elöl- és hátoldala](hardware/hardware_media/PCM5102A_FRONT_BACK_USER.png)

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

### ESP-PSRAM64H, 8 MB

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

A Winamp-stílusú EQ-görbe **csak látványelem**; Bluetooth módban nincs hangszínszabályzás. A képek egyedi 320 × 240-es TFT-megjelenítőben használhatók; önmagukban nem telepítik a kezelőfelületet a gyári [yoRadio](https://github.com/e2002/yoradio) rendszerbe. A yoRadio-hoz illesztőréteg külön készül.

Az I-BT-Radio a yoRadio alapjaira épül. Köszönet a yoRadio készítőinek. Ezek nem hivatalos közösségi skinek; a Winamp és a yoRadio nevei a saját tulajdonosaikhoz tartoznak.
