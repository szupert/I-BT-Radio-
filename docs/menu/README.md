# Menüellenőrzés — V166-R33

Legutóbbi mérés: **2026-10-11**, `STRESS_20261011_151229_617`. A négy manifestfájl és a ZIP tartalma SHA256-ellenőrzött.

**A teljes 600 másodperc lefutott.** A korábbi lejátszásleállás nem ismétlődött meg. A runner eredménye PARTIAL_REVIEW_REQUIRED: a mérési időkorlát a lezárást is megszakította, ezért a végső rádió/DAC visszaállítás nincs igazolva. Ezt a FIX4 runner javítja; offline ellenőrzés PASS, új fizikai kontroll NOT_RUN. Firmware-csere nem kell.

| Futás | Aktív idő | Lejátszásleállás | Végső visszaállítás |
|---|---|---|---|
| [Első](results/STRESS_20261011_145104_497.json) | 424/600 s | FAIL | PASS |
| [Ismétlés](results/STRESS_20261011_151229_617.json) | 600/600 s | Nem észlelt | UNCONFIRMED, runner-időkorlát |

A BT ténylegesen 32 kHz-en ment. A minták 4 pufferkifogyást rögzítettek a kezdeti BT-szakaszban és 47-et a monitor alatt. Core0 átlag 78,24%, Core1 93,62% a monitor LOAD-mintáiban. Ez nem szakadásmentességi PASS, a hallott eredményt a napló nem helyettesíti.

| Ellenőrzés | Eredmény | Bizonyíték / korlát |
|---|---|---|
| Állomások, Hang, Bluetooth, BT-info, Beállítások, Kijelző navigáció | 🟢 PASS | Hat almenü tényleges RAM-only navigációja |
| Próbaérték-módosítás és visszaállítás | 🟢 PASS | 4 RAM-only módosítás, restored=1; NVS-mentés nem tesztelt |
| Menüsorok kirajzolása | 🟢 PASS, csak rajzolás | 22 oldal, 128 külön sor, 500/200/50 ms; kilenc sor disabled |
| SD→DAC és visszaváltás rádióra | 🟢 PASS, rövid kontroll | A szakaszok lefutottak; nem hosszú hangstabilitási igazolás |
| Bluetooth kapcsolódás, internetes rádió→BT 32 kHz | 🟢 PASS, kapcsolódás | 32 kHz tárgyalva, a kapcsolat létrejött |
| Folyamatos internetes rádió→BT lejátszás | ⚪ Hangfolytonosság nem igazolt | A 600 s lefutott; 4 + 47 megfigyelt pufferkifogyás |
| Lezárás és eredeti rádió/DAC út visszaállítása | ⚪ UNCONFIRMED | MENU10_WINDOW_END runnerhiba; FIX4 még fizikai kontrollra vár |
| WebUI, valódi rotary/érintés, BT-IN, gyári törlés, kalibráció | ⚪ NOT_RUN | Nem részei a végrehajtott ellenőrzésnek |
| A többi menüsor funkcionális aktiválása | ⚪ NOT_RUN | RENDERED nem bizonyít működő funkciót |

[Menüsoronkénti rajzolási lefedettség](menu-render-coverage.csv) · [Ellenőrzött eredményösszefoglaló](latest-result.json)

A rajzolási scan nem aktiválja a menüpontokat. A táblázat ezért külön jelöli a navigációt, módosítást és puszta megjelenítést. Az artwork és az MP3 offload ebben a mérésben kikapcsolt állapotú volt. A napló nem igazolja a hallható hangfolytonosságot.

## Teszt indítása

`21_MENU_ELLENORZO_SELFTEST.bat` a 19-es MENU10 FIX3 tesztet indítja WebUI nélkül. A 20-as indító WebUI-terheléssel mér. Automatikus állapotlekérdezés/indulás és lejátszás-előkészítés; 10 perc mérési cél, indulás és visszaállítás ezen felül. Q: leállítás. Windows PowerShell szükséges, firmware-feltöltés nincs.

Az új RETURN_TO_EVA ZIP-et kell visszaadni. Naplómásolat SYSTEM/LOGS alatt is megmarad. Az eredeti méréseket nem módosítom; a sikeres menübejárás és a teljes kör lezárása külön eredmény.
