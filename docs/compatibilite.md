# Compatibility and tested versions

## English · [Français](#version-française)

---

Updated on 07/11/2026. **Rule: a function is “observed” only on the board where it was seen working, with the firmware commit and the date.** Everything else is written “written, not tested on a board”, “planned” or “not verified”.

### The boards

| Board | Display / touch | Role in the project | Unit |
|---|---|---|---|
| **Tab5, ST7121 revision** | ST7121 720 × 1280, Sitronix touch (I2C 0x55, version 1) | **development board** since 06/11/2026 | the author's unit; leaves Home Assistant during development; return to ESPHome possible through the recipe kept with the author's local backup (never tried) |
| Tab5, ST7123 revision | ST7123, Sitronix touch (version 3) | detected in the code, **not initialised** | the author's unit, at M5Stack for repair: not at hand |
| Original Tab5 | ILI9881C, GT911 touch (I2C 0x14) | detected in the code, **not initialised** | no known unit |
| **Waveshare ESP32-P4 7B** | EK79007 1024 × 600, GT911, ES8311 + ES7210, no expander | **board layer written and built (07/11/2026), physical trial not done yet** (phase 6) | the author's unit, the screen in service at home: it is flashed only at the author's request |

The display controller of the Tab5 is **detected at start-up, before the DSI link is initialised** (version register of the Sitronix touch controller on I2C 0x55: 1 = ST7121, 3 = ST7123; GT911 at 0x14 = ILI9881C). A controller that is detected but not yet supported says so in the log and does not try to light the screen.

### What is tested where

States: **Observed** · **Written, not tested on a board** · **Planned** (milestone of the [plan](plan.md)) · **Not verified**.

