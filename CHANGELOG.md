# Skin asset version history

The `skins` version tracks only public pictures, RGB565 skin data, coordinate maps and documentation. It does not version or publish the radio firmware. Fixes to existing assets increment the patch number; new skin features increment the minor number.

## 0.1.1-preview — 2026-10-02

- Updated the Winamp-inspired player/EQ and playlist PNG previews to the V120 visual revision.
- Updated the player/EQ RGB565LE data and corrected the bitrate and sample-rate field coordinates and asset hashes.
- Added current skin pictures to the main description. The cassette asset is unchanged.
- The Winamp EQ curve remains decorative; BT audio has no EQ. The stock-yoRadio display adapter and physical Bluetooth stability test remain pending.

## Hardware documentation — 2026-10-05

- Added the ESP32 Small/Yellow Board V156 GPIO guide with PCM5102A, HW-040 rotary and ESP-PSRAM64H component photos, a PSRAM wiring diagram and a menu status map.
- Added the main-page hardware wiring and a standalone English README; the front page is primarily Hungarian.
- Checked the pin mapping against the V156 board configuration and the PSRAM voltage families against Espressif documentation. Physical wiring on every Yellow Board revision remains to be checked separately.
- Documented that I removed the onboard RGB LED and do not use the built-in amplifier in my radio.
- Added the component photos and essential DAC, rotary, PSRAM and board GPIO wiring directly to the repository front page.
- No firmware, installer or radio source was added; the skin asset version remains `0.1.1-preview`.

## 0.1.0-preview — 2026-10-02

- Initial public Winamp-inspired 320 × 240 player/EQ and playlist assets, plus 320 × 213 cassette asset.
- Added RGB565LE data, coordinate maps, visible README previews and a conversion helper.

