# Météo sur l'appareil — Open-Meteo par HTTPS, cache NVS, indicateur « périmé »

> **Summary (EN)** — Weather on the device, written and compiled on 2026-11-06, **not yet run on a board**. A FreeRTOS task waits for the IP address and the SNTP time, then asks `api.open-meteo.com` over HTTPS (server checked with ESP-IDF's certificate bundle) every 15 min, with a spaced retry after a failure; the answer is read by the pure core, kept in the NVS and published through `meteo_etat_lire()` (`main/meteo_etat.h`), together with a "stale" flag (`perime`). The place is `zone_lire()` / `zone_ecrire()` (`main/meteo_zone.h`), Lyon by default, in the NVS.
> - Checked on the PC only: 25 core tests (294 tests in the whole suite, AddressSanitizer + UBSan, all pass), 39 mutations out of 39 detected (detection = compilation failure), and the request built by the core was accepted by the real API (HTTP 200, 2 837 bytes, read by `analyser`). ESP-IDF 5.5.5 build: 1 587 136 bytes, no warning.
> - **Not seen on the board**: nothing in the "Sur la carte" part below has been observed. Pending: first answer in the serial log, comparison with the real weather, restart from the cache, offline behaviour, memory before/after.
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

`perime` est vrai quand : la réponse a plus de **2 h** ; **ou** 3 essais ratés de suite **et** une réponse de plus de **30 min** (deux rafraîchissements manqués : une box redémarrée il y a 4 minutes ne déclenche rien) ; **ou** l'heure de l'appareil n'est pas réglée ou a reculé de plus de 5 min depuis la réponse (on ne sait pas juger : jamais « à jour » sur une supposition). Conséquence à connaître : au démarrage, la météo relue du cache est marquée périmée jusqu'à la première réponse SNTP, puis redevient fraîche si elle a moins de 2 h. `valide` ne dépend pas de la fraîcheur : une météo périmée reste lisible, y compris au-delà de 6 h (le noyau appelle cet état « très périmé » et recommande de ne plus la présenter comme actuelle : **non appliqué**, le contrat n'a que `perime`).

Jour des min/max : celui qui contient l'heure actuelle ; à défaut (cache de plus de 7 jours) celui de la mesure ; sans prévision, `minmax_connus` est faux et min et max valent la température du moment.

## 3. Critères de « fait » de la tâche

| Critère | État | Preuve |
| --- | --- | --- |
| `meteo_etat.h` et `meteo_zone.h` livrés aux autres sessions | **constaté** (dépôt) | commit `ab05293` |
| Tests du noyau pur sur le PC | **constaté** | 25 tests de `test_meteo_appareil.cpp` ; suite complète 294 tests, ASan + UBSan, au commit `4e390db` |
| Mutations du noyau détectées | **constaté, avec réserve** | 39 sur 39 ; « détectée » = échec de compilation (`static_assert`), l'exécutable du mutant n'est pas lancé ; une mutation écartée (voir [depannage.md](../depannage.md#a-mutation-keeps-the-compiler-busy-for-minutes)) |
| L'adresse construite par le noyau est acceptée par l'API réelle et la réponse est lue | **constaté sur le PC** | voir la section 4 |
| Compilation ESP-IDF 5.5.5 | **constaté** | `.\tools\idf.ps1 build` : 0 erreur, 0 avertissement, binaire de 1 587 136 octets |
| Sur la carte : `meteo_etat_lire` rend la météo réelle de Lyon, comparée à Open-Meteo au même moment | **non fait** | attend l'accord de flash d'Axel |
| Sur la carte : Wi-Fi coupé ou requête fausse → `perime` après le délai, dernière météo conservée | **non fait** | demande Axel à la box (30 min au moins avant que `perime` change) |
| Sur la carte : redémarrage → l'état revient du cache | **non fait** | |
| Sur la carte : mémoire libre avant/après une requête | **non fait** | les lignes sont dans le journal, aucune valeur lue |

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

## 5. Sur la carte — à faire, rien observé

Essai prévu après l'accord d'Axel pour le binaire (flash de l'application seule à `0x10000`, port retrouvé par la MAC, verrou `tab5-st7121`) :

1. Journal série du démarrage : `cache relu` ou `pas de cache météo`, puis `contrat (au démarrage…)`.
2. Dès l'IP et l'heure : `requête Open-Meteo pour « Lyon »`, les trois lignes de mémoire libre, `METEO Lyon : …` ; comparer à l'API appelée depuis le PC au même moment (même intervalle `current`).
3. Redémarrage logiciel : `cache relu`, `valide=1`, `perime=1` tant que SNTP n'a rien reçu, puis `perime=0`.
4. Échec : avec Axel, couper l'accès de la carte à Internet à la box ; `essai raté`, délais de 15 s à 15 min, `perime` après 30 min et 3 échecs ; la dernière météo reste lisible.

Vu sur : à remplir (carte, écran, MAC lue sur la puce et non écrite ici, commit, date).