| Function | Tab5 ST7121 | Tab5 ST7123 | Tab5 ILI9881C | Waveshare 7B |
|---|---|---|---|---|
| ESP32-P4 build chain (ESP-IDF 5.5.5) | Observed (skeleton: 38 s, 204,048 bytes) | same | same | Observed on the PC: builds with `BOARD=waveshare-7b` (07/11/2026, commit `c48c418`, 1,837,760 bytes, 56 % of the application slot free); the Tab5 build is unchanged ([mesures/7b-materiel.md](mesures/7b-materiel.md)) |
| Display controller detection | Observed (“ST7121 détecté” in the log, track A, 06/11) | Written, not tested | Written, not tested | Not done, on purpose: one panel model; “EK79007” is declared, not measured ([carte-7b.md](carte-7b.md)) |
| Screen on, LVGL “Bonjour” image | Observed, with a reservation left open after a power cut (see [troubleshooting](depannage.md#open)); details in `mesures/1a.md` | Not supported (init table to be ported) | Not supported | Written, not tested on a board (EK79007, DSI 900 Mbit/s taken from two sources; Waveshare's BSP says 1000) |
| Touch | Observed (follows the finger; offset fixed at commit `e7a4803`, mean residual error (−6, −4) px over five targets, [mesures/1a.md](mesures/1a.md)) | Not supported | Not supported | Written, not tested on a board (GT911 polled, INT not wired; simulated-press self-test exists, a finger is needed) |
| Software reset (`esp_rom_software_reset_system()`) | Observed: two series of 20 in a row (06/11/2026, commits `77e03de` and `97520e2`), reset cause 0x03 and the C6 link back at the first attempt every time ([mesures/1b.md](mesures/1b.md)) | — | — | Written, not tested on a board (ROM reset; 10 consecutive resets scripted in `tools/essai_7b.py`, not run) |
| Weather on the board (HTTPS Open-Meteo every 15 min, NVS cache, “stale” flag) | Observed (06/11/2026, commit `55644aa`: Lyon read in the log and equal to the live API, twice 15 min apart; state back from the cache after a **software** restart; Wi-Fi cut, power cut and change of place not tested; free internal RAM unexplained drop, see [mesures/meteo.md](mesures/meteo.md)) | Written, not tested | Written, not tested | Planned (phase 6) |
| Wi-Fi through the C6 (esp_hosted pinned) + SNTP — milestone 1b | Link, Wi-Fi association and IP address observed (06/11/2026). Real time displayed: observed once and judged right by the owner, but the SNTP time arrives in steps of about 15 s; a fix is written and **not yet measured**. Full power cut: not done, postponed by the owner ([mesures/1b.md](mesures/1b.md)) | — | — | Written, not tested on a board (esp_hosted 2.12.12 pinned; the factory C6 firmware is never updated) |
| Full-duplex audio, echo cancellation, wake word — milestone 1c | Planned | — | — | Planned (ES8311/ES7210 code written in the board layer, volume capped at 0 dB = 75 %; no audio API in `board.h` yet) |
| Captive portal through SoftAP — milestone 1d | Planned | — | — | Planned |

Software **without a board** (same code for all boards):

| Block | State | Evidence and limit |
|---|---|---|
| WMO weather table → French label + day/night icon; IANA time zones → POSIX TZ string (posix_tz_db, MIT) | Written, verified on the PC | 23 tests **run on the PC** (clang 23.1.5 x86-64, with AddressSanitizer and UBSan, 0 failures) and evaluated at compile time by `static_assert`; 18 mutations detected out of 18; **never run on a board** (RISC-V: sizes, alignment and memory may differ) |
| Minimal JSON reading, Open-Meteo response, local date and time in French | Written, verified on the PC | tests run on the PC with the same detectors, mutations detected (53 of this batch); **real Open-Meteo responses captured by the PC and then stored in the test, no request made by the board**; never run on a board |
| Voice assistant: states, SSE stream, OpenAI-compatible requests, sentence splitting | Written, verified on the PC | tests run on the PC (135 mutations of this batch detected); **no real STT/LLM/TTS service called**; never run on a board |
| Whole core | 252 tests run with ASan + UBSan, 206 mutations detected out of 206 (`main` on 06/11/2026) | proves nothing about the behaviour of the board's RISC-V |
| Settings page (front end) and fake device | Written, tried against a fake device in Node | 49 Playwright checks (phone rendering 390 × 844), 13,905 bytes gzip; **never served by the device**; Chromium only (Safari iOS, Firefox: not verified) |
| Settings API contract | Written ([api-reglages.md](api-reglages.md)) | implemented by the fake device only |

### Firmware and board version: how it is noted

- The firmware is identified by the **git commit** (`git describe --always --dirty`) and the **board** (`BOARD=tab5`, display sub-revision detected at start-up). A firmware built from a modified tree (`-dirty`) is not evidence.
- Every result in `mesures/` and every entry of [troubleshooting](depannage.md) carries a **“Seen on”** line: board, display revision, commit, date. No MAC address is published here: it is read on the board, and compared, before any write. No line = result not usable.
- Every commit that touches a board or the firmware carries two final lines: `Carte:` (`tab5-st7121`, `tab5-st7123`, `tab5-ili9881c`, `waveshare-7b` or `aucune`) and `Teste-sur:` (for example `tab5-st7121 le 2026-11-06` or `non testé sur carte`). The commit **never** says “tested” without a board.
- A **milestone** (end of phase 1a, 1b…) is marked by an annotated tag `jalon-<phase>-<carte>` (for example `jalon-1a-tab5-st7121`) whose message lists what is observed and what is not.
- The **7B**: its column says “Written, not tested on a board” until a physical trial has taken place; a trial adds the date and the commit, as for the Tab5. The procedure is `tools/essai_7b.py` (full flash backup read twice, flash, tests, restoration verified); see [carte-7b.md](carte-7b.md) and [mesures/7b-materiel.md](mesures/7b-materiel.md).

---

## Version Française

---

Mis à jour le 07/11/2026. **Règle : une fonction n'est « constatée » que sur la carte où on l'a vue fonctionner, avec le commit du firmware et la date.** Tout le reste est écrit « écrit, non testé sur carte », « prévu » ou « non vérifié ».

### Les cartes

| Carte | Écran / tactile | Rôle dans le projet | Exemplaire |
|---|---|---|---|
| **Tab5, révision ST7121** | ST7121 720 × 1280, tactile Sitronix (I2C 0x55, version 1) | **carte de développement** depuis le 06/11/2026 | exemplaire de l'auteur ; quitte Home Assistant pendant le développement ; retour à ESPHome possible par la recette gardée avec la sauvegarde locale de l'auteur (jamais essayée) |
| Tab5, révision ST7123 | ST7123, tactile Sitronix (version 3) | détectée dans le code, **pas initialisée** | exemplaire de l'auteur, en SAV chez M5Stack : pas sous la main |
| Tab5 d'origine | ILI9881C, tactile GT911 (I2C 0x14) | détectée dans le code, **pas initialisée** | aucun exemplaire connu |
| **Waveshare ESP32-P4 7B** | EK79007 1024 × 600, GT911, ES8311 + ES7210, sans expandeur | **couche carte écrite et compilée (07/11/2026), essai physique pas encore fait** (phase 6) | exemplaire de l'auteur, l'écran en service à la maison : on ne la flashe qu'à la demande de l'auteur |

Le contrôleur d'écran du Tab5 est **détecté au démarrage, avant d'initialiser la liaison DSI** (registre de version du tactile Sitronix en I2C 0x55 : 1 = ST7121, 3 = ST7123 ; GT911 en 0x14 = ILI9881C). Un contrôleur détecté mais pas encore pris en charge le dit dans le journal et ne tente pas d'allumer l'écran.

### Ce qui est testé où

États : **Constaté** · **Écrit, non testé sur carte** · **Prévu** (jalon du [plan](plan.md)) · **Non vérifié**.

| Fonction | Tab5 ST7121 | Tab5 ST7123 | Tab5 ILI9881C | Waveshare 7B |
|---|---|---|---|---|
| Chaîne de compilation ESP32-P4 (ESP-IDF 5.5.5) | Constaté (squelette : 38 s, 204 048 octets) | idem | idem | Constaté sur le PC : compile avec `BOARD=waveshare-7b` (07/11/2026, commit `c48c418`, 1 837 760 octets, 56 % de l'emplacement d'application libre) ; la compilation du Tab5 n'est pas modifiée ([mesures/7b-materiel.md](mesures/7b-materiel.md)) |
| Détection du contrôleur d'écran | Constaté (« ST7121 détecté » au journal, piste A, 06/11) | Écrit, non testé | Écrit, non testé | Non faite, volontairement : un seul modèle de dalle ; « EK79007 » est déclaré, pas mesuré ([carte-7b.md](carte-7b.md#version-française)) |
| Écran allumé, image LVGL « Bonjour » | Constaté, avec une réserve ouverte après coupure de courant (voir [dépannage](depannage.md#ouverts)) ; détail dans `mesures/1a.md` | Non pris en charge (table d'init à porter) | Non pris en charge | Écrit, non testé sur carte (EK79007, DSI à 900 Mbit/s pris dans deux sources ; le BSP de Waveshare dit 1000) |
| Tactile | Constaté (suit le doigt ; décalage corrigé au commit `e7a4803`, écart résiduel moyen (−6, −4) px sur cinq cibles, [mesures/1a.md](mesures/1a.md)) | Non pris en charge | Non pris en charge | Écrit, non testé sur carte (GT911 interrogé, INT non câblée ; un auto-test à appuis simulés existe, il faut un doigt) |
| Redémarrage logiciel (`esp_rom_software_reset_system()`) | Constaté : deux séries de 20 de suite (06/11/2026, commits `77e03de` et `97520e2`), cause de reset 0x03 et lien C6 revenu au premier essai à chaque fois ([mesures/1b.md](mesures/1b.md)) | — | — | Écrit, non testé sur carte (reset ROM ; dix remises à zéro de suite scriptées dans `tools/essai_7b.py`, non exécutées) |
| Météo sur la carte (Open-Meteo en HTTPS toutes les 15 min, cache NVS, indicateur « périmé ») | Constaté (06/11/2026, commit `55644aa` : Lyon lue dans le journal et égale à l'API réelle, deux fois à 15 min d'écart ; état revenu du cache après un redémarrage **logiciel** ; Wi-Fi coupé, coupure de courant et changement de lieu non testés ; baisse inexpliquée de la RAM interne libre, voir [mesures/meteo.md](mesures/meteo.md)) | Écrit, non testé | Écrit, non testé | Prévu (phase 6) |
| Wi-Fi par le C6 (esp_hosted épinglé) + SNTP — jalon 1b | Lien, association Wi-Fi et adresse IP constatés (06/11/2026). Heure réelle à l'écran : constatée une fois et jugée juste par Axel, mais l'heure SNTP arrive par paliers d'environ 15 s ; un correctif est écrit et **pas encore mesuré**. Coupure de courant complète : non faite, reportée par Axel ([mesures/1b.md](mesures/1b.md)) | — | — | Écrit, non testé sur carte (esp_hosted 2.12.12 épinglé ; le micrologiciel d'origine du C6 n'est jamais mis à jour) |
| Audio duplex, annulation d'écho, mot d'activation — jalon 1c | Prévu | — | — | Prévu (code ES8311/ES7210 écrit dans la couche carte, volume plafonné à 0 dB = 75 % ; pas encore d'API audio dans `board.h`) |
| Portail captif par SoftAP — jalon 1d | Prévu | — | — | Prévu |

Logiciel **sans carte** (même code pour toutes les cartes) :

| Brique | État | Preuve et limite |
|---|---|---|
| Table météo WMO → libellé français + icône jour/nuit ; fuseaux IANA → chaîne POSIX TZ (posix_tz_db, MIT) | Écrit, vérifié sur PC | 23 tests **exécutés sur le PC** (clang 23.1.5 x86-64, avec AddressSanitizer et UBSan, 0 échec) et évalués à la compilation par `static_assert` ; 18 mutations détectées sur 18 ; **jamais lancés sur une carte** (RISC-V : tailles, alignement et mémoire peuvent différer) |
| Lecture JSON minimale, réponse Open-Meteo, date et heure locales en français | Écrit, vérifié sur PC | tests exécutés sur le PC avec les mêmes détecteurs, mutations détectées (53 de ce lot) ; **réponses Open-Meteo réelles captées par le PC puis rangées dans le test, aucune requête faite par la carte** ; jamais lancés sur une carte |
| Assistant vocal : états, flux SSE, requêtes OpenAI-compatibles, découpe en phrases | Écrit, vérifié sur PC | tests exécutés sur le PC (135 mutations de ce lot détectées) ; **aucun service STT/LLM/TTS réel appelé** ; jamais lancés sur une carte |
| Ensemble du noyau | 252 tests exécutés avec ASan + UBSan, 206 mutations détectées sur 206 (`main` au 06/11/2026) | ne prouve rien sur le comportement du RISC-V de la carte |
| Page de réglages (front) et faux appareil | Écrit, essayé contre un faux appareil Node | 49 vérifications Playwright (rendu téléphone 390 × 844), 13 905 octets gzip ; **jamais servi par l'appareil** ; Chromium seulement (Safari iOS, Firefox : non vérifié) |
| Contrat de l'API de réglages | Écrit ([api-reglages.md](api-reglages.md)) | implémenté par le faux appareil seulement |

### Version du firmware et de la carte : comment on la note

- Le firmware se repère par le **commit git** (`git describe --always --dirty`) et la **carte** (`BOARD=tab5`, sous-révision d'écran détectée au démarrage). Un firmware compilé depuis un arbre modifié (`-dirty`) ne sert pas de preuve.
- Chaque résultat de `mesures/` et chaque entrée de [dépannage](depannage.md#version-française) porte une ligne **« Vu sur »** : carte, révision d'écran, commit, date. Aucune adresse MAC n'est publiée ici : elle est lue sur la carte, et comparée, avant toute écriture. Pas de ligne = résultat non utilisable.
- Chaque commit qui touche une carte ou le firmware porte deux lignes finales : `Carte:` (`tab5-st7121`, `tab5-st7123`, `tab5-ili9881c`, `waveshare-7b` ou `aucune`) et `Teste-sur:` (par exemple `tab5-st7121 le 2026-11-06` ou `non testé sur carte`). Le commit ne dit **jamais** « testé » sans carte.
- Un **jalon** (fin de phase 1a, 1b…) est marqué par un tag annoté `jalon-<phase>-<carte>` (par exemple `jalon-1a-tab5-st7121`) dont le message liste ce qui est constaté et ce qui ne l'est pas.
- La **7B** : sa colonne dit « Écrit, non testé sur carte » tant qu'aucun essai physique n'a eu lieu ; un essai y ajoute la date et le commit, comme pour le Tab5. La procédure est `tools/essai_7b.py` (sauvegarde de la flash entière lue deux fois, flash, essais, restauration vérifiée) ; voir [carte-7b.md](carte-7b.md#version-française) et [mesures/7b-materiel.md](mesures/7b-materiel.md).
