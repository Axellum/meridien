# ADR-0005 — Une couche carte : développer sur le Tab5, garder la 7B compilable

> **Summary (EN)** — Proposed on 2026-11-06, awaiting the owner's validation.
> - Context: development happens on the Tab5 (1280×720); the Waveshare 7B (1024×600) is kept compatible and tested "from time to time".
> - Decision: one folder per board (`boards/tab5/`, `boards/waveshare_7b/`) holding its pins, drivers and `sdkconfig.defaults.board`; the application code only knows interfaces (display, touch, backlight, audio, hardware clock); no `if (tab5)` outside `boards/` and the `bsp_*` components.
> - On the Tab5 the display controller is detected at run time (ST7121 / ST7123 / ILI9881C, from the Sitronix touch version register at I2C 0x55; a GT911 at 0x14 means ILI9881C) before the DSI is initialised. The UI is drawn from the real screen size rather than absolute coordinates, which avoids the 936 geometry values that had to be reworked for the 7B in the Tab5 repository.
> - The CI is meant to compile both boards from the first day; the physical test of the 7B happens at the end of phases 2, 4 and 5, not at every batch. Orientation is fixed per board and wired into the panel (software rotation costs about 13 ms per full screen and needs more frame buffers).
> - Consequence: the cost of 7B compatibility is spread from the start instead of a late port.

- **Statut** : proposée le 06/11/2026, à valider par Axel.
- **Contexte** : développement sur le Tab5 (1280×720), compatibilité Waveshare 7B (1024×600) testée « de temps en temps ». Faits matériels : [04-audit-depot-tab5.md](../recherche/04-audit-depot-tab5.md).
- **Décision** :
  1. Un dossier par carte (`boards/tab5/`, `boards/waveshare_7b/`) avec ses broches, ses pilotes et son `sdkconfig.defaults.board` ; le code applicatif ne connaît que les interfaces (écran, tactile, rétroéclairage, audio, horloge matérielle). Aucun `if (tab5)` hors de `boards/` et des composants `bsp_*`.
  2. Tab5 : le **contrôleur d'écran est détecté à l'exécution** (ST7121 / ST7123 / ILI9881C, par le registre de version du tactile Sitronix à 0x55, GT911 à 0x14 = ILI9881C) avant d'initialiser le DSI.
  3. Interface dessinée **à partir de la taille réelle de l'écran** (reprendre l'idée de `tab5_geometrie.h`, ADR-0061 du dépôt Tab5), pas en coordonnées absolues : on évite les 936 valeurs de géométrie qu'il a fallu reprendre pour la 7B.
  4. La CI compile **les deux cartes** dès le premier jour. Le test physique de la 7B se fait à la fin des phases 2, 4 et 5 (jalons), pas à chaque lot.
  5. Orientation fixe par carte, câblée dans le panneau (la rotation logicielle coûte ~13 ms par écran entier et exige plus de frame buffers).
- **Conséquences** : le coût de la compatibilité 7B est étalé dès le départ (couche carte écrite proprement) au lieu d'un portage tardif. Les deux tablettes d'Axel et la 7B sont en service chez lui : voir [plan.md](../plan.md), décision 1.

- **Addendum du 07/11/2026 (couche carte de la 7B, proposé, à valider par Axel)** — ce que la première réalisation de la 7B a précisé ou changé par rapport à ci-dessus :
  1. Le dossier de la 7B s'appelle `boards/waveshare-7b/` (avec un trait d'union, comme la valeur de `BOARD`) et sa couche `components/bsp_waveshare_7b/`, pas `boards/waveshare_7b/`.
  2. **Sélection de la carte** : variable CMake `BOARD` (défaut `tab5`). Les couches `components/bsp_*` des cartes non choisies sont exclues de la compilation (`EXCLUDE_COMPONENTS`) pour que le gestionnaire de composants ne télécharge pas leurs dépendances, et chaque carte autre que le Tab5 a son verrou de dépendances (`boards/<carte>/dependencies.lock` ; le Tab5 garde `dependencies.lock` de la racine). Changer de carte demande un `sdkconfig` neuf (voir [carte-7b.md](../carte-7b.md#compiler)).
  3. **Point 5 nuancé** : la 7B d'Axel est montée à l'envers par rapport aux démos Waveshare. La couche propose donc une orientation **choisie à la compilation** (`WS7B_ROTATION` = 0 par défaut, ou 180), faite par LVGL + l'accélérateur PPA ; en 0 il n'y a aucune rotation ni coût. L'orientation reste fixe pour un binaire donné.
  4. **Extinction réelle de la dalle** : le rétroéclairage de la 7B est analogique et ne devient jamais noir. Sur cette carte, `board_set_backlight_percent(0)` éteint aussi la dalle (voile noir, 0x28, 0x10 ; réveil 0x11, 120 ms, 0x29) ; le comportement du Tab5 n'est pas touché. `board.h` n'a pas changé.
  5. Rien de ceci n'a été vu sur la carte : [mesures/7b-materiel.md](../mesures/7b-materiel.md).
