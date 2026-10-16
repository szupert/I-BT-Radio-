# Xiaomi LYWSD03MMC: a bindkey megszerzése

A gyári érzékelő titkosított MiBeacon-adásához MAC-cím és 16 bájtos (32 hexadecimális karakteres) Mi Bind Key szükséges. A Mi Token és a blt. kezdetű Device known id nem helyettesíti ezeket.

## Böngészős aktiválás

1. Bluetooth-os Androidon Chrome böngészőben nyisd meg: https://atc1441.github.io/TelinkFlasher.html
2. Connect, majd válaszd a LYWSD03MMC érzékelőt.
3. A csatlakozás után Do Activation. Firmware-feltöltés nem szükséges.
4. Siker esetén a Mi Bind Key mező értékét mentsd privát helyre. Ismételt aktiválás után a legutóbbi értéket használd.
5. Disconnect: a rádió ezután passzívan fogadhatja az adást.

A 2026-10-16-i felhasználói Telink napló LYWSD03MMC, Fw_version 1.0.0_0001 mellett sikeres aktiválást és bejelentkezést rögzített. Ez a kulcskinyerési folyamatot igazolja, nem a rádiós vételt. Újabb gyári verziók eltérhetnek.

Az aktiválás új kulcsot állíthat be: a Xiaomi Home újrapárosítást igényelhet. Ha a módszer nem támogatott, Xiaomi Home-hozzárendelés után a Xiaomi Cloud Tokens Extractor használható.

## MAC és bizalmas adatok

A MAC hat hexadecimális pár, például AA:BB:CC:DD:EE:FF. A böngésző Bluetooth-eszközazonosítója nem feltétlenül MAC. A Chrome kísérleti funkcióiról szóló állandó felirat önmagában nem hibajelzés. Ha a böngésző nem tudja kiolvasni, a rádió keresési listájából kell azonosítani.

Ne publikálj valódi kulcsot, tokent, Xiaomi-azonosítót vagy személyes MAC-címet. A firmware nem tartalmaz előre beégetett személyes kulcsot.

## R115 BETA rádiós integráció

R115 célzottan a gyári LYWSD03MMC MiBeacon v5 AES-CCM hitelesített adatát támogatja. Más Xiaomi protokollok titkosított támogatása nem következik ebből. Csak nyugalmi óralapon, STOP lejátszás mellett dolgozik; a belső memóriahatárok változatlanok. Fizikai adatfogadás a dokumentum készítésekor NOT_RUN.

A helyi soros karbantartási parancsok: `r10 sensor search`, `r10 sensor list`, `r10 sensor bind <listaindex> <32_hex_kulcs>`, `r10 sensor`. A bind nyugalmi, leállított scan után kerül alkalmazásra és mentésre; a kulcs nem kerül a válaszba. Nyilvános példában kizárólag helyőrzőt használj. A lista szolgáltatás és cím alapján jelöltet mutat; a kulccsal hitelesített hő/pára igazolja a tényleges egyezést.

Források:
- https://esphome.io/components/sensor/xiaomi_ble/#obtaining-the-bindkey
- https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor
- https://github.com/pvvx/ATC_MiThermometer

