# Données des tests « surf »

Ce dossier sert à `test_surf_reponses.cpp` (noyau pur `components/core/include/ecran/surf_*.hpp`). Contexte, barème et limites :
[ADR-0013](../../../docs/decisions/0013-surf-donnees-note-et-limites.md) ; ce qui a été constaté et ce qui ne l'est pas :
[docs/mesures/surf.md](../../../docs/mesures/surf.md).

## Les réponses réelles (capturées le 07/11/2026 vers 12 h 51 UTC depuis le PC d'Axel)

Quinze positions du monde, **deux réponses chacune**, octet pour octet, avec les adresses exactes du firmware (lues dans
`surf_requete.hpp` par le script de capture, jamais recopiées) :

- `surf_marine_<id>.json` : réponse de l'API **Marine** d'Open-Meteo (onze variables horaires, 54 heures : 6 passées, 48 à venir) ;
- `surf_vent_<id>.json` : réponse de l'API de **prévision** (vent, direction et rafales à 10 m, mêmes 54 heures).

Les quinze positions couvrent les deux hémisphères, des décalages de −10 h à +11 h, des marées de 68 cm à 9,9 m de marnage et quatre
cas limites : **Paris** (hors mer : les onze variables sont `null`), **lac Michigan** (des vagues, mais ni température de l'eau ni
niveau de mer), **Baltique** et **Méditerranée** (mers presque sans marée : 20 et 27 cm de variation sur 54 h, sous le seuil de
30 cm du noyau). La liste, avec l'orientation de plage prise pour chaque lieu, est dans `surf_manifeste.json` ; le script Python
la lit et en tire `surf_cas.inc`, que le test C++ compare ensuite au vrai code du firmware.

**L'orientation de plage de chaque lieu est un choix du test, tiré d'une carte, jamais vérifié sur le terrain** : elle sert à exercer
le calcul, pas à dire comment un spot réel se comporte.

| Produit | Par | Rôle |
|---|---|---|
| `surf_marine_*.json`, `surf_vent_*.json`, `surf_manifeste.json` | `capturer_surf.py` (appelle le réseau : à lancer à la main, pas dans le contrôle) | la donnée réelle |
| `surf_cas.inc` | `controle_python_surf.py` (**généré, ne pas modifier à la main**) | une ligne par lieu : ce que la lecture indépendante en Python attend (heures, extrema de marée, notes, créneaux) |

`python test/host/controle_python_surf.py --verifier` échoue si `surf_cas.inc` n'est pas à jour ;
`python test/host/controle_python_surf.py --expliquer <id> <rang>` détaille un calcul heure par heure.

## Ce que la seconde lecture en Python prouve, et ce qu'elle ne prouve pas

- **Indépendante** du noyau C++ : lecture du JSON en décimal (`decimal`, pas de flottant binaire), `math.cos` pour le cosinus,
  recherche des extrema de marée par une boucle écrite à part. Un défaut d'arrondi, de signe ou de lecture du JSON dans le C++ y
  est vu.
- **Pas indépendante pour le barème et le créneau** : ces deux règles ont été récrites en Python par la même personne, à partir du
  même texte de l'ADR. Si cette lecture du texte est fausse, les deux versions le sont de la même façon. Ce que le test garantit
  est que le code C++ **fait ce que l'ADR écrit**, pas que l'ADR décrit bien un jour de surf.
- Une seule journée de capture : aucune mer en tempête, aucun vent de 80 km/h, aucune marée de vive-eau n'y figure. Ces cas sont
  couverts par des séries fabriquées à la main dans les autres tests (`test_surf_note.cpp`, `test_surf_maree.cpp`), pas par des
  données réelles.
- Une réponse change avec l'heure de la requête et avec les évolutions d'Open-Meteo : refaire une capture change les valeurs ;
  il faut alors relancer `controle_python_surf.py` (et relire l'écart avant de le valider).

**Vu sur** : PC (Windows 11), captures du 07/11/2026 12:51 UTC, aucune carte.

## Licence des données

Les réponses sont des données d'**Open-Meteo**, sous licence **CC BY 4.0** (attribution : [THIRD_PARTY.md](../../../THIRD_PARTY.md)
et [components/core/ATTRIBUTION.md](../../../components/core/ATTRIBUTION.md)). Elles sont gardées telles quelles, sans modification.
