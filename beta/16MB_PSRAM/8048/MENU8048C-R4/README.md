# I-BT-Radio8048 · MENU8048C-R4 BETA

**[Egykattintásos Windows-telepítő](I-BT-Radio-8048-Install.exe)**. Letöltés után indítsd el; külön Python/esptool nem szükséges. Kizárólag **ESP32-8048S070C, ESP32-S3,16MB flash,8MB PSRAM,800×480 kapacitív** panelhez. Nem a SMALL/WROOM feltöltője. A paneltípust a felhasználónak kell ellenőriznie: a telepítő az S3-at és a flashméretet felismeri, az LCD-modellt nem.

A felhasználó kiadásra jóváhagyta. **Teljes navigációs és felületdemó**,55 menüoldal,287 sor,9 skin. A mintaállomások és beállításpróbák RAM-ban működnek. Rádióhang, BT/Wi-Fi háttérrendszer és SD-lejátszás ebben az R4-ben még nincs bekötve. A korábbi fizikai felhasználói visszajelzés szerint a kép és érintés működik; az új hordozható EXE fizikai feltöltése NOT_RUN.

Csomag/S3/hash/partíció ellenőrzés, COM-választás, kétirányú automatikus sebességpróba (1500000/921600/460800/115200), régi boot/NVS64KB mentése és élő feltöltési százalék. Az ismert gyári3MB appkiosztást megtartja; ismeretlen partíciót nem ír felül. Nincs erase-all és automatikus stresszteszt. A boot DIO beállítása megmarad. Feltöltés után PSRAM/GT911/firmwareazonosító ellenőrzés. Napló: Windows helyi alkalmazásadatok I-BT-Radio/Logs és RETURN_TO_EVA.

17 eredeti telepítőteszt, sebességválasztás/visszaállítás és Unicode útvonalas, Python nélküli EXE-próba PASS; COM nem nyílt meg a kiadásellenőrzéskor. Forrás: [8048 alkalmazás](../../../../source/8048/README.md), [telepítő](../../../../tools/installer_8048/). Az alkalmazás binárisa változatlan R4; nem épült újra.
