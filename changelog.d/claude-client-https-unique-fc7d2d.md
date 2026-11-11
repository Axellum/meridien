### Changed

- **One HTTPS path for every online service**: Vigie alerts and the city search now go through the common HTTPS client
  (`main/http_commun.cpp`), like the weather, the sea and the list of spots; no other file opens an HTTPS connection. One gzip
  decoder: Vigie answers are now checked against the gzip CRC-32 (before, only their size), and `gzip_flux.hpp` is removed. The
  city search now asks for gzip (Lyon: 1,534 bytes on the wire instead of 7,208, measured from the PC) and identifies itself. Sizes, timeouts, the shared TLS
  lock and every address are unchanged. Tested on: PC only (host tests with AddressSanitizer and UBSan, mutations), compiled for
  the Tab5 and the Waveshare 7B by the CI; not tested on a board.

### Security

- **HTTP redirects are refused**: ESP-IDF follows up to ten redirects to any `https://` host by default; no address the device
  builds is redirected today (checked from the PC on 2026-11-11), so a 3xx answer is now refused and logged instead of carrying
  the device's position to a host nobody chose. A user-entered Vigie address that redirects must be replaced by its final
  address. Not tested on a board.
