### Fixed

- **The settings page and `GET /api/status` now name the real board.** The firmware always answered `modele` = "M5Stack Tab5", even
  on the Waveshare 7B. The name is now read from the board layer (`board_get_info()`): "M5Stack Tab5" or "Waveshare 7B"
  ([docs/api-reglages.md](../docs/api-reglages.md)). Built for both boards, 0 compiler warnings; not tested on board (the change
  needs a new flash and a read of `/api/status` to be proven).
