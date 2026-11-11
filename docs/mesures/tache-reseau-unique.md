# Une seule tâche réseau des données, et trois variables fixes en PSRAM — compilation seulement, aucune carte

> **Summary (EN)** — The weather, Vigie alerts, sea and spot-list clients no longer each own a task with a 12 kB stack in
> internal RAM: one task, `donnees`, runs them one step at a time, ordered by a pure core scheduler
> (`ecran/ordonnanceur_reseau.hpp`, 20 tests, 20 mutations). Two fixed variables of `main/` (5.0 kB settings host,
> 2.1 kB detailed weather) moved to PSRAM: −7,480 bytes of internal `.bss` on both boards (CI). **Measured on the build only** (see §3); nothing ran on a board.

**Vu sur** : PC et CI seulement, compilation ; aucune carte. ESP-IDF 5.5.5, base `0b8cf7d` (main), branche
`claude/ram-tache-reseau-unique-9cf422`, le 11/11/2026.

Règle de lecture : ce qui n'est pas marqué « mesuré » est une lecture de code ; ce qui n'a pas été observé est écrit **non vérifié**.

## 1. Ce qui change

| Avant | Après |
| --- | --- |
| Tâches `meteo` et `vigie` (12 288 o de pile chacune, toute la vie), `mer` (12 288 o dès qu'un spot est choisi), `mer_liste` (12 288 o le temps d'une lecture) | Une tâche `donnees` (12 288 o, toute la vie), priorité 0 comme avant |
| Chaque tâche dort dans sa boucle (`ulTaskNotifyTake`) | Chaque client fournit un **pas** : un tour de l'ancienne boucle, à l'identique ; là où elle dormait, le pas rend la durée du sommeil |
| Un réglage changé réveille la tâche du client (`xTaskNotifyGive`) | Il réveille la source dans la tâche unique (`tache_donnees_reveiller`) |

**Ordre (noyau pur, testé sur le PC)** : une source ne passe qu'après un réveil ; un réveil n'éloigne jamais un pas prévu
plus tôt, et une rafale de réveils ne repousse pas l'échéance (la météo garde son anti-rebond de 1,5 s compté depuis le
premier changement) ; parmi les sources dues, la **liste des spots** passe d'abord, sinon la plus en retard, puis l'ordre
de la table (météo, Vigie, mer) : aucune ne peut en affamer une autre ; la tâche ne dort jamais plus de 60 s.

