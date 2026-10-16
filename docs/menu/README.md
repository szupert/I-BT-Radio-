# Menü és kezelés — V166-R125 BETA

2026. október 16. A funkciók kipróbált és tervezett állapotát a [funkciólistában](../Features.md) vezetem. A menüpont megjelenése önmagában nem jelent kész funkciót.

| Terület | Mire szolgál? |
|---|---|
| Állomások és lejátszási lista | Rádióadó, SD-fájl választása, keresés és listanavigáció. A Radio Browser külön tervezett bővítés. |
| Forrás és hangút | Rádió/SD, DAC vagy megengedett BT-irány. Az internetes rádió→BT tiltott; SD→BT külön ellenőrzendő. |
| Hang | Hangerő és balansz. Az EQ hangfeldolgozását kikapcsoltam. |
| Bluetooth | BT-IN/OUT, keresés, mentett partner, újracsatlakozás, kapcsolati állapot. A sikertelen kapcsolódás visszaeshet DAC-ra. |
| Óra és ébresztő | Időpont, heti napok, engedélyezés, forrás, lejátszási mód, hangerő/fényerő felfutása és lefutása, óralap. |
| Hálózat és idő | Wi-Fi, DNS, NTP-szerverek, időzóna; a menü fejlécében IP-cím. |
| Kijelző | Felület, fényerő, óra/szöveg színe és stílusa, érintéses skinlapozás beállítása. |
| Szenzor és időjárás | Saját BLE-hőmérő, kulcs/MAC, hőmérséklet/pára és időjárási beállítások. Minden adatút fizikai ellenőrzése még nem kész. |
| Service és diagnosztika | Célzott mérések, kapcsolati/puffer/memórianapló, tesztek. Nem kalibrált mérőműszer és nem biztonsági felülbírálás. |

Érintéssel a Menü és a Vissza vezérelhető; listán fel/le húzással lehet görgetni. Rotaryval a kijelölt sor változik, nyomással lehet választani. A rövid, majd hosszú nyomás a menübe visz, óramódban is. A részletes gombfunkció állapotfüggő.

Az óralap felső sávja nyugalomban érzékelőadatot mutathat; interakcióra a menügombok jelennek meg. A régi olvasási idő és hiba nem állandó főképernyős felirat. Az ébresztőikon a teljes ébresztés kapcsolására, a napjelölések a heti napok beállítására szolgálnak; ezek teljes készülékes ellenőrzése még folyamatban.

## Legutóbbi célzott mérés

R124, Budapest Rock→DAC: menü nélkül Core0/Core1 átlag65,95/76,08%, menüvel69,76/77,38%. A lejátszási generáció nem váltott. Menü: 236/237 kérés kirajzolva, teljes bejárás nem történt. A BT-mérés kapcsolódási hiba miatt nem indult el. R125: hostteszt és fordítás sikeres; az új készülékes kontroll még hátravan.

A scan a kirajzolást ellenőrzi; nem nyom meg minden veszélyes menüpontot és nem bizonyítja az összes funkció működését. A korábbi R33 eredményei a results mappában történeti adatok, nem a jelenlegi firmware mérései.
