### Added

- **Home screen, automatic mode: pure core only (no screen, no setting, nothing in `main/` calls it yet)**: one function,
  `ecran::accueil::decider()` (`components/core/include/ecran/accueil_auto.hpp`), that picks the profile of the home screen's
  bottom zone (Weather, Surf, Wind·Boat, Coast), says why with an enumeration (never text: the words will come from the
  seven-language table) and raises two flags, an enlarged alert banner (official orange or red alert) and a rain card with a
  countdown (rain in less than 60 min, unless a profile is pinned). Guards: at most one profile change per 30 min, never during
  a touch, stale data sends the screen back to Weather at once, a pinned profile wins, a house with no saved spot never goes to
  Surf or Coast, an unknown input never triggers a switch. **The rules and every threshold are a proposal, "Vigie's rule,
  adjustable, to be validated by the owner"**: none comes from an official source and none has been judged on a real screen.
  Tested on: PC only (48 host tests with AddressSanitizer and UBSan, also evaluated by g++ 14.2; 98 targeted mutations, all
  detected). Not compiled for a board, **not flashed and not run on any board**. Details:
  [docs/decisions/0015-accueil-mode-automatique.md](../docs/decisions/0015-accueil-mode-automatique.md),
  [docs/mesures/accueil-auto.md](../docs/mesures/accueil-auto.md).
