# Dessin LVGL sur deux unités (deux cœurs) : essai perdant sur le Tab5

> **Summary (EN)** — Trial of two LVGL software draw units (`LV_OS_FREERTOS`, `LV_DRAW_SW_DRAW_UNIT_CNT=2`) against `main` (`-O2`, one unit), Tab5 ST7121, 2026-11-09, 6 starts per binary. **Result: slower and heavier, not merged.**
> - First image 2287 → 2652 ms (+365 ms); free internal RAM after the main screen 252 123 → 231 235 B (−20.9 kB), largest free block 83 968 → 63 488 B; worst frame render 382–386 → 737 ms (two of the three windows read); CPU/PPA rotation bench unchanged; no crash in either series.
> - **Waveshare 7B (same day, same method, Wi-Fi configured)**: first image 508–517 → 761–766 ms (about +250 ms), free internal RAM after the main screen 357 207 → 336 311 B (−20.9 kB), largest free block 217 088 → 176 128–180 224 B, no crash. The two draw threads (`swdraw`) keep 6148–6156 B free of 8192 (Tab5 and 7B); `taskLVGL` margin unchanged within 50 B.
> - Not measured: real use with a finger, a per-frame render time on the 7B (the board does not log it).

**Vu sur** : M5Stack Tab5, écran ST7121 (carte de développement, MAC lue sur la puce, jamais écrite dans le dépôt), commit `1069571` (`main`, version lue `jalon-1a-tab5-st7121-354-g10695`), binaire « avant » compilé à 13:09:01 (3 298 320 o, `-O2`, une unité), binaire « après » compilé à 13:12:07 (3 299 872 o, seule différence : `CONFIG_LV_OS_FREERTOS=y` et `CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=2` dans `sdkconfig.defaults`) ; 09/11/2026 ; 6 démarrages par binaire par la commande série `redemarrer`, `memoire` à 40 s ; sans Wi-Fi configuré. Le firmware trouvé sur la carte (compilé à 13:36:36) a été sauvegardé, puis réécrit à la fin ; version et heure de compilation relues identiques.

**Vu sur (7B)** : Waveshare ESP32-P4 7B, l'écran de la maison (flash 32 Mo, MAC lue sur la puce et différente de celle du Tab5, jamais écrite dans le dépôt), mêmes commits et mêmes réglages ; binaire « avant » compilé à 13:10:24, « après » à 13:13:36 ; 09/11/2026, accord d'Axel (« faudra tester aussi sur le 7b ») ; 6 démarrages par binaire par `redemarrer`, `memoire` à 40 s, `piles` au dernier cycle ; **Wi-Fi configuré** (NVS de la carte). Firmware trouvé : `jalon-1a-tab5-st7121-354-g10695`, compilé le 09/11/2026 à 13:04:01. Sauvegarde de 0x0 à 0x880000 relue, puis remise du bloc 0x0–0x810000 (amorce, tables, application) : `verify-flash` (MD5 calculé par la puce) concordant, version et heure relues identiques. **La NVS (0x810000) a été lue mais n'a pas été réécrite** : elle n'a jamais été touchée (seule l'application l'a été), et sa vérification par la puce diffère de la lecture parce que le firmware en service y écrit en continu.

## Résultats (mesuré, 6 démarrages par binaire)

| Mesure | `main` (1 unité) | 2 unités | Écart |
| --- | --- | --- | --- |
| Première image (rétroéclairage allumé) | 2285–2290 ms | 2650–2656 ms | +365 ms |
| Mémoire interne libre après l'écran principal | 252 123 o | 231 235 o | −20 888 o |
| Mémoire interne libre à 40 s | 207 727 o | 186 863 o | −20 864 o |
| Plus gros bloc interne libre | 83 968 o | 63 488 o | −20 480 o |
| Pire rendu d'une image (fenêtres lues) | 382–386 ms | 737 ms (2 fenêtres lues sur 3 images) | environ ×1,9 |
| Rendu moyen par image (fenêtres de 3 images) | 142–144 ms | 265 ms | indicatif seulement |
| Banc de rotation au démarrage | PPA 355 µs, processeur 307 µs | PPA 355–358 µs, processeur 304–305 µs | inchangé |
| Plantage | aucun | aucun | |

Les fenêtres « rendu par image » ne contiennent que 3 images chacune : le rendu moyen est indicatif (même réserve que dans [optimisation-o2.md](optimisation-o2.md)).

## Résultats sur la Waveshare 7B (mesuré, 6 démarrages par binaire)

| Mesure | `main` (1 unité) | 2 unités | Écart |
| --- | --- | --- | --- |
| Première image (rétroéclairage allumé) | 508–517 ms | 761–766 ms | environ +250 ms |
| Mémoire interne libre après l'écran principal | 357 207 o | 336 311 o | −20 896 o |
| Mémoire interne libre à 40 s | 281 307–281 463 o | 260 463–260 595 o | environ −20 850 o |
| Plus gros bloc interne libre | 217 088 o | 176 128–180 224 o | environ −38 ko |
| Marge minimale de `taskLVGL` (commande `piles`, dernier cycle) | 1160 o | 1208 o | sans écart notable |
| Marge minimale des deux fils `swdraw` (pile de 8192 o) | — | 6152 et 6156 o | |
| Plantage | aucun | aucun | |

Sur le Tab5 les deux fils `swdraw` gardent 6148 et 6152 o de marge, et `taskLVGL` 1288 o dans les deux binaires. La 7B ne journalise pas le rendu par image : pas de pire rendu mesuré là.

## Ce qui a été lu dans le code de LVGL 9.5.0 (pas constaté sur carte)

- Les caches de TinyTTF (glyphes, données, crénage) et le décodeur d'images ont chacun un verrou, actif seulement avec un OS ; la création d'une entrée de cache se fait sous verrou.
- L'allocateur de `main/lvgl_memoire.cpp` (`heap_caps`) est utilisable depuis plusieurs tâches.
- Les masques d'icônes A8 de `main/ui_meteo_icones.cpp` sont `static const` ; `main/ui_meteo_dessin.h` pose `text_local = 1` ; les `sur_dessin` tournent dans `taskLVGL`.
- Chaque unité ajoute un fil de dessin de 8192 o de pile (`LV_DRAW_THREAD_STACK_SIZE`), priorité 3 (`LV_DRAW_THREAD_PRIO`) ; l'écart de mémoire interne mesuré ci-dessus est supérieur à ces deux piles (cause du reste non analysée : **non vérifié**).

## Non vérifié

Cause de la lenteur (et de la première image plus tardive sur les deux cartes) (concurrence sur la PSRAM ou le cache, coût des verrous, démarrage des fils : non séparées) ; le rendu par image sur la 7B ; l'usage avec un doigt.

Conclusion : pas de PR de code. Le réglage reste à une unité (`LV_OS_NONE`).
