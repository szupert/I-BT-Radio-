# I-BT-Radio BIG P4 / JC1060 – DEV1 tesztbéta

Nyílt forrású **szupert / I-BT-Radio** projekt, [yoRadio alapokon / GNU GPLv3](https://github.com/szupert/I-BT-Radio-). Az eredeti szerzői jelölések és a [komponenslicencek](INSTALLER_LICENSES.txt) megmaradnak.

## Letöltések

- [Windows x64 egykattintásos telepítő](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/BIG_P4_JC1060_DEV1/I-BT-Radio_BIG_BETA_Install.exe) – 15,2 MB; a Python, esptool és a teljes ellenőrzött firmware-csomag beépítve. Külön Python és telepítés közbeni internet nem szükséges.
- [firmware.bin](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/BIG_P4_JC1060_DEV1/firmware.bin) – 2 341 152 bájtos alkalmazáskép. **Ez önmagában nem teljes friss telepítés**; a partíciókhoz és az első telepítéshez használd az EXE-t.
- [SHA256 ellenőrző összegek](SHA256SUMS.txt).

## Támogatott hardver

**JC1060P470C_I_W_Y**, JD9165 **V2**, 1024×600 MIPI kijelző, GT911 érintés, 16 MB flash, ESP32-P4 **rev.0.1–1.99**, C6 Wi-Fi társprocesszor. Beépített **ES8311 mono** hangkimenet. A chiprevíziót és a flashméretet a telepítő ellenőrzi; eltérésnél nem ír. Más panelváltozat vagy P4 rev.3 nem része ennek a csomagnak. A P4+C6 útvonalon nincs Bluetooth Classic/A2DP.

## Használat

1. Indítsd az EXE-t, ellenőrizd a paneltípust a megjelenő leírásban. Enter indít, Q kilép.
2. USB-adatkapcsolat és a panel gyártói útmutatója szerinti letöltési/BOOT mód szükséges. A kiválasztott COM, azonosítás, ellenőrzött sebesség, valódi feltöltési százalék és hash látható.
3. Megfelelő meglévő partíciónál az NVS, SPIFFS, OTA-adat és az SD-kártya megmarad. Üres/eltérő partícióknál a teljes újratelepítés **külön IGEN megerősítést kér**: a Wi-Fi, állomáslista és beállítások törlődnek; az SD-kártya megmarad.
4. Várd meg az írás, a hash és a pontos firmware/ELF/setup ellenőrzés végét. Hiba után nincs automatikus újraflash vagy reset-hurok.

Naplók: `%LOCALAPPDATA%/I-BT-Radio/Logs`. Az EXE `--verify-only` kapcsolója COM nélkül ellenőriz. A `--extract-payload ÚJ_MAPPA` kibontja a beépített firmware-t és fájlokat. Egy második telepítőpéldány nem nyithat új feltöltési kapcsolatot. Nincs automatikus rádió-selftest.

## Natív BIG felület

Nagy érintőgombok, lapozható állomás- és menülisták, magyar ékezetek. Legfeljebb **1200×1200-as borítóforrás**; **464×464-es kezelőnézet** és érintéssel váltható **976×464-es nagyított kép**. Aránytartó középre vágás és simított RGB565 átméretezés tölti ki a képfelületet. A képfrissítés korlátozott részletekben halad; a meglévő hang-, puffer-, memória- és terhelési védelmek megmaradnak.

## Ellenőrzött és még nyitott

Sikeres: 6 célzott, bekapcsolt assertionnel futtatott C++ tesztcsoport és képgenerálási adapterellenőrzés; 27 telepítő-hosteset; 9 tényleges Windows EXE-próba külön Python nélküli, tiszta PATH-os és ékezetes útvonalas indítással, teljes kibontott fájlhash-visszaolvasással, P4-stubokkal, második példány tiltásával és naplózással. A tényleges BIN chip/revízió/flash/partíció/ELF ellenőrzése sikeres.

**Készülékes hang, fizikai képpontok, érintés, Wi-Fi/SD és hosszú terhelési stabilitás: még nem igazolt.** A hostteszt vagy a sikeres indulás nem jelenti ezek teljesítését. A SMALL Poly-/teljes selftest eredményei nem vihetők át a BIG natív felületére. Ez tesztelésre kiadott fejlesztői változat.

Firmware-azonosító: `BIG-P4-JC1060-DEV1`. A letöltések pontos SHA256 értékei a mellékelt ellenőrzőösszeg-fájlban találhatók.
