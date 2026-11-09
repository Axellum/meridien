### Added

- **Home screen, automatic mode wired to the screen (no setting yet, nothing is pinned)**: the decision core of ADR-0015 is now
  called twice a second by a pure "pilot" (`components/core/include/ecran/accueil_pilote.hpp`) fed with the device's real
  measurements (worst official alert already filtered by the minimum-level setting, rain in N minutes, weather freshness,
  sunrise and sunset computed for the place, local offset, touch, time). Three things appear on the home screen:
  a **pastille** beside the place name that says why ("Auto · pluie proche", "Auto · alerte orange", "Auto · alerte rouge",
  "Auto · Météo"; a theme role colour, never a hard-coded one); a **rain card** that covers the five days when rain starts in
  less than 60 minutes or is falling ("Rain in 12 min", strength, duration, total, bars of the next hours); and an **enlarged
  alert banner** for an official orange or red alert (alert card, small rain tile on its right, the five days reduced to a
  strip). When both rules are true the alert wins and the rain stays in the small tile. While a finger is down the screen is
  not redrawn (10 s at most). Inputs that do not exist on the device yet (activities, spots, wind threshold, sea data, low
  tide) are "unknown": the profile can only be Weather today, so the pastille, the rain card and the enlarged banner are the
  only visible changes. Sixteen short phrases and three templates are added to the seven-language table (**German, Dutch,
  Spanish, Italian and Turkish are written by the AI, not checked by a native speaker**). The thresholds are still "Vigie's
  rule, to be validated by the owner", none official. See ADR-0023. Seen on: PC renders (LVGL 9.5.0, 1280x720, 1024x600,
  720x1280, seven languages) and compiled for the Tab5 and the Waveshare 7B; **not flashed, not tested on board**.
