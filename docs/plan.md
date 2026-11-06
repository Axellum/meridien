# Plan — écran autonome

Rédigé le 06/11/2026. Les phases sont ordonnées **du plus risqué au moins risqué** : on prouve d'abord ce qui peut tout faire échouer (duplex audio avec AEC, portail captif par le C6, Wi-Fi avec le C6 non mis à jour), pas l'interface. Chiffrage : [cout.md](cout.md). Choix techniques : [decisions/](decisions/).

**Règle de « fait » (toutes phases)** : un critère n'est rempli que s'il a été **constaté** sur l'appareil (écran, journal série, version et heure de compilation lues sur la carte, redémarrage compris) ou, pour la logique pure, par un test hôte qui échoue sans le correctif. « Build OK » et « flash OK » ne comptent pas.

## Phase 0 — Préparation (faite le 06/11/2026)

Faits constatés : chaîne de compilation ESP32-P4 prouvée (ESP-IDF 5.5.5, squelette compilé en 38 s, `build/ecran_autonome.bin` 204 048 octets, **non flashé**) ; 8 recherches/audits consignés dans [recherche/](recherche/) ; 5 ADR proposées ; CORS et taille de la requête météo mesurés. Rien exécuté sur une carte.

## Phase 1 — Prouver le matériel sur le Tab5 (le plus risqué)

Chaque jalon a un critère mesurable et un repli.

| Jalon | Fini = | Repli si échec |
|---|---|---|
| 1a. Sauvegarde de la flash, écran + tactile + LVGL « bonjour » | sauvegarde relue et vérifiée ; une image LVGL s'affiche **après redémarrage et après coupure** ; le tactile répond ; le contrôleur (ST7121/ST7123/ILI9881C) est identifié dans le journal | revenir à la sauvegarde ESPHome (restauration documentée avant le premier flash) |
| 1b. Wi-Fi par le C6 (esp_hosted épinglé) + SNTP | heure correcte affichée au démarrage, après un redémarrage logiciel (via `esp_rom_software_reset_system()`) et après une coupure de courant ; **20 redémarrages de suite sans perte du lien** | essayer une autre version d'`esp_hosted` ; ne jamais mettre à jour le C6 (décision d'Axel) |
| 1c. Audio duplex + AEC | lecture ES8388 **et** capture ES7210 en même temps ; **mesure** de l'écho du haut-parleur dans le micro avant/après AEC ; le mot d'activation est détecté pendant que le haut-parleur joue | pas d'AEC : micro coupé pendant la réponse, un appui sur l'écran interrompt (ADR-0002) |
| 1d. Portail captif par SoftAP via le C6 | un téléphone Android **et** un iPhone atteignent la page de test (test physique, constaté par Axel) | saisie du Wi-Fi sur l'écran tactile + QR code vers la page |

**Barrière de décision** à la fin de la phase 1 : Axel voit les mesures et confirme ou ajuste le plan avant d'engager les phases 2 à 4.

### Sessions parallèles (décidé le 06/11/2026, Axel : « si tu peux avancer en parallélisant, fais-le »)

Un seul dépôt git (`ecran_autonome/`, local, sans remote, distinct du dépôt Tab5 public). **Un worktree et une branche par session**, fusion dans `main` par la session de coordination après relecture du diff. Deux pistes seulement : il n'y a qu'**un Tab5, un port USB et un PC qui plante à plus d'une compilation**, et la fenêtre de 5 h est déjà entamée.

| Piste | Branche | Touche seulement | Ne touche jamais |
|---|---|---|---|
| **A — matériel** (chaîne 1a → 1b → 1c → 1d, une étape à la fois) | `phase1-materiel` | `boards/`, `components/bsp_*`, `main/`, `sdkconfig.defaults*`, `CMakeLists.txt` racine, `docs/mesures/` | `web/`, `components/core/`, `docs/plan.md` |
| **B — sans matériel** (page de réglages contre un faux serveur, noyau pur testé sur le PC) | `web-et-noyau` | `web/`, `components/core/`, `test/host/`, `docs/api-reglages.md` | tout ce qui est carte/ESP-IDF ; **aucune compilation ESP-IDF**, aucun port série |

