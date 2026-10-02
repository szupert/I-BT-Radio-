# Classic Winamp-inspired skin (320 × 240)

`player_eq.png` and `playlist.png` show the V120 visual revision. The matching `*.rgb565le` files contain one little-endian RGB565 word per pixel in row-major order, without a header. Each raw view is 153,600 bytes. `layout.json` lists the dynamic fields and touch coordinates.

![Player and EQ](player_eq.png)

![Playlist](playlist.png)

The black display wells deliberately contain no baked-in station, time, bitrate or sample-rate text. The renderer supplies live values. The ROCK-shaped EQ curve and sliders are **visual only**; BT audio has no EQ processing. PLAY/PAUSE and previous/next require a renderer and control code; these pictures alone do not add them to stock yoRadio.

Stock yoRadio users can convert the raw data to a C header using [the included helper](../../tools/rgb565le_to_header.py) and connect a display renderer to `layout.json`. A tested stock-yoRadio adapter is planned separately.

