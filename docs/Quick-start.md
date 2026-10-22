# Telepítés és első indítás · R163 BETA

**[Aktuális telepítő és részletes útmutató](../beta/16MB_PSRAM/V166_R163/README.md)** · [Hardver és bekötés](../hardware/README.md)

Egyetlen Windows x64 EXE, beépített futtatókörnyezettel. Csak SMALL ESP32-WROOM + PSRAM, 16 MB flash, 320×240 kijelző és külső DAC. Nem általános ESP32/S3/4 MB telepítő.

Ismert hibás tesztkiadás: a Wi-Fi → DAC hang menü nélkül is szaggat, menüjárás alatt a stream újracsatlakozhat. A Windows 7 bootanimáció villogása nyitott. Hosszú stabilitás nincs igazolva.

A feltöltő élő COM/BOOT/százalékjelzést ad. Megfelelő partíció mellett megőrzi a beállításokat; teljes törlés csak IGEN megerősítéssel. Hiba után kézi újrapróba, automatikus stresszteszt nélkül. Teljes telepítés után I-BT-Radio-Setup hálózat és http://192.168.4.1/; saját állomáslistát kell hozzáadni. Naplók: %LOCALAPPDATA%\I-BT-Radio\Logs.
