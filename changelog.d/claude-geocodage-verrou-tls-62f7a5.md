### Fixed

- **City search on the screen waits for the shared TLS lock**: the city search typed on the device screen had its own HTTPS
  downloader (a task at priority 3) that did not take the "one TLS at a time" lock, so a search during a weather request could
  run two TLS handshakes at once and exhaust the internal RAM. It now goes through the single geocoding module
  (`main/geocodage.cpp`, background priority, under the shared lock), already used by the settings page; the duplicate
  downloader is removed. The places listed and the texts shown are unchanged (same request, same reading of the answer in the
  pure core). A search started while another HTTPS request runs now waits for it (15 s at most) instead of running beside it.
  Tested on: not tested on board (PC tests and Tab5 / 7B builds only).
