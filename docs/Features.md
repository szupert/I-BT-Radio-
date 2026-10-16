# Funkciók és fejlesztési állapot — V166-R125 BETA

Frissítve: **2026. október 16.** Az I-BT-Radiót egy klasszikus ESP32-WROOM + PSRAM köré építem. A SMALL változatot fejlesztem; a nagy kijelzőn egyelőre a menüdemó működik.

## Ami a készüléken már működött

| Funkció | Jelenlegi állapot |
|---|---|
| Internetes rádió → külső DAC | R124 alatt két, nagyjából kétperces Budapest Rock mérés lefutott; menüvel is haladt a lejátszás. Ez rövid kontroll, nem hosszú stabilitási garancia. |
| SD → DAC | Korábbi készülékes próbákon lejátszott; R124 alatt a BT-próbák előtt is eljutott dekódolt, előrehaladó lejátszásig. |
| Telefon → BT-IN → DAC | Korábban a készüléken is működött. A jelenlegi kiadást ezen az úton még külön ellenőrzöm. |
| Menü és helyi kezelés | Érintés, rotary, rádió- és SD-lista. R90 alatt a menüváltás tiszta volt, legfeljebb 1–2 rövid villanással. R124-ben 236 menüsor kirajzolása igazolt; ez nem az összes menüfunkció végrehajtása. |
| Nagy kijelző menüdemó | ESP32-8048S070C,800×480: érintés, görgetés és visszalépés működött. Nem teljes rádióport. |

## Elkészült, de még ellenőrzöm

| Funkció | Mit tartalmaz, mi van még nyitva? |
|---|---|
| SD → BT-OUT | Korábban elindult, pufferkihagyások is voltak. R124 alatt a kapcsolódás szolgáltatásfelderítési hibánál megállt; ebből a futásból nincs BT-hangmérés. |
| R125 BT-kezelés | DAC-ra visszaesés után megszűnt a fölösleges tesztvárakozás; külön ACL-, párosítás- és A2DP-napló segít a kapcsolódási okot elkülöníteni. Hostteszt PASS, készülékes kontroll még hátravan. |
| Óra és ébresztő | Heti napok, időpont, felfutás–tartás–lefutás, hangerő és fényerő, rádió/SD-forrás, helyi „ti-ti-ti” tartalékhang. Az összes időzített helyzetet még nem próbáltam végig. |
| Időszinkron | NTP, négy szerver megadása, időzóna és nyári/téli idő szabálya. Az óra nem találja ki automatikusan a helyet; megfelelő beállítás és elérhető szerver kell. |
| Xiaomi BLE-hőmérő | LYWSD03MMC bekötés, MAC és kulcs megadása, hőmérséklet/pára. R125-ben scan és közvetlen olvasás együtt legfeljebb egyszer indulhat percenként; a ritka direkt fallback 10perces. Csak a hang- és memóriafeltételek mellett, az új ütemezés fizikai próbája még hátravan. |
| Időjárás és borítókép | Beállítások és megjelenítési út elkészült; az API-, szenzor- és forráskombinációk teljes ellenőrzése még folyamatban. |
| Egyszerű WebUI | Lejátszás és kibontott beállítások; a terhelést és a frissítések számát alacsonyan tartom. Nem cél a folyamatos böngészős lekérdezés. |
| Kijelzés | Dinamikus részek frissítése, legfeljebb20fps; terhelés és hanghiba esetén visszalassít. A20fps cél, nem minden helyzetben garantált érték. |
| Selftest és feltöltő | Menüjárás lejátszás mellett,30/60perces futtató, kapcsolati újrakeresés, naplózás. Élő COM/BOOT és valódi feltöltési százalék, csomagellenőrzés; a teljes telepítés külön megerősítéses. |

## Kikapcsolt vagy nem támogatott

- **EQ: a hangfeldolgozás teljesen kikapcsolva.** A Winamp EQ csak látvány; hangerő és balansz marad.
- **Internetes rádió/Wi-Fi-hang → Bluetooth-kimenet: tiltva.** Ez nem tiltja a WebUI-t vagy a rádió→DAC utat.
- USB-hangforrás, második ESP és teljes nagy kijelzős rádióport jelenleg nincs kész.
- BLE nem futhat a lejátszás rovására; a Service sem kerülheti meg a hardveres biztonsági határokat.

## Tervezett és hátralévő munka

1. A BT-szolgáltatásfelderítési hiba pontos elkülönítése, majd az SD-BT puffer és terhelés újramérése.
2. A korábbi hálózati heap- és BT-watchdog hiba célzott kizárása; hosszabb, megszakításokat is megőrző teszt.
3. Teljes ébresztő/szundi, időszinkronvesztés, tartalékhang és indulás közbeni leállítás készülékes ellenőrzése.
4. BLE saját érzékelőre szűrés és az új percenkénti korlát készülékes igazolása; időjárás és legfeljebb két érzékelő felületének véglegesítése.
5. Alaplapi fényérzékelő és automatikus fényerő, akkumulátormérés ellenőrzése a tényleges GPIO-k és bekötés alapján.
6. Radio Browser állomáskeresés és import.
7. Android-távirányító automatikus helyi hálózati felismeréssel, ritka kérésekkel; a többletfeladatokat a telefon végezné.
8. CPU/RAM/PSRAM további takarékos használata, a memóriahatárok csökkentése nélkül.

Minden skin előnézete nyilvános. A Winamp és a Service nyilvános marad; a többi alatt: **A tulajdonos kérésére a forráskód privát.** A régi vegyes kódcsomagokat nem teszem vissza. Korábban közzétett anyagok a Git-előzményekben megmaradhatnak.
