# LIGHT R296 — fejlesztési állapot
Frissítve: 2026. október 28.

A **LIGHT PSRAM nélküli**, a **PRO PSRAM-os** profil. Ez a lap fejlesztési tájékoztató; a nyilvános R270 telepítőt és forráspillanatképet ez a dokumentációs frissítés nem cseréli le.

## Amit készüléken már ellenőriztünk

Az R294 rövid, 13 perces célzott próbájában az SD-s MP3, a támogatott 44,1 kHz / 16 bites FLAC és a Budapest Rock Wi-Fi → DAC lejátszása hallható volt. A felhasználó a VU mozgását is megerősítette. Három hálózati indítás/visszatérés során nem ismétlődött meg a korábbi TLS-memóriafoglalási hiba. Ez nem hosszú stabilitási minősítés, és nem az R296 mérési eredménye.

## Kapcsolható mono/sztereó DSP

Az új helyi jelöltben öt szűrő, csúcshatároló és hangerőfüggő basszusvédelem szerepel. A DSP induláskor kikapcsolt; a Hang menüből és a WebUI-ból állítható. A bal és jobb csatorna külön szűrőelőéletet használ, nagy új hangpuffer nélkül. Az érték alkalmazása és tartós mentése külön visszajelzést kap; mentés csak leállított lejátszásnál történik.

Ez még nem állítható kompresszor vagy a hangszóró watt/ohm adataira épülő fizikai védelem.

Az R295 első készülékes DSP-próbája hibát talált: a CPU-kapu ezrelékes adatát százalékos küszöbhöz hasonlítottuk. Emiatt a DSP nem kapcsolt be; ezt a próbát nem tekintjük sikeres DSP-mérésnek. Az R296-ban a küszöb 800 ezrelék, azaz a tervezett 80%. A határértékek gépi ellenőrzése és a teljes fordítás sikeres.

**Az R296 feltöltése, az ellenőrzőösszeg és a teljes boot igazolt; a beállítások megmaradtak. A folyamatos készülékes selftest 2026. október 28-án 18:14 UTC-kor elindult. Az első SD-s MP3-dekódolás és menüjárás megfigyelt, a DSP Ki / Mono / Sztereó összevetése folyamatban van. Az első Wi-Fi-indítás kapcsolódási/dekódolási időtúllépéssel zárult; a teszt ezt hibaként megőrizte és továbblépett. Ez még nem teljes stabilitási vagy hallási PASS.**

| Fordítási adat, R296 Light4MB | Érték |
|---|---:|
| Fix DRAM | 70 752 bájt |
| Firmware-kép | 2 863 648 bájt |
| Szabad hely az alkalmazáspartícióban | 282 080 bájt |
| Hangfeladat veremmérete | 12 288 bájt |

A fix DRAM és a szabad flashhely nem futás közbeni szabad RAM. Az általános 500/2000 ms-os hangvédelmek megmaradtak.

## Folyamatos selftest és nyitott tételek

A teszt terve Wi-Fi/SD → DAC, menüjárás, RAM/verem, külön DSP Ki / Mono / Sztereó ablakok, SD → BT MP3 és BT → DAC mód. Telefonos forrás hiányában a BT-bemeneti hangpróba nem minősül sikeresnek. A Wi-Fi → BT fejlesztését egyelőre félretettük. Hiba esetén a futtató naplóz, vár és folytat; önálló reset vagy újraflashelési hurok nincs.

Nyitott marad az üzem közbeni 50%-os pufferminimum, az átmeneti WebUI-kapcsolatbontás, egyes nagy/24 bites FLAC-ok és a BT memóriaigénye, a fényerő/infra befejezése, valamint az új DSP tényleges terhelésének és hangjának összevetése. A legutóbbi hálózati próbában kapcsolódási időtúllépés is jelentkezett; ezt nem minősítjük DSP-hibának.

**Fejlesztői béta, nem végleges stabil kiadás.**
