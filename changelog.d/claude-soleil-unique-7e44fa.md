### Changed

- **One single sun calculation for the whole device**: the sunrise and sunset shown with MET Norway and NWS weather (and the "day / night"
  flag of each hour) now come from the same calculation as the main screen's (Open-Meteo), instead of a second one written separately.
  Measured against the same references, the single calculation is closer everywhere: MET Norway's own "Sunrise 3.0" over 79 real days,
  within **60 s** at every latitude (the removed one: 81 s up to 60° of latitude, 257 s beyond 66.5°); Open-Meteo's real sunrises and sunsets,
  within 8 s on 63 days (the removed one: 23 s); polar days and polar nights are identical in every case compared (79 + 63 + 120 days, and
  365 days of 2026 at Longyearbyen, Tromsø and McMurdo). On a PC the full day (sunrise and sunset) costs about 1.26 times as much and the
  "is the sun up?" question about 0.81 times as much; **nothing was measured on the ESP32-P4**. The tolerances of the tests are unchanged
  (2 minutes up to 60° of latitude, 5 minutes beyond). The method is described once, in
  [docs/decisions/0012-l-heure-affichee-est-celle-du-lieu-choisi.md](../docs/decisions/0012-l-heure-affichee-est-celle-du-lieu-choisi.md).
  Tested on: PC only (clang++ 23.1.5, AddressSanitizer and UBSan), commit of this branch on base `3fca1cc`: 68 test files, 1 420 tests,
  0 failure; the two independent proofs of the MET Norway / NWS branch (the "Almanac for Computers" Python recalculation and the 79
  "Sunrise 3.0" days) are now tests of the single module; 126 targeted mutations of the sun calculation and of the readers' use of it
  (118 before), all detected. Compiled on 07/11/2026 for the Tab5 (2 947 952 bytes) and for the Waveshare 7B (2 928 576 bytes), exit
  code 0, no compiler warning. **Not seen on any board**: nothing was flashed and no screen was looked at.
