### Added

- **Tab5 hardware layer** (perceived backlight, sleep, hardware clock, audio, power): `board.h` gets a board-neutral interface for
  all of it (a board that lacks a function answers `ESP_ERR_NOT_SUPPORTED`), implemented for the Tab5 in `components/bsp_tab5/`
  on top of pure, PC-tested logic in `components/core/` (71 tests, 80 targeted mutations detected out of 80). Board sheet with
  the source of every fact: [`docs/carte-tab5.md`](../docs/carte-tab5.md); measurements: [`docs/mesures/tab5-materiel.md`](../docs/mesures/tab5-materiel.md).
  Tested on: `tab5-st7121`, 2026-11-07, commit `5f797e3` (bench without a finger: 40 checks OK, 0 failed; 10 software resets
  in a row reach the screen). Not seen: a real finger on the sleep veil, the headphone path, real speech on the microphones.
- **Brightness 0–100 and sleep**: the application gives a perceived level; the board maps it to a duty cycle (floor 2 %, ceiling
  100 %). Dim, off and wake-on-touch, plus up to four time windows, one of which can cross midnight and belongs to the day it
  starts. Tested on: `tab5-st7121`, 2026-11-07, `5f797e3`: dim after 2100 ms, off 1900 ms later, wake in 100 ms.
- **Hardware clock (RX8130)**: read at start-up only when the system has no plausible time of its own, written only from network
  time, never overwrites a newer one; the power-loss flag is read before it is cleared. Tested on: `tab5-st7121`,
  2026-11-07, `5f797e3` (write/read back, 1 Hz, year rollover, 29 February, restored within ±2 s).
- **Audio out and microphones**: ES8388 / ES7210 drivers, one I2S port at 16 kHz, perceived volume 0–100 with a −6 dB ceiling,
  beep, level meter and PSRAM recording. **Playback and capture run together on the single port** (40 640 frames out = 40 640
  in, 0 overflow). The speaker is on LINE1. Internal RAM: 6 224 bytes while the bus is open, nothing before the first call.
  Tested on: `tab5-st7121`, 2026-11-07, `5f797e3`.
- **Power measurement (INA226)**: voltage, current and state at the battery connector; the charger is forced off at start-up and
  its state as found is logged. Tested on: `tab5-st7121`, 2026-11-07, `5f797e3`: reads 8206 mV and 2 mA with the charger off, which
  contradicts the earlier note “no battery” (open question, see `docs/depannage.md`).
- **On-board bench** `-DTAB5_AUTOTEST_MATERIEL=1`: one `[AT-MAT]` line per check, settings restored afterwards.

### Changed

- `board_set_backlight_percent()` now goes through the perceived-brightness curve (level 1 = 2 % of the duty cycle; levels
  1–10 are almost indistinguishable, left for the author to judge by eye). Internal RAM at rest: −308 bytes against the base
  commit (134 051 vs 134 359 free, largest block unchanged). Tested on: `tab5-st7121`, 2026-11-07, `5f797e3`.
