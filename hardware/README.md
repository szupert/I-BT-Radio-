# I-BT-Radio R10 – Yellow Board hardver és bekötés

A bekötési példa az általam használt PCM5102A DAC-hoz, HW-040 rotaryhoz és ESP-PSRAM64H memóriához készült. A képek az alkatrészek és a csatlakozófeliratok azonosítását segítik; a GPIO-hozzárendelést az alattuk lévő táblázatok mutatják. A Yellow Board revíziója eltérhet, ezért a saját panelen a lábakat és a forrasztási pontokat külön ellenőrizni kell.

## Eredeti ESP32-WROOM-32 pinout

![Eredeti, változatlan ESP32-WROOM-32 pinout](ESP32-WROOM-32-Pinout-ORIGINAL.png)

Az eredeti, változatlan PNG-t a [Last Minute Engineers ESP32-WROOM-32 pinout oldaláról](https://lastminuteengineers.com/esp32-wroom-32-pinout-reference/) ([közvetlen képfájl](https://lastminuteengineers.com/wp-content/uploads/iot/ESP32-WROOM-32-Pinout.png)) töltöttük le. A [helyi kép](ESP32-WROOM-32-Pinout-ORIGINAL.png) 728 × 567 pixeles, SHA256: `c15a9c579854a1923d9e9f8fd7b0db0412832a557cf94e7520b564461e441259`. Ez az **ESP32-WROOM-32 modul** lábkiosztása, nem a teljes Yellow Board külső csatlakozóinak elhelyezkedése. A konkrét DAC/rotary/PSRAM jeleket az alábbi táblázatok adják meg.

A jelenlegi felület külön, piros/zöld állapotú térképe: [MENU_MAP_V156.jpg](MENU_MAP_V156.jpg).

Állapot: 2026-10-05. Ez a **jelenlegi ESP32-as, 320 × 240-es Small/Yellow Board** firmware-kiosztása, a V156 forrás és a V026 hardver-checkpoint alapján. Nem ESP32-S3 vagy 800 × 480-as lap kiosztása. A GPIO szám az ESP32 jele; a panel csatlakozóinak fizikai sorrendje panelrevíziótól függ, ezért a csatlakozón a feliratot/kapcsolási rajzot is ellenőrizni kell. A 16 MB flash a PSRAM-tól független alkatrész.

## Yellow Board referenciafotó és ütközések

![Yellow Board referenciafotó csatlakozó- és GPIO-feliratokkal](hardware_media/YELLOW_BOARD_USER.png)

A fotó és az [ESP32-2432S028 pinleírás](https://github.com/khanhj/ESP32_2432S028/blob/main/PINS.md) az alábbi, **fizikailag már más áramkörre is kötött** jeleket mutatja. A firmware-ben szabadnak jelölni egy GPIO-t nem kapcsolja le a panelen lévő alkatrészt.

**Kötelező hardvermódosítás ennél a kiosztásnál: az alaplapi RGB LED-et el kell távolítani, mielőtt a GPIO4/16/17 jeleket a rotary gomb, a PSRAM és a külső DAC használja. A saját rádiómban ezt a módosítást már elvégeztem.** Az eltávolítást áramtalanított lapon, a konkrét panelrevízió alapján kell elvégezni; a három LED-ág leválasztását mérni kell. A firmware-ben az RGB funkció kikapcsolása önmagában nem helyettesíti a fizikai eltávolítást.

| GPIO | Beépített kapcsolat ezen a panelcsaládon | Jelenlegi rádiós felhasználás | Mit kell ellenőrizni |
|---:|---|---|---|
| 26 | belső hang-erősítő bemenete gyárilag | rotary `CLK` | a belső erősítőt **nem használom**; az esetleg megmaradt fizikai bemeneti terhelést ellenőrizni kell |
| 4 | RGB LED piros ága gyárilag | rotary `SW` | **RGB LED eltávolítva legyen**; gombjel ellenőrzése |
| 16 | RGB LED zöld ága gyárilag | PSRAM `CE#` | **RGB LED eltávolítva legyen**; CE# felhúzás és jel ellenőrzése |
| 17 | RGB LED kék ága gyárilag | PCM5102A `DIN` | **RGB LED eltávolítva legyen**; DAC-adatjel ellenőrzése |
| 21 | TFT háttérfény | továbbra is háttérfény | nem szabad újra kiosztani |

A képen az IO1 csatlakozó `GND / IO35 / IO22 / IO21`, az IO2 csatlakozó `GND / IO22 / IO27 / 3,3 V` jelű. A GPIO26, GPIO4, GPIO16 és GPIO17 **nem mind ezeken a külső csatlakozókon van**; a tényleges átalakítás forrasztási pontjai panelrevíziótól függenek. Az [ESP3D CYD PSRAM-mod leírása](https://esp3d.io/esp3d-tft/version_1x/hardware/esp32/sunton-28-2432/) is RGB-LED-eltávolítást ír le, de ott **GPIO17-et használ PSRAM órának**. Ezt a változatot a jelenlegi rádióval nem szabad összekeverni: itt a firmware PSRAM órája **GPIO6**, a GPIO17 pedig **DAC-adat**. A saját rádiómban az RGB LED-eket eltávolítottam, és a belső erősítőt nem használom. A GPIO26-hoz kötött eredeti erősítőbemenet fizikai leválasztását nem ellenőriztem, ezért ez az útmutató nem tekinthető minden Yellow Board-revízión ellenőrzött forrasztási receptnek.

## Külső I²S DAC: PCM5102A

![Az általam használt PCM5102A modul elöl és hátul](hardware_media/PCM5102A_FRONT_BACK_USER.png)
A hátoldali fotó eredetileg 180°-kal elfordítva szerepelt. Az alábbi részletet a helyes tájolásba forgattam, és a piros keretekkel megjelöltem az átkötéseket. A számok a H1L–H4L beállításokat jelölik.

![A DAC hátoldali átkötései helyes tájolással, vastag piros jelöléssel](hardware_media/PCM5102A_BACK_JUMPERS_ANNOTATED.png)

| Jelölés | PCM5102A funkció | Ajánlott szint | Az annotált képen |
|---|---|---:|---|
| H1L | FLT — digitális szűrő | L | középső + jobb pad |
| H2L | DEMP — de-emphasis | L | középső + jobb pad |
| H3L | XSMT — soft mute | H | bal + középső pad |
| H4L | FMT — adatkeret | L / I²S | középső + jobb pad |

Ezek az értékek a képen látható modulváltozatra és a rádió I²S kimenetére vonatkoznak. `L` = GND, `H` = 3,3 V. Áramtalaníts forrasztás előtt. Ha a DAC most szól, ne forraszd át; előbb hasonlítsd össze a meglévő hidakat. A modulváltozatok eltérhetnek. Hivatkozás: [PCM5102A adatlap](https://www.ti.com/lit/ds/symlink/pcm5100a.pdf) · [modul leírása](https://manuals.plus/asin/B09C5QX228).

A fotón látható lila modul bemeneti oldalán felülről lefelé `SCK`, `BCK`, `DIN`, `LCK`, `GND`, `VIN` szerepel. A jobb oldali jack a vonalszintű analóg kimenet. A hátoldal képe a forrasztási és konfigurációs pontok azonosításához segítség; a táblázat a firmware-hez szükséges vezetékeket adja meg.

| Yellow Board / ESP32 | PCM5102A modul felirata | Funkció |
|---|---|---|
| GPIO22 | `BCK` vagy `BCLK` | bitóra |
| GPIO17 | `DIN` vagy `DATA` | digitális hangadat |
| GPIO27 | `LCK`, `LRCK` vagy `WSEL` | bal/jobb csatorna óra |
| nincs bekötve | `SCK` / `MCLK` | ezen a firmware-úton nincs külső master clock |
| GND | `GND` | közös föld |
| korábban használt, a **konkrét modulon ellenőrzött** táp | `VIN` | modul tápja |

Az `I2S_INTERNAL=false`, tehát a hang a külső I²S DAC-ra megy. Az `SCK/MCLK` nincs kijelölve ebben a firmware-ben; olyan PCM5102A modult használj, amelynek ez nem kötelező külső bemenet. A `VIN` feszültségét a modul jelölése vagy adatlapja alapján ellenőrizd: a régi hardver-checkpoint csak „existing VIN”-t rögzített, **nem igazolt egységes 3,3 vagy 5 V-os bekötést**. A PCM5102A kimenete vonalszintű sztereó hang, passzív hangszórót nem hajt közvetlenül.

## Rotary enkóder

![Az általam használt HW-040 rotary modul fém tengellyel, kupakkal és ötpontos csatlakozóval](hardware_media/ROTARY_HW040_USER.png)

A fotón `HW-040` jelű fekete panel, fém enkóderház és ötpontos csatlakozó látható; a kupak külön van lefényképezve. A vezetékeket a saját modul **nyomtatott `CLK`, `DT`, `SW`, `+`, `GND` feliratához** kösd, ne a fényképen becsült bal–jobb sorrendhez.

| Yellow Board / ESP32 | Rotary modul felirata | Funkció |
|---|---|---|
| GPIO26 | `CLK` / `A` | forgási A jel |
| GPIO35 | `DT` / `B` | forgási B jel |
| GPIO4 | `SW` | benyomás |
| 3,3 V | `+`, `VCC` | modul tápja, ha a modul ezt igényli |
| GND | `GND` | közös föld |

A firmware a GPIO26 és GPIO4 bemeneten belső felhúzást használ. **GPIO35 csak bemenet és nincs benne belső felhúzás**: a DT ágon a rotary modul tényleges 3,3 V-os felhúzása szükséges. Ha a modulon nincs, köss **10 kΩ ellenállást a DT/GPIO35 és a 3,3 V közé**, ahogy a képen szerepel. Ha a modulon már van felhúzó, nincs szükség külön ellenállásra. 5 V-os jelszintet ne adj az ESP32 GPIO-kra. A gombot induláskor ne tartsd nyomva; előbb ellenőrizd a lap bootolását. A korábbi feljegyzés szerinti sorrend: `CLK26 / DT35 / SW4 / 3,3 V / GND`.

## 8 MB PSRAM – az aktuális firmware-jelek

![ESP PSRAM64H lapkafelirat, az 1-es lábat jelölő ponttal](hardware_media/ESP_PSRAM64H_USER.jpg)

Az alkatrész tokján `ESP PSRAM64H` felirat látható. Az Espressif pontos típusszáma **ESP-PSRAM64H**: 64 Mbit (8 MB), SOP-8-150 mil tokozás, 3,3 V. A tokon lévő pont jelöli az **1-es láb oldalát**; a lábak számozását a következő rajz és a gyártói adatlap alapján ellenőrizd.

![ESP-PSRAM64H SOP-8 bekötési rajz és CE# felhúzó](hardware_media/PSRAM64H_PINOUT.svg)

A Small16 build `BOARD_HAS_PSRAM=1`, Quad PSRAM, 40 MHz, automatikus chiptípus-felismerés és **DIO flash mód** beállítást használ. A D0WD-konfigurációban a **PSRAM CS# = GPIO16**, az órajele pedig **a flash közös `SD_CLK` jele / GPIO6**. Ezért a GPIO16 nem szabad általános I/O, a GPIO17 viszont továbbra is a DAC `DIN`. A flash/PSRAM közös busz GPIO6–GPIO10 vezetékei nem általános kivezetések; az SD-kártya ettől külön SPI-n működik. A GPIO9/10 PSRAM-adatvonalainak használata ehhez a DIO flash-konfigurációhoz tartozik; más flash módra váltáskor a bekötést újra kell ellenőrizni.

### PSRAM-típusok

| Pontos típusjel | Kapacitás | Táp | Ehhez a 8 MB-os, 3,3 V-os rajzhoz |
|---|---:|---:|---|
| `ESP-PSRAM32` | 4 MB | 1,8 V | **Nem**; más tápellátás és külön ellenőrzött bekötés kell |
| `ESP-PSRAM32H` | 4 MB | 3,3 V | Csak új, a konkrét tokhoz és 4 MB-hoz ellenőrzött tervvel |
| `ESP-PSRAM64` | 8 MB | 1,8 V | **Nem**; a 3,3 V károsíthatja |
| `ESP-PSRAM64H` | 8 MB | 3,3 V | **Ez a rajzon és az alábbi táblázatban szereplő típus** |

### Helyettesítő chipek

| Típus | Kapacitás / táp | Illeszkedés a jelenlegi kiosztáshoz |
|---|---|---|
| `ESP-PSRAM64H` | 8 MB / 3,3 V | Azonos típus; közvetlen csere, ha a tok is SOP-8-150 mil |
| `LY68L6400SLI` vagy `LY68L6400SLIT` | 8 MB / 3,3 V | Támogatott alternatíva; 8-pin 150 mil SOP változatot válassz |
| `ESP-PSRAM32H` | 4 MB / 3,3 V | Kapacitáscsökkentett alternatíva; a tokozást ellenőrizd |
| `ESP-PSRAM64` vagy `ESP-PSRAM32` | 1,8 V | **Nem közvetlen csere** a jelenlegi 3,3 V-os flash/PSRAM tápon |

Az ESP-IDF 5.5.5 ESP32 konfigurációja kifejezetten felsorolja a `LY68L6400` típust az ESP-PSRAM64 mellett. Ez szoftveres támogatás, nem bizonyíték arra, hogy az adott chipet a konkrét panelen már kipróbáltuk. A Lyontek `LY68L6400SLI` és `SLIT` rendelési változatainak adatlap szerinti tokozása 8-pin, 150 mil SOP; a `BAIT` változat DFN, ezért nem a SOP-8 foglalatba való. Forrás: [Lyontek LY68L6400 adatlap](https://www.lyontek.com.tw/pdf/ddr/LY68L6400-1.2.pdf), [ESP-IDF 5.5.5 PSRAM-beállítás](https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_psram/esp32/Kconfig.spiram).

Az `H` betű itt a 3,3 V-os változatot különbözteti meg; a `32` és `64` **megabit**, nem megabájt. A típust a chip feliratáról és adatlapjáról kell azonosítani. Az automatikus felismerés nem oldja meg a hibás tápfeszültséget vagy a fizikai bekötést. A hagyományos ESP32 egyidejűleg legfeljebb 4 MB külső RAM-ot tud a szokásos adatcímtartományba leképezni; egy 8 MB-os chip további része csak külön HiMem-kezeléssel használható. A [típusok és feszültségek gyártói listája](https://developer.espressif.com/blog/2024/12/zephyr-how-to-use-psram/), valamint az [ESP32 külső RAM útmutatója](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/external-ram.html) alapján ez a felsorolás az ESP32-höz releváns SPI/QSPI családot mutatja; más PSRAM családok, különösen az ESP32-S3 Octal/OPI típusai, nem a jelenlegi lap 1:1 cseredarabjai.

Az alábbi **csak ESP-PSRAM64H, 3,3 V-os, SOP-8 chiphez** tartozó jelbekötés. Más feliratú vagy 1,8 V-os chipre nem alkalmazható automatikusan. A PSRAM-chip 1-es lábát a tok jelölése szerint kell azonosítani, nem a fotó vagy a panel állása alapján.

| ESP-PSRAM64H SOP-8 láb | Jel | Csatlakozás a jelenlegi D0WD-kiosztásban |
|---:|---|---|
| 1 | `CE#` / `CS#` | ESP32 GPIO16, **külön chip-select**, 10 kΩ felhúzó a PSRAM 3,3 V-os tápjára |
| 2 | `SO / SIO1` | flash közös `SD_DATA0` / GPIO7 |
| 3 | `SIO2` | flash közös `SD_DATA3` / GPIO10 |
| 4 | `VSS` | GND |
| 5 | `SI / SIO0` | flash közös `SD_DATA1` / GPIO8 |
| 6 | `SCLK` | flash közös `SD_CLK` / GPIO6 |
| 7 | `SIO3` | flash közös `SD_DATA2` / GPIO9 |
| 8 | `VCC` | az adott flash/PSRAM buszhoz illő **3,3 V** |

A két adatvonal (`SIO2`, `SIO3`) sorrendje könnyen felcserélhető; a fenti táblázatot az Espressif ESP32 pin-to-pin táblázatából és a PSRAM64H lábkiosztásából vezettük le. A külön PSRAM `CS#` és a flash `CS#` **nem köthető össze**. A PSRAM tápját és a flash tápját feszültség szempontjából egyeztetni kell. A 3,3 V-os `ESP-PSRAM64H` és az 1,8 V-os `ESP-PSRAM64` nem felcserélhető. A Yellow Board adott revíziójának flash-chipje, tápja és hozzáférhető forrasztási pontjai **nem igazoltak ebből a forráskódból**; fizikai beépítés előtt ezeket külön meg kell nézni. Forrasztás csak teljesen áramtalanított lapon, a flash-jeleknél rövid vezetékekkel.

**PSRAM-felhúzó:** a chip 1-es `CE#` lába és a GPIO16 közös jeléről egy **10 kΩ ellenállás menjen a PSRAM 8-as `VCC` lábának 3,3 V-os tápjára**. Ez párhuzamos felhúzó, nem a GPIO16–CE# vezetékbe sorosan beépített ellenállás. Az [Espressif ESP32 datasheet](https://documentation.espressif.com/esp32_datasheet_en.html) kifejezetten előírja a GPIO16 felhúzását PSRAM CE# esetén; a [hardvertervezési útmutató](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/schematic-checklist.html) tipikus értéke 10 kΩ. Ha a konkrét panelen ez már be van építve, ne duplikáld ellenőrzés nélkül.

## Más, már foglalt jelek

| Funkció | GPIO-k |
|---|---|
| TFT | CS15, DC2, SCK14, MISO12, MOSI13; háttérfény21 |
| Érintés | SCK25, MISO39, MOSI32, CS33 |
| SD-kártya | SCK18, MISO19, MOSI23, CS5 |
| Fényérzékelő | ADC34 |

Az IR bemenethez a jelenlegi firmware nem jelöl ki új fizikai GPIO-t; a fenti táblázat nem IR-bekötési javaslat.

## Bekötés utáni ellenőrzés

1. Bekapcsolás előtt ellenőrizd a GND-t, a tápfeszültséget, a PSRAM pin-1 tájolását és azt, hogy a flash és a PSRAM `CS#` nem rövidzárt.
2. A rádió indulási naplójában az `ESP.getPsramSize()` és a `[PSRAM] found/total/free` sor mutassa a felismert külső memóriát. A PSRAM stresszteszt csak álló lejátszásnál fusson. A fizikai 8 MB és a processzor által egyszerre címezhető memóriatartomány nem azonos fogalom.
3. Rotary ellenőrzés: mindkét irány, rövid nyomás, hosszú nyomás. Ha DT lebeg vagy a lépések kimaradnak, előbb a GPIO35 felhúzását ellenőrizd.
4. DAC ellenőrzés: ismert állomás jack/vonalszintű kimeneten, mindkét csatorna, majd hangerő. A BT-hang minősége ettől külön teszt.

Forrás a projektben: `outputs/V156_METADATA_BUDGET_FIX/mirror/yoRadio/src/boards/board_cyd28.h`, `src/boards/hal.cpp`, `mirror/sdkconfig.small16`, valamint `work/repo/CHECKPOINTS/V026_BUILD_PASS_20260924.md`.

Gyártói alapok: [ESP-PSRAM64/64H adatlap (Espressif PDF)](https://www.espressif.com/sites/default/files/documentation/esp-psram64_esp-psram64h_datasheet_en.pdf), [ESP32 hardvertervezési útmutató](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/schematic-checklist.html), [ESP32 chip–flash–PSRAM jeltábla](https://documentation.espressif.com/esp32_datasheet_en.html).
