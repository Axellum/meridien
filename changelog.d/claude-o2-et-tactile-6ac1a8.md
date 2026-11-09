### Changed

- **Code built for speed (`-O2`) instead of ESP-IDF's default `-Og`** (`CONFIG_COMPILER_OPTIMIZATION_PERF=y` in `sdkconfig.defaults`,
  both boards). Assertions stay on. Measured before/after on the Tab5 ST7121 development board (6 soft restarts per binary,
  09/11/2026, commits `c3fd97b` → `dca5e93`, Wi-Fi not configured in either build, details in `docs/mesures/optimisation-o2.md`):
  first image 2236 → 2116 ms on average (minimum 2209 → 2115 ms), CPU rotation loop 649–651 → 303–306 µs, PPA path unchanged,
  worst render time of an image 394 → 355 ms, free internal RAM +5.1 kB. **Cost to watch**: the minimum free stack of
  `taskLVGL` fell from 1768 to 716 bytes (stack of 7168 bytes), measured only on the start-up sequence and the main screen.
  The Tab5 binary grows by 164 848 bytes (3 093 184 → 3 258 032, 22 % of the 4 MB partition still free). On the Waveshare 7B
  (same day, same method, `c3fd97b` → `db823e0`): first image 484 → 466 ms, free internal RAM +5.5 kB, no crash, Wi-Fi, NTP and
  weather requests working in both builds; there the minimum free stack of `taskLVGL` fell from 1848 to 584 bytes. Tested on:
  `tab5-st7121` and `waveshare-7b`, 09/11/2026, as above.
- **The `taskLVGL` stack goes from 7168 to 8192 bytes on both boards** (`lvgl_port_cfg_t.task_stack`, one more kilobyte of internal
  RAM), because `-O2` left only 716 bytes (Tab5) and 584 bytes (7B) of margin. Measured on the tree merged with `main`
  (`90c4af2`, `-O2`): minimum free stack 1608 bytes at the end of start-up on both boards; on the Tab5, the on-board settings
  self-test (29 checks, 3 phases, 2 real reboots) went down to 1520 bytes at worst. The 7B did not run the self-test (it writes
  the board's settings). Screens the self-test does not visit (wheel, alerts, weather) are not checked. The self-test reports 1
  failed check out of 29 (`l'installation reprend sur un écran à part`): read in the code and in the log, the main screen has
  resumed the guided installation at start-up since `cd362eb`, so the test finds it already open; not caused by this change, not
  fixed here, not replayed on `c3fd97b`. Tested on: `tab5-st7121` and `waveshare-7b`, 09/11/2026 (details and caveats in
  `docs/mesures/optimisation-o2.md`).
- **The touch is read every 10 ms instead of every 33 ms** (LVGL's default read period), on both boards. **Not measured**: the
  gain, if any, in the delay between a touch and its effect is unknown, and the extra I2C reads are not measured either. Tested
  on: compiled for both boards; both ran this code in the `-O2` measurements above but no touch was made. Separate commit,
  so that it can be reverted alone.

### Fixed

- **Three truncation warnings that `-Og` hid and `-O2` turns into build errors** (`strncpy` in the weather zone copy,
  `snprintf` of the official-alert chip text, `snprintf` of the hardware clock text). The text produced is unchanged; the cut,
  where there was one, is the same and is now written in the code. Tested on: compiled for both boards; the Tab5 and the 7B
  started and displayed the main screen with this code on 09/11/2026 (the three code paths themselves were not exercised on
  purpose).
