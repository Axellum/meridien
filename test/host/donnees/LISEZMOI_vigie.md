# Réponses réelles de Vigie (alertes officielles)

Captures de `GET /v1/vigilances` faites sur l'instance publique `https://vigie.auxfilsdesidees.fr`, **sans modification**. Elles servent d'échantillons aux tests du noyau (`components/core/include/ecran/vigie_*.hpp`) : un analyseur qui n'a vu que des réponses inventées ne prouve rien.

**Vu sur** : instance publique de Vigie, captures du 2026-11-07 entre 08:52 et 08:53 UTC (instant du serveur de chaque capture dans `vigie_captures.json`, champ `serveur_utc`). Ces réponses n'ont pas encore été reçues par une carte : le firmware sur carte reste « non vérifié ».

## Fichiers

| Fichier | Contenu |
|---|---|
| `vigie_<lieu>.json` | corps de la réponse tel que l'API l'a rendu (décompressé, octet pour octet) |
| `vigie_<lieu>.gz` | les octets reçus sur le fil, quand le serveur a compressé (corps d'au moins ≈ 500 octets) : sert à tester l'en-tête gzip |
| `vigie_<lieu>.inc`, `vigie_<lieu>.gz.inc` | les mêmes octets en littéral C++ pour `#include` dans un test |
| `attendu_vigie_<lieu>.inc` | ce que le noyau doit lire de la réponse et montrer à chaque instant d'essai, calculé **à part** par `test/host/controle_python_vigie.py` (une seconde lecture en Python seul, sans une ligne du noyau) |
| `vigie_<lieu>_toutes_langues.json` | une réponse de l'ébauche, **sans** filtre `lang` (tous les textes de l'émetteur), captée à la main le 2026-11-07 vers 08:41 UTC ; conservée telle quelle, elle ne se recapture pas à l'identique |
| `vigie_captures.json` | provenance : adresse demandée, instant du serveur, tailles sur le fil et décompressée, en-têtes utiles |

## Lieux choisis

Chaque lieu apporte un cas que le noyau doit tenir : plusieurs avertissements d'un coup (La Palma), texte de l'émetteur en huit langues (Eberswalde), état « update » (Bodø), texte en anglais seulement (Londres), aucun avertissement alors que des avertissements existent sans pouvoir être rattachés à un point (Funchal, Ancône), le « pas d'alerte » ordinaire (Hambourg, Valence), un pays hors des sources de l'instance (Tokyo). La table commentée est `LIEUX` dans `test/host/capturer_vigie.py`.

## Régénérer

- `python test/host/capturer_vigie.py` refait les captures : **réseau requis**, une dizaine de requêtes espacées de deux secondes (l'instance publique est limitée par adresse et n'est pas faite pour bâtir un service dessus).
- `python test/host/controle_python_vigie.py` réécrit les `.inc` ; avec `--verifier` il ne modifie rien et échoue si un `.inc` n'est plus à jour.

Les alertes changent d'une heure à l'autre : une capture n'est valable qu'à son instant (`serveur_utc`), et les tests prennent cet instant pour « maintenant ». Une recapture change donc les attendus : à relire avant de commiter.
