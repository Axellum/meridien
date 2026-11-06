# Écran autonome (nom provisoire)

Un écran tactile ESP32-P4 qui fonctionne **seul** : heure et date, météo par API, assistant vocal relié à un LLM au choix (local ou cloud), et une page web de réglages servie par l'appareil lui-même.

Aucun Home Assistant, aucun ESPHome : firmware natif **ESP-IDF + LVGL 9**, code propre et testable. Il décline le projet [Tab5 (ESPHome)](../00ProjetTab/README.md) en gardant son goût (thèmes, polices, textes courts, accents) mais pas sa dépendance à Home Assistant.

| | |
|---|---|
| Carte de développement | M5Stack Tab5 V2 (1280×720) |
| Compatibilité visée | Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (1024×600), testée de temps en temps |
| Version 1 | heure/date, météo (Open-Meteo), assistant vocal (mot d'activation → STT → LLM → TTS), page de réglages |
| Version 2 | calendrier Google (par adresse iCal secrète, voir ADR-0004) |
| Hors périmètre | domotique, Home Assistant, ESPHome |

## État

**Phase 0 (préparation) faite le 06/11/2026** : recherches, audit du dépôt Tab5, chaîne de compilation prouvée, plan et coût chiffrés. **Rien n'est encore flashé sur une carte.** Le squelette ne contient qu'un `app_main` qui journalise.

Où lire : [docs/plan.md](docs/plan.md) (phases, critères de « fait », risques) · [docs/cout.md](docs/cout.md) (temps, sessions, tokens) · [docs/decisions/](docs/decisions/) (ADR proposées, à valider) · [docs/recherche/](docs/recherche/) (sources et faits vérifiés).

## Compiler

PowerShell (jamais Git Bash), ESP-IDF 5.5.5 déjà présent dans `H:\idf` :

```powershell
.\tools\idf.ps1 set-target esp32p4
.\tools\idf.ps1 build
```

Flasher une carte : seulement sur demande explicite d'Axel, après sauvegarde complète de la flash (voir [AGENTS.md](AGENTS.md)).
