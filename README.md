# I-BT-Radio skins

This public repository contains **visual skin assets only**. Radio firmware, installers, hardware configuration, station lists and credentials are not published here.

Skin asset version: **0.1.1-preview** · [Version history](CHANGELOG.md). The Winamp-inspired pictures now reflect the V120 visual revision approved on the radio display. This is a visual preview; the Bluetooth audio stability test is still pending.

## Preview images

Classic Winamp-inspired player and EQ:

![Winamp-inspired player and EQ](skins/classic-winamp-320x240/player_eq.png)

Playlist:

![Winamp-inspired playlist](skins/classic-winamp-320x240/playlist.png)

Cassette body; the station name and artwork are drawn at runtime:

![Translucent cassette skin](skins/cassette-320x213/cassette.png)

## Skin files

- [Classic Winamp-inspired skin](skins/classic-winamp-320x240/README.md): 320 × 240 player/EQ and playlist pictures, RGB565LE data and layout coordinates.
- [Cassette skin](skins/cassette-320x213/README.md): 320 × 213 cassette picture, RGB565LE data and touch zones.

The Winamp-style EQ curve is a **visual preview**. BT audio has no EQ processing. The pictures are usable in a custom 320 × 240 TFT renderer; the images alone do not install controls or a skin in stock [yoRadio](https://github.com/e2002/yoradio). A tested stock-yoRadio adapter is planned separately.

I-BT-Radio grew from yoRadio. Thank you to its authors. These are unofficial community skins; Winamp and yoRadio are names of their respective owners.

