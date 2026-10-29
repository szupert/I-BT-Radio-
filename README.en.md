# I-BT-Radio — Light / Pro

ESP32 internet radio with a local display, SD playback and Bluetooth audio routes. LIGHT uses no PSRAM; PRO uses PSRAM. Hardware profiles and wiring differ.

[Controls and menus](docs/menu/README.md) · [Hardware](hardware/README.md) · [Features](docs/Features.md) · [Source](source/README.md)

[Magyar](README.md) · [Deutsch](README.de.md)

[Previously published packages](beta/README.md) are archived with their own versions, checksums and documented limitations. Archived R270 has no complete on-device PASS qualification.

---

## Skins

Interface previews

I show previews of the 13 remaining interfaces, including the Videoton player and boot image, Service and the new BT-Speaker screen. Images are centered at 260–320 pixels wide. I removed the Basic player previews as requested. Design references, static backgrounds and host renders are labelled separately.

This update publishes preview images and descriptions only. I am not uploading source code or firmware.

<h3 align="center">Winamp / Classic</h3>

<table align="center">
<tr><th align="center">Boot</th><th align="center">Desktop · 2 s</th><th align="center">Player</th></tr>
<tr><td align="center"><a href="skins/classic-player-320x240/boot-xp-r161.jpg"><img src="skins/classic-player-320x240/boot-xp-r161.jpg" width="260" alt="Winamp / Classic — Boot" /></a></td><td align="center"><a href="skins/classic-player-320x240/desktop-xp-r161.png"><img src="skins/classic-player-320x240/desktop-xp-r161.png" width="260" alt="Winamp / Classic — Desktop · 2 s" /></a></td><td align="center"><a href="skins/classic-player-320x240/player_eq.png"><img src="skins/classic-player-320x240/player_eq.png" width="260" alt="Winamp / Classic — Player" /></a></td></tr>
</table>

<p align="center"><em>Earlier layout preview.</em></p>



<h3 align="center">Media Player</h3>

<table align="center">
<tr><th align="center">Boot</th><th align="center">Desktop · 2 s</th><th align="center">Player</th></tr>
<tr><td align="center"><a href="skins/media-player-320x240/boot-win7-r161.jpg"><img src="skins/media-player-320x240/boot-win7-r161.jpg" width="260" alt="Media Player — Boot" /></a></td><td align="center"><a href="skins/media-player-320x240/desktop-win7-r161.png"><img src="skins/media-player-320x240/desktop-win7-r161.png" width="260" alt="Media Player — Desktop · 2 s" /></a></td><td align="center"><a href="skins/media-player-320x240/player.png"><img src="skins/media-player-320x240/player.png" width="260" alt="Media Player — Player" /></a></td></tr>
</table>

<p align="center"><em>Earlier layout preview.<br>I published the skin source with the owner’s permission: <a href="https://github.com/szupert/I-BT-Radio-/tree/main/skins/source">skin sources</a>.</em></p>



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
