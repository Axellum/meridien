# ADR-0005 — Une couche carte : développer sur le Tab5, garder la 7B compilable

- **Statut** : proposée le 06/11/2026, à valider par Axel.
- **Contexte** : développement sur le Tab5 (1280×720), compatibilité Waveshare 7B (1024×600) testée « de temps en temps ». Faits matériels : [04-audit-depot-tab5.md](../recherche/04-audit-depot-tab5.md).
- **Décision** :
  1. Un dossier par carte (`boards/tab5/`, `boards/waveshare_7b/`) avec ses broches, ses pilotes et son `sdkconfig.defaults.board` ; le code applicatif ne connaît que les interfaces (écran, tactile, rétroéclairage, audio, horloge matérielle). Aucun `if (tab5)` hors de `boards/` et des composants `bsp_*`.
  2. Tab5 : le **contrôleur d'écran est détecté à l'exécution** (ST7121 / ST7123 / ILI9881C, par le registre de version du tactile Sitronix à 0x55, GT911 à 0x14 = ILI9881C) avant d'initialiser le DSI.
  3. Interface dessinée **à partir de la taille réelle de l'écran** (reprendre l'idée de `tab5_geometrie.h`, ADR-0061 du dépôt Tab5), pas en coordonnées absolues : on évite les 936 valeurs de géométrie qu'il a fallu reprendre pour la 7B.
  4. La CI compile **les deux cartes** dès le premier jour. Le test physique de la 7B se fait à la fin des phases 2, 4 et 5 (jalons), pas à chaque lot.
  5. Orientation fixe par carte, câblée dans le panneau (la rotation logicielle coûte ~13 ms par écran entier et exige plus de frame buffers).
- **Conséquences** : le coût de la compatibilité 7B est étalé dès le départ (couche carte écrite proprement) au lieu d'un portage tardif. Les deux tablettes d'Axel et la 7B sont en service chez lui : voir [plan.md](../plan.md), décision 1.
