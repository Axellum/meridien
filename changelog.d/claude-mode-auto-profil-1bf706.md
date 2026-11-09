### Added

- **Home screen, "Profil" setting (Auto or a pinned profile) and "Profil ▸" in the press-and-hold wheel**: the pilot of the
  automatic mode (ADR-0023) now reads a setting, `profil`: `auto` (default: the rules of ADR-0015 decide) or `meteo` (the screen stays
  on the weather; an official alert still shows its enlarged banner). It is set from the web page (`/api/display`: `profil`, and
  `profils`, the list of what the device offers), from the on-screen settings (a row of pastilles in "Affichage") and from a new
  **"Profil ▸" family in the wheel** (last button of the first ring, after "Mer" when both exist, five buttons at most, chosen by the same
  pure-core list as ADR-0024; second ring: Auto and Weather, the one in force marked in the
  accent colour; choosing it writes the setting and the home screen follows at once). The device offers **only what it can draw**
  (Auto and Weather). A stored value outside the table (a corrupt NVS byte) means "no pin", never a pinned profile by accident. Pinned:
  the pastille says "Météo · épinglé" with "Auto en pause", the rain card does not open; unpinning gives the hand back to the rules at
  once. The setting is stored in its own NVS key (`profil`), next to the existing "affichage" blob, which does not change. Three glyphs
  are added to the wheel's icon font (`mdi_roue.ttf`: 5 → 8 glyphs, 2 140 bytes, regenerated from the merged manifest). Seven languages (**German, Dutch, Spanish, Italian
  and Turkish are written by the AI, not checked by a native speaker**). Nothing about the sea. See ADR-0023 (« Complément »). Seen on:
  PC renders (LVGL 9.5.0, 1280x720, 1024x600, 720x1280), the PC tests and the web walkthrough on the fake device; compiled for the Tab5
  and the Waveshare 7B; **not flashed, not tested on board** (the NVS write and the touch of the wheel are not seen on a real device).

### Fixed

- **Wheel: the word of the unfolded family is no longer drawn under the second ring** (`main/ui_roue.cpp`): the hub already says it, and the
  word, placed outwards on the axis of its button, was covered by the nearest disk of the second ring (« Profil » lost its last letter).
  Seen on PC renders; see `docs/depannage.md`.