**Pourquoi la liste des spots est prioritaire et la recherche de ville reste à part.** Les deux sont demandées à l'écran,
une personne attend. La liste passe dès la fin du pas en cours. La recherche de ville (`geocodage.cpp`) garde sa tâche de
passage (12 288 o le temps d'une recherche, rien au repos) : elle sert aussi la page web, qui attend sa réponse jusqu'à
30 s, et elle ne doit jamais faire la queue derrière une chaîne de requêtes météo (NWS : jusqu'à trois requêtes dans un
pas). Elle ne fait la queue que derrière le verrou TLS commun, comme avant.

**Ce qui ne change pas** : adresses, paramètres et en-têtes des requêtes ; délais (15 s), attente du verrou TLS (40 s),
vérification des certificats, verrou TLS commun ; rythme de chaque client (périodes, `Expires`, relance après échec,
premiers essais décalés au démarrage, relecture des réglages toutes les 5 s pour Vigie et la mer).

**Ce qui change de comportement (lecture de code, non vérifié sur carte)** :

- Les parties hors TLS d'un pas (lecture de la réponse, écriture du cache NVS, journal) ne se chevauchent plus avec la
  requête d'un autre client. Avant, seul l'aller-retour TLS était sérialisé.
- La liste des spots attend la fin du pas en cours (au pire une chaîne NWS + repli Open-Meteo) au lieu d'attendre le verrou
  TLS entre deux requêtes de ce pas.
- Le premier pas de la météo part 1,5 s après le démarrage du client (anti-rebond), avant tout essai réel : le réseau et
  l'heure ne sont jamais prêts aussi vite.
- La commande `piles` montre `donnees` à la place de `meteo`, `vigie` et `mer`.

## 2. Variables fixes de `main/` de plus d'1 Ko en RAM interne

Liste par `riscv32-esp-elf-nm -S --size-sort -C -l` (ELF du Tab5 de l'audit, `59b5fab`, adresses `0x4ff…` = RAM interne) :

| Variable | Octets | Fichier | Décision |
| --- | ---: | --- | --- |
| `hote_de_l_appareil()::hote` | 5 048 | `hote_appareil_esp.cpp` | **PSRAM** : interface et serveur web seulement, aucune interruption, aucun DMA, ni Wi-Fi ni TLS |
| `hote_de_l_appareil()::ports` | 2 428 sur `59b5fab`, **0** sur `main` récent (ELF 7B `851d420`) | `hote_appareil_esp.cpp` | Même bloc que `hote` (déjà presque vide : aucun gain) |
| `rafraichir()::detail` | 2 088 | `ecran_principal.cpp` | **PSRAM** : lu par la seule tâche LVGL, copie de 2 Ko toutes les 500 ms |
| `g_corps` | 4 097 | `serveur_web.cpp` | Laissé : fichier du lot L15 |
| `ui_alertes.cpp` `M()::tampon` | 2 048 | `ui_alertes.cpp` | Laissé : fichiers `ui_*` des lots L16/L17 |
| `ui_mer.cpp` `copie()::tampon` | 1 600 | `ui_mer.cpp` | Laissé : idem |
| `ui_clavier.cpp` `g_pool`, `g_carte`, `g_ctrl` | 1 512 + 1 344 + 1 344 | `ui_clavier.cpp` | Laissé : idem |
| `g` (`.data`, initialisées) | 3 088, 2 104, 1 628 | `ecran_principal.cpp`, `ui_roue.cpp`, `ui_alertes.cpp` | Laissé : seul le `.bss` (zéros) peut aller en PSRAM ; et fichiers d'autres lots |

**Pourquoi pas `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` + `EXT_RAM_BSS_ATTR`** : lu dans ESP-IDF 5.5.5
(`components/lwip/linker.lf:135`, `components/esp_psram/Kconfig.spiram.common:137`), l'option déplace aussi
**automatiquement tout le `.bss` de lwIP** en PSRAM : la pile TCP/IP du Wi-Fi et du TLS, que ce lot ne doit pas toucher.
Les trois variables sont donc allouées une fois en PSRAM (`heap_caps_calloc`, repli en RAM interne) au premier usage. De
même, l'état que les boucles de Vigie et de la mer gardaient sur leur pile est alloué en PSRAM (`creer_etat_de_pas`).

## 3. Mesures (compilation)

**Mesuré** par la CI (`idf.py size`, image ESP-IDF 5.5.5) : base `main` `0b8cf7d` (run 43100734026), branche au commit
`5fd54f4` (run 43113542010).

| | Tab5 avant | Tab5 après | Écart | 7B avant | 7B après | Écart |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| DIRAM (RAM interne prise à la compilation) | 143 666 | 136 186 | **−7 480** | 143 512 | 136 032 | **−7 480** |
| dont `.bss` | 38 712 | 31 232 | −7 480 | 38 504 | 31 024 | −7 480 |
| dont `.data` | 22 264 | 22 264 | 0 | 21 816 | 21 816 | 0 |
| Image du firmware | 3 301 860 | 3 303 858 | +1 998 | 3 280 086 | 3 282 084 | +1 998 |

Détail par `nm` (ELF du Tab5 compilé sur le PC au même commit, comparé à l'ELF 7B de `main` `851d420`) : `hote` −5 044 o,
`detail` −2 084 o, tables `atexit` (`_on_exit_args_instance`, `__atexit0`) −404 o, quelques petits statiques neufs
(`g_pas`, `g_reveils`, pointeurs : < 100 o). `ports` n'était plus en RAM interne sur `main` récent : la liste de l'audit
(sur `59b5fab`) le comptait encore (2 428 o).

**Piles réservées en RAM interne** (lecture de code : tailles passées à `xTaskCreate`, plus le bloc de contrôle de chaque tâche) :

| Situation | Avant | Après | Écart |
| --- | ---: | ---: | ---: |
| Au repos, aucun spot choisi | `meteo` + `vigie` = 24 576 | `donnees` = 12 288 | **−12 288** |
| Au repos, un spot choisi | `meteo` + `vigie` + `mer` = 36 864 | 12 288 | **−24 576** |
| Pendant une lecture de la liste des spots | 49 152 | 12 288 | −36 864 |
| Recherche de ville (inchangé) | + 12 288 le temps de la recherche | + 12 288 | 0 |

**Tests** : `test_ordonnanceur_reseau.cpp`, 20 tests (absents avant : le noyau n'existait pas), 20 mutations toutes
détectées ; `lancer.py` complet en mode « complet (compilé + exécuté) », clang++ 23.1.5 avec AddressSanitizer + UBSan :
tout passe. Compilation : Tab5 sur le PC (code de sortie 0) ; Tab5 et 7B par la CI.

## 4. Non vérifié sur carte — protocole

Rien de ce lot n'a tourné sur une carte. À faire sur le Tab5 de dev puis sur la 7B, sur un binaire de cette branche :

1. Redémarrage logiciel, puis commande série `memoire` toutes les 60 s pendant 5 min (5 relevés, comme la PR #34) :
   interne libre, plus gros bloc libre, minimum depuis le démarrage, à comparer aux mêmes relevés sur `main`.
2. Commande `piles` après au moins une requête de chaque sorte : la tâche `donnees` garde au moins 2 Ko de marge
   (la météo seule utilisait 6 000 o le 11/11/2026) ; plus de tâche `meteo`, `vigie`, `mer`.
3. Chaque donnée arrive comme avant, au journal : ligne `METEO …` au démarrage puis toutes les 15 min ; `VIGIE …` 60 à
   120 s après le réseau ; `MER …` 120 à 180 s après le réseau (avec un spot choisi) ; liste des spots (« Ajouter un
   spot ») ; recherche de ville à l'écran et sur la page.
4. Changer de ville à l'écran : une seule requête météo 1,5 s plus tard, puis Vigie et la mer replanifiées comme avant.
