### Added

- **Sea: pure core of the future "sea" page (no screen, no setting, no network client yet)**: reads the `SEA`, `ACT` and `CRIT`
  lines that the Vigie API adds to its embedded frame when asked for a spot (`GET /v1/embedded/frame?spot=<id>`): four activities
  (surf, kite, boat, shore fishing) with a verdict, its reason, the best slot, two 24-hour strips and the criteria behind the
  verdict; the tides and the (estimated) tide coefficients; the sun; the strongest coastal warning. Accepted only with a right
  CRC-32 **and an `END` line**; an empty field stays unknown (never 0); coastal warnings that could not be read are never "nothing
  to fear"; a frame without a sea block keeps the previous sea and ages it (fresh, aged after 10 minutes, expired after 6 hours or
  once the 24 hours of the strips are over). The height of the sea between two tides is a half-cosine computed in integers.
  Everything is integer arithmetic in `components/core/` (no ESP-IDF include). **Limits written down**: the verdicts are Vigie's
  rules, not official ones and not for safety; the tide coefficient is an estimate, not SHOM's; the freshness thresholds are not
  calibrated; a coastal warning and a "good" surf were not in any real frame that day and are tested only on hand-edited,
  re-signed frames. Data: six real frames of the public Vigie instance (CC BY 4.0 sources, credits added to `THIRD_PARTY.md` and
  `components/core/ATTRIBUTION.md`) are kept as test data. Tested on: PC only (72 host tests with AddressSanitizer and UBSan, also
  built with ESP-IDF's GCC 14.2; 150 targeted mutations, all detected; the six frames checked against an independent reading in
  Python, which is independent for the CRC, the line splitting and the tides but not for the reading of the contract). Not compiled
  for a board (nothing in `main/` uses it yet), **not flashed and not run on any board**. Details:
  [docs/decisions/0018-mer-trame-vigie-et-vues.md](../docs/decisions/0018-mer-trame-vigie-et-vues.md),
  [docs/mesures/mer.md](../docs/mesures/mer.md).
