### Added

- **Two key-less weather services, read by the device itself**: the "Weather source" list (screen and phone page) now offers
  **MET Norway (api.met.no)** for Norway, Finland, Iceland and Svalbard, and **NWS (weather.gov)** for the United States and Puerto
  Rico, next to Open-Meteo's national models. The answers are read by pure, tested readers into the same structure as Open-Meteo's,
  so the screens do not change. What a service does not give is **unknown, never 0**: the UV index is unknown for both, the NWS gives
  no pressure and, most days, no daily rain total. Sunrise and sunset are computed on the device (with the device's single
  sun calculation, within 60 s of MET Norway's own "Sunrise 3.0" on 69 days at every latitude). Each service has its own rhythm (at least 30 min for MET Norway, 60 min for the
  NWS, never before the `Expires` the service sends, at most 90 min, random spread, never on the round hour). When the chosen service
  fails (network, HTTP 404 outside its area, no grid), the weather comes from Open-Meteo "Automatic" in the meantime and the screen and the
  page say so; the credit shown is always the one of the service that really gave the weather (MET Norway: CC BY 4.0 and NLOD 2.0; NWS:
  public domain, "not approved or sponsored by the NWS"; Open-Meteo: CC BY 4.0). Each request has its own size bound (MET Norway 160 KiB,
  NWS grid 384 KiB; the real answers go up to 90 783 and 240 402 bytes). `GET /api/location` lists carry `direct` per source and
  `GET /api/weather` carries `fournisseur` and `repli` (additions only, nothing removed). `If-Modified-Since` is not used. Key-based
  services are not part of this change.
  Tested on: PC only. The readers were run on 14 real answers captured on 07/11/2026 (8 MET Norway places, 6 NWS places, plus the 404
  of a French city) and compared value by value (449 each) with an independent reading written in Python; 1 009 core tests run with
  AddressSanitizer and UBSan, 0 failure; 307 mutations of the 14 suites this change touches, all detected; the phone page: 171 checks out of
  171 against a fake server; the source list rendered on the PC in seven languages. Compiled on 07/11/2026 for the Tab5 (2 894 640
  bytes) and for the Waveshare 7B (2 875 216 bytes), exit code 0, no compiler warning; **not flashed and not run on any board**: the real
  TLS requests, the decompression and analysis of a 240 KB grid in PSRAM (stack margin of the weather task and duration not measured), what
  the screen and the phone page do with real answers, and the fallback after a real outage are not verified. Details:
  [docs/decisions/0014-met-norway-et-nws-comme-sources.md](../docs/decisions/0014-met-norway-et-nws-comme-sources.md),
  [docs/mesures/fournisseurs-meteo.md](../docs/mesures/fournisseurs-meteo.md), [docs/api-reglages.md](../docs/api-reglages.md). The contact
  in the `User-Agent` is the private repository's page until publication. German and Turkish texts are still not reviewed by a speaker of
  the language.
