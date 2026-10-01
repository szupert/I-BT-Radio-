# I-BT-Radio skins

This public repository currently contains **visual assets only**. Firmware, installers, hardware configuration, station lists, credentials and unfinished radio source are intentionally absent.

Skin asset version: **0.1.0-preview** · [Version history](CHANGELOG.md). This version is based on the V118 artwork; its physical display test is still pending.

## Preview images

Winamp-inspired player and EQ, with **simulated** live values:

![Winamp-inspired player and EQ preview](skins/classic-winamp-320x240/preview_simulated.png)

Playlist view:

![Winamp-inspired playlist view](skins/classic-winamp-320x240/playlist.png)

Cassette body; station text and album artwork are added at runtime:

![Translucent cassette skin](skins/cassette-320x213/cassette.png)

## Files

- [Classic Winamp-inspired skin](skins/classic-winamp-320x240/README.md): 320 × 240 player/EQ and playlist views.
- [Cassette skin](skins/cassette-320x213/README.md): 320 × 213 translucent cassette image.

Each skin has a PNG preview, exact little-endian RGB565 data from the I-BT-Radio V118 asset, and a JSON coordinate map. The V118 firmware still needs a physical screen test. The images can be used in a custom 320 × 240 TFT renderer. Stock [yoRadio](https://github.com/e2002/yoradio) supports colors and widget layout through its own configuration; it does **not** automatically install arbitrary bitmap skins. A tested stock-yoRadio display adapter will be published separately when ready.

I-BT-Radio grew from yoRadio. Thank you to its authors. These are unofficial community skins; Winamp and yoRadio are names of their respective owners.
