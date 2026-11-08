### Added

- **Sea: the settings page now chooses the spots, so the wheel's "Sea" button can finally show up**: a new card **"La mer"** ("The sea" in
  the seven languages) in the phone page lists up to 12 spots, lets you pick the one the screen shows, remove one, search Vigie's list of
  spots (from 2 characters, case and accents ignored, 8 results at most) and set the wind range of the kite verdict in whole knots. It
  writes the spot registry through a new route, **`GET`/`PUT /api/sea`** ([contract §15](../docs/api-reglages.md)): all or nothing, seven
  named refusals each with a sentence in the screen's language and the faulty field, and a request that changes nothing writes nothing. The
  rules live in a tested pure-core file (`mer_api.hpp`) on top of the registry operations that already existed. **The device does not
  serve or embed Vigie's list of spots: the page reads it from Vigie itself** (`GET <address>/v1/surf/spots`, the address of the alerts
  or the public service) and sends the device only an identifier and a name; the list comes from OpenStreetMap (ODbL), so the page shows
  the credit and the firmware stays free of it. Two registry buffers (about 2.3 KB each) live in PSRAM; without them `/api/sea` answers
  500 and the rest of the API is unaffected. Tested on: PC only (19 + 11 host tests, 27 targeted mutations all detected, the web page
  played by Playwright against a fake device: 316 of 316 checks, 103 of them for the new card, the seven new captures in
  `web/captures/`) and compiled for the Tab5 and the Waveshare 7B without any warning (+20 240 bytes on both: 3 078 544 and 3 059 008); **not flashed and not run on any
  board; the page has never talked to a real device, and never read Vigie's real list from a browser** (its search was played against an
  invented list). The six translations of the new sentences were written by an AI. Details:
  [docs/decisions/0025-mer-ecriture-du-registre-des-spots-lot-e.md](../docs/decisions/0025-mer-ecriture-du-registre-des-spots-lot-e.md),
  [docs/mesures/mer.md](../docs/mesures/mer.md).
