# Météo sur l'appareil — Open-Meteo par HTTPS, cache NVS, indicateur « périmé »

> **Summary (EN)** — Weather on the device, written on 2026-11-06 and **run on a Tab5 (ST7121) the same evening**. A FreeRTOS task waits for the IP address and the SNTP time, then asks `api.open-meteo.com` over HTTPS (server checked with ESP-IDF's certificate bundle) every 15 min, with a spaced retry after a failure; the answer is read by the pure core, kept in the NVS and published through `meteo_etat_lire()` (`main/meteo_etat.h`), together with a "stale" flag (`perime`). The place is `zone_lire()` / `zone_ecrire()` (`main/meteo_zone.h`), Lyon by default, in the NVS.
> - Checked on the PC: 25 core tests (294 tests in the whole suite, AddressSanitizer + UBSan, all pass), 39 mutations out of 39 detected (detection = compilation failure), and the request built by the core was accepted by the real API (HTTP 200, 2 837 bytes, read by `analyser`). ESP-IDF 5.5.5 build of commit `55644aa`: 1 588 480 bytes, no compiler warning (plus one CMake warning about the ROM debugger file, `ESP_ROM_ELF_DIR` not defined; none of this batch's files is named in it, cause not checked).
> - **Seen on the board** (Tab5 ST7121, 2026-11-06 22:03 to 22:23, commit `55644aa`, application written alone, hash verified): the first answer arrived 16.7 s after the start (Lyon 9.4 °C, "Plutôt dégagé", min 7.6 / max 13.0), equal to the live API called from the PC 35 s later; after a **software** restart the state came back from the NVS cache 3.2 s after the start, before the Wi-Fi; the second periodic request came 15 min 42 s after the previous one (9.2 °C, "Brouillard"), equal to the API; the server certificate was validated each time.
> - **Not seen**: Wi-Fi cut → stale flag, restart after a power cut, deliberately wrong request, change of place through `zone_ecrire`, the screen. **Open point**: the free internal RAM fell by 43.7 KB in 15 min with no weather request in between, cause not identified (section 5).
> - Choices that are the author's to validate: "stale" after 2 h, or after 3 failures in a row with an answer older than 30 min, or when the clock is unknown.

Branche `claude/meteo-appareil-f761d6` (tirée de `claude/jalon-1b-wifi-c2232d`, HEAD `97520e2`). Carte visée : M5Stack Tab5 (écran ST7121). Règle de lecture de ce fichier : tout ce qui n'est pas écrit « constaté » est une décision ou une lecture de source ; ce qui n'a pas été observé est écrit **non vérifié**.

## 1. Ce qui est construit

| Élément | Choix | Où |
| --- | --- | --- |
| Contrats lus par l'écran et la page de réglages | `MeteoEtat` + `meteo_etat_lire()` ; `ZoneMeteo` + `zone_lire()` / `zone_ecrire()` ; noms au niveau global, en-têtes purs (aucun en-tête ESP-IDF) ; un champ ajouté après le premier commit : `minmax_connus` | `main/meteo_etat.h`, `main/meteo_zone.h` |
| Requête | `GET https://api.open-meteo.com/v1/forecast?latitude=…&longitude=…` avec `current`, `hourly` (probabilité de précipitations), `daily` sur 7 jours, `timezone=auto`, `timeformat=unixtime` ; 474 octets pour Lyon, 479 au plus ; sans clé ; le tampon d'envoi d'`esp_http_client` est relevé à 1 024 octets (512 par défaut) | `components/core/include/ecran/meteo_requete.hpp`, `main/meteo_client.cpp` |
| HTTPS | `esp_http_client` + `esp_crt_bundle_attach` (paquet complet de certificats racine d'ESP-IDF 5.5.5, `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL`) : le serveur est vérifié, donc l'heure SNTP est exigée avant la première requête | `main/meteo_client.cpp` |
| Tâche | pile de 12 288 octets, priorité 3 ; attend `ip_obtenue` et `heure_synchronisee` de `reseau_etat()` (lu seulement, `reseau.*` n'est pas modifié) ; tampon de réception de 16 Ko pris en PSRAM et rendu après chaque essai | `main/meteo_client.cpp` |
| Rythme | 15 min si tout va bien ; après un échec 15 s, 30 s, 1 min… doublés jusqu'à 15 min ; au moins 10 min après un HTTP 429 ; jusqu'à 10 % de décalage aléatoire | `components/core/include/ecran/meteo_planning.hpp` |
| Cache | une clé `cache` de la NVS (espace `meteo`) = relevé + position de la zone + somme de contrôle ; relu au démarrage seulement si la position est celle de la zone actuelle | `components/core/include/ecran/meteo_cache.hpp`, `main/meteo_client.cpp` |
| Zone | espace `meteo`, clé `zone` (100 octets) ; défaut Lyon ; nom UTF-8 bien formé, coordonnées finies et dans les bornes, fuseau IANA ; un changement de **position** efface la météo de l'ancien lieu et relance une requête après 1,5 s | `components/core/include/ecran/meteo_zone_regles.hpp`, `main/meteo_zone.cpp` |
| Libellé | libellé français court du tableau WMO du noyau (`meteo_wmo.hpp`), tous de moins de 40 octets (testé) | `components/core/include/ecran/meteo_synthese.hpp` |
| Journal | `METEO <lieu> : … °C …` à chaque réponse ; `contrat (…) : meteo_etat_lire -> valide=… perime=…` au démarrage, quand le réseau est prêt, après chaque réponse et quand « périmé » change ; mémoire libre avant, après la connexion TLS et après chaque requête | `main/meteo_client.cpp` |

## 2. Règle « périmé » (décision à valider par Axel)

`perime` est vrai quand : la réponse a plus de **2 h** ; **ou** 3 essais ratés de suite **et** une réponse de plus de **30 min** (deux rafraîchissements manqués : une box redémarrée il y a 4 minutes ne déclenche rien) ; **ou** l'heure de l'appareil n'est pas réglée ou a reculé de plus de 5 min depuis la réponse (on ne sait pas juger : jamais « à jour » sur une supposition). Conséquence : une météo relue du cache n'est marquée périmée au démarrage que si l'horloge est inconnue (jusqu'à la première heure SNTP, puis fraîche si elle a moins de 2 h). **Constaté** après un redémarrage logiciel : `perime=0` dès le démarrage, donc l'horloge était connue avant le SNTP (probablement conservée au redémarrage logiciel : cause non vérifiée). **Non vérifié** après une coupure de courant, où `perime=1` est attendu. `valide` ne dépend pas de la fraîcheur : une météo périmée reste lisible, y compris au-delà de 6 h (le noyau appelle cet état « très périmé » et recommande de ne plus la présenter comme actuelle : **non appliqué**, le contrat n'a que `perime`).

Jour des min/max : celui qui contient l'heure actuelle ; à défaut (cache de plus de 7 jours) celui de la mesure ; sans prévision, `minmax_connus` est faux et min et max valent la température du moment.

## 3. Critères de « fait » de la tâche

| Critère | État | Preuve |
| --- | --- | --- |
| `meteo_etat.h` et `meteo_zone.h` livrés aux autres sessions | **constaté** (dépôt) | commit `ab05293` |
| Tests du noyau pur sur le PC | **constaté** | 25 tests de `test_meteo_appareil.cpp` ; suite complète 294 tests, ASan + UBSan, au commit `4e390db` |
| Mutations du noyau détectées | **constaté, avec réserve** | 39 sur 39 ; « détectée » = échec de compilation (`static_assert`), l'exécutable du mutant n'est pas lancé ; une mutation écartée (voir [depannage.md](../depannage.md#a-mutation-keeps-the-compiler-busy-for-minutes)) |
| L'adresse construite par le noyau est acceptée par l'API réelle et la réponse est lue | **constaté sur le PC** | voir la section 4 |
| Compilation ESP-IDF 5.5.5 | **constaté** | `.\tools\idf.ps1 build` au commit `55644aa` : 0 erreur, aucun avertissement de compilation (plus un avertissement CMake sur le fichier gdbinit du débogueur, `ESP_ROM_ELF_DIR` non définie ; aucun fichier de ce lot n'y est cité, cause non vérifiée), binaire `meridien.bin` de 1 588 480 octets, SHA-256 `AFD7F57343C1CCD6B8D77F37C5003F2C4D588ECC99B6FFB12401A541E05708D9`, version lue dans le projet : `jalon-1a-tab5-st7121-53-g55644aa`. Les commits suivants ne touchent que la documentation |
| Sur la carte : `meteo_etat_lire` rend la météo réelle de Lyon, comparée à Open-Meteo au même moment | **constaté** | 22 h 04 et 22 h 22 : valeurs identiques à l'API appelée depuis le PC (section 5) |
| Sur la carte : Wi-Fi coupé ou requête fausse → `perime` après le délai, dernière météo conservée | **non fait** | demande Axel à la box (30 min au moins avant que `perime` change) ; une requête fausse demanderait un second binaire |
| Sur la carte : redémarrage → l'état revient du cache | **constaté, avec réserve** | redémarrage **logiciel** seulement (cache relu 3,2 s après le démarrage) ; coupure de courant non faite |
| Sur la carte : la requête se répète toutes les 15 min | **constaté, une répétition** | 22 h 07 min 08 → 22 h 22 min 50 (942 s annoncées par le journal) |
| Sur la carte : mémoire libre avant/après une requête | **constaté, avec réserve** | trois requêtes mesurées (section 5) : interne -368 à +3 512 o, PSRAM -1 536 o puis 0 ; mais la RAM interne libre a baissé de 43,7 Ko entre deux requêtes, cause non identifiée |

## 4. Vérifications sur le PC (aucune sur la carte)

**Chaîne pure contre l'API réelle.** Un petit programme jetable (hors dépôt) construit l'adresse avec `construire_requete()` pour Lyon, le PC la demande avec `curl`, puis `analyser()` et `synthetiser()` lisent la réponse :

| Mesure | Résultat |
| --- | --- |
| Adresse construite par le noyau | 474 octets, acceptée |
| Réponse | HTTP 200, 2 837 octets (la recherche 03 avait mesuré 2 839), 0,16 s |
| Lecture par `analyser()` | acceptée, 7 jours ; `current.time` = 1793997000 (06/11/2026 20 h 30 UTC, 21 h 30 à Paris) |
| Température | 9,8 °C ; min du jour 7,6 °C ; max 13,0 °C ; code WMO 1 « Plutôt dégagé » ; nuit |

Vu sur : PC d'Axel (Windows 11, llvm-mingw clang++ 23.1.5), commit `4e390db`, le 2026-11-06 vers 21 h 40 (heure de Paris). C'est la lecture de l'API **depuis le PC** : elle ne dit rien de ce que la carte recevra par le C6.

**Tests.** `python test/host/lancer.py` (mode « complet (compilé + exécuté) », hook de commit) : 294 tests, 0 échec, ASan + UBSan. Les 25 tests de ce lot couvrent : l'adresse (Lyon exacte, coordonnées négatives, bornes, place insuffisante, tampon d'envoi), les règles de zone (arrondi, NaN, infini, UTF-8 mal formé), la synthèse (Paris, libellés, 2 h pile, échecs, horloge incertaine, min/max et replis, dixièmes en texte), le rythme et le cache (altération d'un bit à chaque octet, version, position invraisemblable, place). Vu sur : PC d'Axel, commit `4e390db`, 2026-11-06.

**Limite** : le PC est en x86-64 ; ce qui passe ici ne prouve ni la mémoire ni le temps de la carte.

## 5. Sur la carte — constaté le 2026-11-06, de 22 h 03 à 22 h 23

Essai sans geste d'Axel et sans regarder l'écran : écriture de l'application seule, puis écoute du port série. Extrait du journal : [meteo-journal-carte.log](meteo-journal-carte.log) (lignes météo, démarrage, mémoire ; secrets et adresse MAC contrôlés absents).

**Écriture.** esptool 5.3.1, `meridien.bin` du commit `55644aa` : 1 588 480 octets, SHA-256 `AFD7F57343C1CCD6B8D77F37C5003F2C4D588ECC99B6FFB12401A541E05708D9`, 834 671 octets compressés écrits en 6,5 s à `0x10000` seulement (esptool n'a touché ni le bootloader, ni la table de partitions, ni otadata, ni la NVS), « Hash of data verified ». La MAC lue sur la puce avant d'écrire est celle de la carte visée (non recopiée ici). Version lue sur la carte : `jalon-1a-tab5-st7121-53-g55644a`.

**Météo lue par `meteo_etat_lire` (ligne `contrat`) comparée à l'API.** L'API est appelée depuis le PC avec la même adresse (474 octets) quelques dizaines de secondes plus tard.

| Réponse de la carte | Valeurs de la carte | API appelée depuis le PC |
| --- | --- | --- |
| 22 h 04 min 19 (2 836 octets en 1 497 ms) | 9,4 °C, ressentie 8,6, code 1 « Plutôt dégagé », nuit, min 7,6 / max 13,0, vent 2,3 km/h 321°, 1 024,2 hPa, humidité 93 %, pluie 0 %, mesure UNIX 1793998800 (22 h 00) | 22 h 04 min 54, HTTP 200, 2 836 octets : même mesure, 9,4 ; 8,6 ; code 1 ; nuit ; min 7,6 / max 13,0 ; 2,3 ; 321 ; 1 024,2 ; 93 ; pluie horaire 0 |
| 22 h 22 min 51 (2 837 octets en 1 530 ms) | 9,2 °C, ressentie 8,4, code 45 « Brouillard », nuit, min 7,6 / max 13,0, vent 1,9 km/h 338°, 1 024,3 hPa, humidité 94 %, mesure UNIX 1793999700 (22 h 15) | 22 h 23 min 28, HTTP 200, 2 837 octets : même mesure, 9,2 ; 8,4 ; code 45 ; min 7,6 / max 13,0 ; 1,9 ; 338 ; 1 024,3 ; 94 |

Limite : les deux lectures portent sur la **même mesure** Open-Meteo (une par quart d'heure) ; l'égalité prouve la chaîne complète (HTTPS, lecture, état, libellé) mais pas que la carte ne retarde pas d'un quart d'heure. La réponse de 22 h 07 (9,4 °C, 2 835 octets en 4 357 ms) n'a pas été comparée à l'API au même moment. À chaque requête le journal porte `esp-x509-crt-bundle: Certificate validated`.

**Déroulement.**

| Heure (Paris) | Fait constaté |
| --- | --- |
| 22 h 04 min 02 | démarrage n° 1 (remise à zéro par RTS demandée par le PC, `rst:0x17`) : `pas de cache météo (NVS : ESP_ERR_NVS_NOT_FOUND)`, `meteo_etat_lire -> valide=0` |
| 22 h 04 min 15 à 17 | IP obtenue à 11,4 s, heure SNTP prête, requête à 15,2 s ; réponse à 16,7 s, cache écrit (204 octets), `valide=1 perime=0`, « prochain essai dans 989 s » |
| 22 h 06 min 46 | redémarrage **logiciel** par la commande série `redemarrer` (`rst:0x3 (SW_SYS_RESET)`, cause lue « redémarrage logiciel (code 3) ») |
| 22 h 06 min 49 | **cache relu 3,2 s après le démarrage, avant le Wi-Fi** (IP à 11,4 s) : `valide=1 perime=0`, 9,4 °C, min 7,6 / max 13,0, « Plutôt dégagé » |
| 22 h 07 min 03 à 08 | requête dès que le réseau et l'heure sont prêts ; réponse ; « prochain essai dans 942 s » |
| 22 h 22 min 50 | deuxième requête de ce démarrage, 942 s après la fin de la précédente (à 0,1 s près) ; réponse à 22 h 22 min 51 ; « prochain essai dans 961 s » |

**Mémoire libre autour des requêtes** (lignes `mémoire libre …` du journal ; plus grand bloc interne constant à 63 488 o) :

| Requête | Interne avant | Interne connexion TLS établie | Interne après | Écart interne | Écart PSRAM |
| --- | --- | --- | --- | --- | --- |
| 22 h 04, 1re du démarrage n° 1 | 192 043 o | 159 423 o | 195 555 o | +3 512 o | -1 536 o |
| 22 h 07, 1re du démarrage n° 2 | 195 667 o | 158 451 o | 199 159 o | +3 492 o | -1 536 o |
| 22 h 22, 2e du démarrage n° 2 | 155 503 o | 118 491 o | 155 135 o | -368 o | 0 o |

Lecture : la connexion TLS emprunte 33 à 37 Ko de RAM interne le temps de la requête, puis les rend ; la requête elle-même ne coûte que -368 à +3 512 o en interne. Le PSRAM perd 1 536 o à la première requête d'un démarrage puis plus rien à la deuxième : une seule répétition ne suffit pas pour exclure une fuite lente.

**Point ouvert.** La RAM interne libre est passée de 199 159 o (après la requête de 22 h 07 min 08) à 155 503 o (avant celle de 22 h 22 min 50), soit -43 656 o en 15 min 42 s, **sans requête météo entre les deux** ; minimum depuis le démarrage 104 696 o. Rien ne l'attribue à la tâche météo (elle dort entre deux requêtes) ni ne l'en exclut : ce binaire ne relève le tas qu'autour des requêtes. À mesurer sur une durée plus longue avant de conclure.

**Journal trompeur, corrigé (non revu sur la carte).** Après le redémarrage logiciel la ligne `cache relu` disait « marqué périmé tant que l'heure du réseau manque » alors que la ligne `contrat` suivante rendait `perime=0`. Le texte renvoie désormais à la ligne `contrat` (commit `53ee733`, compilé, non flashé).

**Non fait ou non vérifié.**
- Wi-Fi coupé → `perime` après 30 min et 3 échecs, dernière météo conservée : **non fait** (demande Axel à la box).
- Requête volontairement fausse sur la carte : **non fait** (second binaire nécessaire).
- Redémarrage après une **coupure de courant** avec un cache : **non vérifié** (horloge inconnue, `perime=1` attendu jusqu'au SNTP).
- Changement de lieu par `zone_ecrire` (et requête immédiate qui suit) : **non vérifié**, ce binaire n'a aucun appelant ; la page de réglages en sera un.
- Affichage de la météo à l'écran : **non vu** (ce binaire n'a pas d'écran météo ; l'écran n'a pas été regardé).
- Plus de deux requêtes d'un même démarrage ; plus de 20 minutes de marche.

**Vu sur** : M5Stack Tab5, écran ST7121 (contrôleur détecté `ST7121`, tactile Sitronix version 1), ESP32-P4 révision v1.3 (MAC lue sur la puce, non recopiée), coprocesseur C6 micrologiciel 1.4.1, Wi-Fi de la maison (RSSI -44 puis -41 dBm), port série retrouvé par le numéro de série USB (MAC), esptool 5.3.1, commit flashé `55644aa` (SHA-256 ci-dessus), le 2026-11-06 de 22 h 03 à 22 h 23. L'API est lue depuis le PC d'Axel (Windows 11) avec curl.
