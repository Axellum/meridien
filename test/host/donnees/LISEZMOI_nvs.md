# Données de `test_nvs_securite.cpp` — pages NVS fabriquées par l'outil d'IDF

`nvs_clair.inc`, `nvs_chiffre.inc` et `attendu_nvs.inc` sont **générés** par [`../capturer_nvs.py`](../capturer_nvs.py) ; on ne les édite pas à la main.

- `nvs_clair.inc` : une partition NVS de 3 pages de 4 096 octets produite par `nvs_partition_gen.py generate` (ESP-IDF 5.5.5) à partir d'un CSV d'essai (4 espaces de noms, 3 textes, 2 entiers, 1 blob). Le mot de passe et le SSID sont des **valeurs d'essai**, pas des données réelles.
- `nvs_chiffre.inc` : le même CSV passé par `nvs_partition_gen.py encrypt` avec une clé **jetable**, tirée au hasard à chaque capture et jamais gardée.
- `attendu_nvs.inc` : les nombres que les tests doivent retrouver (entrées lisibles, illisibles, restes). Ils sont **déduits du CSV**, puis comparés à une analyse Python indépendante de la page produite, avant d'être écrits : le noyau C++ ne sert jamais à fabriquer sa propre réponse.

Refaire la capture (il faut l'ESP-IDF de `tools/local.ps1`) :

```powershell
python test\host\capturer_nvs.py --capturer
```

Vérifier que les trois fichiers sont à jour sans les réécrire :

```powershell
python test\host\capturer_nvs.py --verifier
```

**Vu sur** : PC de développement seulement, ESP-IDF 5.5.5, 07/11/2026. Aucune carte : ces pages n'ont jamais été comparées à la partition `nvs` d'un Tab5 ou d'une 7B.
