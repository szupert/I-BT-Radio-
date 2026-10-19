# I-BT-Radio V166-R163 · nyilvános Windows béta

2026. október 19.

**[Telepítő letöltése — Windows x64](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/V166_R163/I-BT-Radio-R163-Install.exe)**

Egyetlen EXE-t kell letölteni és megnyitni. A Python, az esptool és az öt szükséges flashkép benne van; külön Python-telepítés és a feltöltéshez internetkapcsolat nem szükséges. Ez USB-s feltöltő, nem webes OTA. A firmware és a skinek forráskódját nem tettem a csomagba. A harmadik féltől származó összetevők licencszövegei ebben a mappában vannak.

## Milyen rádióhoz készült?

Kizárólag a projekt **SMALL ESP32-WROOM, 16 MB flash + PSRAM, 320×240-es CYD/Yellow Board kijelző és külső PCM5102A DAC** összeállításához, az itt dokumentált bekötéssel. Nem általános ESP32-firmware: gyári 4 MB-os CYD-ra, ESP32-S3-ra és BIG kijelzőre nem való. A feltöltő a flashméretet ellenőrzi, de a vezetékezést, a PSRAM bekötését és a kijelző típusát nem tudja előre igazolni.

**Feltöltés előtt: [hardver és kötelező panelmódosítások](../../hardware/README.md).** A saját kialakításomban az alaplapi RGB LED-et eltávolítottam; a hozzá tartozó jelek a DAC/PSRAM/rotary feladatát látják el. A GPIO-kiosztás változatlan.

## Telepítés

1. Töltsd le az EXE-t a fenti hivatkozásról, és indítsd el Windows x64 gépen. Nincs telepítés a Program Files mappába és nincs szükség rendszergazdai indításra. Windows 11 x64-en ellenőriztem az indítást; más Windows-változaton még nem.
2. Zárd be a soros monitort és a többi feltöltőt. Csatlakoztasd a rádiót adatátvitelre alkalmas USB-kábellel. Ha a Windows nem ismeri fel az USB–soros átalakítót, annak megfelelő gyártói illesztőprogram szükséges.
3. A hardver ellenőrzése után nyomj Entert. A feltöltő megkeresi a COM-portot; több eszköznél választást kér. Hiányzó USB esetén vár, Ctrl+C-vel megszakítható.
4. A bootloaderhez szükség esetén tartsd nyomva a BOOT gombot, nyomd meg röviden az EN/RESET gombot, majd engedd el a BOOT-ot. Kövesd az ablak jelzéseit.
5. Megfelelő meglévő partíció esetén csak a firmware frissül, a beállítások megmaradnak. Üres vagy eltérő flash esetén teljes telepítést ajánl fel. Ehhez külön **IGEN** választ kell beírni: a flashen lévő Wi-Fi-beállítások, állomáslista és egyéb beállítások törlődnek. Az SD-kártyát nem formázza.
6. Várd meg a valódi feltöltési százalékot, az összes kép ellenőrzését és a **PASS_FLASH_HASH_BOOT** eredményt. Az ablak nyitva marad. Hiba esetén csak kézi R választás indít új próbát; nincs automatikus újraformázás vagy újrafeltöltés.

Az alkalmazás jelenleg nincs digitálisan aláírva; ezt a Windows jelezheti. A fájl SHA-256 ellenőrzőösszege a [SHA256SUMS.txt](SHA256SUMS.txt) fájlban található.

## Első indulás

Teljes telepítés után csatlakozz a rádió **I-BT-Radio-Setup** hálózatához, és nyisd meg a **http://192.168.4.1/** címet. A hálózathoz szükséges adatot a rádió kijelzőjén találod. Állítsd be a saját Wi-Fi-t, majd adj hozzá állomáslistát a webes felületen. A csomag tiszta fájlrendszert tartalmaz, saját Wi-Fi-jelszavam, érzékelőkulcsom és mentett állomáslistám nincs benne. A bootellenőrzés legfeljebb 180 másodpercet vár; a sikertelen ellenőrzést nem nevezi sikeres feltöltésnek akkor sem, ha az írás már megtörtént.

## Mi változott?

- Az állomáslista-indexelést 128 bájtos blokkokra bontottam, köztük tényleges CPU-átadással. A 480 soros indexelés a készüléken 814 ms alatt lefutott, a korábbi watchdog nem tért vissza.
- Benne vannak az SD-fájlváltás és a menü korábbi javításai, a vintage rajzolás veremcsökkentése, a webes tömörítés, valamint az XP/Windows 7 bootnézet és asztal.
- Az MP3 sztereó forró kódrészét belső utasításmemóriába tettem, a háttérben futó memóriavizsgálat gyakoriságát csökkentettem. A friss belépési ellenőrzéseket megtartottam.
- Az 500 ms-os hangvédelem, a memóriahatárok és a GPIO-kiosztás változatlan. EQ kikapcsolva; Wi-Fi-hang → Bluetooth tiltva.

## Ismert hibák — tesztelésre, nem stabil kiadás

**A Wi-Fi → DAC hang menü nélkül és menüjárással is hallhatóan szaggat.** Budapest Rockkal a menü nélküli átlagos PCM-előállítás 99,8% volt, de ez nem jelent folyamatos hangot. Menüjárás alatt a bemeneti puffer kiürült és a stream újracsatlakozott. A Core1 átlagterhelése 57–58% volt; a hiba okát még vizsgálom.

A Windows 7 bootanimáció villogása szintén nyitott hiba. Hosszú R163 stabilitási teszt nem készült. A 262 menüsor kirajzolási visszajelzése nem minden menüművelet teljes tesztje.

## Ellenőrzések

Az R163 fordítása és az indexelő 35 hostesete sikeres. A készülékre feltöltött firmware verziója és hash-e ellenőrzött, az indexelő watchdog a rövid próbában nem jelentkezett. A kijelző mért veremtartaléka legalább 3160 bájt, a hangfeladaté 11636 bájt volt, memóriahiba nélkül. A hangteszt mindkét szakasza FAIL; ezt nem minősítem stabil kiadásnak.

Az önálló EXE csomagellenőrzését és esptool-futtatását külön Python nélkül, a feltöltő 17 szimulált esetét COM-port nélkül ellenőriztem. Ezzel az új EXE-vel fizikai feltöltést nem végeztem. A telepítő nem indít automatikus stressztesztet.

Hibajelzéshez az [Issues](https://github.com/szupert/I-BT-Radio-/issues) oldalon írd meg a panelt, a firmware-verziót, a skint, a hangforrást és kimenetet, valamint a hiba előtti lépéseket. Külön jelezd, hogy hallottál-e hangakadást, újraindult-e a készülék, vagy csak a webes oldal várt. Képet is csatolhatsz.

A telepítési naplók a **%LOCALAPPDATA%\I-BT-Radio\Logs** mappába kerülnek. Közzététel előtt nézd át őket, és távolítsd el a személyes adatokat; a telepítő nem küld fel automatikusan naplót. A SHA-256 a letöltött fájl egyezését ellenőrzi, nem digitális aláírás.

Haladó opciók: `--verify-only` csak a beépített csomagot ellenőrzi, COM-portot sem nyit; `--port COM4` kézi port; `--recovery` 115200 baudú ROM-helyreállítás; `--fresh` teljes telepítést kér, továbbra is külön IGEN megerősítéssel.
