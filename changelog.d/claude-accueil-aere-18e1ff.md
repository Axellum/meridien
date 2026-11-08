### Changed

- **Home screen redone ("airy sketch", Weather profile)**: themes Sky, Dark board and Light now show the place with the Settings
  button, the big clock, the date and the current weather on the left; the next 12 hours (curve, icons, rain bars, maximum and
  minimum) in a glass card on the right; one banner under them (the rain to come, "Rain from 13:00 · 6.5 mm", and the official
  alert, "Strong wind · 14:00–22:00", each keeping its half; in French "Pluie dès 13 h", "14–22 h"); and five days (name and icon, high / low, a ribbon of the
  temperatures, wind with its arrow, rain capsule) in a card that takes the height gained on the 7B. The seven tiles of
  quantities (wind, humidity, pressure, UV, sun, moon) leave the home screen; their data stay in the weather view. The Clock
  theme is unchanged (pixel for pixel, rendered before and after). See ADR-0016. Seen on: PC renders (LVGL 9.5.0, 1280x720,
  1024x600, 720x1280, seven languages); not tested on board.
- **"Nothing to report" and "stale data" states**: with no rain to come and no alert, the banner is a single calm chip; a weather
  reading that is too old is drawn greyed with "Data from 07:52 · stale". The official alert is never greyed.

### Added

- **Pure core `ecran/accueil.hpp`**: monotone curve (integer Fritsch–Carlson), rain episode within 12 hours, banner content,
  rain capsules and bars (square-root scale with a floor, so drizzle stays small), minimum and maximum, layout of the zones,
  greying of a theme. Tested on the PC (compiled and run with AddressSanitizer and UBSan) with mutation tests. Tested on:
  not tested on board.
- Vigie banner: start and end of the alert (`debut_s`, `fin_s`) for the "2–10 pm" window; day wind and direction in the weather
  view.
