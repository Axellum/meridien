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
| **Waveshare ESP32-P4 7B** | EK79007 1024 × 600, GT911, ES8311 + ES7210, no expander | **build aimed at, physical tests now and then** (phase 6) | the author's unit, the screen in service at home: it is flashed only at the author's request |

The display controller of the Tab5 is **detected at start-up, before the DSI link is initialised** (version register of the Sitronix touch controller on I2C 0x55: 1 = ST7121, 3 = ST7123; GT911 at 0x14 = ILI9881C). A controller that is detected but not yet supported says so in the log and does not try to light the screen.

### What is tested where

States: **Observed** · **Written, not tested on a board** · **Planned** (milestone of the [plan](plan.md)) · **Not verified**.

| Function | Tab5 ST7121 | Tab5 ST7123 | Tab5 ILI9881C | Waveshare 7B |
|---|---|---|---|---|
| ESP32-P4 build chain (ESP-IDF 5.5.5) | Observed (skeleton: 38 s, 204,048 bytes) | same | same | to be built (CI planned) |
| Display controller detection | Observed (“ST7121 détecté” in the log, track A, 06/11) | Written, not tested | Written, not tested | not applicable |
| Screen on, LVGL “Bonjour” image | Observed, with a reservation left open after a power cut (see [troubleshooting](depannage.md#open)); details in `mesures/1a.md` | Not supported (init table to be ported) | Not supported | Planned (phase 6) |
| Touch | Observed (follows the finger; offset fixed at commit `e7a4803`, mean residual error (−6, −4) px over five targets, [mesures/1a.md](mesures/1a.md)) | Not supported | Not supported | Planned |
| Software reset (`esp_rom_software_reset_system()`) | Observed: two series of 20 in a row (06/11/2026, commits `77e03de` and `97520e2`), reset cause 0x03 and the C6 link back at the first attempt every time ([mesures/1b.md](mesures/1b.md)) | — | — | Planned (C6 link loop known on this board) |
| Weather on the board (HTTPS Open-Meteo every 15 min, NVS cache, “stale” flag) | Observed (06/11/2026, commit `55644aa`: Lyon read in the log and equal to the live API, twice 15 min apart; state back from the cache after a **software** restart; Wi-Fi cut, power cut and change of place not tested; free internal RAM unexplained drop, see [mesures/meteo.md](mesures/meteo.md)) | Written, not tested | Written, not tested | Planned (phase 6) |
| Wi-Fi through the C6 (esp_hosted pinned) + SNTP — milestone 1b | Link, Wi-Fi association and IP address observed (06/11/2026). Real time displayed: observed once and judged right by the owner, but the SNTP time arrives in steps of about 15 s; a fix is written and **not yet measured**. Full power cut: not done, postponed by the owner ([mesures/1b.md](mesures/1b.md)) | — | — | Planned |
| Full-duplex audio, echo cancellation, wake word — milestone 1c | Planned | — | — | Planned (ES8311: volume limited to 75%) |
| Captive portal through SoftAP — milestone 1d | Planned | — | — | Planned |

Software **without a board** (same code for all boards):

| Block | State | Evidence and limit |
|---|---|---|
| WMO weather table → French label + day/night icon; IANA time zones → POSIX TZ string (posix_tz_db, MIT) | Written, verified on the PC | 23 tests **run on the PC** (clang 23.1.5 x86-64, with AddressSanitizer and UBSan, 0 failures) and evaluated at compile time by `static_assert`; 18 mutations detected out of 18; **never run on a board** (RISC-V: sizes, alignment and memory may differ) |
| Minimal JSON reading, Open-Meteo response, local date and time in French | Written, verified on the PC | tests run on the PC with the same detectors, mutations detected (53 of this batch); **real Open-Meteo responses captured by the PC and then stored in the test, no request made by the board**; never run on a board |
| Voice assistant: states, SSE stream, OpenAI-compatible requests, sentence splitting | Written, verified on the PC | tests run on the PC (135 mutations of this batch detected); **no real STT/LLM/TTS service called**; never run on a board |
| Whole core | 252 tests run with ASan + UBSan, 206 mutations detected out of 206 (`main` on 06/11/2026) | proves nothing about the behaviour of the board's RISC-V |
| Settings page (front end), fake device and device-served page | Written; tried against a fake device in Node (119 checks of `web/tests/parcours.py`, 06/11/2026); embedded in the firmware (30,327 bytes gzip) and served by `main/serveur_web.cpp` | **compiled; never observed on a board** (nothing served to a phone yet); Chromium only for the browser checks (Safari iOS, Firefox: not verified) |
| Settings API contract | Written ([api-reglages.md](api-reglages.md)); first subset served by the firmware (list in the status box of that file) | core logic tested on the PC; firmware **compiled, never observed on a board** |
| City search (settings page and device screen): one shared parser, state machine, download task, shared TLS lock | Parser and state machine written and verified on the PC; download task and lock (`main/geocodage.cpp`, `main/verrou_tls.cpp`) written and compiled | 28 tests **run on the PC** (ASan + UBSan) on 7 **real** Open-Meteo responses captured on 06/11/2026; 40 targeted mutations detected out of 40 (07/11/2026); firmware compiled (ESP-IDF 5.5.5); **no request made by the board**; internal RAM, task priority, lock and timings **not verified** |

### Firmware and board version: how it is noted

- The firmware is identified by the **git commit** (`git describe --always --dirty`) and the **board** (`BOARD=tab5`, display sub-revision detected at start-up). A firmware built from a modified tree (`-dirty`) is not evidence.
- Every result in `mesures/` and every entry of [troubleshooting](depannage.md) carries a **“Seen on”** line: board, display revision, commit, date. No MAC address is published here: it is read on the board, and compared, before any write. No line = result not usable.
- Every commit that touches a board or the firmware carries two final lines: `Carte:` (`tab5-st7121`, `tab5-st7123`, `tab5-ili9881c`, `waveshare-7b` or `aucune`) and `Teste-sur:` (for example `tab5-st7121 le 2026-11-06` or `non testé sur carte`). The commit **never** says “tested” without a board.
- A **milestone** (end of phase 1a, 1b…) is marked by an annotated tag `jalon-<phase>-<carte>` (for example `jalon-1a-tab5-st7121`) whose message lists what is observed and what is not.
- The **7B**: as long as no trial has taken place, its column stays “Planned”; a physical trial adds the date and the commit, as for the Tab5.

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
| **Waveshare ESP32-P4 7B** | EK79007 1024 × 600, GT911, ES8311 + ES7210, sans expandeur | **compilation visée, essais physiques de temps en temps** (phase 6) | exemplaire de l'auteur, l'écran en service à la maison : on ne la flashe qu'à la demande de l'auteur |

Le contrôleur d'écran du Tab5 est **détecté au démarrage, avant d'initialiser la liaison DSI** (registre de version du tactile Sitronix en I2C 0x55 : 1 = ST7121, 3 = ST7123 ; GT911 en 0x14 = ILI9881C). Un contrôleur détecté mais pas encore pris en charge le dit dans le journal et ne tente pas d'allumer l'écran.

### Ce qui est testé où

États : **Constaté** · **Écrit, non testé sur carte** · **Prévu** (jalon du [plan](plan.md)) · **Non vérifié**.

| Fonction | Tab5 ST7121 | Tab5 ST7123 | Tab5 ILI9881C | Waveshare 7B |
|---|---|---|---|---|
| Chaîne de compilation ESP32-P4 (ESP-IDF 5.5.5) | Constaté (squelette : 38 s, 204 048 octets) | idem | idem | à compiler (CI prévue) |
| Détection du contrôleur d'écran | Constaté (« ST7121 détecté » au journal, piste A, 06/11) | Écrit, non testé | Écrit, non testé | sans objet |
| Écran allumé, image LVGL « Bonjour » | Constaté, avec une réserve ouverte après coupure de courant (voir [dépannage](depannage.md#ouverts)) ; détail dans `mesures/1a.md` | Non pris en charge (table d'init à porter) | Non pris en charge | Prévu (phase 6) |
| Tactile | Constaté (suit le doigt ; décalage corrigé au commit `e7a4803`, écart résiduel moyen (−6, −4) px sur cinq cibles, [mesures/1a.md](mesures/1a.md)) | Non pris en charge | Non pris en charge | Prévu |
| Redémarrage logiciel (`esp_rom_software_reset_system()`) | Constaté : deux séries de 20 de suite (06/11/2026, commits `77e03de` et `97520e2`), cause de reset 0x03 et lien C6 revenu au premier essai à chaque fois ([mesures/1b.md](mesures/1b.md)) | — | — | Prévu (boucle du lien C6 connue sur cette carte) |
| Météo sur la carte (Open-Meteo en HTTPS toutes les 15 min, cache NVS, indicateur « périmé ») | Constaté (06/11/2026, commit `55644aa` : Lyon lue dans le journal et égale à l'API réelle, deux fois à 15 min d'écart ; état revenu du cache après un redémarrage **logiciel** ; Wi-Fi coupé, coupure de courant et changement de lieu non testés ; baisse inexpliquée de la RAM interne libre, voir [mesures/meteo.md](mesures/meteo.md)) | Écrit, non testé | Écrit, non testé | Prévu (phase 6) |
| Wi-Fi par le C6 (esp_hosted épinglé) + SNTP — jalon 1b | Lien, association Wi-Fi et adresse IP constatés (06/11/2026). Heure réelle à l'écran : constatée une fois et jugée juste par Axel, mais l'heure SNTP arrive par paliers d'environ 15 s ; un correctif est écrit et **pas encore mesuré**. Coupure de courant complète : non faite, reportée par Axel ([mesures/1b.md](mesures/1b.md)) | — | — | Prévu |
| Audio duplex, annulation d'écho, mot d'activation — jalon 1c | Prévu | — | — | Prévu (ES8311 : volume limité à 75 %) |
| Portail captif par SoftAP — jalon 1d | Prévu | — | — | Prévu |

Logiciel **sans carte** (même code pour toutes les cartes) :

| Brique | État | Preuve et limite |
|---|---|---|
| Table météo WMO → libellé français + icône jour/nuit ; fuseaux IANA → chaîne POSIX TZ (posix_tz_db, MIT) | Écrit, vérifié sur PC | 23 tests **exécutés sur le PC** (clang 23.1.5 x86-64, avec AddressSanitizer et UBSan, 0 échec) et évalués à la compilation par `static_assert` ; 18 mutations détectées sur 18 ; **jamais lancés sur une carte** (RISC-V : tailles, alignement et mémoire peuvent différer) |
| Lecture JSON minimale, réponse Open-Meteo, date et heure locales en français | Écrit, vérifié sur PC | tests exécutés sur le PC avec les mêmes détecteurs, mutations détectées (53 de ce lot) ; **réponses Open-Meteo réelles captées par le PC puis rangées dans le test, aucune requête faite par la carte** ; jamais lancés sur une carte |
| Assistant vocal : états, flux SSE, requêtes OpenAI-compatibles, découpe en phrases | Écrit, vérifié sur PC | tests exécutés sur le PC (135 mutations de ce lot détectées) ; **aucun service STT/LLM/TTS réel appelé** ; jamais lancés sur une carte |
| Ensemble du noyau | 252 tests exécutés avec ASan + UBSan, 206 mutations détectées sur 206 (`main` au 06/11/2026) | ne prouve rien sur le comportement du RISC-V de la carte |
| Page de réglages (front), faux appareil et page servie par l'appareil | Écrit ; essayé contre un faux appareil Node (119 vérifications de `web/tests/parcours.py`, 06/11/2026) ; embarqué dans le firmware (30 327 octets gzip) et servi par `main/serveur_web.cpp` | **compilé ; jamais constaté sur une carte** (rien n'a encore été servi à un téléphone) ; Chromium seulement pour les vérifications du navigateur (Safari iOS, Firefox : non vérifié) |
| Contrat de l'API de réglages | Écrit ([api-reglages.md](api-reglages.md)) ; premier sous-ensemble servi par le firmware (liste dans l'encadré de ce fichier) | logique du noyau testée sur le PC ; firmware **compilé, jamais constaté sur une carte** |
| Recherche de ville (page de réglages et écran de l'appareil) : un analyseur commun, machine d'état, tâche de téléchargement, verrou TLS commun | Analyseur et machine d'état écrits et vérifiés sur le PC ; tâche et verrou (`main/geocodage.cpp`, `main/verrou_tls.cpp`) écrits et compilés | 28 tests **exécutés sur le PC** (ASan + UBSan) sur 7 réponses **réelles** d'Open-Meteo capturées le 06/11/2026 ; 40 mutations ciblées détectées sur 40 (07/11/2026) ; firmware compilé (ESP-IDF 5.5.5) ; **aucune requête faite par la carte** ; RAM interne, priorité de la tâche, verrou et délais **non vérifiés** |

### Version du firmware et de la carte : comment on la note

- Le firmware se repère par le **commit git** (`git describe --always --dirty`) et la **carte** (`BOARD=tab5`, sous-révision d'écran détectée au démarrage). Un firmware compilé depuis un arbre modifié (`-dirty`) ne sert pas de preuve.
- Chaque résultat de `mesures/` et chaque entrée de [dépannage](depannage.md#version-française) porte une ligne **« Vu sur »** : carte, révision d'écran, commit, date. Aucune adresse MAC n'est publiée ici : elle est lue sur la carte, et comparée, avant toute écriture. Pas de ligne = résultat non utilisable.
- Chaque commit qui touche une carte ou le firmware porte deux lignes finales : `Carte:` (`tab5-st7121`, `tab5-st7123`, `tab5-ili9881c`, `waveshare-7b` ou `aucune`) et `Teste-sur:` (par exemple `tab5-st7121 le 2026-11-06` ou `non testé sur carte`). Le commit ne dit **jamais** « testé » sans carte.
- Un **jalon** (fin de phase 1a, 1b…) est marqué par un tag annoté `jalon-<phase>-<carte>` (par exemple `jalon-1a-tab5-st7121`) dont le message liste ce qui est constaté et ce qui ne l'est pas.
- La **7B** : tant qu'aucun essai n'a eu lieu, sa colonne reste « Prévu » ; un essai physique y ajoute la date et le commit, comme pour le Tab5.
