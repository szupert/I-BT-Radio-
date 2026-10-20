# Garmin Fénix 7X és Android kliens — tervezett feladat

Dátum: 2026-10-20. Állapot: TERV / NOT_RUN. A Work firmware-munkáját nem módosítjuk.

Cél: I-BT-Radio távirányítás Garmin Fénix 7X Connect IQ alkalmazással és Android-telefonon, közös rádióvezérlő API-n keresztül. Funkciók: állomásváltás, hangerő, play/pause, aktuális állomás és lejátszási állapot. Elsőként vizsgálni kell, hogy a Garmin Connect IQ telefonon keresztüli HTTP-kéréseivel elérhető-e a rádió, így szükséges-e külön Android-háttéralkalmazás.

Becslés (nem mért): Android alap natív kliens 6–12 óra; Garmin Connect IQ 12–24 óra; integráció és fizikai teszt 4–8 óra. Együttesen 22–44 fejlesztői óra. PWA-alternatíva 2–5 óra, ha a meglévő WebUI megfelelő, és nem helyettesíti a Garmin Connect IQ alkalmazást. A tényleges becslést API-audit és Fénix 7X szimulátorteszt után pontosítani kell.

Sorrend: 1) Work aktuális branch/verzió és API ellenőrzése, ütközés nélkül; 2) Connect IQ kompatibilitás és telefonközvetített kapcsolat próbája; 3) közös minimális API-szerződés; 4) Garmin kliens; 5) Android/PWA igény szerinti kliens; 6) fizikai teszt, hangvédelem és rollback.

Minden változtatás külön ágon, checkpointtal és naplóval: [REVERSIBLE_CHANGE_POLICY](REVERSIBLE_CHANGE_POLICY.md). Firmware-t nem módosítunk a Work munkájával párhuzamosan.
