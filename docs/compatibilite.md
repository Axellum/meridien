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
| Weather on the board (HTTPS Open-Meteo every 15 min, NVS cache, “stale” flag) | Observed (06/11/2026, commit `55644aa`: Lyon read in the log and equal to the live API, twice 15 min apart; state back from the cache after a **software** restart; Wi-Fi cut, power cut and change of place not tested; on the integrated build (07/11/2026, `2a05811`): 29 of 30 reboots with the request observed to the end and successful, 0 watchdog, 0 allocation failure, 2 requests silent for ≥ 10 s, cause not established; **free internal RAM keeps falling, −152 B at every clock minute, source unresolved**, see [mesures/memoire-interne.md](mesures/memoire-interne.md) and [mesures/meteo.md](mesures/meteo.md)) | Written, not tested | Written, not tested | Planned (phase 6) |
| Wi-Fi through the C6 (esp_hosted pinned) + SNTP — milestone 1b | Link, Wi-Fi association and IP address observed (06/11/2026). Real time displayed: observed once and judged right by the owner, but the SNTP time arrives in steps of about 15 s; the fix is **measured on the integrated binary** (`2a05811`, 07/11/2026: time received 30 of 30 boots, 11.4 to 12.3 s after the start, 29 to 279 ms after the IP), **not yet read on the screen by the owner**. Full power cut: not done, postponed by the owner ([mesures/1b.md](mesures/1b.md)) | — | — | Planned |
| Change of Wi-Fi network with automatic roll-back (`main/reseau_wifi.h`, ADR-0007 point 7), milestone 1b | Observed on the author's own network (07/11/2026, commits `5f17506`, `fe54f8d`, `a244af0`): **13 roll-backs out of 13**, the old network back with its IP address 6.0 to 6.15 s after the decision; the 30 s timer fires at +30 000 ms; an unknown network is reported after 7.2 s; a scan takes 2.85 s without dropping the connection; with no known network (local binary ignoring the compiled network) the Wi-Fi starts, scans, reports a failed trial at +30.01 s and adopts a good network in 6.1 s. On a wrong password the radio says 2 and 205 (the “password refused” reason 202 in 1 trial out of 9): the screen is told “other”, and a failure found only by the timer reaches it at about 36 s, after its own 30 s counter. **Not observed**: the real settings screen, a second working network, open / WPA3 / enterprise networks, a power cut during a trial ([mesures/wifi-retour-arriere.md](mesures/wifi-retour-arriere.md)) | — | — | Planned |
| Full-duplex audio, echo cancellation, wake word — milestone 1c | Planned | — | — | Planned (ES8311: volume limited to 75%) |
| Captive portal through SoftAP — milestone 1d | Planned | — | — | Planned |

Software **without a board** (same code for all boards):

