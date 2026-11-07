### Added

- **Waveshare 7B board layer**: the firmware now builds for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B with `BOARD=waveshare-7b`
  (EK79007 1024 × 600 display, GT911 touch polled, ES8311/ES7210 audio code with the volume capped at 0 dB, Wi-Fi through the
  ESP32-C6 with `esp_hosted` pinned and the C6 firmware never updated, interface rotation 0 or 180 chosen at build time, real
  panel off behind `board_set_backlight_percent(0)` because the backlight is analog). `board.h`, the core and the application are
  unchanged. A procedure script, `tools/essai_7b.py`, backs up the whole 32 MB flash (read twice, checked by the chip), flashes a
  test binary and always restores the original firmware, verified. The pure parts are tested on the PC (34 tests, 33 mutations
  detected). Tested on: **not tested on a board** (the 7B was not connected), builds on the PC at commit `c48c418`, 2026-11-07.
  Details: [docs/carte-7b.md](../docs/carte-7b.md), [docs/mesures/7b-materiel.md](../docs/mesures/7b-materiel.md).

### Changed

- **Board selection in the build**: the board layers that are not selected are excluded from the build and each board other than the
  Tab5 has its own dependency lock file (`boards/<board>/dependencies.lock`); the Tab5 keeps the root `dependencies.lock`. Switching
  board needs a new `sdkconfig` ([docs/carte-7b.md](../docs/carte-7b.md#build)). Tested on: builds of both boards on the PC; not
  tested on a board.
