# Données des tests « météo et heure »

Tout ce dossier sert aux tests de `components/core/` (`test_meteo_openmeteo.cpp`, `test_heure_fr.cpp`). Les fichiers
`*.inc` sont **générés** par [`../controle_python.py`](../controle_python.py) : ne pas les modifier à la main
(`python test/host/controle_python.py --verifier` échoue si l'un d'eux n'est pas à jour).

## Réponses réelles d'Open-Meteo (capturées une fois, le 06/11/2026)

Requête = celle de la recherche 03 (§1) : `current=temperature_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,
wind_direction_10m,wind_gusts_10m,pressure_msl,relative_humidity_2m` + `hourly=temperature_2m,weather_code,
precipitation_probability,uv_index` avec `forecast_hours=48` + `daily=weather_code,temperature_2m_max,temperature_2m_min,
sunrise,sunset,uv_index_max,precipitation_probability_max` avec `forecast_days=7` + `timezone=auto&timeformat=unixtime`,
sur `https://api.open-meteo.com/v1/forecast`, Python `urllib`, HTTP 200, sans compression, sans clé.

| Fichier | Lieu demandé | Octets | `current.time` | Particularité |
|---|---|---|---|---|
| `openmeteo_paris.json` / `.inc` | 48,8566 N ; 2,3522 E (réponse : 48,853382 ; 2,355208, `Europe/Paris`) | 2 826 | 1793966400 (06/11/2026 12:00 UTC) | cas courant, 7 jours, 48 heures |
| `openmeteo_longyearbyen.json` / `.inc` | 78,2232 N ; 15,6267 E (réponse : `Arctic/Longyearbyen`) | 2 960 | 1793966400 | **nuit polaire** : pour les 7 jours, `sunrise == sunset ==` minuit local du jour (aucune valeur nulle), `is_day = 0` |

(La recherche avait mesuré 2 839 octets sur une autre position : la taille varie avec les chiffres.)
Une capture de Tromsø a été écartée (le soleil s'y lève encore début novembre : sans intérêt). Le **jour polaire**
(soleil de minuit) n'a pas été observé : comportement d'Open-Meteo à ce moment **non vérifié**.

Pièges rencontrés en capturant (réponses lues le 06/11/2026) :
- `start_date` plus de 16 jours dans le futur : HTTP 400, `"Parameter 'start_date' is out of allowed range from 2026-08-05 to 2026-11-21"`.
  On ne peut donc pas capturer une nuit polaire d'hiver à l'avance : il faut une latitude déjà dans la nuit (Svalbard).
- `forecast_hours` combiné à `start_date`/`end_date` : HTTP 400 sans détail dans mon premier essai. Cause **non isolée** (je n'ai pas
  testé `forecast_hours` seul avec ces dates) ; ne pas s'y fier.

## Contrôle indépendant

`controle_python.py` lit les `.json` avec `json.loads(parse_float=Decimal)` (module `decimal`, pas de flottant pour les
arrondis) et `zoneinfo` (paquet `tzdata` 2026.3, base IANA 2026c) et écrit :

| Fichier | Contenu | Utilisé par |
|---|---|---|
| `openmeteo_<lieu>.inc` | la réponse, octet pour octet, en littéral brut `R"OM(…)OM"` | les deux tests météo |
| `attendu_json_<lieu>.inc` | **chaque** valeur du JSON : 312 nombres (chemin, double exact en hexadécimal, « écrit comme un entier ? », valeur ×10 arrondie loin de zéro), 26 chaînes, 19 tableaux/objets (taille) | `json_*_nombres`, `json_*_textes_et_tailles` |
| `attendu_meteo_<lieu>.inc` | le `Releve` que `analyser()` doit produire (température, ressentie, vent, pression, humidité, probabilité horaire, 7 jours) | `releve_de_*_identique_a_python` |
| `attendu_proba_<lieu>.inc` | la probabilité de précipitations attendue pour des instants pris au début, au milieu, à la fin et juste avant chaque case horaire | `proba_*_par_heure` |
| `attendu_heure.inc` | 1 574 instants (13 zones, dont 85 « limites connues » de `Africa/Casablanca` rangés à part) → date, heure, jour de la semaine, décalage, heure d'été | `heure_*` (`test_heure_fr.cpp`) |
| `attendu_heure_lieu.inc` | 14 zones ; 408 colonnes d'une fenêtre « heure par heure » (étiquette de chaque colonne), 73 jours de prévision (jour de la semaine lu au midi local), 98 levers et couchers réels d'Open-Meteo écrits en heure locale, 104 lignes d'horloge (`hh:mm`, texte de l'heure, date, décalage) et 104 lignes d'heures d'alerte de Vigie (français et anglais) ; ADR-0012 | `test_heure_lieu.cpp`, `test_horloge.cpp`, `test_vigie_heure_lieu.cpp` |
| `attendu_soleil.inc` | 120 levers et couchers recalculés en Python (méthode NOAA, réécriture indépendante du noyau ; vingt lieux, une date par saison) et 63 jours réels d'Open-Meteo (neuf réponses) ; plus grand écart relevé : 8 s | `test_soleil.cpp` |

`attendu_heure_lieu.inc` et `attendu_soleil.inc` sont générés par [`../controle_python_fuseaux.py`](../controle_python_fuseaux.py) (valeurs de `zoneinfo`, tzdata 2026c, et `math` ; `--verifier` échoue si l'un d'eux n'est pas à jour). Les cinq zones ajoutées à `attendu_heure.inc` par l'ADR-0012 : Asia/Kathmandu (+5 h 45), Africa/Casablanca (limite connue de la table : sa chaîne POSIX donne `+01` en permanence, tzdata 2026c non ; les 85 instants où elles diffèrent sont dans la section `ATTENDU_HEURE_LIMITES`, où le test vérifie que l'écart est exactement celui-là), America/Caracas, Pacific/Auckland (le nouvel an local arrive en premier) et Pacific/Honolulu (en dernier). Les fenêtres de `attendu_heure_lieu.inc` comptent notamment New York (changement d'heure du 1er novembre 2026) et Sydney (4 octobre 2026), Tokyo, Kolkata et Katmandou (colonnes alignées sur l'heure locale et sur l'heure UTC), Auckland et Honolulu (nouvel an).

Zones de `attendu_heure.inc` : Europe/Paris, America/New_York, Asia/Kolkata (+5 h 30, sans heure d'été), Australia/Sydney
(hémisphère sud), Pacific/Chatham (+12 h 45 / +13 h 45), Europe/Dublin (heure d'été « négative » : le « dst » de la
chaîne POSIX tombe en hiver), America/Godthab (heures de changement négatives `M3.5.0/-1`), Australia/Lord_Howe
(heure d'été d'une demi-heure). Pour chaque changement d'heure de 2026 à 2028 (trouvé par dichotomie sur zoneinfo, pas
déduit de la chaîne POSIX) : 11 instants entre -24 h et +24 h autour du changement, dont **-1 s, 0 s et +1 s** ; plus une
grille (le 1er à 00:00:00 et le 15 à 12:34:56 de chaque mois, 2026-2028) et des dates de calendrier : 29/02/2000 (siècle
bissextile), 28/02 → 01/03/2100 (siècle non bissextile), 29/02/2028, 31/12/2026 → 01/01/2027, 19/01/2038 03:14:07 (2³¹ − 1).
Godthab n'a pas de date avant 2024 (sa règle a changé en 2023).

## Résultat consigné (06/11/2026)

- `python test/host/controle_python.py` : `paris: 2826 octets, 312 nombres, 26 chaînes, 19 conteneurs` ; `longyearbyen: 2960 octets,
  312 nombres, 26 chaînes, 19 conteneurs` ; `heure: 1093 instants, 8 zones` ; `--verifier` : « tout est à jour ».
- Première passe (06/11/2026, sans compilateur hôte) : le g++ croisé RISC-V d'ESP-IDF en `-fsyntax-only` (**mode « syntaxe »**) :
  les tests étaient seulement évalués à la compilation (`static_assert`), `main()` n'avait jamais tourné.
- Seconde passe (06/11/2026, clang++ 23 d'llvm-mingw installé ensuite, `_outils\env_hote.ps1`) : `python test/host/lancer.py`
  affiche **mode « complet (compilé + exécuté) »** ; 5 fichiers de test (97 fonctions de test), **0 échec**, avec le clang++ simple
  **et** avec `-Sanitizers` (AddressSanitizer + UBSan, `-fno-sanitize-recover=undefined` : la moindre erreur ferait échouer).
  Les détecteurs ont été vérifiés sur deux petits programmes fautifs (débordement de tas, dépassement d'entier signé) : ils signalent
  bien l'erreur et sortent avec le code 1. Clang accepte `-fconstexpr-steps=2000000000` (ajouté par `drapeaux_constexpr` du lanceur).
- `python test/host/lancer.py --mutations` : **71 mutations, 0 problème** avec le clang++ simple (les 18 de départ + 53 de
  `mutations_meteo_heure.py` : 19 sur `json_mini.hpp`, 18 sur `meteo_openmeteo.hpp`, 16 sur `heure_fr.hpp`) ; chacune est détectée par
  le test visé (ou, pour les trois sans test nommé, par une erreur de compilation). Même résultat qu'avec le g++ croisé. Trois mutations
  de la première passe n'étaient pas détectées ou l'étaient par une erreur de compilation sans rapport ; les tests ont été complétés
  (`nullx` = erreur de syntaxe, `objs[0]k`, nom de fuseau de 47/48 octets dans le cache).

**Limites à répéter** : (1) en `--mutations`, une mutation est « détectée » par un échec de **compilation** (`static_assert`) ; l'exécutable
du mutant n'est pas lancé, donc les 71 mutations ne prouvent pas ce que fait `main()`. (2) Les sanitizers couvrent ce que les tests
exécutent : la lecture des deux réponses réelles, les tables Python, les cas limites écrits à la main ; rien n'a été essayé sur d'autres
réponses d'Open-Meteo (par exemple le jour polaire, **non observé**). (3) Compilé pour Windows x86-64, pas pour le RISC-V de l'ESP32-P4
(`size_t` et les pointeurs y font 32 bits au lieu de 64 ; les tests passent aussi le g++ croisé en `-fsyntax-only`). Aucun test n'a tourné sur une carte.

## Réponses de la météo riche (07/11/2026)

Sept vraies réponses d'Open-Meteo, captées le 07/11/2026 à 00 h 12 (heure de Paris) avec l'adresse EXACTE du firmware (lue dans
`meteo_requete.hpp` par `test/host/capturer_meteo_riche.py`, jamais recopiée) : Lyon, Longyearbyen (nuit polaire), Seattle (pluie en
cours), Singapour, Sydney (heure d'été), Katmandou (UTC+5:45) et Terre-Neuve (UTC-3:30). Chacune fait 4 427 à 4 575 octets.

| Fichier | Contenu | Utilisé par |
|---|---|---|
| `openmeteo_riche_<lieu>.json` | la réponse telle que reçue | `controle_python_riche.py` |
| `openmeteo_riche_<lieu>.inc` | la même, en littéral brut C++ (copie octet par octet du `.json`, générée) | `test_meteo_riche.cpp`, `test_meteo_detail.cpp` |
| `attendu_riche_<lieu>.inc` | la météo que `analyser_riche()` doit produire, une valeur entière par ligne, rang et nom en commentaire (générée) | `*_identique_a_python` des deux tests |

Les valeurs attendues sont calculées par `python test/host/controle_python_riche.py` (module `decimal`, arrondi « moitié loin de zéro »),
**sans reprendre une ligne du noyau** ; le décalage d'une étiquette des cumuls y est écrit d'après la documentation d'Open-Meteo.
`--verifier` ne modifie rien et échoue si un `.inc` n'est pas à jour. Ne relancer `capturer_meteo_riche.py` que pour renouveler les
fixtures (il appelle le réseau) : les attendus sont alors à régénérer et les tests à relire (les horodatages changent).

Résultat consigné (07/11/2026, PC d'Axel, clang++ 23.1.5 avec AddressSanitizer + UBSan, mode « complet (compilé + exécuté) ») :
`test_meteo_riche.cpp` 56 tests, `test_meteo_detail.cpp` 32 tests, `test_meteo_tendance.cpp` 10 tests, `test_meteo_appareil.cpp` 25 tests, 0 échec ;
`--mutations` : 40 + 37 + 17 + 39 mutations, toutes détectées. **Limites** : sept villes, une seule avec de la pluie
(la seule qui départage les deux conventions d'étiquetage des cumuls) ; fuseaux +5:30, +3:30 et +10:30, jour polaire non captés ;
compilé pour x86-64, pas pour le RISC-V de la carte. Données Open-Meteo, CC BY 4.0 (https://open-meteo.com), usage non commercial.

## Réponses gzip (07/11/2026)

Deux vraies réponses d'Open-Meteo **telles qu'elles passent sur le fil** avec `Accept-Encoding: gzip`, captées le 07/11/2026 vers 09 h 48
(heure de Paris) par `test/host/capturer_gzip_meteo.py` (même adresse que le firmware, lue dans `meteo_requete.hpp`) : HTTP 200,
`Content-Encoding: gzip`, en-tête gzip `1f 8b 08 00 00 00 00 00 00 03` (aucun champ facultatif, MTIME 0, système 3).

| Fichier | Lieu | Sur le fil | Décompressé |
|---|---|---|---|
| `openmeteo_gzip_lyon.gz` / `.inc` | 45,74906 N ; 4,84789 E | 1 498 octets | 4 443 octets |
| `openmeteo_gzip_sydney.gz` / `.inc` | 33,86785 S ; 151,20732 E | 1 626 octets | 4 575 octets |

`gzip_cas.inc` : douze fichiers gzip FABRIQUÉS par `test/host/controle_python_gzip.py` (flux DEFLATE brut de `zlib` + en-tête écrit à la main,
chacun relu par `gzip.decompress`), un par particularité de la RFC 1952 : aucun champ, FTEXT, FNAME, FEXTRA (plein, vide, de 300 octets),
FCOMMENT, FHCRC, les quatre ensemble, flux vide, 70 000 octets, deux membres collés. Pour chacun : le décalage du flux, le CRC32 et la taille
du pied, calculés par `zlib.crc32` et `len()`, jamais par le noyau. Tous les `.inc` sont générés (`python test/host/controle_python_gzip.py`,
`--verifier` ne modifie rien et échoue si l'un d'eux n'est pas à jour) ; ne relancer `capturer_gzip_meteo.py` que pour renouveler les
captures (il appelle le réseau), puis régénérer les `.inc`. Données Open-Meteo, CC BY 4.0 (https://open-meteo.com), usage non commercial.

Utilisés par `test_gzip.cpp` (noyau `ecran/gzip.hpp`). Résultat consigné (07/11/2026, PC d'Axel, clang++ 23.1.5 avec AddressSanitizer + UBSan,
mode « complet ») : 23 tests, 0 échec ; `--mutations` : 73 mutations, toutes détectées. **Limite** : ces tests prouvent l'enveloppe gzip
(en-tête, pied, CRC32, décision sur `Content-Encoding`), **pas** la décompression, que fait `tinfl_decompress` de la ROM de l'ESP32-P4 et
qui n'a jamais été exécutée (compilée seulement, aucune carte).

## Réponses de MET Norway et du NWS (07/11/2026)

Les vraies réponses des deux services **sans clé** que lit directement l'appareil (ADR-0014), captées le 07/11/2026 entre 13 h 56 et
13 h 57 (heure de Paris, 12 h 56 à 12 h 57 UTC) par `test/host/capturer_fournisseurs_meteo.py`, avec les adresses et le `User-Agent` EXACTS
du firmware (lus dans `meteo_requete_fournisseurs.hpp`, jamais recopiés) et les en-têtes de `main/http_commun.cpp` (`Accept` du service,
`Accept-Encoding: gzip`). Les fichiers `.json` sont la réponse **décompressée, octet pour octet**.

| Fichier | Contenu | Sur le fil → décompressé |
|---|---|---|
| `metno_<lieu>.json` | Locationforecast 2.0 « complete » : Oslo, Reykjavik, Helsinki, Tromsø, Longyearbyen (nuit polaire), Londres, Tokyo, Sydney | 4 549 à 7 515 → 60 818 à 90 783 octets |
| `nws_<lieu>_points.json` | `/points/{lat},{lon}` : la maille (bureau, x, y) et le fuseau : New York, Miami, Seattle, Anchorage, Honolulu, San Juan | 1 006 à 1 014 → 3 913 à 3 937 octets |
| `nws_<lieu>_grille.json` | `/gridpoints/{bureau}/{x},{y}` : les couches de prévision de la maille | 5 714 à 11 209 → 120 347 à 240 402 octets |
| `nws_lyon_points.json` | `/points` pour Lyon, **hors couverture** : HTTP 404 `application/problem+json`, non compressé | 301 octets |
| `*.inc` | les mêmes réponses en littéral brut C++ (copie octet par octet du `.json`, générée) | `test_meteo_metno.cpp`, `test_meteo_nws.cpp`, `test_requete_fournisseurs.cpp` |
| `attendu_metno_<lieu>.inc`, `attendu_nws_<lieu>.inc` | ce que `analyser_metno()` et `analyser_nws()` doivent produire, une valeur entière par ligne, rang et nom en commentaire (généré) | les `*_identique_a_python` |
| `lieux_metno.inc`, `lieux_nws.inc` | pour chaque capture : nom, instant de réception, position en e5, fuseau (et, pour le NWS, bureau et maille lus dans `/points`) (générés) | les `*_identique_a_python` et `test_requete_fournisseurs.cpp` |
| `fournisseurs_capture.json` | pour chaque appel : adresse, statut HTTP, octets sur le fil et décompressés, `Expires`, `Last-Modified`, `Cache-Control`, `Date`, `Age`, instant de réception ; plus l'essai de requête conditionnelle (`If-Modified-Since` → 304 chez MET Norway) | `docs/mesures/fournisseurs-meteo.md` |
| `soleil_metno_reference.json` | 79 réponses du produit « Sunrise 3.0 » de MET Norway (lever et coucher du soleil), captées le 07/11/2026 par `capturer_soleil_metno.py` pour 24 lieux et autant de jours choisis, des cercles polaires à l'équateur, changements d'heure compris | `attendu_soleil_metno.inc` (généré ; même numérotation des cas ; relu par `test_soleil.cpp`) |
| `attendu_soleil_almanac.inc` | lever et coucher recalculés en Python par un AUTRE algorithme (« Almanac for Computers » de l'US Naval Observatory) ; numérotation des cas de `soleil::Situation` (0 normal, 1 jour polaire, 2 nuit polaire) | `test_soleil.cpp` |
| `attendu_symboles_metno.inc` | les 41 symboles de la légende officielle de MET Norway, chacun avec ses variantes `_day`, `_night`, `_polartwilight`, et le code WMO attendu (règle écrite dans `lecture_fournisseurs.py`, d'après l'ADR-0014) | `les_41_symboles_officiels_et_leurs_variantes` |

Les valeurs attendues sont calculées par `python test/host/controle_python_fournisseurs.py` : les réponses sont lues avec
`json.loads(parse_float=Decimal)`, les instants et les jours locaux avec `datetime` et `zoneinfo` (la base de fuseaux de Python, pas celle de
`fuseaux.hpp`), le soleil avec un autre algorithme que le noyau, **sans reprendre une ligne de C++ du noyau** ; les règles de lecture (quel champ
devient quelle valeur, ce qui reste « inconnu ») sont écrites à partir de la documentation de chaque service. `--verifier` ne modifie rien et
échoue si un `.inc` n'est pas à jour. Ne relancer les deux scripts de capture que pour renouveler les fixtures (ils appellent le réseau, une
seconde entre deux appels, un seul appel par adresse) : les attendus sont alors à régénérer et les tests à relire (les horodatages changent).

**Limites.** Huit lieux pour MET Norway, six pour le NWS : une seule mesure par lieu, un seul jour (07/11/2026, jours courts en
hémisphère nord) ; ni Guam, ni les îles Vierges, ni les Mariannes du Nord (le NWS y répond, constaté en direct, mais sans fixture :
l'appareil ne les propose pas encore), et les Samoa américaines n'ont pas de grille (`gridId` nul, refus propre). Des cartes réelles : aucune.
Données MET Norway : CC BY 4.0 et NLOD 2.0 (https://api.met.no/license_data.html) ; données NWS : domaine public (https://www.weather.gov/).

Résultat consigné (07/11/2026, PC d'Axel, clang++ 23.1.5 avec AddressSanitizer + UBSan, mode « complet (compilé + exécuté) », branche
`claude/meteo-fournisseurs-e3cd8b`) : `test_meteo_metno.cpp` 32 tests, `test_meteo_nws.cpp` 45, `test_requete_fournisseurs.cpp` 11,
12 tests du calcul du soleil de l'époque (ce second calcul a été remplacé le 07/11/2026 par celui d'ADR-0012 ; ses preuves sont dans `test_soleil.cpp`), 0 échec (exécution complète du dépôt : 51 fichiers, 1 009 tests, 0 échec) ; `python test/host/controle_python_fournisseurs.py
--verifier` : « tout est à jour » (14 captures, 449 valeurs attendues chacune) ; `--mutations` : 19 (MET Norway) + 17 (NWS) + 8 (requêtes) + 8 (soleil de l'époque)
mutations, toutes détectées par le test visé. **Limites** : mesuré sur x86-64, pas sur le RISC-V de la carte ; aucune carte.