Règles communes : `docs/plan.md` et `contexte_ia/04_Projets/etat_ecran_autonome.md` ne sont écrits que par la session de coordination (les pistes y rapportent dans leur message final). Verrou `tab5-st7121` : seule la piste A touche la tablette, sous le titre « Écran autonome (natif ESP-IDF) ». Premier flash natif : accord explicite d'Axel au moment du flash, avec SHA-256 du binaire.

**Écart assumé par rapport à la barrière ci-dessus** : la piste B avance des éléments des phases 2 et 3 (page de réglages, tables météo/fuseaux) avant la fin de la phase 1. C'est sans risque pour le matériel (code PC, aucun firmware engagé) ; si la phase 1 change le plan (ex. portail captif impossible), seul le front est à reprendre en partie.

## Phase 2 — Écran principal : heure, date, météo

Horloge et date (fuseau en NVS), client Open-Meteo (requête minimale, cache NVS, « périmé » hors ligne), table WMO → libellé français + icône jour/nuit, thèmes et polices repris du dépôt Tab5, textes via une table de traduction, extinction/veille de l'écran. **Fini =** l'écran affiche une météo qui correspond à celle de la même ville sur le site d'Open-Meteo au moment du contrôle ; coupure du Wi-Fi → la dernière météo reste avec la mention « périmée » ; redémarrage → l'écran revient seul avec la bonne heure. Jalon 7B : compile pour les deux cartes (CI) ; essai physique de la 7B.

## Phase 3 — Page de réglages

SoftAP + portail captif (selon 1d), jeton d'appairage affiché à l'écran, mDNS, API REST, front (Wi-Fi, zone avec recherche de lieu, langue/thème/luminosité, trois cartes STT/LLM/TTS avec « tester », export/import sans clés, mise à jour par la page avec rollback). **Fini =** depuis un téléphone neuf : brancher l'appareil, atteindre la page, choisir « Lyon », voir la météo de Lyon à l'écran sans toucher à un fichier ni compiler ; une clé API n'est jamais lisible dans une réponse de l'API ; un téléversement d'une image volontairement cassée revient à l'ancienne par rollback. Axel valide le **rendu** de la page sur son téléphone.

## Phase 4 — Assistant vocal

Mot d'activation → STT → LLM en streaming → TTS, états à l'écran (prêt, écoute, réflexion, réponse, erreur) en phrases courtes, interruption, erreurs parlées ; backends cloud et local par préréglage. **Fini =** 20 échanges réels en français (accents, questions météo et heure) avec un LLM local **et** un cloud, latence mesurée de la fin de la phrase au début de la réponse, nombre de faux déclenchements sur une soirée, redémarrage sans perte des réglages. Jalon 7B : essai physique.

## Phase 5 — Finition et qualité

Tests hôte du noyau pur, simulateur LVGL PC avec captures de référence (refus de créer une référence en CI), CI GitHub Actions des deux cartes, core dump en flash, journal consultable depuis la page, auto-test de démarrage qui valide le rollback, scan des secrets, documentation, premier tag. Jalon 7B : essai physique complet.

## Phase 6 — Compatibilité Waveshare 7B

