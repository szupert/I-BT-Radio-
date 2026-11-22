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

## Skinek

A galériában a projekt különböző kiadásainak képernyői láthatók. A választható skinek panelenként és kiadásonként eltérnek.

Grafikai előnézetek

A galériában 13 megmaradó felület előnézetét mutatom, köztük a Videoton lejátszót és bootképet, a Service és az új BT-Speaker képernyőt. A képek középre rendezve, 260–320 pixel szélességgel jelennek meg. Az Alapnézet / Basic player képeit kérésre eltávolítottam. A látványterveket, statikus háttereket és host-rajzokat külön jelölöm.

A korábbi előnézetek mellett most a teljes alkalmazásforrás is elérhető a source könyvtárban.

<h3 align="center">Winamp / Classic</h3>

<table align="center">
<tr><th align="center">Betöltő</th><th align="center">Asztal · 2 mp</th><th align="center">Lejátszó</th></tr>
<tr><td align="center"><a href="skins/classic-player-320x240/boot-xp-r161.jpg"><img src="skins/classic-player-320x240/boot-xp-r161.jpg" width="260" alt="Winamp / Classic — Betöltő" /></a></td><td align="center"><a href="skins/classic-player-320x240/desktop-xp-r161.png"><img src="skins/classic-player-320x240/desktop-xp-r161.png" width="260" alt="Winamp / Classic — Asztal · 2 mp" /></a></td><td align="center"><a href="skins/classic-player-320x240/player_eq.png"><img src="skins/classic-player-320x240/player_eq.png" width="260" alt="Winamp / Classic — Lejátszó" /></a></td></tr>
</table>

<p align="center"><em>Korábbi grafikai előnézet.</em></p>



<h3 align="center">Media Player</h3>

<table align="center">
<tr><th align="center">Betöltő</th><th align="center">Asztal · 2 mp</th><th align="center">Lejátszó</th></tr>
<tr><td align="center"><a href="skins/media-player-320x240/boot-win7-r161.jpg"><img src="skins/media-player-320x240/boot-win7-r161.jpg" width="260" alt="Media Player — Betöltő" /></a></td><td align="center"><a href="skins/media-player-320x240/desktop-win7-r161.png"><img src="skins/media-player-320x240/desktop-win7-r161.png" width="260" alt="Media Player — Asztal · 2 mp" /></a></td><td align="center"><a href="skins/media-player-320x240/player.png"><img src="skins/media-player-320x240/player.png" width="260" alt="Media Player — Lejátszó" /></a></td></tr>
</table>

<p align="center"><em>Korábbi grafikai előnézet.<br>Forráskód: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skinforrások</a>.</em></p>



<h3 align="center">Cassette</h3>

<p align="center"><img src="skins/cassette-320x213/cassette.png" width="320" alt="Cassette" /></p>

<p align="center"><em>Statikus háttér; a feliratok lejátszáskor kerülnek rá.<br>Forráskód: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skinforrások</a>.</em></p>

<h3 align="center">Kajon-Jon-bt-vu</h3>

<p align="center"><img src="skins/kajon-vu-320x240/player_tapered_preview.png" width="320" alt="Kajon-Jon-bt-vu" /></p>

<p align="center"><em>Korábbi mutatós elrendezési terv.</em></p>

<h3 align="center">Service</h3>

<p align="center"><img src="skins/service-320x240/concept.png" width="320" alt="Service" /></p>

<p align="center"><em>R146 Service látványterv, példaértékekkel; nem készülékfotó vagy kalibrált műszer.</em></p>

<h3 align="center">Classic Wood</h3>

<p align="center"><img src="skins/classic-wood-320x240/background.png" width="320" alt="Classic Wood" /></p>

<p align="center"><em>Statikus háttér; élő feliratok nélkül.<br>Forráskód: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skinforrások</a>.</em></p>

<h3 align="center">Vintage HiFi</h3>

<p align="center"><img src="skins/vintage-hifi-320x240/background.png" width="320" alt="Vintage HiFi" /></p>

<p align="center"><em>Statikus háttér; élő feliratok nélkül.<br>Forráskód: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skinforrások</a>.</em></p>

