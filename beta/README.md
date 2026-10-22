## R216 Light4MB ·2026.10.22.

[Menütesztelt béta és telepítő](https://github.com/szupert/I-BT-Radio-/tree/main/beta/4MB_PSRAM_NELKUL/R216) · [Aktuális Light-forrás](https://github.com/szupert/I-BT-Radio-/tree/main/source/light)

Négy hangútválasztás és Vissza készülékesen igazolt. Nulla tartalékú mérési béta; Wi-Fi hang, BT-OUT memória és veremtartalék továbbra nyitott. Az alábbi régebbi kiadások történeti állapotok.

# Béta-telepítők — négy hardverprofil

| Flash | PSRAM | Telepítők |
|---|---|---|
|4MB|nincs|[4MB_PSRAM_NELKUL](4MB_PSRAM_NELKUL/README.md)|
|4MB|van|[4MB_PSRAM](4MB_PSRAM/README.md)|
|16MB|nincs|[16MB_PSRAM_NELKUL](16MB_PSRAM_NELKUL/README.md)|
|16MB|van|[16MB_PSRAM](16MB_PSRAM/README.md)|

Az egyfájlos Windows telepítő automatikusan ismeri fel a négy hardverprofilt; beépített futtatókörnyezet, nem kell külön Python. A négy mappában ugyanaz az ellenőrzött automata EXE található, Git ugyanazt a bináris objektumot tárolja, nem négy külön fordítást. A verziók nem azonosak: 4MB/noPSRAM:R211diagnosztika (BT/BLE/SD/WebUI OFF), 4MB/PSRAM és16MB/PSRAM:R176,16MB/noPSRAM:R189. Minden változat fejlesztői béta.

A régebbi kiadások változatlan binárishashsel kerültek a megfelelő hardvermappa alá. [Teljes alkalmazásforrás](../source/README.md) · [Fejlesztési blokkok](../docs/DEVELOPMENT_BLOCKS.md).

[8048S070C S3/16MB/8MB menu-demo: kulon egykattintasos telepito](16MB_PSRAM/8048/MENU8048C-R4/README.md). A negyprofilos WROOM automata helyett ezt valaszd az8048 panelhez.
