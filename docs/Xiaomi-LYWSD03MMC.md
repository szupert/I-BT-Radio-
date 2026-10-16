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

## MAC-cím megkeresése Windows alatt: BluetoothLEView

1. Kapcsold be a számítógép Bluetooth-adapterét, és indítsd a NirSoft BluetoothLEView programját: https://www.nirsoft.net/utils/bluetooth_low_energy_scanner.html
2. Várd meg a LYWSD03MMC nevű sort. A First/Last Detected és Counter mező segít ellenőrizni, hogy új hirdetések érkeznek.
3. Másold ki a MAC Address mezőt. Az AA-BB-CC-DD-EE-FF alak a rádió számára AA:BB:CC:DD:EE:FF formára írható át.
4. RSSI alapján megkülönböztethetők a közeli jelöltek, de az önmagában nem bizonyítja a tulajdonost. A saját bindkeyvel hitelesített mérés igazolja az egyezést.

Ez a MAC azonosítására szolgál, a bindkeyt nem nyeri ki. A Scan Response típusú bejegyzés is használható a cím megkeresésére. A felhasználó 2026-10-16-án ezzel találta meg az érzékelőt; a személyes címet nem közöljük.

## R115 BETA rádiós integráció

R115 célzottan a gyári LYWSD03MMC MiBeacon v5 AES-CCM hitelesített adatát támogatja. Más Xiaomi protokollok titkosított támogatása nem következik ebből. Csak nyugalmi óralapon, STOP lejátszás mellett dolgozik; a belső memóriahatárok változatlanok. Fizikai adatfogadás a dokumentum készítésekor NOT_RUN.

A helyi soros karbantartási parancsok: `r10 sensor search`, `r10 sensor list`, `r10 sensor bind <listaindex> <32_hex_kulcs>` vagy ismert cím esetén `r10 sensor pair <MAC> <32_hex_kulcs>`, `r10 sensor`. A bind nyugalmi, leállított scan után kerül alkalmazásra és mentésre; a kulcs nem kerül a válaszba. Nyilvános példában kizárólag helyőrzőt használj. A lista szolgáltatás és cím alapján jelöltet mutat; a kulccsal hitelesített hő/pára igazolja a tényleges egyezést.

Források:
- https://esphome.io/components/sensor/xiaomi_ble/#obtaining-the-bindkey
- https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor
- https://github.com/pvvx/ATC_MiThermometer

## R116 BETA: beállítás közvetlenül a WebUI-ból

A BLE hőmérő szakaszban külön MAC-cím és Mi Bind Key mező, Érzékelő mentése gomb és kézi Állapot frissítése található. A kulcs POST-törzsben kerül átadásra, nem az URL-ben. A mentett kulcsot az API nem adja vissza, a böngésző helyi tárába nem kerül. A rádió nyugalmi óralapon alkalmazza és menti a beállítást; a vételhez STOP lejátszás szükséges.

A korábban elmentett saját érzékelő új keresés nélkül is szerepel a rádió listájában „mentett / vételre vár” állapottal. Ez nem azonos friss észleléssel vagy hitelesített mérési adattal. Az adatmegőrző firmware-frissítés megtartja a kötést; teljes formázás törli.

R115-ön a tényleges helyi próba APPLIED és SAVED választ adott a cím és kulcs alkalmazására és NVS-írására; ez még nem igazolja az újraindítás utáni visszaolvasást, de a generikus dekódertask foglalása után 32196 bájt belső RAM maradt a szükséges32768 helyett. A vétel ezért helyesen halasztott. R116 a célzott Xiaomi adatot külön JSON-dekódertask nélkül, korlátozott idle-feldolgozással kezeli. A biztonsági memóriahatárok változatlanok. Az R116 fizikai vételi eredménye még NOT_RUN; sikeres fordításból nem következik működő érzékelő.

A felhasználó jelezte, hogy az R115 listájában nem látszik a mentett érzékelő. R116 ezért mentés után azonnal visszaolvassa és összeveti a teljes cím–kulcs rekordot, és induláskor külön LOADED állapotot naplóz. A tartós megőrzést új hardveres bootnaplóval kell igazolni.
