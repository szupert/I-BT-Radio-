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


## 2026-10-22: public blockwise development
Read docs/DEVELOPMENT_BLOCKS.md before changes. One logical block per commit/PR; retain existing implementations and proven behaviour. Light4MB Bluetooth/BLE precedes artwork. Read source/README.md for baseline and diagnostic limitations. Never publish credentials, NVS dumps or personal logs. The later explicit user RAM thresholds supersede the old64KiB planning text; do not reinterpret them.
