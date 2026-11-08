### Added

- **"Details" page: the seven quantities the home screen no longer shows** (wind, rain, humidity, pressure, UV index, sun, moon), full
  screen, one "Close" button, nothing animated. A quantity the weather does not give is "—" (never a 0, never an invented value);
  stale weather keeps its last values greyed with "Data from 08:40 · stale"; the moon is computed on the device and is "—" only
  when the time is not reliable. Text never below 24 real pixels, even on the 7B. Nothing opens it yet: the press-and-hold wheel
  will ("Pages ▸ Details"). See ADR-0020. Seen on: PC renders (LVGL 9.5.0, 1280x720, 1024x600, 720x1280; French, German, Turkish;
  three themes); compiled for the Tab5 and the Waveshare 7B; not tested on board.
- **Pure core `ecran/grandeurs.hpp`**: values, units, detail lines, missing state, stale state, fingerprint and layout of the page.
  Tested on the PC (compiled and run with AddressSanitizer and UBSan) with mutation tests. Tested on: not tested on board.
