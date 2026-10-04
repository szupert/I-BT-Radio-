# I-BT-Radio skins and hardware

**Magyar:** [README.md](README.md)

This repository contains display skins and hardware documentation. Firmware, installers, station lists and credentials are not included. Skin version: **0.1.1-preview**. Bluetooth audio stability testing is still in progress.

## Skins

![Winamp-style player and EQ](skins/classic-winamp-320x240/player_eq.png)

![Winamp-style playlist](skins/classic-winamp-320x240/playlist.png)

![Cassette skin](skins/cassette-320x213/cassette.png)

The EQ curve is visual only; Bluetooth audio has no EQ processing. These images need a custom 320 × 240 TFT renderer and do not install themselves in stock yoRadio.

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

### ESP-PSRAM64H, 8 MB

![ESP-PSRAM64H chip](hardware/hardware_media/ESP_PSRAM64H_USER.jpg)

![ESP-PSRAM64H pinout and CE pull-up](hardware/hardware_media/PSRAM64H_PINOUT.svg)

This wiring is only for the 3.3 V **ESP-PSRAM64H SOP-8**. Pin 1 is marked on the package.

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

### Other occupied pins

| Function | GPIOs |
|---|---|
| TFT | CS15, DC2, SCK14, MISO12, MOSI13, backlight21 |
| Touch | SCK25, MISO39, MOSI32, CS33, IRQ36 |
| SD card | SCK18, MISO19, MOSI23, CS5 |
| Light sensor | GPIO34 |

No IR receiver GPIO is assigned in the current firmware.

### Menu status

![V156 menu status map](hardware/MENU_MAP_V156.jpg)

Green indicates positive operating feedback; red indicates disabled, faulty or not fully tested functions. This map is not a stability certification. See the [detailed hardware guide](hardware/README.md) for the original ESP32-WROOM-32 pinout and board-revision notes.
