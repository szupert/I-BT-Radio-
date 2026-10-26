## Aktuális Light4MB: R270 · tesztelhető béta

[**Egykattintásos Windows telepítő**](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/4MB_PSRAM_NELKUL/R270/I-BT-Radio-Auto4-Install.exe) · [Teljes Light-forrás](https://github.com/szupert/I-BT-Radio-/tree/main/source/light) · [Előképek](https://github.com/szupert/I-BT-Radio-/tree/main/skins/light-320x240) · [Menü](https://github.com/szupert/I-BT-Radio-/tree/main/docs/menu) · [Próba és ismert hibák](https://github.com/szupert/I-BT-Radio-/blob/main/docs/LIGHT_R270_TEST_GUIDE.md)

Választható koppintási mód, egységes állomáslista, villogásmentes boot-részletfrissítés, nagyobb fejléc, MP3-cím és BT-listagörgetés javítása. A jelenlegi béta gépen ellenőrzött, R270 készülékes PASS még nincs. Light16/Pro/8048 telepítők változatlanul elérhetők a profiljuk alatt. A korábbi R211/R216 szövegek és galériák alább **történeti kiadások**, nem a jelenlegi Light képességlistája.

![Light lejátszó](https://raw.githubusercontent.com/szupert/I-BT-Radio-/main/skins/light-320x240/player.png)

---

# Source / Installer · 2026-10-22

Full Light/Pro application sources and four hardware profiles are public. One-click Windows installers remain available. R2114MB/noPSRAM is diagnostic-only (BT/BLE/SD/WebUI OFF); Bluetooth restoration is a separate pending block. This is a development beta, not a stability claim.

[Source](source/README.md) · [Installer](beta/README.md) · [Contribution blocks](docs/DEVELOPMENT_BLOCKS.md)

---

# I-BT-Radio — V166-R151 BETA · 19 October 2026

## Public skin sources · 2026.10.20.

I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.


## Current Windows beta · R163 · 2026-10-19

**[Download the standalone installer](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/16MB_PSRAM/V166_R163/I-BT-Radio-R163-Install.exe)** · [Instructions and checksums](beta/16MB_PSRAM/V166_R163/README.md)

For the documented SMALL ESP32-WROOM + PSRAM, 16 MB flash, 320×240 display and external DAC only. Runtime included; no separate Python required. No firmware/skin sources, ELF/MAP files or personal configuration are bundled. Matching partitions allow an update that preserves settings; full erasure requires explicit IGEN confirmation. No automatic stress test.

**Known-bug testing beta, not a stable release:** Wi-Fi → DAC audibly stutters both with and without menu traversal. The stream buffer emptied and reconnected during menu traversal. Windows 7 boot animation flicker is also unresolved.

I fixed the station-index watchdog path: 480 rows completed in 814 ms on hardware without a new watchdog or memory error in the short observation. The 500 ms audio guard and all safety limits remain unchanged. Long-term R163 stability is unverified. Please report reproducible issues through [Issues](https://github.com/szupert/I-BT-Radio-/issues).

---


[Magyar](README.md) · [Deutsch](README.de.md)

Az eredetre és licencekre vonatkozó információ: [licencleírás](https://github.com/szupert/I-BT-Radio-/blob/main/THIRD_PARTY_NOTICES.md).

## Earlier Windows beta · R151

**[Download the standalone installer](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/16MB_PSRAM/V166_R151/I-BT-Radio-R151-Install.exe)** · [Instructions and checksums](beta/16MB_PSRAM/V166_R151/README.md)

I bundled the runtime and firmware in one EXE; no separate Python installation is needed. This beta is only for the documented SMALL ESP32-WROOM + PSRAM, 16 MB flash, 320×240 display and external DAC configuration. Normal updates preserve a matching partition layout; full erasure requires an explicit IGEN confirmation. No automatic stress test starts.

The grouped menu and corrected WebUI links are included. R150 completed a 10-minute combined hardware test with 257/257 menu rows and two 34/34 WebUI loads. R151 removes synchronous WebUI work from the audio task and prevents the web reply queue from waiting on itself. Audio and memory safeguards remain unchanged.

**Experimental beta:** R151 build and host checks passed; R151 hardware flashing, sound and long-term stability are not yet verified. Earlier measurements are not proof for this revision. Firmware and skin sources, personal settings, ELF and MAP files are not included. Please report reproducible issues through [Issues](https://github.com/szupert/I-BT-Radio-/issues).

Earlier decoder measurements (R136–R137):

In the R136 DAC test of the same 15 sample files, PCM production reached 98.44–99.60% of real time. Opus Core1 load was still 94.1%. In R137 I removed redundant per-sample conversion from the signed PCM16 handoff. Build and host checks passed; the R137 test of the same 15 files measured Opus Core1 at 87.5% and PCM production at 99.40%. Each file has one clean approximately 9-second CPU window, so this does not prove sustained load or audible quality. ALAC24 measured 91.3% Core1 and 97.33% PCM and needs further investigation. Safety limits are unchanged.

Radio → DAC, SD → DAC and earlier phone → BT-IN → DAC playback worked on the device. SD → BT also worked with JBL, but underruns and CPU load remain open. Wi-Fi audio → BT and EQ audio processing remain disabled. Alarm, BLE and weather validation is ongoing. The 800×480 ESP32-8048S070C touch-menu demo worked; it is not a full radio port.

## Skins

Updated: 19 October 2026 · R161 startup previews

I show previews of the 13 remaining interfaces, including the Videoton player and boot image, Service and the new BT-Speaker screen. Images are centered at 260–320 pixels wide. I removed the Basic player previews as requested. Design references, static backgrounds and host renders are labelled separately.

This update publishes preview images and descriptions only. I am not uploading source code or firmware.

<h3 align="center">Winamp / Classic</h3>

<table align="center">
<tr><th align="center">Boot</th><th align="center">Desktop · 2 s</th><th align="center">Player</th></tr>
<tr><td align="center"><a href="skins/classic-player-320x240/boot-xp-r161.jpg"><img src="skins/classic-player-320x240/boot-xp-r161.jpg" width="260" alt="Winamp / Classic — Boot" /></a></td><td align="center"><a href="skins/classic-player-320x240/desktop-xp-r161.png"><img src="skins/classic-player-320x240/desktop-xp-r161.png" width="260" alt="Winamp / Classic — Desktop · 2 s" /></a></td><td align="center"><a href="skins/classic-player-320x240/player_eq.png"><img src="skins/classic-player-320x240/player_eq.png" width="260" alt="Winamp / Classic — Player" /></a></td></tr>
</table>

<p align="center"><em>Earlier layout preview.</em></p>


<p align="center"><em>I show the R161 startup sequence side by side. The desktop contains 15 period application and game icons, including Red Alert 2, Need for Speed and GTA: San Andreas. These are previews; R161 on-device visual and audio checks are still pending.</em></p>

<h3 align="center">Media Player</h3>

<table align="center">
<tr><th align="center">Boot</th><th align="center">Desktop · 2 s</th><th align="center">Player</th></tr>
<tr><td align="center"><a href="skins/media-player-320x240/boot-win7-r161.jpg"><img src="skins/media-player-320x240/boot-win7-r161.jpg" width="260" alt="Media Player — Boot" /></a></td><td align="center"><a href="skins/media-player-320x240/desktop-win7-r161.png"><img src="skins/media-player-320x240/desktop-win7-r161.png" width="260" alt="Media Player — Desktop · 2 s" /></a></td><td align="center"><a href="skins/media-player-320x240/player.png"><img src="skins/media-player-320x240/player.png" width="260" alt="Media Player — Player" /></a></td></tr>
</table>

<p align="center"><em>Earlier layout preview.<br>I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.</em></p>


<p align="center"><em>I show the R161 startup sequence side by side. The desktop contains 15 period application and game icons, including Red Alert 2, Need for Speed and GTA: San Andreas. These are previews; R161 on-device visual and audio checks are still pending.</em></p>

<h3 align="center">Cassette</h3>

<p align="center"><img src="skins/cassette-320x213/cassette.png" width="320" alt="Cassette" /></p>

<p align="center"><em>Static background; metadata is drawn during playback.<br>I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.</em></p>

<h3 align="center">Kajon-Jon-bt-vu</h3>

<p align="center"><img src="skins/kajon-vu-320x240/player_tapered_preview.png" width="320" alt="Kajon-Jon-bt-vu" /></p>

<p align="center"><em>Earlier needle-meter layout preview.</em></p>

<h3 align="center">Service</h3>

<p align="center"><img src="skins/service-320x240/concept.png" width="320" alt="Service" /></p>

<p align="center"><em>R146 Service design reference with example values; not a device photo or calibrated instrument.</em></p>

<h3 align="center">Classic Wood</h3>

<p align="center"><img src="skins/classic-wood-320x240/background.png" width="320" alt="Classic Wood" /></p>

<p align="center"><em>Static background without live labels.<br>I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.</em></p>

<h3 align="center">Vintage HiFi</h3>

<p align="center"><img src="skins/vintage-hifi-320x240/background.png" width="320" alt="Vintage HiFi" /></p>

<p align="center"><em>Static background without live labels.<br>I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.</em></p>

<h3 align="center">Retro Neon</h3>

<p align="center"><img src="skins/retro-neon-320x240/background.png" width="320" alt="Retro Neon" /></p>

<p align="center"><em>Static background; a separate vintage theme.<br>I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.</em></p>

<h3 align="center">Neon</h3>

<p align="center"><img src="skins/neon-320x240/player.png" width="320" alt="Neon" /></p>

<p align="center"><em>Preview rendered with the R137 drawing code, example metadata and illustrative spectrum.<br>I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.</em></p>

<h3 align="center">Ébresztőóra / Alarm clock</h3>

<p align="center"><img src="skins/alarm-clock-320x240/player.png" width="320" alt="Ébresztőóra / Alarm clock" /></p>

<p align="center"><em>R124 clock-face preview with example values and photographic background enabled.<br>I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.</em></p>

<h3 align="center">Időjárás / Weather</h3>

<p align="center"><img src="skins/weather-dac-320x240/player.png" width="320" alt="Időjárás / Weather" /></p>

<p align="center"><em>R130 night preview with example data; not a live weather reading.<br>I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.</em></p>

<h3 align="center">Bluetooth IN</h3>

<p align="center"><img src="skins/bluetooth-in-320x240/concept.png" width="320" alt="Bluetooth IN" /></p>

<p align="center"><em>R146 BT-Speaker design reference with example data; not a device photo.</em></p>

Wi-Fi audio → BT remains disabled, as does EQ audio processing. The Winamp EQ curve is decorative. This gallery update is not a new public firmware release.


<h3 align="center">Videoton RT 6303 S</h3>

<table align="center">
<tr><th align="center">Boot</th><th align="center">Player</th></tr>
<tr><td align="center"><a href="skins/videoton-rt6303s-320x240/boot.png"><img src="skins/videoton-rt6303s-320x240/boot.png" width="320" alt="Videoton RT 6303 S — Boot" /></a></td><td align="center"><a href="skins/videoton-rt6303s-320x240/player.png"><img src="skins/videoton-rt6303s-320x240/player.png" width="320" alt="Videoton RT 6303 S — Player" /></a></td></tr>
</table>

<p align="center"><em>I kept the approved original Videoton layout, with label → LED → blank button order and RT 6303 S lettering. R143 removes the green text remnants from the boot image and redraws the loading bar cleanly. The BT button switched directly to IN mode without opening a menu, and phone audio played continuously. The short rendering test covered 36 menu pages and 255 rows without another stack overflow. The preview uses example data; full validation of the remaining functions is ongoing.<br>I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.</em></p>





## Hardware and wiring

This wiring matches the ESP32 Small/Yellow Board V156 configuration. Check the labels and supply voltage on the actual board and modules before soldering.

![Original ESP32-WROOM-32 module pinout](hardware/ESP32-WROOM-32-Pinout-ORIGINAL.png)

This diagram shows the WROOM-32 module pins; the next image shows the Yellow Board connector labels.

![Yellow Board connector and GPIO reference](hardware/hardware_media/YELLOW_BOARD_USER.png)

**Required modification:** remove the onboard RGB LED before using GPIO4, GPIO16 or GPIO17 for the rotary switch, PSRAM and DAC. I have removed it from my radio. I do not use the built-in amplifier. Its input is connected to GPIO26 on this board family; check for electrical loading on the actual board before connecting rotary CLK there.

### PCM5102A DAC

![PCM5102A module, front and back](hardware/hardware_media/PCM5102A_FRONT_BACK_USER.png)

| ESP32 | PCM5102A | Function |
|---|---|---|
| GPIO22 | BCK / BCLK | I²S bit clock |
| GPIO17 | DIN / DATA | I²S audio data |
| GPIO27 | LCK / LRCK / WSEL | word clock |
| GND | GND | common ground |
| verified module supply | VIN | module power |
| unconnected | SCK / MCLK | no external master clock on this firmware path |

The DAC output is line level and cannot drive a passive speaker. Check the exact module’s VIN rating.

### HW-040 rotary

![HW-040 rotary encoder](hardware/hardware_media/ROTARY_HW040_USER.png)

| ESP32 | Rotary | Function |
|---|---|---|
| GPIO26 | CLK / A | encoder A |
| GPIO35 | DT / B | encoder B |
| GPIO4 | SW | push switch |
| 3.3 V | + / VCC | supply, if required |
| GND | GND | common ground |

GPIO35 has no internal pull-up. If the module has none, add a 10 kΩ resistor from DT/GPIO35 to 3.3 V. Never apply 5 V to an ESP32 GPIO.

### ESP-PSRAM64H — 64 Mbit / 8 MB, SOP-8-150 mil, 3.3 V

![ESP-PSRAM64H chip](hardware/hardware_media/ESP_PSRAM64H_USER.jpg)

![ESP-PSRAM64H pinout and CE pull-up](hardware/hardware_media/PSRAM64H_PINOUT.svg)

The package marking is `ESP PSRAM64H`; the exact Espressif part number is **ESP-PSRAM64H**, 64 Mbit (8 MB), SOP-8-150 mil, 3.3 V. Pin 1 is marked on the package. This wiring is only for this part.

| SOP-8 pin | Signal | Connection |
|---:|---|---|
| 1 | CE# / CS# | GPIO16; 10 kΩ pull-up to 3.3 V |
| 2 | SO / SIO1 | GPIO7 / shared flash SD_DATA0 |
| 3 | SIO2 | GPIO10 / shared flash SD_DATA3 |
| 4 | VSS | GND |
| 5 | SI / SIO0 | GPIO8 / shared flash SD_DATA1 |
| 6 | SCLK | GPIO6 / shared flash clock |
| 7 | SIO3 | GPIO9 / shared flash SD_DATA2 |
| 8 | VCC | 3.3 V |

Keep PSRAM CS separate from flash CS. GPIO6–10 are shared flash/PSRAM bus signals, not free GPIOs. The classic ESP32 maps at most 4 MB of an 8 MB chip in its conventional address range; the rest needs separate HiMem handling.

| PSRAM type | Capacity | Supply |
|---|---:|---:|
| ESP-PSRAM32 | 4 MB | 1.8 V |
| ESP-PSRAM32H | 4 MB | 3.3 V |
| ESP-PSRAM64 | 8 MB | 1.8 V |
| ESP-PSRAM64H | 8 MB | 3.3 V |

**Replacement options:** for 8 MB, use `ESP-PSRAM64H` or `LY68L6400SLI(T)` (64 Mbit, 3.3 V, SOP-8-150 mil). For a 4 MB downgrade, use `ESP-PSRAM32H`. ESP-IDF 5.5.5 lists LY68L6400 as a supported ESP32 PSRAM type. The non-H 1.8 V parts are not drop-in replacements for this 3.3 V board. Match the package and verify the chip marking before purchase. [Lyontek datasheet](https://www.lyontek.com.tw/pdf/ddr/LY68L6400-1.2.pdf) · [ESP-IDF 5.5.5 support](https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_psram/esp32/Kconfig.spiram).

### Other occupied pins

| Function | GPIOs |
|---|---|
| TFT | CS15, DC2, SCK14, MISO12, MOSI13, backlight21 |
| Touch | SCK25, MISO39, MOSI32, CS33, IRQ36 |
| SD card | SCK18, MISO19, MOSI23, CS5 |
| Light sensor | GPIO34 |

No IR receiver GPIO is assigned in the current firmware.

### Menu status

<p align="center"><a href="hardware/MENU_MAP.jpg"><img src="hardware/MENU_MAP.jpg" width="900" alt="I-BT-Radio menu map 2026-10-19; next firmware menu preparation"></a></p>

Green indicates positive operating feedback; red indicates disabled, faulty or not fully tested functions. This map is not a stability certification. See the [detailed hardware guide](hardware/README.md) for the original ESP32-WROOM-32 pinout and board-revision notes.

## Support

If you'd like to support the project, you can do so here. I'll use donations to buy new ESP boards, other boards you suggest, and development tools so I can make the radio available on more hardware and keep improving it. If you already have a board you'd like to contribute or send me, please get in touch.

- **Ethereum (ETH):** `0xf044db3277a1b880713e81d66b6E507F65450EE6`
- **Bitcoin (BTC):** `3KLgXnbCs1WCD3igiVErxdy2CAH7QWchLs`
- **PayPal:** `LT253250028810624563`

[Menu verification and known issues (Hungarian)](docs/menu/README.md)