La couche carte est écrite dès la phase 1 (ADR-0005) et la CI compile les deux cartes ; cette phase regroupe le portage des pilotes (EK79007, GT911, ES8311 avec volume limité à 75 %, pas d'expandeur) et les **jalons physiques** sur la 7B de la maison (à la fin des phases 2, 4 et 5). **Fini =** les critères des phases 2 et 4 sont constatés sur la 7B, y compris après 20 redémarrages logiciels (boucle du lien C6 connue sur cette carte) ; écran éteint = vraiment noir.

## Phase 7 — Version 2 : calendrier (ICS)

Voir ADR-0004. Prérequis : évaluer `uICAL` sur un **vrai agenda d'Axel** (événements « toute la journée », récurrences, fuseaux) avant de s'engager. **Fini =** les événements du jour affichés correspondent à ceux de l'agenda ; limite de fraîcheur annoncée dans la page.

## Risques (par gravité)

1. **AEC duplex sur Tab5** : aucun exemple trouvé. Levé en 1c.
2. **Portail captif par esp_hosted** : non démontré sur P4+C6. Levé en 1d.
3. **Lien C6** : boucle SDIO vue sur la 7B, écran noir après OTA sur le Tab5 ; mise à jour du C6 interdite. Levé en 1b (20 redémarrages), reset CORE repris du dépôt Tab5.
4. **Une seule carte Tab5 valide, déjà en service** (voir décision 1).
5. **Dérive de portée** : « un assistant qui fait tout » ; la domotique est hors périmètre. Tout ajout passe par un ADR.
6. **Licences** : WakeNet limité aux produits Espressif ; Open-Meteo non commercial ; IconeMeteo (pack de Laura Reen) non vérifiée.
7. **Quotas de session** : la fenêtre de 5 h est déjà à 42 % à l'écriture de ce plan, toutes sessions confondues. Voir cout.md.

## Décisions à prendre par Axel

1. **Carte pour le développement — tranchée le 06/11/2026** (Axel : « Oui, Tab5 en service ») : le **Tab5 ST7121** (MAC `E8:F6:0A:E2:F6:B6`, IP 192.168.0.52 en DHCP), qui quitte Home Assistant pendant le développement ; la première tablette (ST7123) reste en SAV M5Stack. Constaté ce jour-là : tablette vivante sur le réseau, firmware `3.9.0-glyphes-19d8527-st7121` (cliché `C:\Users\axell\.tab5\preuves\20261106-1123-tab5-st7121-avant.json`), verrou `tab5-st7121` pris. **Sauvegarde faite le 06/11/2026, volontairement partielle** (Axel : « le code est dans git ») : la tablette est branchée sur COM11 (port retrouvé par la MAC `E8:F6:0A:E2:F6:B6` ; il change avec la prise) ; ce qui n'est pas dans git est dans `H:\AuxFilsDesIdees\_sauvegardes\tab5_st7121_avant_ecran_autonome_2026-11-06\` : début de flash (bootloader, table de partitions, otadata, phy_init) et NVS de 448 Ko (Wi-Fi, réglages), chacun lu deux fois à l'identique, avec `LISEZMOI.md` (recette de remise en état en trois écritures, **jamais essayée**). Le firmware, lui, est `~/.tab5/elf/3.9.0-glyphes-19d8527-st7121/firmware.ota.bin`, **prouvé identique à l'app0 en flash** (`verify-flash` : digest matched ; otadata : app0 actif). Non sauvegardée : app1 (ancienne mise à jour, inutile). Effet de bord constaté : le redémarrage USB a ramené `number.tab5_volume` de 85 à 100 (le firmware ESPHome ne le conserve apparemment pas ; non corrigé). L'appareil de COM14 (`1C:DB:D4:40:07:64`) est un **ESP32-S3** d'un autre projet, pas le Tab5 : lu une fois par `flash-id`, rien d'écrit.
2. **Mot d'activation** : « Bonjour ESP » (modèle officiel, français) ou entraînement d'un mot propre (coût et licence non documentés). Recommandation : « Bonjour ESP » pour la phase 4, mot propre ensuite.
3. **Où tournent les STT/TTS locaux** : LM Studio n'a pas d'audio. Il faut un service STT et un TTS sur le PC (ou le Deck). Recommandation : tester d'abord le cloud (rapide), puis Kokoro (voix française) + un STT local, sur le PC d'Axel.
4. **Nom, dépôt, visibilité, licence** : **nom tranché le 06/11/2026 : « Meridien »** (Axel ; quatre candidats libres sous `Axellum`, `cadran` écarté : une application Mac d'horloge le porte). **Dépôt GitHub privé `Axellum/meridien`** créé le 06/11/2026, séparé du dépôt Tab5 (public, ESPHome). **Reste à décider par Axel** : passage en public (une publication) et licence (MIT suggérée comme le dépôt Tab5 ; usage d'Open-Meteo : non commercial, CC BY 4.0, à citer). Avant le public : licence de WakeNet (limitée aux produits Espressif d'après la recherche), licence du pack d'icônes météo (non vérifiée), `gitleaks` sur tout l'historique, retrait des adresses MAC et chemins locaux des docs si Axel le veut, décision doc bilingue ou française seule.
5. **Version d'ESP-IDF** : 5.5.5 (recommandé) ou 6.x (ADR-0001).
6. **Calendrier** : accepter une fraîcheur de 6 à 24 h avec l'adresse iCal (ADR-0004).
