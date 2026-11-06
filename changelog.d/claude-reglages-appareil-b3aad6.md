### Added

- **Settings page served by the device**: the Tab5 now serves the settings page (`web/app`, 30 327 bytes gzip) and the first part
  of its API (`docs/api-reglages.md`) on port 80, in Wi-Fi station mode only. A 6-digit pairing code is drawn at each start and
  shown on the screen with the address (`http://<IP>/` and `ecran-xxxx.local`). From the page: choose the weather city (the
  device searches Open-Meteo itself), the screen language, the assistant language, the theme and the brightness; all of it is
  kept in the NVS. Functions the firmware does not do yet (changing Wi-Fi, STT/LLM/TTS services, export/import, update) answer
  "not available yet" in the screen language. `web/tests/parcours.py` can run against the real device (`URL_APPAREIL`).
  Tested on: not tested on board (compiled only, commit `933b628`).
