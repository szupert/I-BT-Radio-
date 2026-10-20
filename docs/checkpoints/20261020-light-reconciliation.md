# SMALL Light — dokumentációs checkpoint, 2026-10-20

Ez lezárt státuszpillanatkép, **nem második master roadmap**. Az egyetlen aktív terv:
`/AI-Projects/I-BT-Radio/CHECKPOINTS/I-BT-Radio_SMALL_WROOM_PSRAM_ROADMAP_R05_20261006.md`.
A közös leírás a Dropbox WORK_README.md; START_HERE és CHECKPOINT csak átadás.

## Kiinduló állapot

- Repository: szupert/I-BT-Radio-, main baseline: 5e6e090ff054097e0f3b2132326e77b1f4d42a99.
- A main követelményei ehhez a commithoz rögzítve olvasva:
  AGENTS.md, LIGHT_PERFORMANCE_OPTIMIZATION.md, REVERSIBLE_CHANGE_POLICY.md.
- Ez a checkpoint külön dokumentációs ágon készül; nincs main-merge, firmware-módosítás vagy flash.
- A helyi teljes firmware-forrás verziója R171, a kiadott Light4-é R170.
  Ezek a forráskönyvtárak nem Git-checkoutok: saját ág/HEAD nincs igazolva.
  A dokumentációs SHA-t nem lehet a helyi BIN forráscommitjának nevezni.
- A külön publikálási checkout main HEAD-je b3db54214a603685712d6c0305b3257eaf64f85a,
  módosított és nem követett fájlokkal; nem történt pull/reset/rebase/stash/checkout.
- Teljes R171 rev1 BIN: 877452785025f4e52c0ef0fdff9c71ac3c512a9658319f6eac968c3f4fc42ee8.
- Kiadott Light4 R170 BIN: 533b2e940d2bef20bb6406926309656e979e3d9dd5792fa129931505176917cc.
- Helyi, R172 munkacsomagnak nevezett navigációs jelölt:
  7e3da850749a32a63333af8db4ef8dd99073e1d5af8f741896402c298a48e2c8.
  Belső verziója még R170; host/lite4 build PASS, fizikai NOT_RUN, nincs USER-ben.
  A GitHub-terv R172 MP3-hibahivatkozásával való azonossága nincs igazolva.

## Összevezetés és érvényes prioritás

A Dropboxban egy aktív roadmap maradt. Hét átadás/részterv tartalma deduplikálva összevetve;
a régi résztervek REFERENCE státuszt és központi hivatkozást kaptak.
Az eredeti bájtok a fájlok történeti részében megmaradtak, nincs törlés vagy új archívummásolat.
Tíz távoli Dropbox-visszaolvasás a friss blokkok pontos egyezését igazolta.

Elsődleges a PSRAM nélküli SMALL Light stabil mag. 500 ms PCM, 64 KiB DRAM-védelem,
max.5 FPS és a meglévő RAM/BLE/GPIO/PSRAM/partícióhatárok maradnak. IR egyelőre KI.
A működő 6404L referencia védett, PSRAM64H külön nyitott eltérés; hamisítás nem bizonyított.

A telefonos és Garmin Fénix 7X alkalmazás **DEFERRED**, nem része a stabil rádió kiadásának.
Az R171 állomásszerkesztő/import/export/CSV-egyesítés ALREADY_PRESENT; nem készül második.
A meglévő teszteket és a működő firmware-t ez a dokumentációs kör nem módosította.

## WEBUI_VERSION_GUARD — TERV / NOT_RUN

A szerver három külön értéket közöl: fő firmware-verzió, R-release, WebUI asset/build tag.
Bármelyik eltérésekor a következő WebUI-megnyitás/újranyitás egyszeri, verziózott
HTML/JS/CSS navigációt vált ki. Megnyitáskor egy kis no-store verziókérés,
legfeljebb egy automatikus reload; nincs periodikus polling vagy végtelen reload.
Változatlan verziónál nincs reload. Offline/timeoutban marad a meglévő felület,
következő kézi megnyitáskor újrapróba. A HTML no-cache, az asset URL buildtaggel verziózott.
Meglévő ETag/cache- és verziókezelés auditja előzi az implementálást.
Ez nem firmware-újraflash, rádióreset vagy hangútváltás.
Elfogadás: három külön eltérési eset, azonos/offline/timeout/tartós eltérés és MP3-underrun kontroll.

