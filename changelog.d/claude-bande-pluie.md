### Added

- **Home screen, rain strip on the central line (one bar per quarter of an hour)**: when it is raining, or rain is expected within the
  next two hours, the line between the clock / hour-curve card and the five days shows the rain as bars, one per quarter of an hour
  (the step of the Open-Meteo data the device already keeps), coloured by strength (weak, moderate, strong, the same thresholds as the
  hour bars), with the time under every half hour (every hour when they do not fit), a « now » mark, a dotted « in one hour » mark and
  the sentence beside it ("Moderate rain", "Rain in 25 min", "Rain from 3 pm"; "· ends in 42 min" only when it fits). The strip
  always takes the left half of the line, the size of the old pill, never the full width: the right half stays for the official
  alert (only the portrait 720 x 1280, whose half is too narrow for the bars, gives it the full width when there is no alert, and
  keeps the old pill when there is one). A quarter without a value is an empty bar, never a dry one, and never says
  the rain stops. Without rain nothing changes: the pill ("Nothing to report", "Rain from…") stays. One phrase is added to the
  seven-language table ("ends in %d min"; **German, Dutch, Spanish, Italian and Turkish are written by the AI, not checked by a native
  speaker**). The rules are in a tested pure-core file (`accueil_bande_pluie.hpp`). See ADR-0027. Seen on: PC renders (LVGL 9.5.0,
  1280x720 and 1024x600), with the real quarters Open-Meteo gave for the house on 2026-11-09 at 12:18; **not flashed, not tested on
  board**.

### Changed

- **Home screen, the big "rain card" no longer covers the five days**: the automatic mode's rain composition (ADR-0023) is no longer
  chosen; the rain is said by the strip above, the five days stay visible. The orange or red alert composition is unchanged (its small
  rain tile stays). The card's code is still in the tree and will be removed once the strip is validated on a board.
