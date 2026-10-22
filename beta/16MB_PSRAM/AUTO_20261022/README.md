# Egykattintásos automatikus béta-telepítő

Töltsd le az **I-BT-Radio-Auto4-Install.exe** fájlt, majd indítsd el. A Python/esptool futtatókörnyezet beépített. A telepítő beolvassa a flash méretét, ellenőrzi a PSRAM-ot, és a megfelelő profilt választja; a mappanév nem kényszerít idegen hardverre képet. A feltöltés sebességét kétirányú próbából választja. A tényleges COM-, kapcsolati és írási százalék élőben látható; hiba és naplóútvonal megmarad. Nincs automatikus stresszteszt vagy hiba utáni újraflashelés. Teljes törléshez külön megerősítés kell.

**Ehhez a mappához tartozó profil:** 16MB_PSRAM · V166-R176.

**4MB/noPSRAM esetén jelenleg R211 DIAGNOSZTIKA települ: BT/BLE/SD/WebUI kikapcsolva.** Nem a készülő Bluetooth-kiadás, nem stabil verzió! 4MB/PSRAM és16MB/PSRAM:R176;16MB/noPSRAM:korábbi R189. A firmware-ek most nem épültek újra, saját fizikai próbáik nem minősítik az új EXE-t. Csomaghash és beépített esptool/segédindítás: számítógépes PASS, fizikai feltöltés e nyilvános EXE-vel NOT_RUN.

A napló a Windows helyi alkalmazásadatok I-BT-Radio/Logs és RETURN_TO_EVA mappáiba kerül. A forrás a repository source/ és tools/installer_auto4 könyvtára alatt található. Hibajelzéshez add meg a hardverprofilt, firmware-verziót és naplót; jelszót vagy kulcsot ne küldj.
