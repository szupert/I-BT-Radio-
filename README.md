# I-BT-Radio skins

This public repository currently contains **visual assets only**. Firmware, installers, hardware configuration, station lists, credentials and unfinished radio source are intentionally absent.

- [Classic Winamp-inspired skin](skins/classic-winamp-320x240/README.md): 320 × 240 player/EQ and playlist views.
- [Cassette skin](skins/cassette-320x213/README.md): 320 × 213 translucent cassette image.

Each skin has a PNG preview, exact little-endian RGB565 data from the tested I-BT-Radio asset, and a JSON coordinate map. The images can be used in a custom 320 × 240 TFT renderer. Stock [yoRadio](https://github.com/e2002/yoradio) supports colors and widget layout through its own configuration; it does **not** automatically install arbitrary bitmap skins. A tested stock-yoRadio display adapter will be published separately when ready.

I-BT-Radio grew from yoRadio. Thank you to its authors. These are unofficial community skins; Winamp and yoRadio are names of their respective owners.
