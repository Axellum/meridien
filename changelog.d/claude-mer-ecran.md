### Added

- **Sea: choose, add and remove spots on the screen itself, no phone needed**: a third tab, "La mer", in the on-screen
  location-and-weather settings. It lists the registered spots (touch one to show it on the screen, "Retirer" removes it), has
  "Ajouter un spot" and the wind range of the kite verdict (− and + in whole knots; a button whose step the registry would refuse is
  greyed and sends nothing). "Ajouter un spot" opens a search over **Vigie's list of spots, which the device now reads itself**
  (`GET /v1/surf/spots`, 20 405 bytes raw and 4 695 gzipped on 08/11/2026), **on demand only**: one background task per read and
  none at boot, the TLS lock shared with the city search, the list kept for 10 minutes, a 5 s pause after a failure, an answer
  over 96 KB refused. The search ignores case and accents; spots already kept stay visible and greyed ("Déjà ajouté"); the
  OpenStreetMap credit (ODbL) is written under the list; touching a spot adds it at the end without changing the displayed one.
  The settings page of a phone stays and writes the same registry. **Limits written down**: the internal-RAM cost of the TLS
  handshake of this download on a board is not measured; a failing DNS, TLS or a busy TLS lock says "no connection" even when
  Vigie is the one that is down; the six translations are written by an AI; "Réinitialiser" leaves the sea registry untouched.
  Tested on: PC only (126 host tests run in full mode, 56 new targeted mutations all detected, a render bench with a virtual
  finger in seven languages at three sizes) and compiled for the Tab5 and the Waveshare 7B; **not flashed and not run on any
  board**. Details: [docs/decisions/0026-mer-choisir-ajouter-retirer-sur-l-ecran-lot-f.md](../docs/decisions/0026-mer-choisir-ajouter-retirer-sur-l-ecran-lot-f.md),
  [docs/mesures/mer.md](../docs/mesures/mer.md).