<h3 align="center">Retro Neon</h3>

<p align="center"><img src="skins/retro-neon-320x240/background.png" width="320" alt="Retro Neon" /></p>

<p align="center"><em>Statikus háttér; ez a külön vintage téma.<br>Forráskód: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skinforrások</a>.</em></p>

<h3 align="center">Neon</h3>

<p align="center"><img src="skins/neon-320x240/player.png" width="320" alt="Neon" /></p>

<p align="center"><em>Az R137 rajzolókódjával készült előnézet, példaadatokkal és szemléltető spektrummal.<br>Forráskód: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skinforrások</a>.</em></p>

<h3 align="center">Ébresztőóra / Alarm clock</h3>

<p align="center"><img src="skins/alarm-clock-320x240/player.png" width="320" alt="Ébresztőóra / Alarm clock" /></p>

<p align="center"><em>R124 óralap-előnézet példaadatokkal, bekapcsolt fényképes háttérrel.<br>Forráskód: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skinforrások</a>.</em></p>

<h3 align="center">Időjárás / Weather</h3>

<p align="center"><img src="skins/weather-dac-320x240/player.png" width="320" alt="Időjárás / Weather" /></p>

<p align="center"><em>R130 éjszakai előnézet példaadatokkal; nem élő időjárásmérés.<br>Forráskód: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skinforrások</a>.</em></p>

<h3 align="center">Bluetooth IN</h3>

<p align="center"><img src="skins/bluetooth-in-320x240/concept.png" width="320" alt="Bluetooth IN" /></p>

<p align="center"><em>R146 BT-Speaker látványterv, példaadatokkal; nem készülékfotó.</em></p>

A Wi-Fi-hang → BT továbbra is tiltott, az EQ hangfeldolgozás kikapcsolt. A Winamp EQ-görbéje látványelem. A galéria nem új nyilvános firmware-kiadás.


<h3 align="center">Videoton RT 6303 S</h3>

<table align="center">
<tr><th align="center">Betöltő</th><th align="center">Lejátszó</th></tr>
<tr><td align="center"><a href="skins/videoton-rt6303s-320x240/boot.png"><img src="skins/videoton-rt6303s-320x240/boot.png" width="320" alt="Videoton RT 6303 S — Betöltő" /></a></td><td align="center"><a href="skins/videoton-rt6303s-320x240/player.png"><img src="skins/videoton-rt6303s-320x240/player.png" width="320" alt="Videoton RT 6303 S — Lejátszó" /></a></td></tr>
</table>

<p align="center"><em>Az elfogadott eredeti Videoton-felületet tartottam meg, felirat → LED → üres gomb sorrenddel és RT 6303 S típusfelirattal. Az R143-ban a bootképből eltűntek a zöld szövegmaradványok, a betöltősáv tisztán rajzolódik. A BT gomb menü nélkül IN módra váltott, a telefonról folyamatosan szólt a hang. A 36 oldalas, 255 menüsort érintő rövid rajzolási próbában nem tért vissza a veremtúlcsordulás. Az előnézeten példaadatok látszanak; az előnézet nem teljes firmware-minősítés.<br>Forráskód: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skinforrások</a>.</em></p>

## További változatok

[BIG P4 / JC1060](beta/BIG_P4_JC1060_DEV1/README.md) · [Korábbi LIGHT és PRO kiadások, skingaléria és panelbekötések](KORABBI_KIADASOK_ES_SKINEK.md) · [Forrás](source/README.md) · [Licencek](THIRD_PARTY_NOTICES.md)

## Támogatás

Ha szívesen támogatnád a projektet, itt megteheted. A támogatásból új ESP-paneleket, az általatok javasolt más paneleket és fejlesztőeszközöket veszek, hogy többféle hardveren is elérhetővé és jobbá tehessem a rádiót. Ha van ilyen paneled, vagy szívesen küldenél egyet, vedd fel velem a kapcsolatot.

- **Ethereum (ETH):** `0xf044db3277a1b880713e81d66b6E507F65450EE6`
- **Bitcoin (BTC):** `3KLgXnbCs1WCD3igiVErxdy2CAH7QWchLs`
- **PayPal:** `LT253250028810624563`