| Block | State | Evidence and limit |
|---|---|---|
| WMO weather table → French label + day/night icon; IANA time zones → POSIX TZ string (posix_tz_db, MIT) | Written, verified on the PC | 23 tests **run on the PC** (clang 23.1.5 x86-64, with AddressSanitizer and UBSan, 0 failures) and evaluated at compile time by `static_assert`; 18 mutations detected out of 18; **never run on a board** (RISC-V: sizes, alignment and memory may differ) |
| Minimal JSON reading, Open-Meteo response, local date and time in French | Written, verified on the PC | tests run on the PC with the same detectors, mutations detected (53 of this batch); **real Open-Meteo responses captured by the PC and then stored in the test, no request made by the board**; never run on a board |
| Voice assistant: states, SSE stream, OpenAI-compatible requests, sentence splitting | Written, verified on the PC | tests run on the PC (135 mutations of this batch detected); **no real STT/LLM/TTS service called**; never run on a board |
| Wi-Fi trial with roll-back (decision machine) and ranking of the networks found | Written, verified on the PC; **also tried on a board** (row above) | 41 tests run on the PC (ASan + UBSan), 38 mutations detected out of 38; the sequences of reasons seen on the board are replayed in the tests; the part that talks to `esp_wifi` (`main/reseau.cpp`) is tried only on the board |
| Whole core | 252 tests run with ASan + UBSan, 206 mutations detected out of 206 (`main` on 06/11/2026) | proves nothing about the behaviour of the board's RISC-V |
| Settings page (front end) and fake device | Written, tried against a fake device in Node | 49 Playwright checks (phone rendering 390 × 844), 13,905 bytes gzip; **never served by the device**; Chromium only (Safari iOS, Firefox: not verified) |
| Settings API contract | Written ([api-reglages.md](api-reglages.md)) | implemented by the fake device only |

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
| Météo sur la carte (Open-Meteo en HTTPS toutes les 15 min, cache NVS, indicateur « périmé ») | Constaté (06/11/2026, commit `55644aa` : Lyon lue dans le journal et égale à l'API réelle, deux fois à 15 min d'écart ; état revenu du cache après un redémarrage **logiciel** ; Wi-Fi coupé, coupure de courant et changement de lieu non testés ; sur le build intégré (07/11/2026, `2a05811`) : 29 redémarrages sur 30 avec la requête suivie jusqu'au bout et réussie, 0 chien de garde, 0 échec d'allocation, 2 requêtes restées muettes ≥ 10 s, cause non établie ; **la RAM interne libre continue de baisser, −152 o à chaque minute de l'horloge, source non résolue**, voir [mesures/memoire-interne.md](mesures/memoire-interne.md) et [mesures/meteo.md](mesures/meteo.md)) | Écrit, non testé | Écrit, non testé | Prévu (phase 6) |
| Wi-Fi par le C6 (esp_hosted épinglé) + SNTP — jalon 1b | Lien, association Wi-Fi et adresse IP constatés (06/11/2026). Heure réelle à l'écran : constatée une fois et jugée juste par Axel, mais l'heure SNTP arrive par paliers d'environ 15 s ; le correctif est **mesuré sur le binaire intégré** (`2a05811`, 07/11/2026 : heure reçue sur 30 démarrages sur 30, 11,4 à 12,3 s après le départ, 29 à 279 ms après l'IP), **pas encore relu à l'écran par Axel**. Coupure de courant complète : non faite, reportée par Axel ([mesures/1b.md](mesures/1b.md)) | — | — | Prévu |
| Changement de réseau Wi-Fi avec retour arrière automatique (`main/reseau_wifi.h`, ADR-0007 point 7), jalon 1b | Constaté sur le propre réseau d'Axel (07/11/2026, commits `5f17506`, `fe54f8d`, `a244af0`) : **13 retours arrière sur 13**, l'ancien réseau revenu avec son adresse IP 6,0 à 6,15 s après la décision ; le minuteur de 30 s tire à +30 000 ms ; un réseau inexistant est signalé après 7,2 s ; une recherche dure 2,85 s sans couper la connexion ; sans réseau connu (binaire local qui ignore le réseau de la compilation) le Wi-Fi démarre, cherche, signale un essai raté à +30,01 s et adopte un bon réseau en 6,1 s. Avec un mot de passe faux la radio dit 2 et 205 (la raison « mot de passe refusé », 202, dans 1 essai sur 9) : l'écran reçoit « autre », et un échec que seul le minuteur détecte lui parvient à ≈ 36 s, après son propre compte à rebours de 30 s. **Non constaté** : le vrai écran de réglages, un second réseau qui marche, les réseaux ouverts / WPA3 / d'entreprise, une coupure de courant pendant un essai ([mesures/wifi-retour-arriere.md](mesures/wifi-retour-arriere.md)) | — | — | Prévu |
| Audio duplex, annulation d'écho, mot d'activation — jalon 1c | Prévu | — | — | Prévu (ES8311 : volume limité à 75 %) |
| Portail captif par SoftAP — jalon 1d | Prévu | — | — | Prévu |

