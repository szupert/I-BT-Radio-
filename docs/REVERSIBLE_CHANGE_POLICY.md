# Visszavonható fejlesztési és naplózási szabályzat

Dátum: 2026-10-20. Hatály: I-BT-Radio SMALL/Light/Pro16, firmware, telepítő, tesztek és dokumentáció. Minden fejlesztőnek és AI-ügynöknek kötelező.

1. **Kiinduló állapot:** változtatás előtt rögzítsd a repositoryt, ágat, HEAD commit SHA-t, firmware-verziót, hardverprofilt, kapcsolódó checkpointot és a reprodukálható hibát. Ne nevezd stabilnak azt, aminek nincs fizikai PASS bizonyítéka.
2. **Izoláció:** minden hibához külön ág; az alapágat, a működő 6404L-konfigurációt, pinoutot és stabil BIN-t ne módosítsd közvetlenül. Egy körben egy konkrét hibát javíts.
3. **Változásnapló:** minden commitnál legyen cél, érintett fájlok, előtte/utána viselkedés, kockázat, tesztparancs vagy mérés, eredmény (PASS/FAIL/NOT_RUN) és rollback-pont. Ne tárolj titkokat vagy nagy bináris naplókat Gitben.
4. **Teszt:** először célzott ellenőrzés; build PASS után azonnali checkpoint. Fizikai teszt nélkül ne állíts hardveres PASS-t. Sikertelen kísérletnél rögzítsd a próbát, eredményt, okhipotézist és a következő kör lényegi különbségét. Két sikertelen kör után gyökérokelemzés, nem vak ismétlés.
5. **Visszaállítás:** őrizd meg a kiinduló commit SHA-t és a működő firmware SHA-256 hashét. Visszavonáskor preferáld a Git revertet vagy az elkülönített ág elvetését; megosztott történetet ne írj át. Eszközre visszatöltés előtt ellenőrizd a hardverprofilt, partíciókat és az adatmegőrzési feltételeket. Automatikus újraflashelés tilos.
6. **Átadás:** frissítsd a START_HERE/checkpoint és a tesztnapló rövid státuszát: aktuális ág/SHA, bizonyított állapot, nyitott hiba, sikertelen próbák, következő egyetlen feladat, visszaállítás módja. A Dropbox helyi mentését ne tekintsd felhős PASS-nak visszaolvasás nélkül.

## PSRAM64H ismert eltérés
Azonos ESP32-WROOM bekötés, felhúzás és firmware mellett a 6404L memória írás-visszaolvasása a felhasználó beszámolója szerint PASS, a PSRAM64H-nál FAIL jelentkezett. A pontos hibacím és gyökérok még nincs igazolva. A 6404L működő referencia; a 64H külön vizsgálati ág, nem általánosan ajánlott csere. Azonos 40 MHz-es beállítás önmagában nem bizonyítja a kompatibilitást. Sem ellenállást, sem SDK-t, sem működő profilt ne változtass bizonyíték nélkül.

## Commit-sablon
```
Cél:
Kiinduló SHA / firmware hash:
Hardverprofil:
Módosított fájlok:
Hipotézis:
Teszt és eredmény (PASS/FAIL/NOT_RUN):
Rollback (commit/firmware):
Következő lépés:
```
