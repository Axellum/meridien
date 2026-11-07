### Added

- **Settings and guided first install on the screen (ADR-0007)**: a list of six sections (Network, Place and weather, Language,
  Display, Voice assistant, Device), an on-screen keyboard in seven languages (Turkish included), a Wi-Fi list and connection
  screen with a 30-second timeout, a city search, and a 7-step guided installation that resumes after a reboot, can be replayed
  ("Refaire l'installation guidée") and is distinct from the two-gesture "Réinitialiser l'appareil". Texts are in all seven
  languages (the 53 new translations were written by an AI and are not reviewed by native speakers). Seen on: the PC rendering
  bench (software LVGL, 15 scripted journeys in 7 languages and 3 sizes, among them a check that every button and the
  brightness slider reach 72 px and that a page whose content overflows shows a scrollbar), see [docs/mesures/2-reglages-ecran.md](../docs/mesures/2-reglages-ecran.md).
- **The settings screens are wired to the device (milestone 6)**: `HoteAppareil` (pure core, 28 tests, 45 mutations detected) keeps
  language, theme, brightness and place in the same single source as the web page, the service presets/URLs/models/voices and keys
  (never read back), the Wi-Fi kept after a success, and the install progress byte, in NVS; `main/ui_reglages_ouvrir.cpp` opens the
  screens on a screen of their own. Seen on: Tab5 ST7121, commit `176de0d`, 2026-11-07, **without a finger** (a virtual pointer
  replays the gestures through LVGL's own object search): 29 checks, 0 failures — the brightness slider writes the value the web page
  reads, a real software restart keeps the brightness, the service text and the install step, the install resumes at step 3 and does
  not reopen once finished. **Not verified**: a real finger, the look on the real screen, the keyboard keys one by one, the real
  Wi-Fi scan/connect (a stub answers until `jalon-1b` delivers it), the city search, a power cut. Details and the memory measurements:
  [docs/mesures/6-reglages-appareil.md](../docs/mesures/6-reglages-appareil.md).

### Changed

- **TinyTTF glyph bitmaps go to PSRAM** (all fonts, on the first opening of the settings): about 22 KB of glyph bitmaps (measured on
  the settings list) no longer sit in internal RAM. Internal RAM stays very tight while the weather's TLS handshake runs (down to 20 bytes free
  measured): see the memory section of the page above; the TLS fix is on the `jalon-1b` track and is not verified here.

*En français : réglages et installation guidée branchés à l'appareil ; vus sur le Tab5 ST7121 sans doigt (29 vérifications, 0 échec) ; doigt réel, rendu à l'œil, Wi-Fi réel et recherche de ville non vérifiés ; mémoire interne très juste pendant le TLS de la météo.*
