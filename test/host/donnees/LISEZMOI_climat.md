# Réponses réelles de Vigie (normales climatiques)

Captures de `GET /v1/climate/normals?lat=&lon=` faites le 21/11/2026 sur l'instance publique `https://vigie.auxfilsdesidees.fr`, **sans modification** (octet pour octet, décompressées). Elles servent d'échantillons à `test_climat_normales.cpp`.

| Fichier | Lieu demandé | Cas |
|---|---|---|
| `climat_lorient.json` / `.inc` | 47,748 / -3,366 | station Météo-France active à 5,5 km (normale du maximum 13,0 °C, record de chaleur 19,6 °C, record de froid -5,0 °C, fiche éditée le 2026-11-06) |
| `climat_reunion.json` / `.inc` | -21,115 / 55,536 | outre-mer (normale 23,9 °C, records 29,8 et 3,5 °C) |
| `climat_londres.json` / `.inc` | 51,507 / -0,128 | hors couverture (`outside_coverage`, aucune fiche à moins de 50 km) |

Les `.inc` sont les mêmes octets en littéral brut C++ (`R"CL(…)CL"`) pour `#include` dans un test. Une recapture change les attendus (le mois, la date d'édition) : à relire avant de commiter.
