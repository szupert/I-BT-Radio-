# R270 Light4MB · tesztelhető béta

Csak ESP32 4MB/noPSRAM, a dokumentált bekötéssel. Nem ESP32-S3/8048 firmware. A négyprofilos EXE felismeri a flash/PSRAM méretet; csak a Light4 kép új R270, a többi R189/R176. A meglévő 8048 és Pro telepítők külön megmaradnak.

## Mit ellenőriztünk?

Gépi menü/rajzoló-, két koppintási mód-, sorhatár-, boot-részletfrissítés-, hosszúVissza-, valódi MP3 ID3-címke-, BT-húzás-,22 WebUI- és18 verzióvédelmi eset; fordítás és csomaghash/feltöltő-próbák soros port nélkül. Nem hallhatóhang- vagy hardver-PASS. Az utolsó csakolvasó verziólekérdezés a rádión még R268-at mutatott, ezért az új funkciókat nem teszteltként közöljük.

R268 SD–DAC MP3 megfigyelés:94340B szabad belső RAM,90100B legnagyobb blokk,7044/12288B hangverem minimum szabad; CPU0 39,78%, CPU1 59,18% öt egyedi ablak45,097s idővel súlyozva. Ez nem R270-mérés és nem minden hangútra érvényes érték. R270 fordított fixDRAM69640B, IRAMfree25056B; ezek nem szabadheap-értékek.

## Rövid kézi próba

1. Telepítés után rögzítsd a valódi firmware-verziót; a régi bootbanner önmagában nem bizonyíték. Normál frissítés adatmegőrző. Teljes formázást csak a telepítő kifejezett IGEN kérdésének és a törlési tájékoztatónak az elfogadásával válassz.
2. Bootkép villogás nélkül,4pötty, Wi-Fi után2s.
3. Próbáld mindkét Koppintás-beállítást: Bluetooth kijelölve/Időjárás sor érintve. „Kijelölt sor”→Bluetooth; „Érintett sor”→Időjárás. A fejléc Vissza és a húzás ne nyisson másik sort.
4. Állomás-/SD-lista görgetése, sorhatárok, előadó/cím megfelelő ID3-címkés MP3-mal.
5. SD-BT MP3 hangszórón; kereső/Vissza kapcsolatbontás nélkül, majd SD-DAC. Hallhatóságot külön jelezd; a dekóderszámláló önmagában nem hang-PASS.

A firmware nem indít automatikus stressztesztet. Hibajelentéshez verzió, panel/profil, hangút/fájlformátum, pontos menülépések, várt/tényleges viselkedés és személyes adatoktól megtisztított rövid napló kell.

## Négy kiemelt nyitott hiba

1. Wi-Fi→DAC TLS/RAM-hiány és esetenkénti újracsatlakozási hiba.
2. Időjárás API TLS-memóriafoglalás: friss adatlekérés sikertelen lehet.
3. SD→BT FLAC munkatárhiány; további javítása félretéve. SD-DAC FLAC több fájllal hallható volt.
4. Óra/BLE/időjárás mellett korábban magas CPU0-terhelés; célzott azonosmódú profilozás szükséges.

Megmaradó határok: BT32kHz/250ms, általános500/2000ms, hangverem12288B, TX32768/RX8192/PCM4608B, Wi-Fi16KiB/75%, reserve0. Nem minősített stabil kiadás.
