# Documentation — écran autonome

Firmware natif ESP-IDF + LVGL 9 pour ESP32-P4 : heure et date, météo par API, assistant vocal relié à un LLM au choix, page de réglages servie par l'appareil. Sans Home Assistant, sans ESPHome. Développé sur le M5Stack Tab5, compatibilité Waveshare 7B gardée et testée de temps en temps.

Cette documentation est tenue **en même temps que le code** : chaque résultat porte la carte et la révision d'écran sur lesquelles il a été constaté, et chaque incident est consigné avec sa cause et sa solution.

| Je veux… | À lire |
|---|---|
| comprendre le projet en deux minutes | le [README du dépôt](../README.md) |
| savoir **sur quelle carte ça tourne** et ce qui est testé où (Tab5 ST7121, ST7123, ILI9881C, Waveshare 7B) | [Compatibilité et versions](compatibilite.md) |
| comprendre comment c'est construit | [Architecture](architecture.md), puis les [décisions](decisions/README.md) |
| compiler, flasher, retrouver la tablette | [README du dépôt](../README.md#compiler), [AGENTS.md](../AGENTS.md) |
| corriger un symptôme déjà vu (écran noir, port série, compilation…) | [Dépannage](depannage.md) : symptôme → cause → solution |
| savoir où en est le projet, les critères de « fait » et les risques | [Plan](plan.md) |
| connaître le coût en temps, sessions et tokens | [Coût](cout.md) |
| voir ce qui a été mesuré sur la carte | `mesures/` (une page par jalon, par exemple `mesures/1a.md`) |
| brancher la page de réglages au firmware | [Contrat de l'API de réglages](api-reglages.md), [la page et son faux appareil](../web/README.md) |
| retrouver d'où vient un fait (versions, licences, broches) | [Recherches sourcées](recherche/) |

## Règles de lecture

- **« Constaté »** = vu sur la carte (écran, journal série, version lue sur l'appareil). **« Écrit, non testé sur carte »** = le code existe, rien n'a été observé sur le matériel. **« Non vérifié »** = ni l'un ni l'autre. Un « build OK » ou un test vert seul ne vaut pas « constaté ».
- Une décision qui semble étrange est probablement dans une [ADR](decisions/README.md) ou dans le [dépannage](depannage.md) : vérifier avant de « corriger ».
- Quand le code et un document divergent, le code gagne : corriger le document.
