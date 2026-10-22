# Source / Installer · 2026-10-22

Die vollständigen Light-/Pro-Anwendungsquellen und vier Hardwareprofile sind öffentlich. Die Windows-Einklick-Installer bleiben erhalten. R2114MB/ohnePSRAM ist eine Diagnoseversion (BT/BLE/SD/WebUI AUS); die Bluetooth-Wiederherstellung folgt als eigener Arbeitsblock. Entwicklungsbeta, keine Stabilitätszusage.

[Source](source/README.md) · [Installer](beta/README.md) · [Contribution blocks](docs/DEVELOPMENT_BLOCKS.md)

---

# I-BT-Radio — V166-R151 BETA · 19. Oktober 2026

## Öffentliche Skin-Quellcodes · 2026.10.20.

Ich habe den Skin-Quellcode mit Erlaubnis des Eigentümers veröffentlicht: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">Skin-Quellcode</a>.


## Aktuelle Windows-Beta · R163 · 19.10.2026

**[Eigenständigen Installer herunterladen](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/16MB_PSRAM/V166_R163/I-BT-Radio-R163-Install.exe)** · [Anleitung und Prüfsummen](beta/16MB_PSRAM/V166_R163/README.md)

Nur für die dokumentierte SMALL-Konfiguration mit ESP32-WROOM, PSRAM, 16 MB Flash, 320×240-Display und externem DAC. Laufzeitumgebung enthalten, kein separates Python nötig. Keine Firmware-/Skinquellen, ELF/MAP-Dateien oder persönlichen Einstellungen. Passende Partitionen ermöglichen ein Update mit Erhalt der Einstellungen; vollständiges Löschen nur nach ausdrücklichem IGEN. Kein automatischer Stresstest.

**Test-Beta mit bekannten Fehlern, keine stabile Version:** Wi-Fi → DAC stottert hörbar mit und ohne Menüdurchlauf. Beim Menüdurchlauf leerte sich der Streampuffer und die Verbindung wurde neu aufgebaut. Das Flackern der Windows-7-Bootanimation ist ebenfalls offen.

