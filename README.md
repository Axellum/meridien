# Meridien

## English · [Français](#version-française)

---

*Name chosen on 06/11/2026: the meridian fixes the time and the place, which is what the screen shows. The GitHub repository `Axellum/meridien` is **private** for now; making it public is the author's decision. The local folder and the CMake project name are still `ecran_autonome`.*

A touchscreen on an ESP32-P4 meant to work **on its own**: time and date, weather from an API, a voice assistant linked to an LLM of your choice (local or cloud), and a settings web page served by the device itself. **That is the goal, and the project is only at its beginning**: what exists, and what has been seen on a real board, is in [Status](#status).

No Home Assistant, no ESPHome: native **ESP-IDF + LVGL 9** firmware, with clean, testable code. It is a sibling of the [Tab5 project (ESPHome)](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL): it keeps its taste (themes, fonts, short texts, accents) but not its dependency on Home Assistant.

A personal project, shared in case it is useful to someone, built with the help of AI: the author sets the goals and judges the result on the real screen.

| | |
|---|---|
| Development board | M5Stack Tab5, **ST7121 display revision** (1280×720) |
| Target compatibility | Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (1024×600), physical tests now and then — **not tried yet** |
| Version 1 | time/date, weather (Open-Meteo), voice assistant (wake word → STT → LLM → TTS), settings page |
| Version 2 | Google calendar (through a secret iCal address, see [ADR-0004](docs/decisions/0004-calendrier-par-adresse-ical-secrete.md)) |
| Out of scope | home automation, Home Assistant, ESPHome |

## Status

As of 06/11/2026.

- **Phase 0 (preparation) done**: research, audit of the Tab5 repository, build chain proven, plan and cost estimated.
- **Phase 1 in progress** (proving the hardware, from the riskiest to the least risky): the display, the touch and an LVGL “Bonjour” have been tried on the Tab5 ST7121 (milestone 1a, [measurements](docs/mesures/1a.md)); one reservation stays open after a power cut ([troubleshooting](docs/depannage.md#open)). Wi-Fi through the C6, audio and the captive portal: planned, research notes written.
- **Without a board**: the core (weather, time zones, time, voice assistant: states, stream, requests, sentence splitting) is written and checked on the PC (252 tests run with AddressSanitizer and UBSan, 206 mutations detected); the settings page and its fake device too (49 checks). None of this has been run on a board or served by the device.

What is tested on which board, with which version: **[docs/compatibilite.md](docs/compatibilite.md)**. Nothing is written “works” there without a board, a commit and a date.

## Where to read

| | |
|---|---|
| [docs/README.md](docs/README.md) | the home of the documentation (“I want… → I read…”) |
| [docs/compatibilite.md](docs/compatibilite.md) | boards, versions, what is observed where |
| [docs/architecture.md](docs/architecture.md) | the layers and the folders |
| [docs/depannage.md](docs/depannage.md) | symptom → cause → solution, with the board it happened on |
| [docs/outils.md](docs/outils.md) | development tools and automatic checks |
| [docs/api-reglages.md](docs/api-reglages.md) · [web/README.md](web/README.md) | contract of the settings API; the settings page and its fake device |
| [docs/plan.md](docs/plan.md) · [docs/cout.md](docs/cout.md) | phases, criteria for “done”, risks; time, sessions, tokens (working documents, in French) |
| [docs/decisions/](docs/decisions/README.md) · [docs/recherche/](docs/recherche/) | decisions (ADR); sourced facts and their “not verified” (working documents, in French) |
| [AGENTS.md](AGENTS.md) | instructions for the AI agents that work on this repository |
| [LICENSE](LICENSE) · [THIRD_PARTY.md](THIRD_PARTY.md) | the MIT licence; third-party components and their licences |

## Build and test

PowerShell (never Git Bash). `tools/idf.ps1` runs the ESP-IDF 5.5.5 that ESPHome had already installed on the author's PC; where the script looks for it is written at the top of `tools/idf.ps1`. It has not been tried on any other PC.

```powershell
.\tools\idf.ps1 set-target esp32p4
.\tools\idf.ps1 build
```

A build that succeeds does not prove that anything works: see [AGENTS.md](AGENTS.md).

Tests of the core on a PC (they need a host C++ compiler, clang or g++: see [docs/outils.md](docs/outils.md)):

```powershell
python test/host/lancer.py              # must display the mode « complet (compilé + exécuté) »
python test/host/lancer.py --mutations  # every mutation of the core must be detected
```

Also: `python web/tests/parcours.py` replays the settings page against its fake device (it needs Internet: it really calls Open-Meteo, see [web/README.md](web/README.md)), and `python tools/controle_liens.py` checks the relative links and the anchors of the Markdown files.

Flashing a board: only on the author's explicit request, after a full backup of the flash and a check of the board's MAC address (see [AGENTS.md](AGENTS.md)).

## Layout

```
main/                   application (a “Bonjour” screen for now)
components/core/        pure C++ core, no ESP-IDF, testable on a PC
components/board_api/   common board interface (pure C)
components/bsp_tab5/    Tab5 board support: display, touch, I/O expander
boards/tab5/            partitions and chip settings of the board
web/                    settings page, fake device (Node), browser tests
test/host/              tests of the core, run on the PC
docs/                   documentation, plan, decisions (ADR), research
tools/                  build launcher and checks
```

## Licence and credits

- Code: [MIT licence](LICENSE). Third-party components and their licences: [THIRD_PARTY.md](THIRD_PARTY.md).
- **Weather data by [Open-Meteo.com](https://open-meteo.com/)**, under the [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) licence. Open-Meteo's free API is for non-commercial use only ([ADR-0003](docs/decisions/0003-reglages-par-page-web-et-meteo-open-meteo.md)).

---

## Version Française

---

*Nom retenu le 06/11/2026 : le méridien fixe l'heure et le lieu, ce que l'écran affiche. Le dépôt GitHub `Axellum/meridien` est **privé** pour l'instant ; le passer en public est une décision de l'auteur. Le dossier local et le nom du projet CMake s'appellent encore `ecran_autonome`.*

Un écran tactile ESP32-P4 conçu pour fonctionner **seul** : heure et date, météo par API, assistant vocal relié à un LLM au choix (local ou cloud), et une page web de réglages servie par l'appareil lui-même. **C'est l'objectif, et le projet n'en est qu'au début** : ce qui existe, et ce qui a été vu sur une vraie carte, est dans l'[État](#état).

Aucun Home Assistant, aucun ESPHome : firmware natif **ESP-IDF + LVGL 9**, code propre et testable. C'est un projet voisin du [projet Tab5 (ESPHome)](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL) : il garde son goût (thèmes, polices, textes courts, accents) mais pas sa dépendance à Home Assistant.

Un projet personnel, partagé au cas où il servirait à quelqu'un, fait avec l'aide de l'IA : l'auteur fixe les objectifs et juge le résultat sur l'écran réel.

| | |
|---|---|
| Carte de développement | M5Stack Tab5, **révision d'écran ST7121** (1280×720) |
| Compatibilité visée | Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (1024×600), essais physiques de temps en temps — **pas encore essayée** |
| Version 1 | heure/date, météo (Open-Meteo), assistant vocal (mot d'activation → STT → LLM → TTS), page de réglages |
| Version 2 | calendrier Google (par adresse iCal secrète, voir [ADR-0004](docs/decisions/0004-calendrier-par-adresse-ical-secrete.md)) |
| Hors périmètre | domotique, Home Assistant, ESPHome |

## État

Au 06/11/2026.

- **Phase 0 (préparation) faite** : recherches, audit du dépôt Tab5, chaîne de compilation prouvée, plan et coût chiffrés.
- **Phase 1 en cours** (prouver le matériel, du plus risqué au moins risqué) : l'écran, le tactile et un « Bonjour » LVGL ont été essayés sur le Tab5 ST7121 (jalon 1a, [mesures](docs/mesures/1a.md)) ; une réserve reste ouverte après coupure de courant ([dépannage](docs/depannage.md#ouverts)). Wi-Fi par le C6, audio et portail captif : prévus, notes de recherche écrites.
- **Sans carte** : le noyau (météo, fuseaux, heure, assistant vocal : états, flux, requêtes, découpe en phrases) est écrit et vérifié sur le PC (252 tests exécutés avec AddressSanitizer et UBSan, 206 mutations détectées) ; la page de réglages et son faux appareil aussi (49 vérifications). Rien de cela n'a été lancé sur une carte ni servi par l'appareil.

Ce qui est testé sur quelle carte, avec quelle version : **[docs/compatibilite.md](docs/compatibilite.md#version-française)**. Rien n'y est écrit « fonctionne » sans carte, commit et date.

## Où lire

| | |
|---|---|
| [docs/README.md](docs/README.md#version-française) | l'accueil de la documentation (« je veux… → je lis… ») |
| [docs/compatibilite.md](docs/compatibilite.md#version-française) | cartes, versions, ce qui est constaté où |
| [docs/architecture.md](docs/architecture.md#version-française) | les couches et les dossiers |
| [docs/depannage.md](docs/depannage.md#version-française) | symptôme → cause → solution, avec la carte sur laquelle c'est arrivé |
| [docs/outils.md](docs/outils.md) | outils de développement et contrôles automatiques |
| [docs/api-reglages.md](docs/api-reglages.md) · [web/README.md](web/README.md) | contrat de l'API de réglages ; la page de réglages et son faux appareil |
| [docs/plan.md](docs/plan.md) · [docs/cout.md](docs/cout.md) | phases, critères de « fait », risques ; temps, sessions, tokens (documents de travail) |
| [docs/decisions/](docs/decisions/README.md) · [docs/recherche/](docs/recherche/) | décisions (ADR) ; faits sourcés et leurs « non vérifié » (documents de travail) |
| [AGENTS.md](AGENTS.md) | consignes pour les agents IA qui travaillent sur ce dépôt |
| [LICENSE](LICENSE) · [THIRD_PARTY.md](THIRD_PARTY.md) | la licence MIT ; composants tiers et leurs licences |

## Compiler et tester

PowerShell (jamais Git Bash). `tools/idf.ps1` lance l'ESP-IDF 5.5.5 qu'ESPHome avait déjà installé sur le PC de l'auteur ; l'endroit où le script le cherche est écrit en tête de `tools/idf.ps1`. Il n'a été essayé sur aucun autre PC.

```powershell
.\tools\idf.ps1 set-target esp32p4
.\tools\idf.ps1 build
```

Une compilation qui réussit ne prouve pas que quoi que ce soit fonctionne : voir [AGENTS.md](AGENTS.md).

Tests du noyau sur le PC (ils demandent un compilateur C++ hôte, clang ou g++ : voir [docs/outils.md](docs/outils.md)) :

```powershell
python test/host/lancer.py              # doit afficher le mode « complet (compilé + exécuté) »
python test/host/lancer.py --mutations  # chaque mutation du noyau doit être détectée
```

Aussi : `python web/tests/parcours.py` rejoue la page de réglages contre son faux appareil (il demande Internet : il appelle vraiment Open-Meteo, voir [web/README.md](web/README.md)), et `python tools/controle_liens.py` contrôle les liens relatifs et les ancres des fichiers Markdown.

Flasher une carte : seulement sur demande explicite de l'auteur, après une sauvegarde complète de la flash et la vérification de l'adresse MAC de la carte (voir [AGENTS.md](AGENTS.md)).

## Structure

```
main/                   application (un écran « Bonjour » pour l'instant)
components/core/        noyau en C++ pur, sans ESP-IDF, testable sur le PC
components/board_api/   interface commune des cartes (C pur)
components/bsp_tab5/    support de la carte Tab5 : écran, tactile, expandeur d'E/S
boards/tab5/            partitions et réglages de puce de la carte
web/                    page de réglages, faux appareil (Node), tests dans le navigateur
test/host/              tests du noyau, exécutés sur le PC
docs/                   documentation, plan, décisions (ADR), recherches
tools/                  lanceur de compilation et contrôles
```

## Licence et crédits

- Code : [licence MIT](LICENSE). Composants tiers et leurs licences : [THIRD_PARTY.md](THIRD_PARTY.md).
- **Données météo par [Open-Meteo.com](https://open-meteo.com/)**, sous licence [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). L'API gratuite d'Open-Meteo est réservée à un usage non commercial ([ADR-0003](docs/decisions/0003-reglages-par-page-web-et-meteo-open-meteo.md)).
