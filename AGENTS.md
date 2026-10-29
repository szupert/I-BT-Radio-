# Kötelező fejlesztői és AI-ügynök utasítások

Minden firmware-, kód-, hardverhibakeresési és dokumentációs feladat ELŐTT olvasd el: [docs/REVERSIBLE_CHANGE_POLICY.md](docs/REVERSIBLE_CHANGE_POLICY.md).

- A működő firmware, pinout, partíciók, PSRAM-profil és korábbi fizikai PASS eredmények védettek.
- Előbb a meglévő checkpoint és bizonyíték; egyszerre egy célzott hiba. Új globális keresés csak konkrét nyitott bizonytalansághoz.
- Módosítás előtt baseline commit SHA, firmware-hash és hardverprofil rögzítése; külön ág és visszaállítási terv. Nem írunk közvetlenül a stabil ágra.
- Minden változás külön commitban: ok, érintett fájlok, teszt, PASS/FAIL/NOT_RUN és rollback-pont.
- BUILD PASS után checkpoint; fizikai PASS csak valódi mérés alapján. Két sikertelen kör után gyökérokelemzés.
- Ne legyen automatikus firmware-flash, NVS-törlés vagy hardverváltoztatás jóváhagyás nélkül.
- Átadáskor rövid START_HERE/checkpoint frissítés, következő egyetlen feladat és pontos visszaállítási út.

A PSRAM64H írás-visszaolvasási hibája nyitott, a működő 6404L konfigurációhoz nem nyúlunk. A két chipre vonatkozó állításokat külön tesztbizonyítékkal kezeljük.

## Nyilvános kiadások
Csak az adott forrásból és binárisból készült, teljes releváns készülékes teszten PASS eredménnyel lezárt verzió publikálható. A build, hostteszt, feltöltési hash, részteszt vagy RUNNING állapot nem teljes PASS. BOOT/feltöltésre várás, helyi tesztjelölt, fejlesztési lépések, következő feladatok és feltételes ígéretek belső dokumentációba kerüljenek. A nyilvános leírás a kiadott verzió tényleges viselkedését és lényeges korlátait ismertesse. Korábbi hibás vagy hiányosan tesztelt kiadást tilos átnevezéssel PASS-nak minősíteni.
