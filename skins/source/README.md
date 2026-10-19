# SMALL skinforrások · R165 · 2026.10.20.

A tulajdonos engedélyével közzétettem az összes SMALL skin rajzoló kódját, elrendezését, betű- és képadatát. Ez a könyvtár az R165 forrásából készült, a fájlokat változtatás nélkül másoltam ide.

- [Közös SMALL renderelő](ui/small/frontend_small.cpp): Winamp, Media Player, kazetta, vintage/retro/fa, Neon, Videoton, óra/ébresztő, időjárás, BT-Speaker és Service.
- [Képek, ikonok, betűk, boot és asztal](ui/small/): RGB565/JPEG/tömörített adatok és a hozzájuk tartozó kirajzolás.
- [Elrendezések és menüfeliratok](ui/): érintési területek, közös felület és menütábla.
- [Korábban kivett Kajon eredeti renderelője és elrendezése](../kajon-vu-320x240/source/): a skin az R165 firmware-ben már nincs benne.
- [Fájlméretek és SHA256](SOURCES.json).

A renderer a meglévő rádió frontend/board/Snapshot, lejátszó-, szenzor- és állapot-API-ját használja. Ez skinmodul-gyűjtemény, önmagában nem teljes, fordítható rádiófirmware. A működtető host interfészek egy része külön projektfüggőség; a relatív include-ok az eredeti firmware fájlszerkezetére hivatkoznak.

Wi-Fi-jelszó, személyes állomáslista, érzékelő-MAC/bind key, NVS-mentés, napló, BIN és ELF nincs ebben a forrásmappában. A meglévő szerzői és licencjelöléseket megőriztem; a képi márkajelzések és külső eredetű elemek jogait ez a közzététel nem módosítja. A forrás eredetét és a meglévő szerzői jelöléseket továbbadáskor fel kell tüntetni.

Az előnézetek korábbi mintaképeket is tartalmaznak. A forrás közzététele nem új fizikai teszteredmény. A rádió külön futó négyórás tesztjét a publikálás nem módosítja.
