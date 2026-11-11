# Mémoire interne de la 7B : réglages communs du Tab5 appliqués (11/11/2026)

> **Summary (EN)** — The Waveshare 7B was built with ESP-IDF's defaults (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384`, mbedTLS buffers in internal RAM) until PR #32 moved the Tab5's two settings into the common `sdkconfig.defaults`. Measured on the board, same protocol before and after (software restart, then the serial command `memoire` every 60 s for 5 min): free internal RAM **281.2 → 293.8 kB**, largest free internal block **217.1 → 229.4 kB**, and above all the **minimum since boot 235.0 → 280.6 kB (+45.7 kB)**: the low point reached during the TLS handshakes of the start (weather, Vigie, sea). Weather, Vigie and sea requests answered as before; same two known error lines; no panic, no restart.

**Vu sur** : Waveshare 7B (écran EK79007, GT911), puce ESP32-P4 révision v3.2, ESP-IDF 5.5.5 ; avant : `main` `59b5fab` (version lue sur l'appareil `jalon-1a-tab5-st7121-372-g59b5f`, compilée le 11/11/2026 à 07:30:25) ; après : `main` `b526a94` (version `jalon-1a-tab5-st7121-375-gb526a`, compilée le 11/11/2026 à 19:08:58, lue sur l'appareil) ; le 11/11/2026 entre 19 h 08 et 19 h 20. La carte est retrouvée par sa MAC lue sur la puce (jamais écrite dans le dépôt). Journaux bruts conservés hors dépôt.

## 1. Question

Les réglages de mémoire interne du Tab5 (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256`, `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`, voir [memoire-interne.md](memoire-interne.md)) n'étaient que dans le fichier de carte du Tab5 ; la PR #32 les rend communs. Que gagne la 7B ?

## 2. Méthode

- Application seule écrite à `0x10000` (3 284 800 octets, « Hash of data verified ») : bootloader, table de partitions, `otadata` (lu avant : `ota_0`, état valide) et NVS non touchés. Le Wi-Fi est relu en mémoire permanente (« réseau Wi-Fi de départ : retenu en mémoire permanente »).
- Avant et après, même protocole : port série ouvert sans remise à zéro (DTR et RTS à zéro), commande `redemarrer`, puis commande `memoire` à 60, 120, 180, 240 et 300 s. Les chiffres sont ceux de la ligne `MEMOIRE` (`heap_caps_*` sur `MALLOC_CAP_INTERNAL`).

## 3. Résultat (mesuré)

| Ligne `MEMOIRE` (octets) | Avant (`59b5fab`) | Après (`b526a94`) | Écart |
|---|---:|---:|---:|
| `interne_libre` (60 → 300 s) | 281 095 → 281 151 | 293 791 → 293 755 | **+12 604** |
| `plus_gros_bloc` | 217 088 | 229 376 | **+12 288** |
| `interne_min` (minimum depuis le démarrage) | 234 972 | 280 644 | **+45 672** |
| `psram_libre` à 300 s | 31 812 664 | 31 879 584 | +66 920 |

Au démarrage (`JALON_MEM`, 0,5 s, avant toute requête) : 342 411 → 352 703 octets libres en interne (+10 292).

Le minimum depuis le démarrage est atteint pendant les prises de contact TLS du démarrage (météo, Vigie, mer) : c'est lui que les tampons de mbedTLS en PSRAM relèvent. Le reste (+12 Ko libres en régime) vient des allocations de 256 octets à 16 Ko passées en PSRAM.

Comportement : requête météo, Vigie et mer réussies avant et après (« contrat v2 (après la réponse) » à 12 s ; Vigie : réseau 1 009 → 1 013 ms ; mer : réseau 1 565 → 1 281 ms, un seul essai chacun : rien à en conclure sur la durée des prises de contact). Mêmes lignes d'erreur connues avant et après (`swap_xy is not supported`, `mac type is incorrect`), aucune panique, aucun redémarrage inattendu sur 5 min ; `/api/status` rend la nouvelle version en mode station.

## 4. Non vérifié

- L'écran après le flash n'a pas été regardé par l'auteur de la mesure (seulement le journal et `/api/status`).
- Durée des prises de contact TLS sur la 7B : non mesurée (sur le Tab5 : 2,8 s au lieu de 1,5 s, [memoire-interne.md](memoire-interne.md)).
- Une seule série de 5 min de chaque côté ; pas de mesure sur plusieurs heures.