Den Watchdog-Pfad der Senderindexierung habe ich korrigiert: 480 Zeilen in 814 ms am Gerät, ohne neuen Watchdog oder Speicherfehler während der kurzen Beobachtung. Der 500-ms-Audioschutz und alle Sicherheitsgrenzen bleiben erhalten. Die Langzeitstabilität von R163 ist nicht geprüft. Reproduzierbare Fehler bitte unter [Issues](https://github.com/szupert/I-BT-Radio-/issues) melden.

---


[Magyar](README.md) · [English](README.en.md)

Az eredetre és licencekre vonatkozó információ: [licencleírás](https://github.com/szupert/I-BT-Radio-/blob/main/THIRD_PARTY_NOTICES.md).

## Frühere Windows-Beta · R151

**[Eigenständigen Installer herunterladen](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/16MB_PSRAM/V166_R151/I-BT-Radio-R151-Install.exe)** · [Anleitung und Prüfsummen](beta/16MB_PSRAM/V166_R151/README.md)

Ich habe Laufzeitumgebung und Firmware in einer EXE gebündelt; eine separate Python-Installation ist nicht nötig. Die Beta ist ausschließlich für den dokumentierten SMALL ESP32-WROOM mit PSRAM, 16 MB Flash, 320×240-Display und externem DAC vorgesehen. Normale Updates erhalten eine passende Partitionierung. Vollständiges Löschen erfordert die ausdrückliche Bestätigung IGEN. Es startet kein automatischer Stresstest.

Das gruppierte Menü und die korrigierten WebUI-Links sind enthalten. R150 absolvierte einen zehnminütigen Gerätetest mit 257/257 Menüzeilen und zweimal 34/34 WebUI-Seiten. In R151 habe ich synchrone WebUI-Arbeit aus dem Audiotask entfernt und das Warten der Antwortwarteschlange auf sich selbst beseitigt. Audio- und Speichergrenzen bleiben unverändert.

**Experimentelle Beta:** R151 wurde erfolgreich kompiliert und auf dem PC geprüft. Das tatsächliche Flashen, der Ton und die Langzeitstabilität von R151 sind noch nicht am Gerät geprüft. Frühere Messungen sind kein Nachweis für diese Revision. Firmware- und Skinquellen, persönliche Einstellungen sowie ELF/MAP-Dateien sind nicht enthalten. Fehler bitte unter [Issues](https://github.com/szupert/I-BT-Radio-/issues) melden.

Frühere Decoder-Messungen (R136–R137):

Im R136-DAC-Test derselben 15 Beispieldateien erreichte die PCM-Erzeugung 98,44–99,60% der Echtzeit. Opus belastete Core1 noch mit 94,1%. In R137 habe ich die überflüssige Einzelkonvertierung bei der PCM16-Übergabe entfernt. Build und Hosttests sind erfolgreich; im R137-Test derselben 15 Dateien lag Opus Core1 bei 87,5%, die PCM-Erzeugung bei 99,40%. Pro Datei liegt ein sauberes CPU-Fenster von etwa 9 Sekunden vor; das belegt weder Dauerlast noch hörbare Qualität. ALAC24 erreichte 91,3% Core1 und 97,33% PCM und bleibt zu untersuchen. Die Sicherheitsgrenzen bleiben unverändert.

Radio → DAC, SD → DAC und zuvor Telefon → BT-IN → DAC funktionierten am Gerät. SD → BT lief auch mit JBL; Pufferaussetzer und hohe Prozessorlast bleiben offen. WLAN-Audio → BT und EQ-Signalverarbeitung bleiben abgeschaltet. Wecker, BLE und Wetter brauchen weitere Tests. Die Touch-Menüdemo auf dem 800×480-Panel ESP32-8048S070C lief; sie ist noch kein vollständiger Radio-Port.

## Skins

Aktualisiert: 19. Oktober 2026 · R161-Startvorschauen

Ich zeige Vorschauen der 13 verbleibenden Oberflächen, darunter Videoton-Player und Bootbild, Service und den neuen BT-Speaker. Die Bilder sind zentriert und 260–320 Pixel breit. Die Vorschau des Basic Players habe ich auf Wunsch entfernt. Entwürfe, statische Hintergründe und Host-Zeichnungen kennzeichne ich getrennt.

In diesem Update veröffentliche ich ausschließlich Vorschaubilder und Beschreibungen, keinen Quellcode und keine Firmware.

<h3 align="center">Winamp / Classic</h3>

<table align="center">
<tr><th align="center">Startbild</th><th align="center">Desktop · 2 s</th><th align="center">Player</th></tr>
<tr><td align="center"><a href="skins/classic-player-320x240/boot-xp-r161.jpg"><img src="skins/classic-player-320x240/boot-xp-r161.jpg" width="260" alt="Winamp / Classic — Startbild" /></a></td><td align="center"><a href="skins/classic-player-320x240/desktop-xp-r161.png"><img src="skins/classic-player-320x240/desktop-xp-r161.png" width="260" alt="Winamp / Classic — Desktop · 2 s" /></a></td><td align="center"><a href="skins/classic-player-320x240/player_eq.png"><img src="skins/classic-player-320x240/player_eq.png" width="260" alt="Winamp / Classic — Player" /></a></td></tr>
</table>

<p align="center"><em>Frühere Layout-Vorschau.</em></p>


<p align="center"><em>Ich zeige den R161-Startablauf nebeneinander. Auf dem Desktop stehen 15 zeittypische Programm- und Spielsymbole, darunter Red Alert 2, Need for Speed und GTA: San Andreas. Dies sind Vorschauen; Bild und Ton von R161 müssen noch am Gerät geprüft werden.</em></p>

<h3 align="center">Media Player</h3>

<table align="center">
<tr><th align="center">Startbild</th><th align="center">Desktop · 2 s</th><th align="center">Player</th></tr>
<tr><td align="center"><a href="skins/media-player-320x240/boot-win7-r161.jpg"><img src="skins/media-player-320x240/boot-win7-r161.jpg" width="260" alt="Media Player — Startbild" /></a></td><td align="center"><a href="skins/media-player-320x240/desktop-win7-r161.png"><img src="skins/media-player-320x240/desktop-win7-r161.png" width="260" alt="Media Player — Desktop · 2 s" /></a></td><td align="center"><a href="skins/media-player-320x240/player.png"><img src="skins/media-player-320x240/player.png" width="260" alt="Media Player — Player" /></a></td></tr>
</table>

<p align="center"><em>Frühere Layout-Vorschau.<br>Ich habe den Skin-Quellcode mit Erlaubnis des Eigentümers veröffentlicht: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">Skin-Quellcode</a>.</em></p>


<p align="center"><em>Ich zeige den R161-Startablauf nebeneinander. Auf dem Desktop stehen 15 zeittypische Programm- und Spielsymbole, darunter Red Alert 2, Need for Speed und GTA: San Andreas. Dies sind Vorschauen; Bild und Ton von R161 müssen noch am Gerät geprüft werden.</em></p>

<h3 align="center">Cassette</h3>

<p align="center"><img src="skins/cassette-320x213/cassette.png" width="320" alt="Cassette" /></p>

<p align="center"><em>Statischer Hintergrund; Metadaten erscheinen bei der Wiedergabe.<br>Ich habe den Skin-Quellcode mit Erlaubnis des Eigentümers veröffentlicht: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">Skin-Quellcode</a>.</em></p>

<h3 align="center">Kajon-Jon-bt-vu</h3>

<p align="center"><img src="skins/kajon-vu-320x240/player_tapered_preview.png" width="320" alt="Kajon-Jon-bt-vu" /></p>

<p align="center"><em>Frühere Layout-Vorschau mit Zeigeranzeigen.</em></p>

<h3 align="center">Service</h3>

<p align="center"><img src="skins/service-320x240/concept.png" width="320" alt="Service" /></p>

<p align="center"><em>R146-Service-Entwurf mit Beispielwerten; kein Gerätefoto und kein kalibriertes Messgerät.</em></p>

<h3 align="center">Classic Wood</h3>

<p align="center"><img src="skins/classic-wood-320x240/background.png" width="320" alt="Classic Wood" /></p>

<p align="center"><em>Statischer Hintergrund ohne dynamische Beschriftung.<br>Ich habe den Skin-Quellcode mit Erlaubnis des Eigentümers veröffentlicht: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">Skin-Quellcode</a>.</em></p>

<h3 align="center">Vintage HiFi</h3>

<p align="center"><img src="skins/vintage-hifi-320x240/background.png" width="320" alt="Vintage HiFi" /></p>

<p align="center"><em>Statischer Hintergrund ohne dynamische Beschriftung.<br>Ich habe den Skin-Quellcode mit Erlaubnis des Eigentümers veröffentlicht: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">Skin-Quellcode</a>.</em></p>

<h3 align="center">Retro Neon</h3>

<p align="center"><img src="skins/retro-neon-320x240/background.png" width="320" alt="Retro Neon" /></p>

<p align="center"><em>Statischer Hintergrund; das separate Vintage-Thema.<br>Ich habe den Skin-Quellcode mit Erlaubnis des Eigentümers veröffentlicht: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">Skin-Quellcode</a>.</em></p>

<h3 align="center">Neon</h3>

<p align="center"><img src="skins/neon-320x240/player.png" width="320" alt="Neon" /></p>

<p align="center"><em>Vorschau mit R137-Zeichencode, Beispieldaten und illustrativem Spektrum.<br>Ich habe den Skin-Quellcode mit Erlaubnis des Eigentümers veröffentlicht: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">Skin-Quellcode</a>.</em></p>

<h3 align="center">Ébresztőóra / Alarm clock</h3>

<p align="center"><img src="skins/alarm-clock-320x240/player.png" width="320" alt="Ébresztőóra / Alarm clock" /></p>

<p align="center"><em>R124-Uhrvorschau mit Beispieldaten und aktiviertem Fotohintergrund.<br>Ich habe den Skin-Quellcode mit Erlaubnis des Eigentümers veröffentlicht: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">Skin-Quellcode</a>.</em></p>

<h3 align="center">Időjárás / Weather</h3>

<p align="center"><img src="skins/weather-dac-320x240/player.png" width="320" alt="Időjárás / Weather" /></p>

<p align="center"><em>R130-Nachtvorschau mit Beispieldaten; keine aktuelle Wettermessung.<br>Ich habe den Skin-Quellcode mit Erlaubnis des Eigentümers veröffentlicht: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">Skin-Quellcode</a>.</em></p>

<h3 align="center">Bluetooth IN</h3>

<p align="center"><img src="skins/bluetooth-in-320x240/concept.png" width="320" alt="Bluetooth IN" /></p>

<p align="center"><em>R146-BT-Speaker-Entwurf mit Beispieldaten; kein Gerätefoto.</em></p>

WLAN-Audio → BT bleibt gesperrt, die EQ-Signalverarbeitung abgeschaltet. Die Winamp-EQ-Kurve ist dekorativ. Dieses Galerie-Update ist keine neue öffentliche Firmware-Veröffentlichung.


<h3 align="center">Videoton RT 6303 S</h3>

<table align="center">
<tr><th align="center">Startbild</th><th align="center">Player</th></tr>
<tr><td align="center"><a href="skins/videoton-rt6303s-320x240/boot.png"><img src="skins/videoton-rt6303s-320x240/boot.png" width="320" alt="Videoton RT 6303 S — Startbild" /></a></td><td align="center"><a href="skins/videoton-rt6303s-320x240/player.png"><img src="skins/videoton-rt6303s-320x240/player.png" width="320" alt="Videoton RT 6303 S — Player" /></a></td></tr>
</table>

<p align="center"><em>Ich habe das freigegebene originale Videoton-Layout mit der Reihenfolge Beschriftung → LED → unbeschriftete Taste und dem Schriftzug RT 6303 S beibehalten. R143 entfernt die grünen Textreste im Bootbild und zeichnet den Ladebalken sauber. Die BT-Taste wechselte ohne Menü direkt in den IN-Modus; die Audiowiedergabe vom Telefon lief durchgehend. Im kurzen Zeichentest mit 36 Menüseiten und 255 Zeilen trat kein weiterer Stacküberlauf auf. Die Vorschau zeigt Beispieldaten; die vollständige Prüfung der übrigen Funktionen läuft weiter.<br>Ich habe den Skin-Quellcode mit Erlaubnis des Eigentümers veröffentlicht: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">Skin-Quellcode</a>.</em></p>





## Hardware und Anschlüsse

Die folgende Belegung entspricht der dokumentierten Small/Yellow-Board-Ausführung mit 320 × 240 Pixeln. Platinenrevision vor dem Löten vergleichen.

![ESP32-WROOM-32 Pinout](hardware/ESP32-WROOM-32-Pinout-ORIGINAL.png)
![Yellow Board](hardware/hardware_media/YELLOW_BOARD_USER.png)

**Die eingebaute RGB-LED muss vor der Nutzung von GPIO4, GPIO16 und GPIO17 entfernt werden.** Der interne Lautsprecherverstärker wird nicht verwendet. Seine mögliche elektrische Belastung an GPIO26 ist bei der jeweiligen Platinenrevision zu prüfen.

### PCM5102A DAC

![DAC Vorder- und Rückseite](hardware/hardware_media/PCM5102A_FRONT_BACK_USER.png)
![Lötbrücken, korrekt ausgerichtet](hardware/hardware_media/PCM5102A_BACK_JUMPERS_ANNOTATED.png)

Die Rückseitenaufnahme wurde um 180° gedreht. I²S-Brücken: **H1L=L, H2L=L, H3L=H, H4L=L**. Ein bereits funktionierendes Modul nicht ohne Prüfung umlöten.

| ESP32 | DAC |
|---|---|
| GPIO22 | BCK / BCLK |
| GPIO17 | DIN / DATA |
| GPIO27 | LCK / LRCK / WSEL |
| GND | GND |
| gemäß konkretem Modul | VIN |
| nicht angeschlossen | SCK / MCLK |

VIN-Spannung am konkreten Modul prüfen. Der DAC liefert ein Line-Signal und treibt keine passiven Lautsprecher direkt.

### HW-040 Drehgeber

![HW-040](hardware/hardware_media/ROTARY_HW040_USER.png)

| ESP32 | Drehgeber |
|---|---|
| GPIO26 | CLK / A |
| GPIO35 | DT / B |
| GPIO4 | SW |
| 3,3 V | + / VCC |
| GND | GND |

GPIO35 hat keinen internen Pull-up. Falls das Modul keinen enthält, **10 kΩ zwischen DT/GPIO35 und 3,3 V** einsetzen. Keine 5 V an die GPIOs legen.

### ESP-PSRAM64H

**ESP-PSRAM64H: 64 Mbit / 8 MB, 3,3 V, SOP-8-150 mil.**

![Speicherchip](hardware/hardware_media/ESP_PSRAM64H_USER.jpg)
![Pinbelegung mit Pull-up](hardware/hardware_media/PSRAM64H_PINOUT.svg)

| Pin | Signal | Anschluss |
|---:|---|---|
| 1 | CE# / CS# | GPIO16, 10 kΩ nach 3,3 V |
| 2 | SIO1 | GPIO7 |
| 3 | SIO2 | GPIO10 |
| 4 | VSS | GND |
| 5 | SIO0 | GPIO8 |
| 6 | SCLK | GPIO6 |
| 7 | SIO3 | GPIO9 |
| 8 | VCC | 3,3 V |

GPIO6–10 sind gemeinsam genutzte Flash-/PSRAM-Bussignale, keine freien GPIOs. PSRAM-CS an GPIO16 ist vom Flash-CS getrennt. Beim klassischen ESP32 sind gewöhnlich höchstens 4 MB direkt abgebildet; der übrige Speicher benötigt HiMem-Unterstützung.

| Typ | Kapazität | Spannung |
|---|---:|---:|
| ESP-PSRAM32 | 4 MB | 1,8 V |
| ESP-PSRAM32H | 4 MB | 3,3 V |
| ESP-PSRAM64 | 8 MB | 1,8 V |
| ESP-PSRAM64H | 8 MB | 3,3 V |

Als Alternativen sind ESP-PSRAM64H oder LY68L6400SLI(T) mit passendem 3,3-V-SOP-8-Gehäuse dokumentiert; ESP-PSRAM32H hat nur 4 MB. Die 1,8-V-Typen sind kein direkter Ersatz. [Lyontek-Datenblatt](https://www.lyontek.com.tw/pdf/ddr/LY68L6400-1.2.pdf) · [ESP-IDF-Unterstützung](https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_psram/esp32/Kconfig.spiram).

### Weitere belegte Pins

| Funktion | GPIO |
|---|---|
| TFT | CS15, DC2, SCK14, MISO12, MOSI13, Hintergrundlicht21 |
| Touch | SCK25, MISO39, MOSI32, CS33, IRQ36 |
| SD | SCK18, MISO19, MOSI23, CS5 |
| Lichtsensor | GPIO34 |

<p align="center"><a href="hardware/MENU_MAP.jpg"><img src="hardware/MENU_MAP.jpg" width="900" alt="I-BT-Radio menu map 2026-10-19; next firmware menu preparation"></a></p>

Grün kennzeichnet vorhandene Funktionsrückmeldungen. Rot kennzeichnet deaktivierte, fehlerhafte oder noch zu prüfende Funktionen. Die historische V156-Karte ist kein vollständiger Stabilitätsnachweis und bildet das neue Oberflächenmenü noch nicht ab.

## Fehler melden

[GitHub Issues](https://github.com/szupert/I-BT-Radio-/issues): Firmwareversion, Platine, Quelle (Internet/SD), Ausgang (DAC/BT), Bluetooth-Gerät, genaue Schritte und Zeitpunkt des Fehlers angeben. WLAN-Passwörter, SSIDs und andere persönliche Daten vor dem Hochladen aus Protokollen entfernen. Weitere Details: [Hardwaredokumentation](hardware/README.md).

## Unterstützung

Wenn du das Projekt unterstützen möchtest, kannst du das hier tun. Von den Spenden kaufe ich neue ESP-Boards, weitere von euch vorgeschlagene Boards und Entwicklungswerkzeuge, damit ich das Radio auf mehr Hardware nutzbar machen und weiter verbessern kann. Wenn du selbst ein passendes Board hast und es mir zur Verfügung stellen oder zuschicken möchtest, nimm bitte Kontakt mit mir auf.

- **Ethereum (ETH):** `0xf044db3277a1b880713e81d66b6E507F65450EE6`
- **Bitcoin (BTC):** `3KLgXnbCs1WCD3igiVErxdy2CAH7QWchLs`
- **PayPal:** `LT253250028810624563`

[Menüprüfung und bekannte Fehler (Ungarisch)](docs/menu/README.md)
