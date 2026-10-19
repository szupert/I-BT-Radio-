# Telepítés és első indítás · R151 BETA

Az aktuális nyilvános telepítő és a részletes útmutató a **[beta/V166_R151](../beta/V166_R151/README.md)** mappában található. Egyetlen Windows x64 EXE, beépített Python/esptool és firmware, külön függőségtelepítés nélkül.

Csak a [dokumentált SMALL hardverhez](../hardware/README.md): ESP32-WROOM, 16 MB flash, PSRAM, 320×240 kijelző és külső DAC. A saját panelmódosításokat és GPIO-kat feltöltés előtt ellenőrizni kell.

Adatmegőrző frissítés megfelelő partíció esetén; teljes telepítés üres vagy eltérő flashre, külön IGEN megerősítéssel. Élő COM/BOOT és valódi százalékjelzés, hash- és bootellenőrzés. Hiba után kézi újrapróba; automatikus stresszteszt nincs.

Teljes telepítés után I-BT-Radio-Setup Wi-Fi, majd http://192.168.4.1/ és a saját hálózat beállítása. Az állomáslistát külön kell hozzáadni. A naplók helye: %LOCALAPPDATA%\I-BT-Radio\Logs.

R151: fordítás és hostteszt sikeres; fizikai feltöltés/hang/stabilitás még NOT_RUN. Hibajelzéshez verzió, hardver, skin, forrás/kimenet és pontos reprodukció szükséges. Személyes adatot ne tegyél a nyilvános hibajegybe.
