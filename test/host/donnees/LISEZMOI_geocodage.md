# Captures du géocodage d'Open-Meteo (tests de `geocodage.hpp`)

Les fichiers `geocodage_*.json` sont des **réponses réelles** du service, jamais éditées à la main. Les `geocodage_*.inc` en sont
le littéral C++ brut (`R"OM(...)OM"`), inclus par `test_geocodage.cpp`. `attendu_geocodage.inc` donne ce que l'analyseur doit lire
dans chaque capture ; il est calculé par Python (`json.loads`), donc **indépendamment** du noyau C++ qu'il vérifie.

> English summary: `geocodage_*.json` are unedited real responses of Open-Meteo's geocoding API, captured on 2026-11-06 (about
> 23:57, local time). `.inc` files are generated raw string literals, `attendu_geocodage.inc` is the expected output computed by
> Python's `json` module. Regenerate with `python test/host/capturer_geocodage.py`; re-capture (network) with `--capturer`.

**Vu sur** : captures faites le 06/11/2026 vers 23 h 57 (heure locale) depuis le PC d'Axel, par `capturer_geocodage.py --capturer`
sur l'adresse `https://geocoding-api.open-meteo.com/v1/search?name=…&count=8&language=…&format=json`. Rien n'a été capturé sur une
carte : ce sont des réponses du service, lues par le noyau sur le PC (tests hôte compilés et exécutés).

## Pourquoi ces sept cas

| Fichier | Recherche (langue) | Ce qu'il fait vérifier |
| --- | --- | --- |
| `geocodage_marseille` | « Marseille » (fr) | Le cas de l'essai du matin. Huit résultats, trois fuseaux (France, Haïti, États-Unis). Une région au libellé inattendu (« Région Provence-Alpes-Côte d'Azur ») : le libellé affiché est celui du service, rien n'est « corrigé ». Le champ `population` manque sur certains lieux. |
| `geocodage_saint_etienne` | « Saint-Étienne » (fr) | Accents et trait d'union dans la recherche (pourcentage UTF-8) et dans les noms de lieux. |
| `geocodage_springfield` | « Springfield » (en) | Huit résultats (le maximum demandé, `count=8`), tous aux États-Unis, dans huit États différents : plusieurs lieux de même nom, seule la région les distingue dans le libellé. |
| `geocodage_zurich` | « Zürich » (de) | Un tréma dans la recherche ; quatre pays (Suisse, Pays-Bas, États-Unis, Canada) et sept fuseaux ; région en allemand (« Kanton Zürich ») ; `population` absent sur certains. |
| `geocodage_istanbul` | « İstanbul » (tr) | Un « İ » majuscule pointé (deux octets) dans la recherche et dans les régions ; six résultats seulement ; **altitude `-9999.0` = inconnue** sur « Istanbul Old Town » : le noyau ne la garde pas (règle : altitude gardée entre −500 m et 9 000 m) ; fuseaux `Europe/Istanbul` et `Europe/Athens`. |
| `geocodage_paris` | « Paris » (fr) | Un lieu très connu : le premier résultat est en France (Île-de-France), les sept autres sont des homonymes américains (quatre fuseaux en tout). |
| `geocodage_aucun_resultat` | « xqzvbnmwkj » (fr) | **Aucun résultat** : le service ne renvoie alors **aucune clé `results`** (33 octets, seulement `generationtime_ms`). C'est une réponse valide, pas une erreur : zéro lieu. |

## Commandes

```powershell
python test\host\capturer_geocodage.py --capturer   # RÉSEAU : refait les .json depuis Open-Meteo, puis les .inc
python test\host\capturer_geocodage.py              # sans réseau : régénère les .inc depuis les .json
python test\host\capturer_geocodage.py --verifier   # sans réseau : code 1 si un .inc n'est pas à jour
python test\host\lancer.py --seulement test_geocodage
```

## Limites (à lire avant de s'y fier)

- Ce sont des photographies d'un jour : le service peut changer l'ordre des résultats, les noms ou les champs. Les tests ne
  disent pas « le service renverra ceci demain », ils disent « l'analyseur lit correctement ce que le service a renvoyé ce jour-là ».
- Une nouvelle capture (`--capturer`) peut faire échouer `attendu_geocodage.inc` ou les tests de libellé : c'est voulu, il faut alors
  relire ce qui a changé avant d'accepter.
- Les captures ne couvrent ni une réponse tronquée, ni du JSON invalide, ni un lieu aux champs manquants : ces cas sont dans
  `test_geocodage.cpp` sur des textes écrits à la main, et le test le dit. Une erreur HTTP ou réseau ne passe pas par l'analyseur :
  elle est gérée côté appareil (`main/geocodage.cpp`), pas encore essayée sur une carte.
- Rien ici ne prouve le téléchargement lui-même (TLS, mémoire, délais) : il n'a pas été essayé sur une carte.
