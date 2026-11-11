### Changed

- **Test serial commands out of the normal firmware**: `tourner3` / `tourner0` (a task holds core 0 for 8 s to trip the
  watchdog) and the Wi-Fi bench (`wifi_scan`, `wifi_faux`, `wifi_faux_abandon`, `wifi_inconnu`, `wifi_bon`) are now only in a
  measurement build, `.\tools\idf.ps1 -DMERIDIEN_BANC_ESSAI=ON build`. The diagnostic commands `memoire`, `blocs`, `piles` and
  `redemarrer` stay. Not tested on board.
- **Stored Wi-Fi read from one source**: at start-up the network layer reads the remembered network through
  `wifi_stocke_lire`, which takes the NVS namespace and keys from the same constants as the settings host that writes them
  (same values as before: boards already set up keep their network). Not tested on board.

### Removed

- `main/bouchons/` (weather and Wi-Fi stubs, no longer built since the real `meteo_etat.h` and `reseau_wifi.h` exist), its
  CMake blocks, the matching include fallback of the render bench and the `MERIDIEN_METEO_BOUCHON` blocks it alone defined.
