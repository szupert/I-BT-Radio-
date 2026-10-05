# I-BT-Radio V159 — BETA teszt

**Korlátozottan működőképes béta. Csak tesztelésre; beépítésre, mindennapos vagy felügyelet nélküli használatra nem ajánlott.** A tesztelők jelezhetnek hibát a GitHub Issues oldalon. A naplók beküldése előtt töröld belőlük az SSID-t, jelszót, IP-címet, MAC-címet és más személyes adatot.

## Letöltés

- [One-click telepítő ZIP](I-BT-Radio_V159_HTTPS_STREAM_BETA.zip)
- [Telepítő SHA-256](I-BT-Radio_V159_HTTPS_STREAM_BETA.zip.sha256)
- [Forrásanyagok + GPL-3.0 + V159 patch](V159_SOURCE_GPL3.zip)
- [Forrás ZIP SHA-256](V159_SOURCE_GPL3.zip.sha256)
- [English test notes](README.en.md)

A telepítő az alkalmazást írja a meglévő 16 MB-os kiosztásra. A feltöltő Python 3.9+-t igényel. A menü-önteszt PowerShellből fut, és nem tölti újra a firmware-t.

## V159 változás

Visszaállítja a közvetlen HTTPS stream útvonalat. A közvetlen HTTP marad; HLS továbbra sincs engedélyezve. A TLS forgalom titkosított, de a tanúsítvány-ellenőrzés ki van kapcsolva, ezért ez nem tekinthető hitelesített TLS-kapcsolatnak.

Firmware SHA-256: `4a7f09075de23e1508daedae1cd6004ad861a3ff509fa9f9cb81232afc4b3bb0`
ELF SHA-256: `aa7d4220335b4340bc6328153f9d45dbb4d6fb032bc23d5961a765e21866b4aa`
Telepítő ZIP SHA-256: `a93883e1d02f7ef0cee246edfce4eacef3051487a1180815c203a42987b111f0`

## Jelenlegi tesztállapot

A felhasználói tesztben a V159 felment, elindult, és a rádió DAC-on szólt. A menüteszt azonnal `start=DENIED` választ adott, **0 menüútvonallal**; a teszt nem várakozott ténylegesen, és nem bizonyítja a menü működését. A HTTPS stream, BT folyamatosság és hosszú távú stabilitás nincs igazolva. A DAC hangja önmagában nem jelent teljes teszt-PASS-t.

Forráscsomag: a V157 forrásalap, a GPL-3.0 licenc és a V159 HTTPS patch. A csomagot még nem építettük újra külön környezetben; azonos firmware-bináris előállítását nem állítjuk.
