# I-BT-Radio · LIGHT R471

Internetrádió, SD-lejátszó, ébresztőóra és relaxációs programok a klasszikus ESP32-WROOM-os, 320 × 240-es érintőkijelzős rádióhoz. A LIGHT kiadás 4 MB flashsel, PSRAM nélkül fut.

**[R471 tesztkiadás letöltése](https://github.com/szupert/I-BT-Radio-/releases/tag/V166-R471-LIGHT)** · [Telepítés](docs/releases/R471-LIGHT.md) · [Funkciók](docs/Features.md) · [Hardver és bekötés](hardware/README.md)

A nyilvános ZIP meglévő, azonos partíciókiosztású R470 LIGHT rádió frissítésére szolgál. A PRO és a BIG/P4 saját firmware-t használ.

## Rádió és borítóletöltés

A hálózati borítóletöltés leállított webrádiónál indulhat. Wi-Fi → DAC lejátszáskor az MP3- és AAC-puffer elkérheti a letöltő felszabadult, legfeljebb 7 KiB-os munkaterületét. A puffer telítettségét és újratöltéseit a **Beállítások → Hang → Hangpuffer** oldalon követheted.

## Menürendszer állapottérképe

[![Az R471 öt főmenücsempéje, a Beállítások tíz csoportja és a borítóletöltő elérési útja](hardware/MENU_MAP.jpg)](docs/menu/README.md)

[Menüpontok és kezelési útvonalak szövegesen](docs/menu/README.md)

## Kijelző és programválasztás

| Főmenü | Relaxáció | Álló programlista |
|---|---|---|
| ![LIGHT főmenü](docs/images/light-r471/main.png) | ![Hang, Fény és Kombinált](docs/images/light-r471/modes.png) | <img src="docs/images/light-r471/list.png" height="180" alt="Programlista álló kijelzőn"> |

A három előnézetet a LIGHT felületrajzolójával készítettem.

<details><summary>Óranézet, időjárási hátterek és a Borítókép és VU menü</summary>

![Borítókép és VU](docs/images/light-r418/menu-artwork-vu.png)

Óranézet és érintésvezérlés · látványtervek

<img src="docs/images/light-r418/clock-layout-concept.png" width="320" alt="Óranézet"> <img src="docs/images/light-r418/clock-touch-concept.png" width="320" alt="Óravezérlés">

Időjárási hátterek · napos, felhős, esős, havas és fagyos idő

<img src="docs/images/light-r418/weather/SUN.jpg" width="150" alt="Napos"> <img src="docs/images/light-r418/weather/CLOUD.jpg" width="150" alt="Felhős"> <img src="docs/images/light-r418/weather/RAIN.jpg" width="150" alt="Esős"> <img src="docs/images/light-r418/weather/SNOW.jpg" width="150" alt="Havas"> <img src="docs/images/light-r418/weather/FROST.jpg" width="150" alt="Fagyos"> 

</details>

## További változatok

[BIG P4 / JC1060](beta/BIG_P4_JC1060_DEV1/README.md) · [Korábbi LIGHT és PRO kiadások, skingaléria és panelbekötések](KORABBI_KIADASOK_ES_SKINEK.md) · [Forrás](source/README.md) · [Licencek](THIRD_PARTY_NOTICES.md)

## Támogatás

Ha szívesen támogatnád a projektet, itt megteheted. A támogatásból új ESP-paneleket, az általatok javasolt más paneleket és fejlesztőeszközöket veszek, hogy többféle hardveren is elérhetővé és jobbá tehessem a rádiót. Ha van ilyen paneled, vagy szívesen küldenél egyet, vedd fel velem a kapcsolatot.

- **Ethereum (ETH):** `0xf044db3277a1b880713e81d66b6E507F65450EE6`
- **Bitcoin (BTC):** `3KLgXnbCs1WCD3igiVErxdy2CAH7QWchLs`
- **PayPal:** `LT253250028810624563`
