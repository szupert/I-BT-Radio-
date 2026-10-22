# Credits and licences

I-BT-Radio is based on [e2002/yoRadio](https://github.com/e2002/yoradio), GNU GPLv3. Original copyright and licence notices remain in the source. Project modifications are attributed to szupert / I-BT-Radio; keep provenance and document changes when redistributing.

Individual third-party components retain their own licences. The Espressif extra-codec archive is a separately identified binary dependency under the adjacent Espressif Modified MIT licence, for Espressif hardware only; its vendor-internal source is not supplied or claimed to be project-owned GPL source. Font, graphics, brand and bundled installer-tool notices remain applicable. See the existing beta component licence files and each component directory.


## Product-name migration

The product is named I-BT-Radio. Original upstream copyright and licence notices remain applicable. Archived binary releases and proof manifests keep their original hashes; a source-path or branding update is not a new tested firmware. Source roots are now I-BT-Radio, the own Light frontend is frontend_light.inc and its namespace lightui. Legacy persisted names can still be recognized for migration.

### README.de.md

Ich entwickle mein eigenes yoRadio-basiertes ESP32-WROOM-Radio mit PSRAM und konzentriere mich auf SMALL. [Funktionsstand](docs/Features.md) · [Menü](docs/menu/README.md) · [Anschlüsse](hardware/README.md).

### README.en.md

I am building my own yoRadio-based ESP32-WROOM + PSRAM radio, focusing on SMALL. [Feature status](docs/Features.md) · [Menu](docs/menu/README.md) · [Wiring](hardware/README.md).

### README.md

Az I-BT-Radio a saját, yoRadio-alapú ESP32-WROOM + PSRAM rádióprojektem. A SMALL változaton dolgozom.

### README.md

A lejátszó EQ-görbéje **csak látványelem**; Bluetooth módban nincs hangszínszabályzás. A képek egyedi 320 × 240-es TFT-megjelenítőben használhatók; önmagukban nem telepítik a kezelőfelületet a gyári [yoRadio](https://github.com/e2002/yoradio) rendszerbe. A yoRadio-hoz illesztőréteg külön készül.

### README.md

Az I-BT-Radio a yoRadio alapjaira épül. Köszönet a yoRadio készítőinek.

### source/README.md

Az `esp_audio_codec` külső, előre fordított Espressif-függőség, saját mellékelt licenccel; nem állítjuk, hogy annak gyártói belső forrása rendelkezésünkre áll. Az alkalmazás saját kódja és módosított yoRadio-forrása olvasható, beleértve az összes átadott skin kódját. Harmadik felek és márkajelzések jogai megmaradnak.

### source/light/yoRadio/src/pluginsManager/README.md

### `on_setup()`
- **Description:** Called at the beginning of the sketch setup process.  
- **Location:** `setup()`, `yoRadio.ino`.

### source/light/yoRadio/src/pluginsManager/README.md

### `on_end_setup()`
- **Description:** Called at the end of the sketch setup process.  
- **Location:** `setup()`, `yoRadio.ino`.

### source/pro/yoRadio/src/pluginsManager/README.md

### `on_setup()`
- **Description:** Called at the beginning of the sketch setup process.  
- **Location:** `setup()`, `yoRadio.ino`.

### source/pro/yoRadio/src/pluginsManager/README.md

### `on_end_setup()`
- **Description:** Called at the end of the sketch setup process.  
- **Location:** `setup()`, `yoRadio.ino`.

