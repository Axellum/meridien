# Démarrage de la 7B : test mémoire de la PSRAM coupé et attente fixe retirée (09/11/2026)

> **Summary (EN)** — Six software restarts of the Waveshare 7B before and after the change, read on the serial log with the chip's own timestamps. The light now comes on **1030 ms after the start of the bootloader instead of 1996.5 ms (−966 ms, −48 %)**. Two causes, each seen separately in the log: the PSRAM memory test (`CONFIG_SPIRAM_MEMTEST=n`) took 885 ms, and the fixed 100 ms wait before the backlight became 19 ms (the last display transfer was already finished when it was checked), i.e. −80 ms between "board ready" and the light. **Not verified**: the image on the panel at the moment the light comes on (nobody looked at it), a cold start (power cut), the "sky" theme, the Tab5 (not measured, only compiled by the CI).

**Vu sur** : Waveshare 7B (écran EK79007, GT911), puce ESP32-P4 révision v3.2, ESP-IDF 5.5.5 ; avant : commit `c3fd97b` (binaire `meridien-lot-f-c3fd97b-7b`, empreinte ELF `514da6b60…`) ; après : commit `86d5352` (compilé le 09/11/2026 à 08:43:36, lu sur l'appareil) ; 6 redémarrages logiciels de chaque, thème sombre, luminosité 63 %, le 09/11/2026 entre 08 h 30 et 09 h 00. La carte est retrouvée par sa MAC lue sur la puce (jamais écrite dans le dépôt). Journaux bruts conservés hors dépôt.

Règle de lecture : « mesuré » = lu sur le journal série de la carte ; ce qui n'a pas été observé est écrit **non vérifié**.

## 1. Question

Combien de temps entre la remise à zéro et la première lumière, et que peut-on en retirer sans rien sacrifier d'autre ? Le rapport d'analyse de la veille chiffrait 886 ms pour le test de la PSRAM et ~100 ms pour l'attente fixe ; ce sont les deux changements de ce lot, rien d'autre.

## 2. Méthode

Commande série `redemarrer` envoyée 6 fois, 30 s d'écart, un seul `open` du port (DTR et RTS à zéro avant l'ouverture, pour ne pas réinitialiser la puce). Les instants sont ceux que la puce écrit devant chaque ligne (`(N)` en ms depuis le début du bootloader), donc exacts même si les premières lignes d'un démarrage étaient perdues. L'application est écrite seule à l'adresse `0x10000` (3 073 296 octets), le MD5 calculé par la puce confirme l'écriture ; le bootloader, la table de partitions, `otadata` et la NVS ne sont pas touchés (le Wi-Fi retenu en mémoire permanente est resté).

## 3. Résultat (mesuré, moyenne de 6 démarrages)

| Étape (ligne du journal) | Avant (`c3fd97b`) | Après (`86d5352`) | Écart |
|---|---:|---:|---:|
| PSRAM trouvée (`esp_psram: Found 32MB`) | 606,8 ms | 606,3 ms | −0,5 |
| Fin du test mémoire (`app_init`) | 1 503,8 ms | 618,5 ms | **−885,3** |
| `app_main` | 1 514,0 ms | 629,0 ms | −885,0 |
| « Carte prête » | 1 832,2 ms | 945,5 ms | −886,7 |
| **Rétroéclairage allumé** | **1 996,5 ms** (1 996-1 997) | **1 030,3 ms** (1 029-1 031) | **−966,2** |
| Carte prête → lumière | 164,3 ms | 84,8 ms | **−79,5** |

**Mesure refaite** (même binaire, compilé le 09/11/2026 à 08:43:36 lu sur l'appareil, sans nouveau flash, 6 autres redémarrages logiciels vers 09 h 05, journal `meridien_demarrage_7b_serie_lot1_mesure2_2026-11-09.log` hors dépôt) : lumière à **1 029,2 ms** en moyenne (1 028 à 1 030, valeurs 1029, 1028, 1029, 1030, 1030, 1029), soit 1 ms de la première série ; fin du test mémoire 618,7 ms, « Carte prête » 944,7 ms, « dernier transfert attendu » 19 ms aux 6 démarrages, mêmes deux lignes d'erreur qu'avant (6 fois chacune), aucune panique, `coupures=0` sur les 6 lignes `BOOT`. Le résultat se répète : −967 ms par rapport à la référence.

Le réseau n'a pas changé de comportement : lien du C6 à 2,1 s (avant 2,2 s), association à 7,3 s, adresse IP à 10,4 s, heure à 10,4 s après `app_main` (comme avant), donc toute la chronologie réseau arrive ~0,9 s plus tôt après la remise à zéro (dérivé : l'heure juste n'a pas été mesurée depuis la remise à zéro). Aucune ligne d'erreur nouvelle : les deux seules (`swap_xy is not supported`, `mac type is incorrect`) sont les mêmes, six fois, avant et après ; ni panique, ni `underrun`, ni redémarrage inattendu.

La ligne « rétroéclairage allumé » indique maintenant le temps passé à attendre : **19 ms** à chacun des 6 démarrages. Moins de 20 ms au total : le drapeau `flushing` de LVGL était donc déjà retombé quand on l'a regardé, et l'attente se réduit à la période de balayage (20 ms), pas à la fin du transfert (déduit des 19 ms du journal, le drapeau lui-même n'est pas journalisé).

## 4. Ce que ça change, ce que ça coûte

- La mémoire de 32 Mo n'est plus balayée à chaque démarrage : une barrette défectueuse ne serait plus vue d'emblée. ESPHome l'avait déjà coupé sur cette carte et sur le Tab5.
- L'attente n'est plus un nombre magique de 100 ms : elle suit la fin réelle du transfert (au plus 100 ms si le rappel ne vient jamais) puis une période de balayage à 60 Hz. Elle lit un drapeau privé de LVGL (`lv_display_private.h`), qui pourrait changer avec une autre version de LVGL ; le build casserait alors, il ne mentirait pas.
- Le bootloader est inchangé (`0x5d10` octets, 3 % libres).

## 5. Non vérifié

- **L'image sur la dalle** à l'instant où la lumière s'allume (déchirure, bande manquante) : personne ne l'a regardée, la mesure est celle du journal. À regarder à l'œil sur quelques redémarrages.
- Un démarrage à froid (coupure de courant) : la 7B n'a pas d'horloge matérielle, l'heure vient du réseau ; non mesuré.
- Le thème « ciel » sur la 7B, et le Tab5 (compilé par la CI seulement, jamais flashé avec ce lot).
- La part de chaque cause sur la sensation d'allumage : la mesure donne des millisecondes, pas ce que l'œil perçoit (cf. barre de progression d'ESPHome, rapport du 09/11).
