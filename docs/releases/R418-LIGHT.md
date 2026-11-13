# R418 LIGHT – közös tesztkiadás

LIGHT / SMALL, klasszikus ESP32, 4 MB flash, PSRAM nélkül. PRO és BIG hardverre nem telepíthető.

- Tartós SD-borítóleltár két váltott példánnyal: újraindítás után a tényleges, állomásazonosítóval egyező képfájlokból áll vissza az AW számláló.
- Négy tájolás, adaptív érintési területek, álló lejátszó-, óra- és időjárásnézet.
- Öt menübetűméret, mezőn belül gördülő hosszú feliratok, egységes lekerekített menük és Wi-Fi-oldalak.
- Kapcsolható lapozós óra- és kapcsolódási animáció; órára koppintva Rádió gyorsgomb.
- Választható SD-óraháttér-csomagok, aránytartó képkitöltés és belső háttérre visszatérés hiányzó vagy hibás fájlnál.

## Telepítés

Töltsd le és bontsd ki az `I-BT-Radio_LIGHT_R418_TEST.zip` csomagot, majd indítsd az `1_R418_FELTOLTES.bat` fájlt. Ez a csomag Python 3-at és a feltöltő Python-függőségeit igényli; nem önálló EXE-telepítő. A pontos fájlokat és használatot a csomag leírása tartalmazza. A normál frissítés adatmegőrző; a csomag a megfelelő hardver és firmware ellenőrzését is elvégzi. Az SD-s skinekhez lásd a mellékelt `SD_CLOCK_SKINS.md` leírást.

## Ellenőrzés és ismert korlátok

Teljes lite4 fordítás és linkelés, öt feltöltő-hostteszt, 57 csomagfájl és 58 ZIP-bejegyzés teljes hash-ellenőrzése PASS. A feltöltő és selftest ellenőrző futása PASS, soros port megnyitása nélkül.

A készülékes feltöltés, tájolás/érintés, újraindítás utáni SD-leltár, animáció, ébresztés, tartós WiFi–DAC/borító, Poly és teljes teszt még nincs lezárva. Ez tesztkiadás, nem igazolt végleges stabil verzió. A hangvédelmek megmaradtak. A hiányzó funkció vagy kihagyott mérés nem PASS.

Nyílt I-BT-Radio projekt: https://github.com/szupert/I-BT-Radio- . A korábbi és harmadik féltől származó összetevők szerzői és licencfeltételei továbbra is érvényesek.

Forrásrevízió: `39e06a06b81cfb476c1205dcbd741bb7b840eb34`.

Firmware SHA-256: `14502a928b9ca469b3c33b51317088ee234cf4f4bbf46ad3df24b3fa85618caa`.

ZIP SHA-256: `1b3e46b7693402a922ec05101655a9504d87bf9dab4919a4bbcc710e36cfe1b9`.
