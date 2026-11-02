# I-BT-Radio V166-R356 — Windows x64 tesztbéta

Nyílt forrású projekt: **szupert / I-BT-Radio**, [projekt és források](https://github.com/szupert/I-BT-Radio-), yoRadio alapokon / GNU GPLv3. Az eredeti szerzői és licencjelölések megmaradnak; a beépített eszközök licencszövegeit a telepítő is tartalmazza.

## Letöltés és használat

[Windows-telepítő letöltése](https://github.com/szupert/I-BT-Radio-/raw/refs/heads/main/beta/V166_R356/I-BT-Radio_BETA_Install.exe). Külön Python nem szükséges. Windows x64, ESP32 SMALL, **4 MB flash / PSRAM nélkül**. Eltérő hardvernél a telepítő elutasítja a feltöltést.

1. Indítsd el az EXE-t, és olvasd el a megjelenő hardver- és csomagellenőrzést.
2. Csatlakoztasd USB-n a rádiót; a telepítő kiírja a BOOT/EN teendőt, ha szükséges. Enter indít, Q kilép.
3. A kiválasztott COM, az ellenőrzött sebesség, a valódi feltöltési százalék és a végső hash/boot eredmény látható. Várd meg a befejezést.

Megfelelő meglévő partícióknál adatmegőrző frissítés. Az üres/eltérő flash teljes telepítését csak a külön, adatvesztést jelző IGEN megerősítés engedélyezi. Az SD-t nem formázza. Nincs automatikus selftest vagy újraflashelési hurok. A napló útvonala a konzolban látható: `%LOCALAPPDATA%/I-BT-Radio/Logs`. Egy második telepítőpéldány nem indíthat új COM-műveletet.

## Kis terhelésű, külön kapcsolható DSP

A rádió webes beállításaiban a DSP modulok alatt elérhető:

- A meglévő öt szűrőn belüli frekvencia, erősítés, Q és szűrőtípus; a válaszgörbét a böngésző rajzolja.
- Négy csillapító gyári profil és három egyéni profilhely. Mentés lejátszás közben csak várólistára kerül, írás leállítás után, visszaellenőrzéssel történik.
- Normalizált M/S sztereószélesség, bal/jobb csillapítás. Sztereó csatornamódban használható; nem emeli a kimeneti csúcsszintet.
- Külön kapcsolható, 1/32 mintavételes bemeneti/kimeneti csúcs-, RMS-, klippelési és limiteradat. Ez nem valódi csúcsdetektor; borítórajzolás és terhelési tiltás alatt nincs friss mérési állítás.

Minden induláskor a DSP és az új modulok kikapcsoltak. Bekapcsoláshoz ismert, magonként 95% alatti terhelés és megfelelő memória szükséges. Az új modulok 95%-nál automatikusan kikapcsolnak, visszakapcsolásuk kézi. Az eredeti általános 95%-os és hangvédelmi korlátok megmaradnak. Nincs háttérben futó webes lekérdezési hurok.

FFT, kompresszor, automatikus erősítés, többkimenetes keresztváltó és analóg ADC-bemenet nem része ennek a SMALL bétának.

## Ellenőrzések és nyitott korlátok

57 helyi tesztcsoport, firmware-kép/csomag/hash-ellenőrzés és a tényleges önálló EXE külön Python nélküli indítása sikeres. Az új modulok kikapcsolásakor 240 000 vizsgált hangminta bitre azonos a korábbi feldolgozással. Ezek a helyi próbák nem igazolnak készülékes hangminőséget vagy hosszú stabilitást.

Az előző R352 készülékes próbáján a kis és teljes 320×240-es SD-borító forrás-CRC és szoftveres rajzolás teljesült; a Poly-mérés 3/6 után egy új képgeneráció megnyitási várakozásánál megállt. A teljes kör és egyes WiFi-adatfolyamok vizsgálata nyitott. Az R356 ezt nem nyilvánítja javítottnak. Az R354 sztereó DSP-próbájának 88,75%-os Core1-átlagánál az eredeti80%-os modulvédelem működött. A felhasználó kérésére az R355/R356 CPU-határa95%; ez önmagában nem új készülékes stabilitásbizonyíték. Az R355 teljes próbájának indulásakor előjött offline SD-CLI parancsfelismerést az R356 normál hangparancssoron át javítja, új készülékes visszamérésre várva. Fizikai képpont-, hallás- és érintésvizsgálat nincs teljesítettnek állítva.

Az ellenőrző összegek a [SHA256SUMS.txt](SHA256SUMS.txt) fájlban vannak. Az EXE `--verify-only` kapcsolója soros port nélkül ellenőrzi a beépített csomagot; `--extract-payload ÚJ_MAPPA` új mappába bontja ki azt. A korábbi [R352 ZIP](../V166_R352/README.md) megmarad történeti bétaként.
