# I-BT-Radio 8048S070C – teljes navigációs demó

**MENU8048C-R4 BETA**. Kizárólag ESP32-8048S070C kapacitív, 800×480-as, ESP32-S3/16MB/8MB panelhez. A SMALL firmware változatlan. Ez a rádió teljes menükatalógusának és felületeinek bemutatója, nem működő hangrendszer.

## Feltöltés és teszt
1_FELTOLTES.bat: saját panel USB, COM-választás, S3/16MB ellenőrzés, támogatott tábla, 64KB boot/NVS/partíciómentés, élő százalék, boot/PSRAM/GT911 ellenőrzés. A bootloader saját DIO módja megmarad; nincs erase-all/NVS-törlés. A gyári 4MB kiosztás 3MB apphelyét megtartja; nagyobb firmware vagy ismeretlen tábla esetén megáll. Két feltöltő nem foglalhatja ugyanazt a COM-portot. Ha nem kapcsolódik: BOOT nyomva, rövid RESET, BOOT elenged.
2_MENU_SELFTEST.bat: verzióellenőrzés, 55 oldal minden sorának render-bejárása. A sorszámot a firmware válaszából rögzíti. Ez nem funkcionális audio vagy touch PASS. SYSTEM/LOGS az elsődleges napló, RETURN_TO_EVA a visszaadható másolat. Hibánál is ment.

## Kezelés
A kezdőképernyőn a bal oldali Menü gomb a rádió eredeti sorrendű főmenüjét nyitja. Jobb oldali > vagy vízszintes söprés váltja a felületeket. Menüben bal felül MENÜ, jobb felül Vissza; a visszalépés a tényleges bejárt útvonalat követi. Hosszú listán függőleges söprés, Fel/Le és görgetősáv. Kijelző → Felület: a rádió kilenc választása. A 320×240-es hátterek 2×, képaránytartó nagyítást kapnak: 640×480, 80-80 pixeles oldalsávval.

A menü nevei, alapértékei és elemszámai közvetlenül a SMALL R83 content.h és tiszta policy fejlécmásolataiból jönnek. A hálózat/állomás/SD/BT listák mintaadatok. A nem navigáló sorok értékpróbája RAM-demó; újraindításkor elvész. Valódi beállításmentés, hang, EQ, BT/Wi-Fi kapcsolat, SD-olvasás, ébresztés és jelgenerátor NINCS bekötve. Service nézetben jelalak/frekvencia/kimenet/Start–Stop csak demó; nincs mérőjel. Óranézet mintaidőt mutat, nincs NTP. A bitmapek dinamikus zeneadatai/mutatói nem a rádió élő adatai; nem minden eredeti bitmapgomb kapott saját működést. Winamp EQ terület megnyitja a 10 sávos menüt.

## Villogási kontroll és érintés
LovyanGFX1.2.33 saját Sunton8048S070 profilja: 14MHz RGB, gyári pinout, 80/4/16 vízszintes és22/4/4 függőleges időzítés. A működő GT911 saját határidős I2C pollingja változatlan marad; az LCD-könyvtár nem inicializál második touch-drivert. GPIO2 háttérvilágítás PWM alapértéke96/255 (~38%); Kijelző → Fényerő alatt 10/25/50/75/100% állítható. Auto itt demó38%, nincs fényérzékelő. Ez tápterhelési kontroll, nem bizonyított táphiba-javítás. Idle állapotban nincs újrarajzolás; az RGB hardver továbbra folyamatosan frissít. Lapváltás külön sprite-ban készül, értékcsere csak a változott sort viszi át. Két képpuffer kb.1,54MB a statikus RAM-on felül.

## Határ és bizonyíték
R3 felhasználói feltöltése/boot/PSRAM/GT911 PASS; képe megjelent, de idle is villogott, touch később a felhasználó szerint működött. R4 valós villogás/érintés NOT_RUN. A teljes katalógus gépi navigációja és célbuild külön bizonyíték; nem nevezzük működő rádióportnak. A menüben jelzett DAC, GPIO, akkumulátoradat a SMALL-ból hozott demószöveg, nem az S3 panel végleges hardverkiosztása.

SYSTEM/SOURCE/I-BT-Radio_MENU_8048S070C tartalmazza a forrást és a tools/package.py USER-helyreállítót. A USER mappa ebből újraépíthető, szükség esetén PlatformIO-val újrafordítás után. Licenc: LICENSE_LovyanGFX.txt és a másolt források saját megjegyzései. A korábbi Arduino_GFX BSD asset-attribúció LICENSE_Arduino_GFX.txt helyett a SYSTEM korábbi kiadásában marad; saját rádióbitmapjeink változatlanok.

Források: https://github.com/lovyan03/LovyanGFX/blob/1.2.33/src/lgfx_user/LGFX_Sunton_ESP32-8048S070.h és https://github.com/moononournation/Arduino_GFX/issues/641 . Az utóbbi azonos panelen könyvtárváltási tapasztalat, nem bizonyíték a mi tápunk hibájára. RGB565 képpontokat számszerűen ellenőrzött, változatlan másolatként használjuk (skin_audit.json).
