### Added

- **Weather on the device**: the firmware asks Open-Meteo over HTTPS every 15 minutes (server certificate checked), keeps the
  last answer in the NVS so it survives a restart, and offers it to the screens through `meteo_etat_lire()` with a French
  label, the day's minimum and maximum, and a "stale" flag (answer older than 2 h, or three failures in a row with an answer
  older than 30 minutes, or clock not set). The place is stored in the NVS (Lyon by default) and a new place triggers a new
  request at once. Core pieces are tested on the PC (25 tests, 39 mutations detected); the request was checked against the real
  API from the PC (HTTP 200). Tested on: Tab5 ST7121, 2026-11-06, commit `55644aa` (first answer equal to the live API, state back from the cache after a software restart, second request 15 min later); Wi-Fi cut, power cut and change of place not tested.
  Details: [docs/mesures/meteo.md](../docs/mesures/meteo.md).
