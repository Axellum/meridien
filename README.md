# Meridien

## English · [Français](#version-française)

---

*Name chosen on 06/11/2026: the meridian fixes the time and the place, which is what the screen shows. The CMake project and the binary are called `meridien`.*

A touchscreen on an ESP32-P4 meant to work **on its own**: time and date, weather from an API, a voice assistant linked to an LLM of your choice (local or cloud), and a settings web page served by the device itself. **That is the goal, and the project is only at its beginning**: what exists, and what has been seen on a real board, is in [Status](#status).

No Home Assistant, no ESPHome: native **ESP-IDF + LVGL 9** firmware, with clean, testable code. It is a sibling of the [Tab5 project (ESPHome)](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL): it keeps its taste (themes, fonts, short texts, accents) but not its dependency on Home Assistant.

A personal project, shared in case it is useful to someone, built with the help of AI: the author sets the goals and judges the result on the real screen.

| | |
|---|---|
| Development board | M5Stack Tab5, **ST7121 display revision** (1280×720) |
| Target compatibility | Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (1024×600), board layer written and built, **not tried on the board yet** ([details](docs/carte-7b.md)) |
| Version 1 | time/date, weather (Open-Meteo), voice assistant (wake word → STT → LLM → TTS), settings page |
| Version 2 | Google calendar (through a secret iCal address, see [ADR-0004](docs/decisions/0004-calendrier-par-adresse-ical-secrete.md)) |
| Out of scope | home automation, Home Assistant, ESPHome |

## Status

As of 07/11/2026. What is described as running was seen on **one** board, the author's Tab5 (ST7121 display revision), through its serial log and by the author looking at it. The exact list, with commits and dates, is in **[docs/compatibilite.md](docs/compatibilite.md)**.

- **Runs on the Tab5 ST7121**: the display and the touch, Wi-Fi through the ESP32-C6 (with an automatic roll-back when a new network does not work), the time (network first, the hardware clock as a fallback), the weather from Open-Meteo (current conditions, 24 hours, 7 days, rain per quarter of an hour, kept across restarts), the main screen (a sky drawn from the time of day and the weather, themes) and the settings screens. What was not looked at or not measured is written as such in the compatibility table.
- **Not running yet**: the voice assistant (its logic is written and checked on a PC, and no real speech or language service has been called; echo cancellation, the wake word and the speech-to-LLM-to-speech chain are not started), the Google calendar, the captive portal, and the Waveshare 7B (board layer written and compiled, **never run on the board**).
- **Languages**: the screen texts exist in seven languages (French is the source, then English, German, Dutch, Spanish, Italian, Turkish). The German and Turkish texts have not been reviewed by a native speaker.
- **Checked on a PC, without a board**: the core (weather, time zones, time, voice-assistant logic, settings) runs under AddressSanitizer and UBSan, with mutation testing (a change of the code that no test notices fails the check); the settings web page runs against a fake device. This says nothing about the behaviour on the board.
- **Known limits**, written in [SECURITY.md](SECURITY.md): the Wi-Fi password and the settings are kept in the device's flash **without encryption**, and the settings page is served over plain local HTTP behind a 6-digit pairing code.

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
| [SECURITY.md](SECURITY.md) | known security limits, and how to report a problem |
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
main/                   application: main screen, settings screens, Wi-Fi, weather, settings web server
components/core/        pure C++ core, no ESP-IDF, testable on a PC
components/board_api/   common board interface (pure C)
components/bsp_tab5/    Tab5 board support: display, touch, I/O expander, backlight, clock, audio, power
components/bsp_waveshare_7b/  Waveshare 7B board support (never run on the board)
boards/                 partitions and chip settings, one folder per board
i18n/                   screen texts in seven languages
web/                    settings page, fake device (Node), browser tests
test/host/              tests of the core, run on the PC
docs/                   documentation, plan, decisions (ADR), research
tools/                  build launcher and checks
```

## Licence and credits

- Code: [MIT licence](LICENSE). Third-party components and their licences: [THIRD_PARTY.md](THIRD_PARTY.md).
- **Weather data by [Open-Meteo.com](https://open-meteo.com/)**, under the [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) licence. Open-Meteo's free API is for non-commercial use only ([ADR-0003](docs/decisions/0003-reglages-par-page-web-et-meteo-open-meteo.md)). When you pick them as the source, the device reads **MET Norway** (api.met.no; CC BY 4.0 and NLOD 2.0, data adapted) or the **NWS** (api.weather.gov; public domain, not approved or sponsored by the NWS) itself, and shows the credit of the service that really gave the weather ([ADR-0014](docs/decisions/0014-met-norway-et-nws-comme-sources.md), [THIRD_PARTY.md](THIRD_PARTY.md)).

---

## Version Française

---

*Nom retenu le 06/11/2026 : le méridien fixe l'heure et le lieu, ce que l'écran affiche. Le projet CMake et le binaire s'appellent `meridien`.*

Un écran tactile ESP32-P4 conçu pour fonctionner **seul** : heure et date, météo par API, assistant vocal relié à un LLM au choix (local ou cloud), et une page web de réglages servie par l'appareil lui-même. **C'est l'objectif, et le projet n'en est qu'au début** : ce qui existe, et ce qui a été vu sur une vraie carte, est dans l'[État](#état).

Aucun Home Assistant, aucun ESPHome : firmware natif **ESP-IDF + LVGL 9**, code propre et testable. C'est un projet voisin du [projet Tab5 (ESPHome)](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL) : il garde son goût (thèmes, polices, textes courts, accents) mais pas sa dépendance à Home Assistant.

Un projet personnel, partagé au cas où il servirait à quelqu'un, fait avec l'aide de l'IA : l'auteur fixe les objectifs et juge le résultat sur l'écran réel.

| | |
|---|---|
| Carte de développement | M5Stack Tab5, **révision d'écran ST7121** (1280×720) |
| Compatibilité visée | Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (1024×600), couche carte écrite et compilée, **pas encore essayée sur la carte** ([détails](docs/carte-7b.md#version-française)) |
| Version 1 | heure/date, météo (Open-Meteo), assistant vocal (mot d'activation → STT → LLM → TTS), page de réglages |
| Version 2 | calendrier Google (par adresse iCal secrète, voir [ADR-0004](docs/decisions/0004-calendrier-par-adresse-ical-secrete.md)) |
| Hors périmètre | domotique, Home Assistant, ESPHome |

## État

Au 07/11/2026. Ce qui est décrit comme « tourne » a été vu sur **une** carte, le Tab5 de l'auteur (révision d'écran ST7121), par son journal série et par l'auteur qui le regardait. La liste exacte, avec commits et dates, est dans **[docs/compatibilite.md](docs/compatibilite.md#version-française)**.

- **Tourne sur le Tab5 ST7121** : l'écran et le tactile, le Wi-Fi par l'ESP32-C6 (avec un retour arrière automatique quand un nouveau réseau ne marche pas), l'heure (le réseau d'abord, l'horloge matérielle en secours), la météo d'Open-Meteo (conditions du moment, 24 heures, 7 jours, pluie par quarts d'heure, gardée d'un redémarrage à l'autre), l'écran principal (un ciel dessiné d'après l'heure et la météo, des thèmes) et les écrans de réglages. Ce qui n'a pas été regardé ou pas mesuré est écrit comme tel dans le tableau de compatibilité.
- **Ne tourne pas encore** : l'assistant vocal (sa logique est écrite et vérifiée sur un PC, et aucun vrai service de voix ou de langage n'a été appelé ; l'annulation d'écho, le mot d'activation et la chaîne voix → LLM → voix ne sont pas commencés), le calendrier Google, le portail captif, et la Waveshare 7B (couche carte écrite et compilée, **jamais lancée sur la carte**).
- **Langues** : les textes de l'écran existent en sept langues (le français est la source, puis anglais, allemand, néerlandais, espagnol, italien, turc). Les textes allemands et turcs n'ont pas été relus par un locuteur natif.
- **Vérifié sur un PC, sans carte** : le noyau (météo, fuseaux, heure, logique de l'assistant vocal, réglages) tourne sous AddressSanitizer et UBSan, avec des mutations (un changement du code qu'aucun test ne remarque fait échouer le contrôle) ; la page web de réglages tourne contre un faux appareil. Cela ne dit rien du comportement sur la carte.
- **Limites connues**, écrites dans [SECURITY.md](SECURITY.md#version-française) : le mot de passe Wi-Fi et les réglages sont gardés dans la flash de l'appareil **sans chiffrement**, et la page de réglages est servie en HTTP local simple derrière un code d'appairage à 6 chiffres.

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
| [SECURITY.md](SECURITY.md#version-française) | limites de sécurité connues, et comment signaler un problème |
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
main/                   application : écran principal, écrans de réglages, Wi-Fi, météo, serveur web de réglages
components/core/        noyau en C++ pur, sans ESP-IDF, testable sur le PC
components/board_api/   interface commune des cartes (C pur)
components/bsp_tab5/    support de la carte Tab5 : écran, tactile, expandeur d'E/S, rétroéclairage, horloge, audio, alimentation
components/bsp_waveshare_7b/  support de la Waveshare 7B (jamais lancé sur la carte)
boards/                 partitions et réglages de puce, un dossier par carte
i18n/                   textes de l'écran en sept langues
web/                    page de réglages, faux appareil (Node), tests dans le navigateur
test/host/              tests du noyau, exécutés sur le PC
docs/                   documentation, plan, décisions (ADR), recherches
tools/                  lanceur de compilation et contrôles
```

## Licence et crédits

- Code : [licence MIT](LICENSE). Composants tiers et leurs licences : [THIRD_PARTY.md](THIRD_PARTY.md).
- **Données météo par [Open-Meteo.com](https://open-meteo.com/)**, sous licence [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). L'API gratuite d'Open-Meteo est réservée à un usage non commercial ([ADR-0003](docs/decisions/0003-reglages-par-page-web-et-meteo-open-meteo.md)). Quand vous les choisissez comme source, l'appareil lit lui-même **MET Norway** (api.met.no ; CC BY 4.0 et NLOD 2.0, données adaptées) ou le **NWS** (api.weather.gov ; domaine public, ni approuvé ni parrainé par le NWS) et montre la mention du service qui a réellement donné la météo ([ADR-0014](docs/decisions/0014-met-norway-et-nws-comme-sources.md), [THIRD_PARTY.md](THIRD_PARTY.md)).
