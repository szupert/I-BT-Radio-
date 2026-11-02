# I-BT-Radio R352 BETA

[**Tesztcsomag letöltése**](I-BT-Radio_R352_BETA.zip)

4 MB flash / PSRAM nélküli ESP32-WROOM, 320×240 kijelző és külső DAC összeállításhoz. Windows, Python3.12 szükséges; ez a ZIP nem tartalmaz beépített Python-futtatókörnyezetet.

1. Csomagold ki a ZIP-et.
2. A kicsomagolt I-BT-Radio_TEST mappában telepítsd a függőségeket:

   `py -3 -m pip install -r INSTALLER/LIGHT4_AUTO/INSTALLER/requirements.txt`

3. Futtasd a `0_CSOMAGELLENORZES.bat` fájlt.
4. USB csatlakoztatása és BOOT/EN után indítsd az `1_R352_FELTOLTES.bat` fájlt. Az automatikus feltöltő élő COM-, sebesség- és százalékjelzést ad. Megfelelő partíció mellett megőrzi a beállításokat.

A csomag tesztverzió. Részletes használat: a ZIP-ben található OLVASS_EL.txt és RESZLETES_LEIRAS.md.

Visszajelzés: [GitHub Issues](https://github.com/szupert/I-BT-Radio-/issues).
