# Kajon-Jon-bt-vu · nyilvános skinforrás

<p align="center"><img src="player_tapered_preview.png" width="320" alt="Kajon-Jon-bt-vu" /></p>

A Kajon felületet helytakarékosság miatt kivettem az R165 SMALL firmware-ből. A korábbi előnézetet és a kizárólag ehhez a skinhez tartozó forrást itt hagytam:

- [RGB565 háttér és műszerlapok](source/kajon_assets_320.h)
- [Mutatók, VU-skála és érintési területek](source/kajon_layout.h)
- [Eredeti kijelzőrajzoló függvények](source/kajon_renderer.inc)

Az eredeti renderelő a közös SMALL kijelzőillesztőt, Snapshot/Action/Point felületet, szövegkirajzolást és VU-postafiókot használja. Ez egy meglévő firmware-ből kiemelt skinmodul, önmagában nem fordítható teljes rádiófirmware. Az include útvonalakat más projektbe illesztéskor hozzá kell igazítani. A közös firmware és más skinek forrása nincs ebben a mappában.

Ez a skin külön engedélyezett nyilvános kivétel. A repó licenc- és forrásmegjelölési feltételeit meg kell tartani. A kép korábbi elrendezési előnézet, nem új készülékteszt.
