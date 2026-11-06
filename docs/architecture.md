# Architecture

Mis à jour le 06/11/2026, après la fusion des quatre pistes de la première journée. Ce qui existe sur `main` est dit « sur main » ; ce qui vit encore sur la branche d'une session en cours serait dit « sur branche » (fusionné après relecture) ; il n'y en a plus aujourd'hui. Ce qui n'existe pas encore est dit « prévu ».

## Les couches

```
                 page de réglages (HTML/CSS/JS, ~14 Ko gzip)         ← web/            sur main
                          │  REST (docs/api-reglages.md)
 ┌────────────────────────┴──────────────────────────────────────────┐
 │ application (main/)  écran « Bonjour » aujourd'hui ; plus tard     │  sur main
 │ horloge, météo, assistant, réglages                                 │  (« Bonjour » seul)
 ├─────────────────────────────────────────────────────────────────────┤
 │ noyau pur (components/core/)  C++ standard, sans ESP-IDF            │  météo, heure,
 │ tables, formats, états, protocoles — testable sur le PC             │  assistant : sur main
 ├─────────────────────────────────────────────────────────────────────┤
 │ interface de carte (components/board_api/include/board.h, C pur)    │  sur main
 ├──────────────────────────────┬──────────────────────────────────────┤
 │ bsp_tab5  (boards/tab5/)     │ bsp_waveshare_7b (boards/…)          │  Tab5 : sur main
 │ écran, tactile, expandeur    │ EK79007, GT911, ES8311/ES7210        │  7B : prévu
 └──────────────────────────────┴──────────────────────────────────────┘
        ESP-IDF 5.5.5 · LVGL 9.5 + esp_lvgl_port · ESP32-P4 (+ C6 pour le Wi-Fi)
```

Règles qui tiennent l'ensemble ([AGENTS.md](../AGENTS.md)) : le noyau n'inclut rien d'ESP-IDF ; une carte = un dossier ; aucun `if (tab5)` dans l'application ; erreurs en `esp_err_t` aux frontières, pas d'exceptions ; aucune clé API lisible nulle part.

## Dossiers

| Dossier | Contenu | État |
|---|---|---|
| `main/` | `app_main`, écran « Bonjour » (`ui_bonjour.cpp`), palette (`palette.h`), infos de démarrage (`boot_info.*`), police Roboto Bold embarquée (accents, OFL) ; contient encore la **mire** d'essai du tactile, à retirer avant la phase 2 | sur main (essayé sur le Tab5 ST7121, [mesures/1a.md](mesures/1a.md)) |
| `components/board_api/` | `board.h` : l'interface commune des cartes (C pur) | sur main |
| `components/bsp_tab5/` | reset par l'expandeur PI4IOE5V6408, détection du contrôleur (`tab5_detect.c`), écran (`tab5_display.c`), tactile (`tab5_touch.c`), entrées/sorties | sur main (essayé sur le Tab5 ST7121) |
| `boards/tab5/` | `partitions.csv`, `sdkconfig.defaults.board` (puce rev 1.3, CPU, PSRAM, flash 16 Mo) | sur main |
| `components/core/` | météo : `meteo_wmo.hpp`, `meteo_openmeteo.hpp` (lecture de la réponse Open-Meteo) ; heure : `fuseaux.hpp` + `fuseaux_donnees.inc` (généré par `outils/generer_fuseaux.py` depuis `posix_tz_db`, MIT, voir `ATTRIBUTION.md`), `heure_fr.hpp` ; lecture JSON : `json_mini.hpp` ; assistant : `assistant_etats.hpp`, `assistant_texte.hpp`, `flux_sse.hpp`, `requetes_openai.hpp`, `extrait_openai.hpp`, `decoupe_phrases.hpp` | sur main (testé sur le PC seulement) |
| `test/host/` | tests du noyau (constexpr évalués par `static_assert`, puis **exécutés** avec ASan + UBSan) ; `lancer.py` avec preuve par mutations (`mutations_*.py`) ; `controle_*.py` | sur main |
| `web/` | `app/` (la page), `faux-serveur/` (faux appareil Node), `tests/parcours.py` (Playwright), `esquisses/` (les 3 propositions, la B retenue) | sur main |
| `docs/` | cette documentation, le plan, les ADR, les recherches | sur main |
| `tools/idf.ps1` | lanceur de compilation (voir ci-dessous) | sur main |

## Compiler

`tools/idf.ps1` appelle `idf.py` de l'ESP-IDF 5.5.5 de `H:\idf` avec le Python de son environnement et le `PATH` des outils (`export.ps1` d'ESPHome ne marche pas : [dépannage](depannage.md#outils-et-compilation)). La carte se choisit par la variable CMake `BOARD` (`tab5` par défaut). Une seule compilation à la fois sur le PC, `CMAKE_BUILD_PARALLEL_LEVEL=4`.

Les réglages de puce vivent dans `boards/<carte>/sdkconfig.defaults.board`, jamais dans le `sdkconfig.defaults` commun. Après toute modification, supprimer `sdkconfig` puis relancer `set-target` : les valeurs par défaut ne sont relues que sur un `sdkconfig` neuf.

## Partitions du Tab5 (16 Mo)

`otadata` 0x9000 · `phy_init` 0xb000 · `ota_0` 0x10000 (4 Mo) · `ota_1` 0x410000 (4 Mo) · `nvs` 0x810000 · `esphome_nvs` 0xf90000 (laissée intacte : un retour à ESPHome ne perd pas les réglages de la tablette). Mise à jour par la page avec retour arrière automatique (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, prévu).

## Flux de données prévus

- **Heure** : SNTP → secondes UTC → règle POSIX TZ du fuseau choisi (gardé en NVS) → date et heure locales en français.
- **Météo** : réglages (lieu choisi par la page) → requête Open-Meteo minimale → lecture → cache en NVS → écran ; hors ligne, la dernière météo reste avec la mention « périmée ».
- **Assistant** : mot d'activation sur l'appareil → STT → LLM en flux → TTS, chacun un service OpenAI-compatible indépendant, local ou cloud ([ADR-0002](decisions/0002-assistant-vocal-en-cascade.md)).
- **Réglages** : téléphone → page servie par l'appareil → API REST protégée par un jeton affiché à l'écran → NVS. Une clé API s'écrit, ne se relit jamais.
