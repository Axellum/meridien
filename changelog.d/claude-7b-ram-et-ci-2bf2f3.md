### Fixed

- **Waveshare 7B: the internal-RAM settings of the Tab5 now apply to every board.** `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256`
  and `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` moved from `boards/tab5/sdkconfig.defaults.board` to the common
  `sdkconfig.defaults`; the 7B was built with the ESP-IDF defaults (16384, TLS buffers in internal RAM). Checked in the
  generated `sdkconfig` of both boards; not tested on board (free internal RAM on the 7B still to be measured after a flash).

### Added

- **CI: the Waveshare 7B firmware is compiled on every pull request** (job `firmware-waveshare-7b`, same ESP-IDF image as
  the Tab5), see [docs/outils.md](docs/outils.md).