Logiciel **sans carte** (même code pour toutes les cartes) :

| Brique | État | Preuve et limite |
|---|---|---|
| Table météo WMO → libellé français + icône jour/nuit ; fuseaux IANA → chaîne POSIX TZ (posix_tz_db, MIT) | Écrit, vérifié sur PC | 23 tests **exécutés sur le PC** (clang 23.1.5 x86-64, avec AddressSanitizer et UBSan, 0 échec) et évalués à la compilation par `static_assert` ; 18 mutations détectées sur 18 ; **jamais lancés sur une carte** (RISC-V : tailles, alignement et mémoire peuvent différer) |
| Lecture JSON minimale, réponse Open-Meteo, date et heure locales en français | Écrit, vérifié sur PC | tests exécutés sur le PC avec les mêmes détecteurs, mutations détectées (53 de ce lot) ; **réponses Open-Meteo réelles captées par le PC puis rangées dans le test, aucune requête faite par la carte** ; jamais lancés sur une carte |
| Assistant vocal : états, flux SSE, requêtes OpenAI-compatibles, découpe en phrases | Écrit, vérifié sur PC | tests exécutés sur le PC (135 mutations de ce lot détectées) ; **aucun service STT/LLM/TTS réel appelé** ; jamais lancés sur une carte |
| Essai de Wi-Fi avec retour arrière (machine de décision) et classement des réseaux trouvés | Écrit, vérifié sur PC ; **aussi essayé sur une carte** (ligne ci-dessus) | 41 tests exécutés sur le PC (ASan + UBSan), 38 mutations détectées sur 38 ; les suites de raisons vues sur la carte sont rejouées dans les tests ; la partie qui parle à `esp_wifi` (`main/reseau.cpp`) n'est essayée que sur la carte |
| Ensemble du noyau | 252 tests exécutés avec ASan + UBSan, 206 mutations détectées sur 206 (`main` au 06/11/2026) | ne prouve rien sur le comportement du RISC-V de la carte |
| Page de réglages (front) et faux appareil | Écrit, essayé contre un faux appareil Node | 49 vérifications Playwright (rendu téléphone 390 × 844), 13 905 octets gzip ; **jamais servi par l'appareil** ; Chromium seulement (Safari iOS, Firefox : non vérifié) |
| Contrat de l'API de réglages | Écrit ([api-reglages.md](api-reglages.md)) | implémenté par le faux appareil seulement |

### Version du firmware et de la carte : comment on la note

- Le firmware se repère par le **commit git** (`git describe --always --dirty`) et la **carte** (`BOARD=tab5`, sous-révision d'écran détectée au démarrage). Un firmware compilé depuis un arbre modifié (`-dirty`) ne sert pas de preuve.
- Chaque résultat de `mesures/` et chaque entrée de [dépannage](depannage.md#version-française) porte une ligne **« Vu sur »** : carte, révision d'écran, commit, date. Aucune adresse MAC n'est publiée ici : elle est lue sur la carte, et comparée, avant toute écriture. Pas de ligne = résultat non utilisable.
- Chaque commit qui touche une carte ou le firmware porte deux lignes finales : `Carte:` (`tab5-st7121`, `tab5-st7123`, `tab5-ili9881c`, `waveshare-7b` ou `aucune`) et `Teste-sur:` (par exemple `tab5-st7121 le 2026-11-06` ou `non testé sur carte`). Le commit ne dit **jamais** « testé » sans carte.
- Un **jalon** (fin de phase 1a, 1b…) est marqué par un tag annoté `jalon-<phase>-<carte>` (par exemple `jalon-1a-tab5-st7121`) dont le message liste ce qui est constaté et ce qui ne l'est pas.
- La **7B** : tant qu'aucun essai n'a eu lieu, sa colonne reste « Prévu » ; un essai physique y ajoute la date et le commit, comme pour le Tab5.
