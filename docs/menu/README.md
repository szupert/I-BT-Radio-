# Menüellenőrzés — V166-R33

Legutóbbi mérés: **2026-10-11**, `STRESS_20261011_145104_497`. A négy manifestfájl és a ZIP tartalma SHA256-ellenőrzött.

**A menüellenőrzések sikerültek, a teljes stabilitási kör FAIL.** A 600 másodperces célból 424 másodperc teljesült: `PLAYBACK_OR_ROUTE_LOST`. A lejátszás 14:59:21-kor (Budapest) leállt, miközben a Bluetooth még kapcsolódott. Az előző mintákban a stream puffer 28 132 → 964 → 0 bájtra fogyott. A kiváltó ok még nincs igazolva.

| Ellenőrzés | Eredmény | Bizonyíték / korlát |
|---|---|---|
| Állomások, Hang, Bluetooth, BT-info, Beállítások, Kijelző navigáció | 🟢 PASS | Hat almenü tényleges RAM-only navigációja |
| Próbaérték-módosítás és visszaállítás | 🟢 PASS | 4 RAM-only módosítás, restored=1; NVS-mentés nem tesztelt |
| Menüsorok kirajzolása | 🟢 PASS, csak rajzolás | 22 oldal, 128 külön sor, 500/200/50 ms; kilenc sor disabled |
| SD→DAC és visszaváltás rádióra | 🟢 PASS, rövid kontroll | A szakaszok lefutottak; nem hosszú hangstabilitási igazolás |
| Bluetooth kapcsolódás, internetes rádió→BT 32 kHz | 🟢 PASS, kapcsolódás | 32 kHz tárgyalva, a kapcsolat létrejött |
| Folyamatos internetes rádió→BT lejátszás | 🔴 FAIL | Lejátszás leállt; pufferkifogyások is rögzítve |
| Lezárás és eredeti rádió/DAC út visszaállítása | 🟢 PASS | DAC_WEB_ORIGINAL_STATION_CONFIRMED |
| WebUI, valódi rotary/érintés, BT-IN, gyári törlés, kalibráció | ⚪ NOT_RUN | Nem részei a végrehajtott ellenőrzésnek |
| A többi menüsor funkcionális aktiválása | ⚪ NOT_RUN | RENDERED nem bizonyít működő funkciót |

[Menüsoronkénti rajzolási lefedettség](menu-render-coverage.csv) · [Ellenőrzött eredményösszefoglaló](latest-result.json)

A rajzolási scan nem aktiválja a menüpontokat. A táblázat ezért külön jelöli a navigációt, módosítást és puszta megjelenítést. Az artwork és az MP3 offload ebben a mérésben kikapcsolt állapotú volt. A napló nem igazolja a hallható hangfolytonosságot.

## Teszt indítása

`21_MENU_ELLENORZO_SELFTEST.bat` a 19-es MENU10 FIX3 tesztet indítja WebUI nélkül. A 20-as indító WebUI-terheléssel mér. Automatikus állapotlekérdezés/indulás és lejátszás-előkészítés; 10 perc mérési cél, indulás és visszaállítás ezen felül. Q: leállítás. Windows PowerShell szükséges, firmware-feltöltés nincs.

Az új RETURN_TO_EVA ZIP-et kell visszaadni. Naplómásolat SYSTEM/LOGS alatt is megmarad. A RUN_STATUS régi RUNNING_OR_INTERRUPTED jelölése nincs a manifestben; a lezárt RESULT, cleanup és hash-ellenőrzött ZIP alapján értékelünk, az eredeti fájlt nem írjuk át.
