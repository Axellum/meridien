# AGENTS.md — consignes pour les agents IA (Claude, Codex, Cursor, Gemini…)

Point d'entrée de tout agent qui travaille sur ce dépôt. Court exprès ; quand il diverge du code, le code gagne : corriger ce fichier.

## Ce qu'est le projet

Firmware natif ESP-IDF + LVGL 9 pour ESP32-P4 : heure/date, météo, assistant vocal LLM, page de réglages. **Autonome : ni Home Assistant, ni ESPHome.** Développé sur le M5Stack Tab5 ; compatibilité Waveshare 7B conservée. Lire [README.md](README.md), puis [docs/plan.md](docs/plan.md).

## À lire avant de toucher au code

1. [docs/plan.md](docs/plan.md) — phase en cours, critères de « fait », risques.
2. [docs/decisions/](docs/decisions/) — pourquoi les choix non évidents ont été faits (avant de « corriger » quelque chose qui a l'air faux).
3. [docs/recherche/](docs/recherche/) — faits sourcés et leurs « non vérifié ». Un fait « non vérifié » reste à vérifier avant de s'y appuyer.
4. [docs/recherche/04-audit-depot-tab5.md](docs/recherche/04-audit-depot-tab5.md) — broches, puces et pièges payés sur le firmware ESPHome (écran noir au boot, bus I2S, reset C6…).

## Compiler

PowerShell, jamais Git Bash. `.\tools\idf.ps1 build` utilise l'ESP-IDF 5.5.5 de `H:\idf` (propriété d'ESPHome : ne jamais le modifier). `CMAKE_BUILD_PARALLEL_LEVEL=4` (le PC a planté à 6). Une seule compilation à la fois sur le PC : vérifier qu'aucune autre session ne compile (`ninja`, `esphome`). Un `.\tools\idf.ps1 build` qui réussit ne prouve pas que ça marche : voir « Fait ».

## Règles de code

1. **Noyau pur** (`components/core/`) : C++ sans include ESP-IDF, testable sur PC. Pilotes, interface et carte sont derrière des interfaces.
2. **Une carte = un dossier** `boards/<carte>/` (broches, pilotes, `sdkconfig.defaults.board`). Aucun `if (tab5)` dans le code applicatif.
3. Erreurs : `esp_err_t` aux frontières, pas d'exceptions C++ (désactivées par défaut dans ESP-IDF).
4. Aucun secret dans le dépôt, les journaux, les exports ni la page web (clés masquées). Les réglages et clés vivent en NVS.
5. Aucune couleur écrite en dur : rôles de palette. Textes d'écran en français via une table de traduction ; accents et « ° » toujours ; une icône MDI n'est jamais un rectangle vide.
6. Pas de domotique, pas d'identifiant d'entité Home Assistant, pas de dépendance à ESPHome.
7. `esp_hosted` / `esp_wifi_remote` : **épingler** à la version qui marche avec le C6 actuel. Ne jamais proposer la mise à jour du C6 (décision d'Axel du 16/10/2026 : « j'ai peur de bloquer l'écran »).
8. Pas de numéro de version dans les noms de modules ou de dossiers : les versions sont des tags git.

## Matériel : limites strictes

- **Flasher une carte seulement sur demande explicite d'Axel.** Les deux Tab5 et la 7B sont à lui ; au moins deux sont en service chez lui. Avant le premier flash d'une carte : sauvegarde complète de la flash (`esptool read-flash`, `--no-stub`, vérifier le fichier relu), vérification de la MAC.
- Verrou de l'appareil et ports : `C:\Users\axell\.tab5\verrous.py` ; COM6 = Tab5 (MAC `30:ED:A0:EA:8E:87`, en SAV), COM8 = Waveshare 7B. Toujours lire la MAC avant d'écrire.
- Ouvrir ou fermer le port série USB peut réinitialiser la puce.
- Terminer chaque redémarrage logiciel par `esp_rom_software_reset_system()` (sur P4 `esp_restart()` ne remet à zéro que les cœurs ; boucle du lien C6 sur la 7B).
- Deux révisions d'écran Tab5 (ST7123, ST7121) plus l'ILI9881C d'origine : détecter le contrôleur avant d'initialiser le DSI.

## Préférences du produit (le goût de l'auteur, jugé sur l'écran réel)

Transitions instantanées, sans fondu. Popups au pixel. Phrases courtes à l'écran et à la voix, jamais un code ni un état brut. L'écran revient tout seul après une mise à jour ou un redémarrage. Une réutilisation plutôt que N copies. Plus joli d'abord, mesurer avant de sacrifier l'un à l'autre.

## Fait

**Un changement est « fait » seulement quand l'effet demandé a été constaté** : écran, journal série, version et heure de compilation lues sur l'appareil, redémarrage compris. « Build OK », « flash OK » ou un test vert ne suffisent pas ; sinon écrire « non vérifié ».

## Git

Branche + PR vers `main`, jamais de push direct. Stager fichier par fichier. Un commit = un sujet. Messages multilignes : `git commit -F <fichier>`. Nouvelle décision d'architecture : un ADR dans `docs/decisions/` (numéro suivant).