## Lezárt mérési eredmény és korlátja

2026-10-20 20:35 Budapest: 42 gyermek lezárt, 3 teljes tervkör.
28 DECODED_PLAYBACK_NOT_STABLE, lejátszási fázis0, menüwalk0.
A rádió R171-ként válaszol, skinek állapotváltása megfigyelt, de az előre/vissza és
Wi-Fi/SD/BT lejátszási körök NOT_RUN. Station0/decoded0: hiányzó vagy hibás
állomásindex okhipotézis, külön listaolvasás nélkül nem gyökérok-PASS.
BT-IN hangadat0; BLE enabled0/devices0. A hat kimaradt BLE RETURN-riport SYSTEM
mastere visszanyerhető volt; a host Get-FileHash hiánya törte meg a másolást.
A Service90s probe csak60s-t kapott a cap130−cleanup70 miatt; teljes HTTP-lefedettség nincs.
A lejátszás előtti némítás readiness-FAIL esetén nem állt vissza: teszt-restore hiba.

Service/Web megfigyelt átlag CPU0 kb.43,8%, CPU1 0,7%;
BT-IN hang nélküli várakozás kb.60%/57%. Ez nem aktív audio-összevetés.
Belső szabad RAM az inicializálás után Service34 292 B, BT-IN33 536 B;
largest32 756 B, kijelző-HWM minimum2564 B. A későbbi azonos állapotok közel állandók;
folyamatos leak nincs bizonyítva. A kezdeti ~47 KiB megtartott belső foglalás vizsgálandó.
A lezárt naplókban reset/WDT/heap/stack-panic0, mem_errors0; ez nem teljes stabilitási PASS.
A futó nonstop teszt érintetlen, nincs új COM-tulajdonos vagy külön fizikai teszt.

## Újraszámolt fejlesztői órák

| Hátralévő munka | Nettó óra |
|---|---:|
| Verzió/forrás/Git-baseline és visszaállítási pont | 2–3 |
| Readiness, hostexport, szakaszidő javítása | 2–4 |
| NoPSRAM MP3-foglalás gyökérok és javítás | 5–10 |
| Wi-Fi/SD-DAC és két BT-szerep stabilizálása | 7–12 |
| Menü/módváltás regresszió lezárása | 4–7 |
| Light részleges UI, eseményvezérelt háttér | 4–7 |
| Háromrészes WebUI-verzióőr | 2–3 |
| Kontroll/regresszió/csomag/dokumentáció | 6–10 |
| **Összesen** | **32–56** |

25% bizonytalansági tartalékkal **40–70 nettó óra**. Tervezési becslés, nem óranapló
vagy naptári vállalás. A már elkészült R171-szerkesztő és host/build próbák újramunkája,
telefon/Garmin, új hardver és PSRAM64H-javítás nincs benne. Soak és emberi válaszra várás
nem nettó fejlesztői idő. A 16 MB/noPSRAM csomag külön regressziója kb.+2–4 óra,
ha a közös kód nem igényel új javítást. Az MP3 gyökérok lezárása után újrakalibrálandó.

## Checkpoint, rollback, következő egyetlen feladat

- Ok: GitHub követelmények és részleges Dropbox-állapotok összehangolása;
  a build/host/készülék eredmények és firmware-azonosítók szétválasztása.
- Teszt: dokumentum-deduplikáció és helyi hash/visszaolvasás PASS;
  10 Dropbox távoli pontos blokkegyezés PASS. Aktív USER/runner hash változatlan.
- Kód/build/flash/reset/NVS-művelet ebben a körben0. Új fizikai teszt NOT_RUN.
- Helyi részletes bizonyítékok: R171_NONSTOP_CLOSED_ANALYSIS.json,
  R171_R172_ROADMAP_RECONCILIATION.json, R171_R172_RECONCILIATION_CLOUD.json.
- Rollback: ennek az izolált dokumentációs commitnak a revertje vagy ágának elvetése;
  Dropboxnál a checkpoint előtte/utána SHA és prefix/suffix bájthossz ellenőrzésével
  az eredeti bájtok visszanyerhetők. Módosult fájlt nem írhat felül vak rollback.
- Következő egyetlen feladat: valódi verziózott Light-forrásbaseline és a meglévő
  MP3-foglalási bizonyíték összerendelése; utána egy célzott noPSRAM gyökérokelemzés,
  külön ágas commit/checkpointtal. Fizikai próbát külön egyeztetünk.
