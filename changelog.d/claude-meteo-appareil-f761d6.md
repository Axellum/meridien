### Added

- **Weather on the device**: the firmware asks Open-Meteo over HTTPS every 15 minutes (server certificate checked), keeps the
  last answer in the NVS so it survives a restart, and offers it to the screens through `meteo_etat_lire()` with a French
  label, the day's minimum and maximum, and a "stale" flag (answer older than 2 h, or three failures in a row with an answer
  older than 30 minutes, or clock not set). The place is stored in the NVS (Lyon by default) and a new place triggers a new
  request at once. Core pieces are tested on the PC (25 tests, 39 mutations detected); the request was checked against the real
  API from the PC (HTTP 200). Tested on: Tab5 ST7121, 2026-11-06, commit `55644aa` (first answer equal to the live API, state back from the cache after a software restart, second request 15 min later); Wi-Fi cut, power cut and change of place not tested.
  Details: [docs/mesures/meteo.md](../docs/mesures/meteo.md).
- **Rich weather** (current conditions, 24 hours, 7 days, rain per quarter of an hour): the request asks for more variables
  (858 bytes for Lyon, answer 4.4 to 4.6 KB) and a new contract, `meteo_detail_lire()` / `meteo_detail_version()` in
  `main/meteo_etat.h`, gives them to the screen with the "hour now", "day now" and "rain in N minutes" indices recomputed for the
  device clock; `meteo_etat_lire()` is unchanged. A pressure trend over 3 h (`courant.tendance_pression_3h_hpa`, NaN while unknown) is
  measured by the device itself from the readings it receives, not requested from Open-Meteo. The weather state lives in the PSRAM and the NVS cache is now versioned: an
  older cache is recognised and replaced by the first answer. Open-Meteo labels sums by the end of their interval and starts its
  series off the hour in zones such as UTC+5:45 and UTC-3:30: both are handled and tested on seven real responses compared
  value by value with an independent Python computation (56 + 32 + 10 tests with AddressSanitizer + UBSan, 40 + 37 + 17 mutations detected).
  Tested on: PC only (the firmware builds, 1 862 512 bytes); **not run on a board**.
  Data: Open-Meteo, CC BY 4.0 (https://open-meteo.com), non-commercial use.
  Details: [docs/mesures/meteo.md](../docs/mesures/meteo.md), section 6.
