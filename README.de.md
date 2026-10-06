# I-BT-Radio — Hardware, Oberflächen und BETA

[Magyar](README.md) · [English](README.en.md)

> **BETA — ausschließlich zum Testen.** Die Software funktioniert nur eingeschränkt. Nicht für den festen Einbau, den unbeaufsichtigten Betrieb oder als zuverlässiges Alltagsgerät empfohlen.

I-BT-Radio ist ein ESP32-Internetradio auf Basis von [yoRadio](https://github.com/e2002/yoradio). Vielen Dank an dessen Entwickler. Das Projekt verwendet GPL-3.0-lizenzierten Quellcode.

## Installation und Teststatus

Das derzeit veröffentlichte Paket ist [V159 mit One-Click-Starter](beta/V159_HTTPS_STREAM/I-BT-Radio_V159_HTTPS_STREAM_BETA.zip). Dazu gehören [Prüfsumme](beta/V159_HTTPS_STREAM/I-BT-Radio_V159_HTTPS_STREAM_BETA.zip.sha256), [Status und Anleitung](beta/V159_HTTPS_STREAM/README.md) sowie [Quellmaterial und Patch](beta/V159_HTTPS_STREAM/V159_SOURCE_GPL3.zip).

ZIP entpacken und den enthaltenen Installer starten. Python 3.9 oder neuer wird benötigt. Der Installer prüft das Gerät und die vorhandene **16-MB-Partitionierung** und aktualisiert nur die Anwendung. Nicht auf eine beliebige 4-MB-Platine übertragen. Portauswahl und tatsächlicher Schreibfortschritt erscheinen im Fenster. Der Menütest verwendet Windows PowerShell.

V159 wurde gestartet und gab Radio über den DAC aus. Der Menütest wurde mit `start=DENIED` abgewiesen; das ist kein bestandener Test. HTTPS und langfristig unterbrechungsfreier Bluetooth-Betrieb sind nicht vollständig bestätigt. Der HTTPS-Client verschlüsselt, prüft derzeit aber das Serverzertifikat nicht. Der veröffentlichte Quellstand enthält eine Basis und einen Patch; ein eigenständig reproduzierbarer V159-Build ist noch nicht bestätigt.

## Oberflächen

![Klassischer Player](skins/classic-player-320x240/player_eq.png)
![Senderliste](skins/classic-player-320x240/playlist.png)
![Kassette](skins/cassette-320x213/cassette.png)

Die EQ-Kurve ist eine Anzeige, kein aktiver Bluetooth-Equalizer. Die Bilder allein installieren keine Oberfläche in unverändertem yoRadio.

In der lokalen V164-Testfassung werden die Oberflächen unter **Einstellungen → Anzeige → Oberfläche** zusammengefasst: Einfach, Kassette, Klassisch und Media Player. Die Auswahl ersetzt getrennte Ein/Aus-Schalter. Diese Menüänderung gehört noch nicht zum oben verlinkten V159-Paket.

Die Media-Player-Oberfläche ist für 320 × 240 Pixel im Querformat ausgelegt. Sie nutzt die bestehenden Aktionen für Radio/SD, Wiedergabe/Stopp, vorherigen/nächsten Titel, Liste, Lautstärke, BT OUT und Menü. USB bleibt ohne passende Hardware inaktiv. Ein neutrales Musiksymbol ersetzt vorerst ein echtes Cover. Keine Spektrumanimation; bei Internet-Bluetooth bleibt die reduzierte Anzeige erhalten. Die gemeldete blaue Farbdarstellung wird noch untersucht. Noch keine Freigabe als geprüfte neue Firmware.

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

![Menüstatus V156](hardware/MENU_MAP_V156.jpg)

Grün kennzeichnet vorhandene Funktionsrückmeldungen. Rot kennzeichnet deaktivierte, fehlerhafte oder noch zu prüfende Funktionen. Die historische V156-Karte ist kein vollständiger Stabilitätsnachweis und bildet das neue Oberflächenmenü noch nicht ab.

## Fehler melden

[GitHub Issues](https://github.com/szupert/I-BT-Radio-/issues): Firmwareversion, Platine, Quelle (Internet/SD), Ausgang (DAC/BT), Bluetooth-Gerät, genaue Schritte und Zeitpunkt des Fehlers angeben. WLAN-Passwörter, SSIDs und andere persönliche Daten vor dem Hochladen aus Protokollen entfernen. Weitere Details: [Hardwaredokumentation](hardware/README.md).

## Unterstützung

Die Unterstützung ist freiwillig. Beiträge können für ESP-Boards, Messgeräte und Entwicklungswerkzeuge sowie für die weitere Projektentwicklung verwendet werden.

- **Ethereum (ETH):** `0xf044db3277a1b880713e81d66b6E507F65450EE6`
- **Bitcoin (BTC):** `3KLgXnbCs1WCD3igiVErxdy2CAH7QWchLs`
