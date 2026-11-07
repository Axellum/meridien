# Modèles météo nationaux d'Open-Meteo : ce que chacun rend, ce que le lecteur en garde — PC seulement, aucune carte

> **Summary (EN)** — For each national model reachable through Open-Meteo's `models=` parameter, the **real** request of the screen
> (same address as the device, plus `&models=<value>`) was captured on 2026-11-07 in one or several cities of the model's country
> (29 real answers kept in `test/host/donnees/`) and passed through the **same reader as the firmware** (`analyser_riche`).
> - **Measured (PC only)**: only **MET Norway** (6 cities), **KNMI** (3 cities), **NOAA GFS** (7 cities: Europe, North and South America, Asia, Oceania, Africa) and the
>   default « Automatic » come back complete (current, 24 hours, 7 days, quarter-hours) and are accepted by the reader. The other
>   ten national models and ECMWF IFS are refused by the strict reader because a variable is empty: **UV index** (all of them
>   except MET Norway, KNMI and NOAA GFS), **wind gusts** and **rain probability** (JMA, ItaliaMeteo ARPAE, Météo-France), or
>   **fewer than seven days** (Météo-France, MeteoSwiss, ItaliaMeteo ARPAE). The menu therefore offers « Automatic » plus those
>   three models; the others are in the table with their limit and are **not offered**.
> - **Decision for the owner**: relax the reader (show the screen with the missing variable hidden) to offer more of them.
> - **Not verified**: nothing here ran on a board; countries other than the cities captured; `kma_seamless` and `bom_access_global`
>   (excluded by the brief: empty values, answers not kept).

**Vu sur** : PC seulement (Windows 11, clang++ 23.1.5, `_outils\env_hote.ps1`), aucune carte ; réponses réelles capturées le 07/11/2026 depuis le PC d'Axel, instant de réception 1 794 040 300 (UNIX) ; branche `claude/source-meteo-client-reseau-29c0af`, base `9c89ad5`. L'adresse exacte de l'appareil est LUE dans `meteo_requete.hpp` par le script de capture, jamais recopiée. Aucune MAC ni adresse IP ici.

Règle de lecture : « complète » = aucune valeur `null` dans la section ; « vide » = la variable est `null` partout ; « trous » = `null` pour certains jours seulement ; « lecteur » = ce que `analyser_riche()` fait de la réponse. Ce qui n'est pas marqué « mesuré » est une lecture de code ; ce qui n'a pas été observé est écrit **non vérifié**.

## 1. Question

Quels modèles nationaux peut-on demander à Open-Meteo (`models=`) sans que l'écran de météo devienne vide, et dans quels pays ?

## 2. Mesuré sur le PC : une réponse réelle par ville et par modèle

Chaque ligne = la requête complète de l'écran (courant complet, 24 heures, 7 jours, quarts d'heure de pluie) avec `&models=<valeur>`. Taille : texte décompressé, 4 410 à 4 684 octets pour toutes.

| Source (`id`) | `models=` | Villes (pays) | Courant | 24 heures | 7 jours | Quarts d'heure | Lecteur du firmware | Proposée |
|---|---|---|---|---|---|---|---|---|
| Automatique (`auto`) | *(rien)* | Lyon (FR) | complète | complète | complète | complète | **acceptée** (24 h, 7 j, 9 quarts) | oui |
| MET Norway (`metno`) | `metno_seamless` | Oslo (NO), Stockholm (SE), Helsinki (FI), Copenhague (DK), Reykjavik (IS), Tallinn (EE) | complète | complète | complète | complète | **acceptée** (6 villes) | **oui** |
| KNMI (`knmi`) | `knmi_seamless` | Amsterdam (NL), Bruxelles (BE), Luxembourg (LU) | complète | complète | complète | complète | **acceptée** (3 villes) | **oui** |
| NOAA GFS (`noaa`) | `gfs_seamless` | Lyon (FR), New York (US), Ottawa (CA), Tokyo (JP), Sydney (AU), São Paulo (BR), Le Cap (ZA) | complète | complète | complète | complète | **acceptée** (7 villes) | **oui, tous pays** |
| Météo-France (`meteofrance`) | `meteofrance_seamless` | Lyon (FR), Saint-Denis de La Réunion (RE) | complète | vide : probabilité de pluie, UV | vide : UV max, probabilité max ; **3 jours sur 7 vides** pour toutes les autres variables | complète | refusée : `daily.uv_index_max` | non |
| DWD (`dwd`) | `icon_seamless` | Berlin (DE) | complète | vide : UV | vide : UV max | complète | refusée : `daily.uv_index_max` | non |
| Met Office (`metoffice`) | `ukmo_seamless` | Londres (GB) | complète | complète | trous : ensoleillement 5/7, UV max 5/7 | complète | refusée : `daily.uv_index_max` | non |
| DMI (`dmi`) | `dmi_seamless` | Copenhague (DK) | complète | vide : UV | vide : UV max | complète | refusée : `daily.uv_index_max` | non |
| MeteoSwiss (`meteosuisse`) | `meteoswiss_icon_seamless` | Berne (CH) | complète | vide : UV | vide : UV max ; **2 jours sur 7 vides** pour les autres variables | complète | refusée : `daily.uv_index_max` | non |
| JMA (`jma`) | `jma_seamless` | Tokyo (JP) | **vide : rafales** | vide : probabilité de pluie, rafales, UV | vide : UV max, probabilité max, rafales max ; ensoleillement 4/7 | complète | refusée : `current.wind_gusts_10m` | non |
| ECCC (`gem`) | `gem_seamless` | Ottawa (CA) | complète | vide : UV | vide : UV max | complète | refusée : `daily.uv_index_max` | non |
| GeoSphere Austria (`geosphere`) | `geosphere_seamless` | Vienne (AT) | complète | vide : UV | vide : UV max | complète | refusée : `daily.uv_index_max` | non |
| ItaliaMeteo ARPAE (`arpae`) | `italia_meteo_arpae_icon_2i` | Rome (IT) | complète | vide : probabilité de pluie, UV | vide : UV max, probabilité max ; **4 jours sur 7 vides** pour les autres variables | complète | refusée : `daily.uv_index_max` | non |
| CHMI (`chmi`) | `chmi_aladin_seamless` | Prague (CZ) | complète | vide : UV | vide : UV max | complète | refusée : `daily.uv_index_max` | non |
| ECMWF IFS (`ecmwf`) | `ecmwf_ifs025` | Istanbul (TR) | complète | vide : UV | vide : UV max | complète | refusée : `daily.uv_index_max` | non |

