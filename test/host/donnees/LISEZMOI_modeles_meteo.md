# Données des tests « sources météo par pays »

Ce dossier sert à `test_fournisseurs_meteo.cpp` et `test_meteo_appareil.cpp` (table des sources de la météo,
`components/core/include/ecran/fournisseurs_meteo.hpp`). Contexte et décision : [ADR-0010](../../../docs/decisions/0010-source-meteo-par-pays-et-client-reseau-commun.md) ;
ce que chaque modèle rend : [docs/mesures/meteo-modeles.md](../../../docs/mesures/meteo-modeles.md).

## Les réponses réelles (capturées le 07/11/2026 depuis le PC d'Axel)

`openmeteo_modele_<id>_<ville>.json` = la réponse RÉELLE d'Open-Meteo à la **requête complète de l'écran** (courant complet,
24 heures, 7 jours, UV, pluie par quarts d'heure ; adresse lue dans `meteo_requete.hpp` par le script de capture, jamais
recopiée) avec `&models=<valeur>`, pour une ville DANS le pays du modèle. 29 fichiers : « Automatique » (Lyon, sans `models=`) ;
un fichier par source de la table pour son pays de référence ; un de plus par pays supplémentaire que la table annonce
(MET Norway : 6 villes, KNMI : 3, Météo-France : Lyon et Saint-Denis de La Réunion) ; NOAA GFS : 7 villes (Europe, Amérique du Nord et du Sud, Asie, Océanie, Afrique).
Taille : 4 410 à 4 684 octets (texte décompressé). Instant de réception retenu pour tous : `1794040300` (UNIX).

| Produit | Par | Rôle |
|---|---|---|
| `openmeteo_modele_*.json` | `capturer_modeles_meteo.py` (appelle le réseau : à lancer à la main) | la donnée réelle |
| `openmeteo_modele_*.inc`, `modeles_liste.inc` | `controle_python_modeles.py` (**générés, ne pas modifier à la main**) | la réponse en littéral C++, et une ligne par réponse : source, ville, pays, instant, limites, verdict attendu du lecteur |

`python test/host/controle_python_modeles.py --verifier` échoue si un `.inc` n'est pas à jour. Les limites (pas d'UV, pas de
rafales, pas de probabilité de pluie, moins de sept jours, pas d'ensoleillement) et le verdict sont calculés **en Python**, en
cherchant les `null` du JSON, sans reprendre une ligne du noyau C++ ; le test C++ compare ensuite le vrai lecteur du firmware
(`analyser_riche`) et la table à ce calcul indépendant.

## Ce que ces données ne couvrent pas

- Aucune réponse **hors du pays** d'un modèle n'est gardée : le comportement d'Open-Meteo pour un modèle national demandé à
  l'étranger est **non vérifié** ici.
- `kma_seamless` et `bom_access_global` (exclus d'office) n'ont pas de fichier ici.
- Une réponse change avec l'heure de la requête et avec les évolutions d'Open-Meteo : refaire une capture peut changer les
  limites d'un modèle ; le test le dira (la table déclare des limites exactes, pas approximatives).

**Vu sur** : PC (Windows 11), 07/11/2026, aucune carte.
