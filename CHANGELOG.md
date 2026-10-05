# Skin asset version history

The `skins` version tracks only public pictures, RGB565 skin data, coordinate maps and documentation. It does not version or publish the radio firmware. Fixes to existing assets increment the patch number; new skin features increment the minor number.

## 0.1.2-preview — 2026-10-05

- Removed the Winamp-style lightning marks from the player and EQ title bars and the simulated preview.
- Regenerated the RGB565LE player asset; retained the original 320 × 240 layout and controls.
- Described the artwork as Winamp-inspired, with I-BT RADIO branding and no original Winamp logo artwork.
- Added a PowerShell helper for repeating the exact title-mark removal and RGB565LE conversion.

## 0.1.1-preview — 2026-10-02

- Updated the Winamp-inspired player/EQ and playlist PNG previews to the V120 visual revision.
- Updated the player/EQ RGB565LE data and corrected the bitrate and sample-rate field coordinates and asset hashes.
- Added current skin pictures to the main description. The cassette asset is unchanged.
- The Winamp EQ curve remains decorative; BT audio has no EQ. The stock-yoRadio display adapter and physical Bluetooth stability test remain pending.

## Hardware documentation — 2026-10-05

- Added the ESP32 Small/Yellow Board V156 GPIO guide with PCM5102A, HW-040 rotary and ESP-PSRAM64H component photos, a PSRAM wiring diagram and a menu status map.
- Added the main-page hardware wiring and a standalone English README; the front page is primarily Hungarian.
- Listed 3.3 V, pinout-compatible PSRAM replacements and marked the 1.8 V parts as non-drop-in options.
- Checked the pin mapping against the V156 board configuration and the PSRAM voltage families against Espressif documentation. Physical wiring on every Yellow Board revision remains to be checked separately.
- Documented that I removed the onboard RGB LED and do not use the built-in amplifier in my radio.
- Added the component photos and essential DAC, rotary, PSRAM and board GPIO wiring directly to the repository front page.
- Published the V159 HTTPS BETA installer and a source-materials archive with GPL-3.0 license and V159 patch. This is a separate firmware beta, not a skin-version change; V159 is not declared stable.

## Hardware photo — 2026-10-05

- Rotated the PCM5102A rear photo to match the front orientation.
- Marked H1L–H4L solder links with thick red frames and numbered badges; added a legend and power-off soldering caution.

## 0.1.0-preview — 2026-10-02

- Initial public Winamp-inspired 320 × 240 player/EQ and playlist assets, plus 320 × 213 cassette asset.
- Added RGB565LE data, coordinate maps, visible README previews and a conversion helper.

