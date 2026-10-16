# Xiaomi LYWSD03MMC: a bindkey megszerzése

A gyári érzékelő titkosított MiBeacon-adásához MAC-cím és 16 bájtos (32 hexadecimális karakteres) Mi Bind Key szükséges. A Mi Token és a blt. kezdetű Device known id nem helyettesíti ezeket.

## Böngészős aktiválás

1. Bluetooth-os Androidon Chrome böngészőben nyisd meg: https://atc1441.github.io/TelinkFlasher.html
2. Connect, majd válaszd a LYWSD03MMC érzékelőt.
3. A csatlakozás után Do Activation. Firmware-feltöltés nem szükséges.
4. Siker esetén a Mi Bind Key mező értékét mentsd privát helyre. Ismételt aktiválás után a legutóbbi értéket használd.
5. Disconnect: a rádió ezután passzívan fogadhatja az adást.

A böngészős aktiválást 2026. október 16-án egy gyári 1.0.0_0001 firmware-es LYWSD03MMC-vel is kipróbáltam: az aktiválás és a bejelentkezés sikerült. A kulcsot így ki tudtam nyerni; a rádiós mérési adatok vételét ettől külön kell ellenőrizni. Újabb gyári verziók eltérhetnek.

Az aktiválás új kulcsot állíthat be: a Xiaomi Home újrapárosítást igényelhet. Ha a módszer nem támogatott, Xiaomi Home-hozzárendelés után a Xiaomi Cloud Tokens Extractor használható.

## MAC és bizalmas adatok

A MAC hat hexadecimális pár, például AA:BB:CC:DD:EE:FF. A böngésző Bluetooth-eszközazonosítója nem feltétlenül MAC. A Chrome kísérleti funkcióiról szóló állandó felirat önmagában nem hibajelzés. Ha a böngésző nem tudja kiolvasni, a rádió keresési listájából kell azonosítani.

Ne publikálj valódi kulcsot, tokent, Xiaomi-azonosítót vagy személyes MAC-címet. A firmware nem tartalmaz előre beégetett személyes kulcsot.

## MAC-cím megkeresése Windows alatt: NirSoft BluetoothLEView

A saját LYWSD03MMC hőmérőm MAC-címét a **[NirSoft BluetoothLEView – Bluetooth Low Energy Scanner](https://www.nirsoft.net/utils/bluetooth_low_energy_scanner.html)** programmal találtam meg. A futtatható fájl neve **BluetoothLEView.exe**. Ehhez a lépéshez ezt a Windowsos keresőt használtam; a Mi Bind Key kiolvasását külön, a Telink Flasherrel végeztem.

1. Kapcsold be a számítógép Bluetooth-adapterét, és indítsd a NirSoft BluetoothLEView programját: https://www.nirsoft.net/utils/bluetooth_low_energy_scanner.html
2. Várd meg a LYWSD03MMC nevű sort. A First/Last Detected és Counter mező segít ellenőrizni, hogy új hirdetések érkeznek.
3. Másold ki a MAC Address mezőt. Az AA-BB-CC-DD-EE-FF alak a rádió számára AA:BB:CC:DD:EE:FF formára írható át.
4. RSSI alapján megkülönböztethetők a közeli jelöltek, de az önmagában nem bizonyítja a tulajdonost. A saját bindkeyvel hitelesített mérés igazolja az egyezést.

Ez a MAC azonosítására szolgál, a bindkeyt nem nyeri ki. A Scan Response típusú bejegyzés is használható a cím megkeresésére. A saját érzékelőm címét 2026. október 16-án ezzel találtam meg; a személyes MAC-címet nem teszem közzé.

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

Az R115 kipróbálásakor APPLIED és SAVED választ kaptam a cím és kulcs alkalmazására és NVS-írására; az újraindítás utáni visszaolvasást ekkor még nem ellenőriztem. A dekódertask elindítása után 32 196 bájt belső RAM maradt a szükséges 32 768 helyett, ezért a memóriaőr nem engedte elindítani a vételt. R116 a célzott Xiaomi adatot külön JSON-dekódertask nélkül, korlátozott idle-feldolgozással kezeli. A biztonsági memóriahatárok változatlanok. Az R116-tal a tényleges vételt ekkor még nem próbáltam ki; a sikeres fordítás ezt nem helyettesíti.

Az R115 kipróbálásakor a mentett érzékelő nem jelent meg a listában. R116 ezért mentés után azonnal visszaolvassa és összeveti a teljes cím–kulcs rekordot, és induláskor külön LOADED állapotot naplóz. Azt, hogy a mentés újraindítás után is megmarad-e, még külön ellenőriznem kell.
