# SMALL Light — Performance Optimization (ellenőrzött terv)
Dátum: 2026-10-20. Státusz: TERV / NOT_RUN. Hatály: PSRAM nélküli ESP32 Light. Kötelező: [visszavonhatósági szabályzat](REVERSIBLE_CHANGE_POLICY.md).

## Változatlan védelmek
- 500 ms hangvédelmi tartalék, 64 KiB belső DRAM-védelmi küszöb, jelenlegi max. 5 FPS. Ezeket ne csökkentsd optimalizálás címén.
- Meglévő működő pinout, telepítő, partíciók, hangútvonalak, konfiguráció és checkpointok megőrzendők.
- Hangkimaradás, underrun, reset, vagy memóriahatár-sértés esetén az optimalizáció FAIL; visszaállítás.

## Mérési sorrend (ne párhuzamosítsd)
1. R171 fizikai eredmények lezárása; aktuális firmware/hardver hash és állapot rögzítése.
2. R172 MP3Decoder_AllocateBuffers hiba célzott gyökérokelemzése; mérd a belső 8 bites szabad RAM-ot, legnagyobb blokkot, minimumot a foglalás előtt/után. Ne gyengítsd a 64 KiB védelmet.
3. Közös rövid alapmérés: 320 kbit/s MP3 SD és Wi-Fi lejátszás, kijelzőgörgetés/érintés, WebUI válaszidő, core0/core1 és task CPU-idő, heap, stack minimum, buffer underrun, SD/network késleltetés. Azonos tesztadatok és állapot; naplózás ne blokkolja a hangot.
4. Legnagyobb igazolt szűk keresztmetszet egyetlen célzott javítása. Jelöltek: részleges kijelzőfrissítés, SPI-forgalom csökkentése, WebUI blokkolások eltávolítása, eseményvezérelt háttérmunka, fölös heapfoglalás/fragmentáció megszüntetése, Wi-Fi/BLE coexistence és task affinity finomhangolás. Ne változtass vakon prioritást, SDK-t vagy codecet.
5. Azonos kontrollmérés; csak valódi gyorsulás és változatlan hangvédelem esetén PASS, külön commit és checkpoint. Két sikertelen kísérlet után stratégiaváltás.

## Hivatalos verifikáció
- ESP-IDF heap_caps_get_free_size / get_largest_free_block / get_minimum_free_size: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/mem_alloc.html
- ESP-IDF speed optimization: Wi-Fi/BT rendszerfeladatok magas prioritása és maghozzárendelési kockázat: https://docs.espressif.com/projects/esp-idf/en/v5.2.4/esp32/api-guides/performance/speed.html
- ESP32 Wi-Fi/BT/BLE coexistence időosztás: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/coexist.html
- ESP-IDF v5.5 konfiguráció: CONFIG_ESP_COEX_SW_COEXIST_ENABLE: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/kconfig-reference.html

A források az API-kat és optimalizálási irányokat igazolják, **nem** a mi R172 firmware-ünk mért gyorsulását. A 320 kbit/s yoRadio 0.9.420 referencia más firmware, nem Light PASS.

## WEBUI_VERSION_GUARD — egyszeri ellenőrzés frissítés után (TERV / NOT_RUN)
- NINCS 60 másodperces polling és nincs folyamatos verziófigyelés. A szerver a firmware/WebUI buildazonosítójának hiteles forrása.
- A böngésző a WebUI megnyitásakor vagy újranyitásakor **egyszer** lekéri a kis méretű, `Cache-Control: no-store` verzióvégpontot, és összeveti a betöltött UI buildazonosítójával. Ez biztosítja, hogy a firmware-frissítés utáni első látogatásnál a régi cache felismerhető legyen; ha a lap nyitva marad, automatikus frissítés nem garantált polling vagy push nélkül.
- Eltéréskor egyszeri navigáció az új verziózott HTML/JS/CSS URL-re; verziózott statikus erőforrásoknál hosszú cache engedett, a fő HTML legyen újraellenőrzött (`no-cache`), dinamikus rádióállapot `no-store`.
- Maximum **egy** automatikus újratöltés az adott lapbetöltési ciklusban; tartós eltérésnél figyelmeztetés, nincs végtelen reload. Hálózati timeout esetén a meglévő felület marad, és következő kézi megnyitáskor próbál újra.
- Ne építs új párhuzamos verziókezelést: előbb ellenőrizd a meglévő R172 HTTP fejléceket és FW/WebUI buildazonosítást. Nincs flash/NVS írás, hangmegszakítás vagy nagy RAM-puffer.
- Elfogadás: frissítés után első új megnyitás helyes UI-verziót tölt; változatlan verziónál nincs extra reload; offline/timeout esetén nincs loop; MP3 alatt nincs új underrun. Fizikai teszt még NOT_RUN.

## Verzióazonosítás pontosítása — 2026-10-20
- A szerver külön közli a **fő firmware-verziót**, az **R release-számot**, és a **WebUI asset/build tag** értékét. A böngészőben betöltött megfelelő értékekhez hasonlítjuk őket.
- Ha **bármelyik** eltér, a WebUI új verziójú erőforrásokra navigál, legfeljebb egyszer az adott megnyitás során. A megnyitáskori egyszeri lekérésen túl nincs periodikus polling.
- A szerververziót cache nélkül kérjük, a HTML újraellenőrzendő, a CSS/JS URL buildtag alapján verziózott. Frissítéskor ugyanaz a főverzió és R szám mellett megváltozott asset tag is kiváltja a frissítést.
- Ez kizárólag WebUI újratöltés; **nem** indít firmware-újraflashelést, rádió-resetet vagy hangútvonal-váltást.
