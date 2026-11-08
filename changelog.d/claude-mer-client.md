### Added

- **Sea: network client and spot registry (no screen, no page, no setting yet)**: a background task asks the Vigie API for the
  embedded frame of **the one chosen spot** (`GET /v1/embedded/frame?device_type=generic&spot=<id>`, plus the kite wind range only
  when it is not Vigie's default 12–25 knots), reads it with the core of the previous change and keeps the sea in two PSRAM buffers
  for the future page. **The position of the house is not sent.** The registry holds up to 12 spots (a Vigie slug and a name),
  exactly one of them chosen, and the wind range; it is kept in NVS and repaired on read (a damaged entry or a duplicate is dropped,
  the other spots are kept). **An empty registry, the state of a new device, means no request and not even a task** (its 12 KB stack
  in internal RAM is taken only when a spot is first chosen), so this change adds no traffic and no memory use until something
  chooses a spot — and nothing but code can write the registry yet. Pace: one request every 10 minutes at most (the alerts'
  politeness rules: first try 120–180 s after the network and the clock are ready, doubling waits after failures, 30 minutes after a
  429 or 503, 60 s between two requests, a short debounce when the spot or the wind changes); the Vigie address is the one of the
  alerts. A frame without a sea block keeps the previous sea (it ages). **Limits written down**: a spot that Vigie does not know
  (404) or a wind range it refuses (422) is retried at the normal back-off, not stopped; the memory cost of the task is not measured
  on a board; the 10-minute period and the freshness thresholds are not calibrated. Tested on: PC only (55 host tests with
  AddressSanitizer and UBSan, also built with ESP-IDF's GCC 14.2; 68 targeted mutations, all detected) and compiled for the Tab5 and
  the Waveshare 7B; **not flashed and not run on any board**. Details:
  [docs/decisions/0019-mer-client-et-registre-de-spots.md](../docs/decisions/0019-mer-client-et-registre-de-spots.md),
  [docs/mesures/mer.md](../docs/mesures/mer.md).
