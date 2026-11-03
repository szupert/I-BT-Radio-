## BIG P4 / JC1060 – DEV1 tesztverzió

**[Windows-telepítő, beépített Pythonnal](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/BIG_P4_JC1060_DEV1/I-BT-Radio_BIG_BETA_Install.exe)** · **[Firmware alkalmazáskép](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/BIG_P4_JC1060_DEV1/firmware.bin)**

JC1060P470C_I_W_Y, JD9165 V2, ESP32-P4 rev.0.1–1.99, 16 MB flash, beépített ES8311 mono. [Telepítés és tesztállapot](BIG_P4_JC1060_DEV1/README.md). Kísérleti kiadás: a készülékes hang-, kép-, érintés- és stabilitásvizsgálat még nincs lezárva.

## Új Windows-telepítő: V166-R356 BETA

**[Letöltés: I-BT-Radio_BETA_Install.exe](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/V166_R356/I-BT-Radio_BETA_Install.exe)**

Windows x64, 4 MB flash, PSRAM nélküli SMALL. A Python és a feltöltő beépítve; külön Python-telepítés nem szükséges. [Telepítés, kapcsolható DSP és ismert korlátok](V166_R356/README.md). Kísérleti béta, a teljes készülékes teszt még nem lezárt.

# Korábban közzétett telepítők — archívum

## R352 béta tesztcsomag · 2026-11-02

[Letöltés és telepítés – 4 MB, PSRAM nélkül](V166_R352/README.md).


A csomagok történeti kiadások. A hozzájuk tartozó tesztleírás és ismert hibák érvényesek; az archívumban való szereplés nem teljes készülékes PASS minősítés.

# Béta-telepítők — négy hardverprofil

| Flash | PSRAM | Telepítők |
|---|---|---|
|4MB|nincs|[4MB_PSRAM_NELKUL](4MB_PSRAM_NELKUL/README.md)|
|4MB|van|[4MB_PSRAM](4MB_PSRAM/README.md)|
|16MB|nincs|[16MB_PSRAM_NELKUL](16MB_PSRAM_NELKUL/README.md)|
|16MB|van|[16MB_PSRAM](16MB_PSRAM/README.md)|

Az egyfájlos Windows telepítő automatikusan ismeri fel a négy hardverprofilt; beépített futtatókörnyezet, nem kell külön Python. A négy mappában ugyanaz az ellenőrzött automata EXE található, Git ugyanazt a bináris objektumot tárolja, nem négy külön fordítást. A verziók nem azonosak: 4MB/noPSRAM:R211diagnosztika (BT/BLE/SD/WebUI OFF), 4MB/PSRAM és16MB/PSRAM:R176,16MB/noPSRAM:R189. Minden változat fejlesztői béta.

A régebbi kiadások változatlan binárishashsel kerültek a megfelelő hardvermappa alá. [Teljes alkalmazásforrás](../source/README.md).

[8048S070C S3/16MB/8MB menu-demo: kulon egykattintasos telepito](16MB_PSRAM/8048/MENU8048C-R4/README.md). A negyprofilos WROOM automata helyett ezt valaszd az8048 panelhez.
