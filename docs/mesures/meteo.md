# Météo sur l'appareil — Open-Meteo par HTTPS, cache NVS, indicateur « périmé »

> **Summary (EN)** — Weather on the device, written on 2026-11-06 and **run on a Tab5 (ST7121) the same evening**. A FreeRTOS task waits for the IP address and the SNTP time, then asks `api.open-meteo.com` over HTTPS (server checked with ESP-IDF's certificate bundle) every 15 min, with a spaced retry after a failure; the answer is read by the pure core, kept in the NVS and published through `meteo_etat_lire()` (`main/meteo_etat.h`), together with a "stale" flag (`perime`). The place is `zone_lire()` / `zone_ecrire()` (`main/meteo_zone.h`), Lyon by default, in the NVS.
> - Checked on the PC: 25 core tests (294 tests in the whole suite, AddressSanitizer + UBSan, all pass), 39 mutations out of 39 detected (detection = compilation failure), and the request built by the core was accepted by the real API (HTTP 200, 2 837 bytes, read by `analyser`). ESP-IDF 5.5.5 build of commit `55644aa`: 1 588 480 bytes, no compiler warning (plus one CMake warning about the ROM debugger file, `ESP_ROM_ELF_DIR` not defined; none of this batch's files is named in it, cause not checked).
> - **Seen on the board** (Tab5 ST7121, 2026-11-06 22:03 to 22:23, commit `55644aa`, application written alone, hash verified): the first answer arrived 16.7 s after the start (Lyon 9.4 °C, "Plutôt dégagé", min 7.6 / max 13.0), equal to the live API called from the PC 35 s later; after a **software** restart the state came back from the NVS cache 3.2 s after the start, before the Wi-Fi; the second periodic request came 15 min 42 s after the previous one (9.2 °C, "Brouillard"), equal to the API; the server certificate was validated each time.
> - **Not seen**: Wi-Fi cut → stale flag, restart after a power cut, deliberately wrong request, change of place through `zone_ecrire`, the screen. **Open point**: the free internal RAM fell by 43.7 KB in 15 min with no weather request in between, cause not identified (section 5).
> - Choices that are the author's to validate: "stale" after 2 h, or after 3 failures in a row with an answer older than 30 min, or when the clock is unknown.
> - **Rich weather (batch of the night of 2026-11-06 to 07, section 6)**: the request now also asks for the full current
>   conditions, 24 hours, 7 days and the rain per quarter of an hour; `meteo_detail_lire()` (`main/meteo_etat.h`, contract v2,
>   v1 unchanged) gives them to the screen, with the indices recomputed for the device clock. Written and checked **on the PC only**
>   (56 + 32 + 10 + 25 tests with AddressSanitizer + UBSan, 40 + 37 + 17 + 39 mutations detected, seven REAL
>   responses compared value by value with an independent Python computation); the firmware builds (1 862 512 bytes); **never run on a board**.
>   Open-Meteo labels sums by the END of their interval and its series start off the hour for zones such as UTC+5:45 and UTC-3:30:
>   the core handles both, see section 6.

Branche `claude/meteo-appareil-f761d6` (tirée de `claude/jalon-1b-wifi-c2232d`, HEAD `97520e2`). Carte visée : M5Stack Tab5 (écran ST7121). Règle de lecture de ce fichier : tout ce qui n'est pas écrit « constaté » est une décision ou une lecture de source ; ce qui n'a pas été observé est écrit **non vérifié**.

> **Les sections 1 à 5 décrivent le firmware flashé et essayé le 06/11/2026 (commit `55644aa`).** Depuis, le lot « météo riche » (section 6) a changé la requête (858 octets pour Lyon au lieu de 474), le cache (format v2 versionné) et l'état (en PSRAM) ; le contrat `meteo_etat_lire()` de la section 1 est inchangé.

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

**Suite du point ouvert (07/11/2026, build intégré, Tab5 ST7121).** Mesuré sur le binaire final du lot « budget de mémoire interne » (`2a05811`) : relevé chaque minute pendant 30 min, −202,6 o/min brut et −172,4 o/min après 5 min, soit **−152 o à chaque changement de minute de l'horloge** (PSRAM stable après 00 h 13), plus −368 à −384 o par requête météo. La baisse de 43,7 Ko en 15 min d'ici (≈ 2,8 Ko/min) **n'est pas retrouvée** sur ce binaire (≈ 0,17 Ko/min en régime) et **reste non expliquée** ; les −152 o/min restants sont **non résolus**. Détail, tableau et limites : [memoire-interne.md](memoire-interne.md) §8.

**Journal trompeur, corrigé (non revu sur la carte).** Après le redémarrage logiciel la ligne `cache relu` disait « marqué périmé tant que l'heure du réseau manque » alors que la ligne `contrat` suivante rendait `perime=0`. Le texte renvoie désormais à la ligne `contrat` (commit `53ee733`, compilé, non flashé).

**Non fait ou non vérifié.**
- Wi-Fi coupé → `perime` après 30 min et 3 échecs, dernière météo conservée : **non fait** (demande Axel à la box).
- Requête volontairement fausse sur la carte : **non fait** (second binaire nécessaire).
- Redémarrage après une **coupure de courant** avec un cache : **non vérifié** (horloge inconnue, `perime=1` attendu jusqu'au SNTP).
- Changement de lieu par `zone_ecrire` (et requête immédiate qui suit) : **non vérifié**, ce binaire n'a aucun appelant ; la page de réglages en sera un.
- Affichage de la météo à l'écran : **non vu** (ce binaire n'a pas d'écran météo ; l'écran n'a pas été regardé).
- Plus de deux requêtes d'un même démarrage ; plus de 20 minutes de marche.

**Vu sur** : M5Stack Tab5, écran ST7121 (contrôleur détecté `ST7121`, tactile Sitronix version 1), ESP32-P4 révision v1.3 (MAC lue sur la puce, non recopiée), coprocesseur C6 micrologiciel 1.4.1, Wi-Fi de la maison (RSSI -44 puis -41 dBm), port série retrouvé par le numéro de série USB (MAC), esptool 5.3.1, commit flashé `55644aa` (SHA-256 ci-dessus), le 2026-11-06 de 22 h 03 à 22 h 23. L'API est lue depuis le PC d'Axel (Windows 11) avec curl.

## 6. Météo riche — courant complet, 24 heures, 7 jours, pluie par quarts d'heure (lot de la nuit du 06 au 07/11/2026)

Demande de l'écran « très visuel » (session `ecran-visuel`) : de petites icônes météo, la température, la pluie « et tout ce que tu peux, quelque chose de complet ». **État : écrit et vérifié sur le PC, compilé avec ESP-IDF 5.5.5 ; jamais exécuté sur une carte** (la structure de la tâche météo et la prise de contact TLS sont retravaillées par la session du jalon 1b, qui n'avait pas livré quand ce lot a été écrit : ces lignes ne sont pas touchées ; aucune carte n'a été touchée par ce lot, le Tab5 étant tenu par une autre session).

### 6.1 Ce qui est livré

| Élément | Choix | Où |
| --- | --- | --- |
| Contrat v2 | `MeteoDetail` (courant complet, `heures[24]`, `jours[7]`, `quarts_precipitation_mm[9]`, `pluie` déduite, indices `heure_courante` / `jour_courant` / `quart_courant`, `version`), `meteo_detail_lire()`, `meteo_detail_version()` ; `MeteoEtat` et `meteo_etat_lire()` **inchangés** ; communiqué à `ecran-visuel` au commit `f9537c5` avant l'implémentation | `main/meteo_etat.h` |
| Requête | les mêmes paramètres qu'avant plus `hourly` (12 variables, `forecast_hours=25`), `minutely_15=precipitation` (`forecast_minutely_15=9`) et 16 variables `daily` ; **858 octets pour Lyon, 863 au plus**, ligne de requête 851 octets au plus ; `buffer_size_tx` reste 1 024 (seule la ligne de requête doit y tenir) | `components/core/include/ecran/meteo_requete.hpp`, `main/meteo_client.cpp` |
| Réponse | **4 427 à 4 575 octets** sur sept villes (non compressée) ; tampon de réception de 16 Ko **en PSRAM**, rendu après chaque essai : marge de 3,5, une réponse plus grande est refusée (`TropGrand`), jamais lue à moitié | `main/meteo_client.cpp` |
| Lecture | `analyser_riche()` : la partie de base reste STRICTE (une valeur invraisemblable refuse toute la réponse, comme avant) ; chaque section riche (heures, jours, quarts d'heure) est contrôlée à part et **vidée** si elle est cassée, sans toucher au reste ; le journal dit laquelle et pourquoi | `components/core/include/ecran/meteo_riche.hpp` |
| Conversion pour l'écran | `remplir_detail()` (gabarit sur le type du contrat, compilé contre le VRAI `meteo_etat.h` par le test) : dixièmes entiers → `float` exacts au dixième, indices et pluie proche recalculés avec l'heure de l'appareil, `jours` publiés seulement si les sept ont leurs compléments | `components/core/include/ecran/meteo_detail.hpp` |
| Tendance de pression sur 3 h | champ `courant.tendance_pression_3h_hpa` AJOUTÉ à la fin de `MeteoCourant` à la demande de `ecran-visuel` (les autres champs gardent leur ordre) : pression de la dernière mesure moins celle d'il y a environ 3 h, **mesurée par l'appareil** (un relevé toutes les 15 min, historique de 16 mesures : 168 octets en RAM interne sur le PC), et non fournie par Open-Meteo (`past_hours=3` ajouterait trois heures avant l'heure en cours dans toutes les séries et décalerait tous les indices) ; **NaN** tant qu'il n'a pas de mesure de 2 h 30 à 3 h 30 (les ~3 premières heures après un démarrage ou un changement de zone : l'historique n'est pas gardé en cache) ; il compare des instants de MESURE donnés par le serveur, donc ne dépend pas de l'horloge de l'appareil | `components/core/include/ecran/meteo_tendance.hpp`, `main/meteo_etat.cpp` |
| État | une `Meteo` de 1 312 octets (taille mesurée sur le PC ; le journal de démarrage donnera celle du RISC-V) allouée **en PSRAM** à la première publication, sous un mutex FreeRTOS (pas une section critique : la conversion écrit 2 088 octets) ; en RAM interne : le mutex et quelques entiers ; repli en RAM interne seulement si la PSRAM manque ; numéro de version atomique, remis à 0 quand la zone change | `main/meteo_etat.cpp`, `main/meteo_interne.h` |
| Cache NVS | format **v2 versionné** : marque, version, position, relevé de base (format v1 avec sa somme de contrôle), bloc riche, CRC-32 ; 857 octets au plus ; un cache d'une autre version (le v1 écrit par `55644aa`) est reconnu à sa version **avant** toute lecture, ignoré (« cache d'un ancien format ») et remplacé à la première réponse | `components/core/include/ecran/meteo_riche.hpp`, `main/meteo_client.cpp` |
| Pile de la tâche | inchangée (12 288 octets, priorité réglée par le jalon 1b) ; l'analyse de la réponse et la relecture du cache sont des fonctions à part (`noinline`) et la `Meteo` lue est en PSRAM, pour que le pic de pile reste la prise de contact TLS ; le journal donne la marge de pile après chaque essai | `main/meteo_client.cpp` |
| Journal | `contrat v2 (…) : meteo_detail_lire -> valide=… perime=… version=…`, puis le courant, l'heure en cours, le jour en cours et la pluie proche, avec les mêmes mots que l'écran ; au démarrage, les tailles réelles (`tailles : météo … o, contrat v2 … o`) | `main/meteo_client.cpp` |

### 6.2 Ce qu'Open-Meteo fait de particulier (et que le noyau gère)

1. **Un cumul est étiqueté par la FIN de son intervalle.** Précipitations, probabilité et UV moyen de l'étiquette `H` portent sur `[H - 1 h, H)` ; `minutely_15.precipitation` et `current.precipitation` sur les 15 minutes qui précèdent l'étiquette. Les valeurs instantanées (température, vent, code…) sont à l'étiquette. Sans correction, la pluie d'une heure s'afficherait une heure trop tôt. Le noyau décale d'une étiquette (`heures[i]` : instantanés à l'étiquette `i`, cumuls à l'étiquette `i + 1`) et la requête demande 25 étiquettes pour garder 24 heures complètes. **Recoupé** sur la réponse réelle de Seattle (la seule capture où il pleut ; les six autres donnent 0 pour les deux conventions) : étiquette +3 600 s = 1,0 mm = 0,3 + 0,3 + 0,2 + 0,2 (les quatre quarts d'heure étiquetés +900 à +3 600 s) alors que l'autre convention donnerait 0,4 ; étiquette +7 200 s = 0,2 mm = 0,1 + 0,0 + 0,1 + 0,0. Une seconde capture de Seattle (06/11/2026 à 23 h 45 UTC, rejouée à la main) confirme : pour l'étiquette du 07/11 à 1 h UTC, le cumul horaire est 0,2 mm, la somme des quatre quarts d'heure qui la précèdent (0,2) et non des quatre qui la suivent (0,1). `current.precipitation` = premier quart d'heure (0,8 = 0,8 puis 0,3 = 0,3). Le message du commit `f9537c5` rapporte un recoupement plus large fait le 06/11/2026 sur l'API (22 heures sur 22) : le script jetable n'a pas été gardé, **non rejoué**.
2. **Fuseaux qui ne sont pas à l'heure ronde.** Les séries horaires et en quarts d'heure commencent à l'instant courant arrondi à l'heure UTC, moins le reste de `utc_offset_seconds` modulo 3 600 (signe du C). Mesuré : Katmandou (UTC+5:45) commence 2 700 s **avant** `current.time` ; Terre-Neuve (UTC-3:30) 1 800 s **après** ; Lyon, Sydney, Singapour à 0. Donc `heure_courante` vaut -1 à Terre-Neuve pendant la demi-heure qui précède la première étiquette, et la probabilité du moment est alors `METEO_PROBA_INCONNUE`, **pas celle de l'heure voisine**. Les autres fuseaux fractionnaires (+5:30, +3:30, +10:30) suivent la même règle mais **n'ont pas été captés**.
3. **Nuit polaire** (Longyearbyen) : `sunrise == sunset`, `sunshine_duration` = 0 ; le contrat porte `sans_soleil`. Le jour polaire n'a **pas été observé**.
4. **Pluie proche.** « Il pleut » = au moins 0,1 mm sur un quart d'heure (précision de l'API, bruine comprise) ; le quart en cours compte en entier (on ne sait pas à quel moment du quart d'heure il pleut) ; les minutes sont arrondies au supérieur ; `fin_dans_min` = -1 si la pluie touche la fin de la fenêtre ; les quarts de plus de 30 min restent lisibles mais `perimee` est vrai ; une horloge en retard de plus de 5 min sur la réception, ou qui ne couvre aucun quart, donne `connue` faux. Tout est recalculé à chaque lecture avec l'heure de l'appareil.
5. **Ensoleillement** : `sunshine_duration` est en secondes, gardé à la minute près ; le contrat le donne en heures à la minute près (8,55 h = 8 h 33), pas arrondi au dixième (un double arrondi fausserait 0,1 h environ 4 jours sur 100).

### 6.3 Vérifications sur le PC

| Mesure | Résultat |
| --- | --- |
| Réponses réelles | sept villes captées le 07/11/2026 à 00 h 12 (heure de Paris) avec l'adresse exacte du firmware : Lyon, Longyearbyen (nuit polaire), Seattle (pluie), Singapour, Sydney, Katmandou (+5:45), Terre-Neuve (-3:30) ; `test/host/capturer_meteo_riche.py` |
| Valeurs attendues | calculées par `test/host/controle_python_riche.py` (module `decimal`, aucune ligne du noyau reprise) ; chaque test « identique à Python » compare une centaine à plus de trois cents nombres par ville, à la valeur près |
| Tests | `test_meteo_riche.cpp` 56 tests, `test_meteo_detail.cpp` 32 tests (dont sept comparant la STRUCTURE DU CONTRAT à Python), `test_meteo_tendance.cpp` 10 tests, `test_meteo_appareil.cpp` 25 tests (adresse, en-tête) : 0 échec, clang++ 23.1.5 x86-64 avec AddressSanitizer + UBSan |
| Mutations | 40 sur `meteo_riche.hpp`, 37 sur `meteo_detail.hpp`, 17 sur `meteo_tendance.hpp`, 39 déjà existantes sur le noyau météo de l'appareil : **toutes détectées** par le test visé (ou par une erreur de compilation pour les mutations de champs, attendues des tests « identique à Python ») ; la première passe de `meteo_detail.hpp` en avait laissé deux passer (une durée négative, un libellé jour/nuit identique) : un test a été ajouté et une mutation équivalente remplacée |
| Compilation du firmware | `.\tools\idf.ps1 build` : 0 erreur, aucun avertissement de compilation (le seul est l'avertissement CMake `ESP_ROM_ELF_DIR` déjà connu) ; `meridien.bin` 1 862 512 octets, SHA-256 `485d442a6d51f83b2975b919d1a4c42ebe6abc07d53c4c469e41b2411674d037`, 56 % de la partition libre ; compilé le 07/11/2026 depuis un arbre propre au commit `8f31fa0` (copie non suivie par git : `.cache/livrables/meridien-meteo-8f31fa0.bin` du worktree) |

**Limite** : le PC est en x86-64 ; ce qui passe ici ne prouve ni la mémoire, ni la pile, ni le temps de la carte.

### 6.4 Décisions prises à la place d'Axel (à valider)

- Tampon de réception 16 Ko en PSRAM (marge 3,5) ; état, zone de travail et cache de conversion en PSRAM, pas en RAM interne.
- 24 heures, 7 jours, 9 quarts d'heure (le dernier quart écoulé et les deux heures qui viennent) ; seuil de pluie 0,1 mm par quart d'heure ; « périmé » pour les quarts d'heure après 30 min.
- `jours` publié seulement si les sept jours ont tous leurs compléments (sinon `nb_jours` = 0 : l'écran ne dessine pas un jour sans vent ni ensoleillement) ; une section cassée est vidée, le reste est gardé.
- Cache v1 abandonné (pas de migration : une seule réponse le remplace) ; ensoleillement donné à la minute près.
- `meteo_detail_version()` repasse à 0 quand la zone change, pour que l'écran relise ; un numéro n'est jamais réutilisé.
- Tendance de pression : mesurée par l'appareil plutôt que demandée à Open-Meteo (`past_hours` décale tous les indices) ; fenêtre de 2 h 30 à 3 h 30 autour de 3 h, mesure la plus proche (à égalité la plus ancienne), aucune mise à l'échelle ; inconnue (NaN) pendant les ~3 premières heures après un démarrage ou un changement de zone, historique en RAM seulement (pas en cache NVS).

### 6.5 Non fait ou non vérifié

- **Rien n'a tourné sur une carte** : TLS, cache v2 après un redémarrage, ancien cache v1 ignoré, marge de pile, tailles sur le RISC-V (le PC annonce 1 312 et 2 088 octets), RAM interne libre avant/après : **non vérifiés**.
- Crédit : les données viennent d'Open-Meteo, licence CC BY 4.0 (https://open-meteo.com), usage non commercial, 600 appels/min, 5 000/h, 10 000/jour ; l'appareil en fait un toutes les 15 min.
- Fuseaux +5:30, +3:30, +10:30, jour polaire, réponse tronquée par le réseau : non captés.
- Tendance de pression : jamais vue sur une carte ; le délai d'établissement (~3 h après un démarrage) et la taille de l'historique sur le RISC-V ne sont pas vérifiés.
- Affichage à l'écran : relève de la session `ecran-visuel`.

**Vu sur** : PC d'Axel (Windows 11), llvm-mingw clang++ 23.1.5 x86-64 avec AddressSanitizer + UBSan, ESP-IDF 5.5.5 ; commits `f9537c5` (contrat), `ff793a6` (noyau riche), `ef49052` (conversion), `4489127` (appareil), `8f31fa0` (tendance de pression) de la branche `claude/meteo-appareil-f761d6` ; le 2026-11-07, de 00 h à 2 h (heure de Paris) ; réponses captées par le PC avec curl-équivalent Python, aucune requête faite par une carte ; **carte : aucune**.
