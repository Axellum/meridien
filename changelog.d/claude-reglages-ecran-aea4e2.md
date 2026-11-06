### Added

- **Settings and guided first install on the screen (ADR-0007)**: a list of six sections (Network, Place and weather, Language,
  Display, Voice assistant, Device), an on-screen keyboard in seven languages (Turkish included), a Wi-Fi list and connection
  screen with a 30-second timeout, a city search, and a 7-step guided installation that resumes after a reboot, can be replayed
  ("Refaire l'installation guidée") and is distinct from the two-gesture "Réinitialiser l'appareil". Texts are in all seven
  languages (the 53 new translations were written by an AI and are not reviewed by native speakers). Seen on: the PC rendering
  bench only (software LVGL, 15 scripted journeys in 7 languages and 3 sizes, among them a check that every button and the
  brightness slider reach 72 px and that a page whose content overflows shows a scrollbar), see [docs/mesures/2-reglages-ecran.md](../docs/mesures/2-reglages-ecran.md).
  Not tested on board: no flash, no real touch, no persistent memory and no real Wi-Fi yet.

*En français : réglages et installation guidée sur l'écran ; vérifiés sur le banc de rendu PC seulement, pas encore sur la carte.*
