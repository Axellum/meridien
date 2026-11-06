### Added

- **Settings page served by the device**: the Tab5 now serves the settings page (`web/app`, 30 327 bytes gzip) and the first part
  of its API (`docs/api-reglages.md`) on port 80, in Wi-Fi station mode only. A 6-digit pairing code is drawn at each start and
  shown on the screen with the address (`http://<IP>/` and `ecran-xxxx.local`). From the page: choose the weather city (the
  device searches Open-Meteo itself), the screen language, the assistant language, the theme and the brightness; all of it is
  kept in the NVS. Functions the firmware does not do yet (changing Wi-Fi, STT/LLM/TTS services, export/import, update) answer
  "not available yet" in the screen language. `web/tests/parcours.py` can run against the real device (`URL_APPAREIL`).
  Tested on: not tested on board (compiled only, commit `933b628`).
- **One place search for the page and the screen**: the city search the settings page uses is now a module shared with the
  device screen (`geocodage_chercher` / `geocodage_etat` / `geocodage_resultats`, at most 8 places, last request wins). The
  places are read by one pure parser (`components/core/include/ecran/geocodage.hpp`), tested on the PC against seven **real**
  Open-Meteo responses (`test/host/donnees/`) with targeted mutations. The download runs in its own low-priority task, never in the
  screen's task, and shares one lock with the weather request (`main/verrou_tls.h`) so two TLS sessions never run at the same
  time. Tested on: not tested on board (PC tests only, see [docs/mesures/reglages.md](../docs/mesures/reglages.md)).

### Fixed

- **Unknown altitude no longer shown**: Open-Meteo writes `-9999` when a place has no known altitude (seen on “Istanbul Old
  Town”); the search now leaves `altitude` out instead of passing that value on. Tested on: PC only.
