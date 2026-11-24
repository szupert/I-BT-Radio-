# R479 LIGHT

A 4 MB-os, PSRAM nélküli LIGHT rádióhoz. A fix IPv4-cím mellett a maszk, az átjáró és a DNS egymástól függetlenül kérhető a DHCP-től vagy adható meg kézzel. A mentett profil USB-n is állítható. A veszteségmentesen tömörített indítókép mind a 76 800 képpontja egyezik az előzővel.

**Készülékpróba.** A Budapest Rock a router DNS-ére állított profillal elindult. A teszthálózat routere a rádió 1,5 másodperces várakozási idején belül nem válaszolt. A tartalék 1.1.1.1 átvette a névfeloldást (34 ms). A kézzel megadott 1.1.1.1 első próbára válaszolt (79 ms). Ezzel a címmel a Budapest Rock az első lejátszási próbán hangot adott az I²S-kimenetre. Az ismétléskor a névfeloldás után a TCP/TLS-kapcsolat hibázott, a hangkimenetre 0 bájt jutott. A Danubius egyik indítása is meghiúsult. A DHCP-re visszaváltást újraindítás után ellenőriztük. A külön globális kézi DNS felülbírálja a profil sorrendjét.

A ZIP meglévő LIGHT R478 telepítés alkalmazásfrissítése. A telepítési útmutató és az ellenőrzőösszeg a csomagban található. Szabad hely az alkalmazáspartícióban: 22 224 bájt.

## Telepítés és beállítás

I-BT-Radio R479 LIGHT – alkalmazásfrissítés
Klasszikus ESP32-WROOM, 4 MB, PSRAM nélkül, a meglévő LIGHT R478 partíciókiosztásával.
Bontsd ki a csomagot, majd a rádió portjára állított LIGHT/esptool 5.3.0 környezetből írd az alkalmazást:
python -m esptool --chip esp32 --port COMx write-flash 0x10000 firmware.bin
A COMx helyére a rádió portja kerül. Ellenőrizd az R479 azonosítót az újraindítás után.
Mentett hálózatok → hálózat → IP-beállítás. Fix IP mellett az üres maszk, átjáró és DNS mező DHCP-adatot vesz át. A módosítás a következő csatlakozáskor érvényesül.
USB, 115200 baud: r10 ip 1 (profil olvasása), r10 ip 1 fixed 192.168.0.22 auto auto auto, r10 ip 1 fixed 192.168.0.22 auto auto 1.1.1.1, r10 ip 1 dhcp. A példák az első mentett hálózatra vonatkoznak.
A profil DNS-ének használatához legyen a Névfeloldás (DNS) menüben Automatikus mód. A globális Kézi mód külön, magasabb elsőbbségű beállítás.

