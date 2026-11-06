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
| `attendu_heure.inc` | 1 093 instants (8 zones) → date, heure, jour de la semaine, décalage, heure d'été | `heure_*` (8 tests) |

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
