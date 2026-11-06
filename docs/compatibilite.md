# Compatibilité et versions testées

Mis à jour le 06/11/2026. **Règle : une fonction n'est « constatée » que sur la carte où on l'a vue fonctionner, avec le commit du firmware et la date.** Tout le reste est écrit « écrit, non testé sur carte », « prévu » ou « non vérifié ».

## Les cartes

| Carte | Écran / tactile | Rôle dans le projet | Exemplaire |
|---|---|---|---|
| **Tab5, révision ST7121** | ST7121 720 × 1280, tactile Sitronix (I2C 0x55, version 1) | **carte de développement** depuis le 06/11/2026 | MAC `E8:F6:0A:E2:F6:B6` ; quitte Home Assistant pendant le développement ; retour à ESPHome possible par la recette de la sauvegarde (jamais essayée) |
| Tab5, révision ST7123 | ST7123, tactile Sitronix (version 3) | détectée dans le code, **pas initialisée** | MAC `30:ED:A0:EA:8E:87`, en SAV chez M5Stack : pas sous la main |
| Tab5 d'origine | ILI9881C, tactile GT911 (I2C 0x14) | détectée dans le code, **pas initialisée** | aucun exemplaire connu |
| **Waveshare ESP32-P4 7B** | EK79007 1024 × 600, GT911, ES8311 + ES7210, sans expandeur | **compilation visée, essais physiques de temps en temps** (phase 6) | MAC `E8:F6:0A:E8:BB:1A`, écran de la maison : on ne la flashe qu'à la demande d'Axel |

Le contrôleur d'écran du Tab5 est **détecté au démarrage, avant d'initialiser la liaison DSI** (registre de version du tactile Sitronix en I2C 0x55 : 1 = ST7121, 3 = ST7123 ; GT911 en 0x14 = ILI9881C). Un contrôleur détecté mais pas encore pris en charge le dit dans le journal et ne tente pas d'allumer l'écran.

## Ce qui est testé où

États : **Constaté** · **Écrit, non testé sur carte** · **Prévu** (jalon du [plan](plan.md)) · **Non vérifié**.

| Fonction | Tab5 ST7121 | Tab5 ST7123 | Tab5 ILI9881C | Waveshare 7B |
|---|---|---|---|---|
| Chaîne de compilation ESP32-P4 (ESP-IDF 5.5.5) | Constaté (squelette : 38 s, 204 048 octets) | idem | idem | à compiler (CI prévue) |
| Détection du contrôleur d'écran | Constaté (« ST7121 détecté » au journal, piste A, 06/11) | Écrit, non testé | Écrit, non testé | sans objet |
| Écran allumé, image LVGL « Bonjour » | Constaté, avec une réserve ouverte après coupure de courant (voir [dépannage](depannage.md#ouverts)) ; détail dans `mesures/1a.md` | Non pris en charge (table d'init à porter) | Non pris en charge | Prévu (phase 6) |
| Tactile | Constaté (suit le doigt ; décalage corrigé au commit `e7a4803`, mesure du décalage résiduel : voir `mesures/1a.md`) | Non pris en charge | Non pris en charge | Prévu |
| Redémarrage logiciel (`esp_rom_software_reset_system()`) | Écrit ; l'écran revient-il tout seul : voir `mesures/1a.md` | — | — | Prévu (boucle du lien C6 connue sur cette carte) |
| Wi-Fi par le C6 (esp_hosted épinglé) + SNTP — jalon 1b | Prévu | — | — | Prévu |
| Audio duplex, annulation d'écho, mot d'activation — jalon 1c | Prévu | — | — | Prévu (ES8311 : volume limité à 75 %) |
| Portail captif par SoftAP — jalon 1d | Prévu | — | — | Prévu |

Logiciel **sans carte** (même code pour toutes les cartes) :

| Brique | État | Preuve et limite |
|---|---|---|
| Table météo WMO → libellé français + icône jour/nuit ; fuseaux IANA → chaîne POSIX TZ (posix_tz_db, MIT) | Écrit, vérifié à la compilation sur PC | 23 tests **exécutés sur le PC** (clang 23.1.5 x86-64, avec AddressSanitizer et UBSan, 0 échec) et évalués à la compilation par `static_assert` ; 18 mutations détectées sur 18 ; **jamais lancés sur une carte** (RISC-V : tailles, alignement et mémoire peuvent différer) |
| Page de réglages (front) et faux appareil | Écrit, essayé contre un faux appareil Node | 49 vérifications Playwright (rendu téléphone 390 × 844), 13 905 octets gzip ; **jamais servi par l'appareil** ; Chromium seulement (Safari iOS, Firefox : non vérifié) |
| Contrat de l'API de réglages | Écrit ([api-reglages.md](api-reglages.md)) | implémenté par le faux appareil seulement |

## Version du firmware et de la carte : comment on la note

- Le firmware se repère par le **commit git** (`git describe --always --dirty`) et la **carte** (`BOARD=tab5`, sous-révision d'écran détectée au démarrage). Un firmware compilé depuis un arbre modifié (`-dirty`) ne sert pas de preuve.
- Chaque résultat de `mesures/` et chaque entrée de [dépannage](depannage.md) porte une ligne **« Vu sur »** : carte, révision d'écran, MAC, commit, date. Pas de ligne = résultat non utilisable.
- Chaque commit qui touche une carte ou le firmware porte deux lignes finales : `Carte:` (`tab5-st7121`, `tab5-st7123`, `tab5-ili9881c`, `waveshare-7b` ou `aucune`) et `Teste-sur:` (par exemple `tab5-st7121 le 2026-11-06` ou `non testé sur carte`). Le commit ne dit **jamais** « testé » sans carte.
- Un **jalon** (fin de phase 1a, 1b…) est marqué par un tag annoté `jalon-<phase>-<carte>` (par exemple `jalon-1a-tab5-st7121`) dont le message liste ce qui est constaté et ce qui ne l'est pas.
- La **7B** : tant qu'aucun essai n'a eu lieu, sa colonne reste « Prévu » ; un essai physique y ajoute la date, le commit et la MAC lue sur la carte, comme pour le Tab5.
