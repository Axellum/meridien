### Added

- **Surf: pure core of the future "surf" page (no screen, no setting, no phone page yet)**: spots typed by the person (name,
  position, beach orientation, optional preferred wave height; at most 12), the two Open-Meteo requests (Marine API for waves,
  swell, sea temperature and sea level; forecast API for the wind, 54 hours each) and a tolerant reader of their answers
  (`null` is "no data", never 0; an answer with another unit is refused), the wind relative to the beach (offshore / cross-shore /
  onshore, our own formula and thresholds), the tide as indicative high and low waters, a house 0-10 session score with its limiting
  factor, the ranking of the spots and the best 2-6 hour slot of the day (06:00-21:00 local). Everything is integer arithmetic in
  `components/core/` (no ESP-IDF include). **Limits written down**: the direction of the swell relative to the beach is not
  modelled, the score has never been compared with real spots (a comparison with the owner's own spots is proposed), the heights
  are open-sea model estimates, the tide is indicative. Data: Open-Meteo, CC BY 4.0 (attribution added to `THIRD_PARTY.md` and
  `components/core/ATTRIBUTION.md`); 30 real answers (15 places worldwide, one of them off the sea) are kept as test data.
  Tested on: PC only (164 host tests with AddressSanitizer and UBSan; 142 targeted mutations, all detected; the 15 real places
  checked against an independent reading in Python, which is independent for the JSON reading, the cosine and the tide but not for
  the score and the slot rules). Not compiled for a board (nothing in `main/` uses it yet), **not flashed and not run on any
  board**. Details: [docs/decisions/0013-surf-donnees-note-et-limites.md](../docs/decisions/0013-surf-donnees-note-et-limites.md),
  [docs/mesures/surf.md](../docs/mesures/surf.md).
