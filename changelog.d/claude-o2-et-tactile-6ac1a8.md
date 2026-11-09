### Changed

- **Code built for speed (`-O2`) instead of ESP-IDF's default `-Og`** (`CONFIG_COMPILER_OPTIMIZATION_PERF=y` in `sdkconfig.defaults`,
  both boards). Assertions stay on. A previous session measured it on the Tab5 ST7121 development board (commit `1324550`,
  08/11/2026, 5 start-ups per binary): the CPU rotation loop went from 649 to 307 µs, the first image from 379 to 340 ms at the
  maximum, the PPA path unchanged. **Those figures were not measured again here.** Cost: the Tab5 binary grows by 164 800 bytes
  (3 093 184 → 3 257 984, 22 % of the 4 MB partition still free). Tested on: compiled for `tab5-st7121` and `waveshare-7b`
  with no warning; **not flashed and not measured on any board**.
- **The touch is read every 10 ms instead of every 33 ms** (LVGL's default read period), on both boards. **Not measured**: the
  gain, if any, in the delay between a touch and its effect is unknown, and the extra I2C reads are not measured either. Tested
  on: compiled for both boards; not flashed. Separate commit, so that it can be reverted alone.

### Fixed

- **Three truncation warnings that `-Og` hid and `-O2` turns into build errors** (`strncpy` in the weather zone copy,
  `snprintf` of the official-alert chip text, `snprintf` of the hardware clock text). The text produced is unchanged; the cut,
  where there was one, is the same and is now written in the code. Tested on: compiled; not flashed.