Le lecteur refuse **toute la réponse** dès qu'une de ces valeurs manque (`daily.uv_index_max`, `current.wind_gusts_10m`, `daily.precipitation_probability_max`, code de ciel d'un des sept jours) : il ne montre jamais une valeur inventée. C'est pourquoi un modèle sans UV n'est pas « un peu moins bien », il vide l'écran de météo. Chaque limite de la colonne « Lecteur » est aussi dans la table (`limites` de `fournisseurs_meteo.hpp`), et `test_fournisseurs_meteo.cpp` relit chaque réponse réelle pour vérifier que les limites déclarées sont exactement celles de la réponse (si Open-Meteo change un modèle, une capture refaite fait échouer le test).

Écartés par le brief sans nouvelle capture conservée ici : `kma_seamless` (Corée) et `bom_access_global` (Australie), valeurs vides relevées par la coordination le 07/11/2026 ; **non vérifié** par cette session.

Jamais proposés, jamais dans la table : `icon_global`, `icon_eu`, `icon_d2` (un test refuse qu'ils entrent).

## 3. Ce que ça donne dans le menu

La liste d'une ville = « Automatique » en tête, puis les services de son pays, puis les modèles mondiaux, **seulement ceux que le lecteur accepte aujourd'hui** :

| Pays de la ville | Liste proposée |
|---|---|
| FR, DE, GB, CH, JP, CA, AT, IT, CZ, tout autre pays | Automatique, NOAA GFS |
| NO, SE, FI, DK, IS, EE | Automatique, MET Norway, NOAA GFS |
| NL, BE, LU | Automatique, KNMI, NOAA GFS |
| pays inconnu (ville gardée avant ce réglage) | Automatique, NOAA GFS |

Le menu est donc **court tant que le lecteur reste strict** : à Lyon, Automatique et NOAA GFS. Ce n'est pas un oubli : Météo-France, le DWD et le Met Office ne rendent pas l'indice UV d'après la réponse réelle.

## 4. Pour proposer un modèle de plus

1. Décider (Axel) que l'écran montre une météo **sans** la variable manquante : assouplir `meteo_riche.hpp` / `meteo_openmeteo.hpp` pour cacher la zone concernée (UV, rafales, probabilité de pluie) au lieu de refuser la réponse.
2. Ajouter le bit correspondant à `LECTEUR_TOLERE` dans `fournisseurs_meteo.hpp` **dans le même changement** (les tests le font varier pour montrer ce qui apparaîtrait).
3. Refaire la capture (`python test/host/capturer_modeles_meteo.py`, appelle le réseau : à lancer à la main) et `python test/host/controle_python_modeles.py` pour régénérer les données de test.

## 5. Non vérifié

- Rien de tout cela n'a tourné sur une carte : la requête réelle avec `models=` depuis l'appareil, la lecture de la réponse sur la carte.
- Les pays couverts : seuls ceux des villes de la colonne « Villes » ont été capturés. KNMI rend aussi la Belgique et le Luxembourg (mesuré) ; son comportement ailleurs, et celui de MET Norway hors des six pays, ne sont pas dans la table (`pays`) et n'ont pas été essayés ici. Le script de capture a un mode `--complements` pour essayer des villes hors du pays d'un modèle ; aucune réponse hors pays n'est gardée dans les données de test : **non vérifié**.
- La stabilité dans le temps : les modèles et leurs variables peuvent changer côté Open-Meteo sans préavis.
