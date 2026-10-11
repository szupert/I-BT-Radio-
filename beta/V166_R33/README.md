# V166-R33 BETA

**Csak tesztelésre; beépítésre nem ajánlott.**

1. Töltsd le és bontsd ki az [egy kattintásos csomagot](I-BT-Radio_V166-R33_BETA.zip).
2. Indítsd a `1_FELTOLTES.bat` fájlt. Klasszikus ESP32, 16 MB flash, meglévő kompatibilis partíció szükséges. A telepítő csak az alkalmazást írja; a Wi-Fi/NVS megmarad. Python 3.9+ szükséges.
3. A `19_MENU_STABILITAS_10PERC.bat` tízperces menü/audio teszt WebUI nélkül; a `20_MENU_WEB_STABILITAS_10PERC.bat` WebUI-val. A teszt PowerShellt használ. RETURN_TO_EVA az átadandó eredmény.

R33: internetes rádió→BT 32 kHz; SD→BT forrásfüggő tárgyalt ráta; DAC a forrás/hardver szerint. Az USB-forrás inaktív. A hosszú Wi-Fi/BT hangfolytonosság még nem igazolt, kihagyás előfordul. Service: nem kalibrált jelgenerátor, nem hiteles mérőműszer.

BIN és ELF korábban lefordítva; a csomag/hash, feltöltő élő progress/timeout/baud/partíció ellenőrzése és a runner offline selftest PASS. Az új nyilvános csomag fizikai tesztje NOT_RUN. A menüteszt FIX3 rövidszakasz-mintavételi javítását tartalmazza. Ez nem új audio-stabilitási PASS.

[Telepítő SHA256](I-BT-Radio_V166-R33_BETA.zip.sha256) · [forráspillanatkép és buildrecept](V166-R33_SOURCE.zip) · [forrás SHA256](V166-R33_SOURCE.zip.sha256). A forrás ugyanahhoz az R33-hoz tartozik; független tiszta gépes újrafordítás NOT_RUN. yoRadio-alapú kód GPL-3.0; a grafika jogai az eredeti szerzőknél maradnak.
