# Meridien

*Nom retenu par Axel le 06/11/2026 : le méridien fixe l'heure et le lieu, ce que l'écran affiche. Dépôt GitHub **privé** `Axellum/meridien`. Le dossier local et le nom du projet CMake s'appellent encore `ecran_autonome`.*

Un écran tactile ESP32-P4 qui fonctionne **seul** : heure et date, météo par API, assistant vocal relié à un LLM au choix (local ou cloud), et une page web de réglages servie par l'appareil lui-même.

Aucun Home Assistant, aucun ESPHome : firmware natif **ESP-IDF + LVGL 9**, code propre et testable. Il décline le projet [Tab5 (ESPHome)](../00ProjetTab/README.md) en gardant son goût (thèmes, polices, textes courts, accents) mais pas sa dépendance à Home Assistant.

| | |
|---|---|
| Carte de développement | M5Stack Tab5, **révision d'écran ST7121** (1280×720) |
| Compatibilité visée | Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (1024×600), testée de temps en temps — **pas encore essayée** |
| Version 1 | heure/date, météo (Open-Meteo), assistant vocal (mot d'activation → STT → LLM → TTS), page de réglages |
| Version 2 | calendrier Google (par adresse iCal secrète, voir ADR-0004) |
| Hors périmètre | domotique, Home Assistant, ESPHome |

## État (06/11/2026)

- **Phase 0 (préparation) faite** : recherches, audit du dépôt Tab5, chaîne de compilation prouvée, plan et coût chiffrés.
- **Phase 1 en cours** (prouver le matériel, du plus risqué au moins risqué) : l'écran, le tactile et un « Bonjour » LVGL ont été essayés sur le Tab5 ST7121 (jalon 1a, [mesures](docs/mesures/1a.md)) ; une réserve reste ouverte après coupure de courant ([dépannage](docs/depannage.md#ouverts)). Wi-Fi par le C6, audio et portail captif : prévus, notes de recherche écrites.
- **Sans carte** : le noyau (météo, fuseaux, heure, assistant vocal : états, flux, requêtes, découpe en phrases) est écrit et vérifié sur le PC (252 tests exécutés avec AddressSanitizer et UBSan, 206 mutations détectées) ; la page de réglages et son faux appareil aussi (49 vérifications). Rien de cela n'a été lancé sur une carte ni servi par l'appareil.

Ce qui est testé sur quelle carte, avec quelle version : **[docs/compatibilite.md](docs/compatibilite.md)**. Rien n'y est écrit « fonctionne » sans carte, commit et date.

## Où lire

| | |
|---|---|
| [docs/README.md](docs/README.md) | l'accueil de la documentation (« je veux… → je lis… ») |
| [docs/compatibilite.md](docs/compatibilite.md) | cartes, versions, ce qui est constaté où |
| [docs/architecture.md](docs/architecture.md) | les couches et les dossiers |
| [docs/depannage.md](docs/depannage.md) | symptôme → cause → solution, avec la carte sur laquelle c'est arrivé |
| [docs/plan.md](docs/plan.md) · [docs/cout.md](docs/cout.md) | phases et critères de « fait », risques ; temps, sessions, tokens |
| [docs/decisions/](docs/decisions/README.md) · [docs/recherche/](docs/recherche/) | décisions (ADR) ; faits sourcés et leurs « non vérifié » |

## Compiler

PowerShell (jamais Git Bash), ESP-IDF 5.5.5 déjà présent dans `H:\idf` :

```powershell
.\tools\idf.ps1 set-target esp32p4
.\tools\idf.ps1 build
```

Flasher une carte : seulement sur demande explicite d'Axel, après sauvegarde de la flash et vérification de la MAC (voir [AGENTS.md](AGENTS.md)).
