# Plugin Class

The `Plugin` class serves as a base class for creating plugins that hook into various system events.  
To use it, inherit from `Plugin` and override the necessary virtual methods.  
Examples of plugin usage can be found in the `examples/plugins` folder.
Place your new class in the `src/plugins/<MyPlugin>` directory
---

## Public Methods

### Constructor
`Plugin();`  
Initializes the plugin when an instance is created.

---

### Destructor
`virtual ~Plugin();`  
Cleans up resources when the plugin is destroyed.

---

## Virtual Methods (Override in derived classes)

Az eredetre és licencekre vonatkozó információ: [licencleírás](https://github.com/szupert/I-BT-Radio-/blob/main/THIRD_PARTY_NOTICES.md).

Az eredetre és licencekre vonatkozó információ: [licencleírás](https://github.com/szupert/I-BT-Radio-/blob/main/THIRD_PARTY_NOTICES.md).

### `on_connect()`
- **Description:** Triggered after a successful network connection.  
- **Location:** `MyNetwork::begin()`, `I-BT-Radio/src/core/network.cpp`.

### `on_start_play()`
- **Description:** Triggered when playback starts.  
- **Location:**  
  - `Player::_play(uint16_t stationId)`, `I-BT-Radio/src/core/player.cpp`.  
  - `Player::browseUrl()`, `I-BT-Radio/src/core/player.cpp`.

### `on_stop_play()`
- **Description:** Triggered when playback stops.  
- **Location:** `Player::_stop(bool alreadyStopped)`, `I-BT-Radio/src/core/player.cpp`.

### `on_track_change()`
- **Description:** Triggered when the current track changes.  
- **Location:** `Display::_title()`, `I-BT-Radio/src/core/display.cpp`.

### `on_station_change()`
- **Description:** Triggered when the current station changes.  
- **Location:** `Player::loop()`, `I-BT-Radio/src/core/player.cpp`.

### `on_display_queue(requestParams_t &request, bool &result)`
- **Description:** Triggered when a command is dequeued for the display.  
- **Parameters:**  
  - `request`: Reference to a `requestParams_t` structure (defined in `I-BT-Radio/src/core/common.h`).  
  - `result`: Set to `false` to stop queue processing or `true` to continue.  
- **Location:** `Display::loop()`, `I-BT-Radio/src/core/display.cpp`.

### `on_display_player()`
- **Description:** Triggered when the player UI is displayed.  
- **Location:**  
  - `Display::_start()`, `display.cpp`.  
  - `Display::_swichMode(displayMode_e newmode)`, `I-BT-Radio/src/core/display.cpp`.

### `on_ticker()`
- **Description:** Triggered once every second.  
- **Location:** `ticks()`, `I-BT-Radio/src/core/network.cpp`.

### `on_btn_click(controlEvt_e &btnid)`
- **Description:** Triggered when a button is clicked.  
- **Parameters:**  
  - `btnid`: Reference to the button ID (defined in `I-BT-Radio/src/core/common.h`).  
- **Location:** `onBtnClick(int id)`, `I-BT-Radio/src/core/controls.cpp`.

---

## Protected Methods

### `registerPlugin()`
- **Description:** Registers the plugin with the plugin manager.

