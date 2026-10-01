# Cassette skin (320 × 213)

`cassette.png` is the clean cassette body. `cassette.rgb565le` contains its exact RGB565 pixels (136,320 bytes, row-major, little-endian, no header). On a 320 × 240 screen, center it at y = 13; the lower margin remains available for a buffer indicator.

![Cassette](cassette.png)

The station name and album artwork are dynamic overlays. `layout.json` records the reel and center-window touch zones. In I-BT-Radio, the left reel selects the previous station, the right reel the next station, and the center toggles playback. The image alone does not implement those actions in stock yoRadio.
