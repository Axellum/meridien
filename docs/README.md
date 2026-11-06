# Documentation

## English · [Français](#version-française)

---

Native ESP-IDF + LVGL 9 firmware for the ESP32-P4: time and date, weather from an API, a voice assistant linked to an LLM of your choice, a settings page served by the device. No Home Assistant, no ESPHome. Developed on the M5Stack Tab5; compatibility with the Waveshare 7B is aimed at (no physical test yet: see [compatibility](compatibilite.md)).

This documentation is kept **at the same time as the code**: every result carries the board and the display revision it was observed on, and every incident is recorded with its cause and its solution. The plan, the cost, the milestone measurements, the research notes and the decisions (ADR) are working documents, written in French.

| I want to… | Read |
|---|---|
| understand the project in two minutes | the [repository README](../README.md) |
| know **which board it runs on** and what is tested where (Tab5 ST7121, ST7123, ILI9881C, Waveshare 7B) | [Compatibility and versions](compatibilite.md) |
| understand how it is built | [Architecture](architecture.md), then the [decisions](decisions/README.md) |
| build, flash, find the board again | [repository README](../README.md#build-and-test), [AGENTS.md](../AGENTS.md) |
| install or find the development tools (the PC's compiler, error detectors, formatting) | [Development tools](outils.md) |
| fix a symptom already seen (black screen, serial port, build…) | [Troubleshooting](depannage.md): symptom → cause → solution |
| know where the project stands, the criteria for “done” and the risks | [Plan](plan.md) |
| know the cost in time, sessions and tokens | [Cost](cout.md) |
| see what has been measured on the board | `mesures/` (one page per milestone, for example [`mesures/1a.md`](mesures/1a.md)) |
| plug the settings page into the firmware | [Settings API contract](api-reglages.md), [the page and its fake device](../web/README.md) |
| find where a fact comes from (versions, licences, pins) | [Sourced research](recherche/) |
| check the licence of the code or of a component it uses | [LICENSE](../LICENSE) (MIT), [THIRD_PARTY.md](../THIRD_PARTY.md) |

### Reading rules

- **“Observed”** = seen on the board (screen, serial log, version read on the device). **“Written, not tested on a board”** = the code exists, nothing has been observed on the hardware. **“Not verified”** = neither. A “build OK” or a green test alone is not “observed”.
- A decision that looks strange is probably in an [ADR](decisions/README.md) or in [troubleshooting](depannage.md): check before “fixing” it.
- When the code and a document disagree, the code wins: fix the document.

---

## Version Française

---

Firmware natif ESP-IDF + LVGL 9 pour ESP32-P4 : heure et date, météo par API, assistant vocal relié à un LLM au choix, page de réglages servie par l'appareil. Sans Home Assistant, sans ESPHome. Développé sur le M5Stack Tab5 ; compatibilité Waveshare 7B visée (aucun essai physique pour l'instant : voir la [compatibilité](compatibilite.md#version-française)).

Cette documentation est tenue **en même temps que le code** : chaque résultat porte la carte et la révision d'écran sur lesquelles il a été constaté, et chaque incident est consigné avec sa cause et sa solution. Le plan, le coût, les mesures de jalon, les notes de recherche et les décisions (ADR) sont des documents de travail, écrits en français.

| Je veux… | À lire |
|---|---|
| comprendre le projet en deux minutes | le [README du dépôt](../README.md#version-française) |
| savoir **sur quelle carte ça tourne** et ce qui est testé où (Tab5 ST7121, ST7123, ILI9881C, Waveshare 7B) | [Compatibilité et versions](compatibilite.md#version-française) |
| comprendre comment c'est construit | [Architecture](architecture.md#version-française), puis les [décisions](decisions/README.md) |
| compiler, flasher, retrouver la carte | [README du dépôt](../README.md#compiler-et-tester), [AGENTS.md](../AGENTS.md) |
| installer ou retrouver les outils (compilateur du PC, détecteurs d'erreurs, mise en forme) | [Outils de développement](outils.md) |
| corriger un symptôme déjà vu (écran noir, port série, compilation…) | [Dépannage](depannage.md#version-française) : symptôme → cause → solution |
| savoir où en est le projet, les critères de « fait » et les risques | [Plan](plan.md) |
| connaître le coût en temps, sessions et tokens | [Coût](cout.md) |
| voir ce qui a été mesuré sur la carte | `mesures/` (une page par jalon, par exemple [`mesures/1a.md`](mesures/1a.md)) |
| brancher la page de réglages au firmware | [Contrat de l'API de réglages](api-reglages.md), [la page et son faux appareil](../web/README.md) |
| retrouver d'où vient un fait (versions, licences, broches) | [Recherches sourcées](recherche/) |
| vérifier la licence du code ou d'un composant qu'il utilise | [LICENSE](../LICENSE) (MIT), [THIRD_PARTY.md](../THIRD_PARTY.md) |

### Règles de lecture

- **« Constaté »** = vu sur la carte (écran, journal série, version lue sur l'appareil). **« Écrit, non testé sur carte »** = le code existe, rien n'a été observé sur le matériel. **« Non vérifié »** = ni l'un ni l'autre. Un « build OK » ou un test vert seul ne vaut pas « constaté ».
- Une décision qui semble étrange est probablement dans une [ADR](decisions/README.md) ou dans le [dépannage](depannage.md#version-française) : vérifier avant de « corriger ».
- Quand le code et un document divergent, le code gagne : corriger le document.
