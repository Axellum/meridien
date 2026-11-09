# Dessin LVGL sur deux unités (deux cœurs) : essai perdant sur le Tab5

> **Summary (EN)** — Trial of two LVGL software draw units (`LV_OS_FREERTOS`, `LV_DRAW_SW_DRAW_UNIT_CNT=2`) against `main` (`-O2`, one unit), Tab5 ST7121, 2026-11-09, 6 starts per binary. **Result: slower and heavier, not merged.**
> - First image 2287 → 2652 ms (+365 ms); free internal RAM after the main screen 252 123 → 231 235 B (−20.9 kB), largest free block 83 968 → 63 488 B; worst frame render 382–386 → 737 ms (two of the three windows read); CPU/PPA rotation bench unchanged; no crash in either series.
> - Not measured: stack margin of the draw threads, the Waveshare 7B, real use with a finger.

**Vu sur** : M5Stack Tab5, écran ST7121 (carte de développement, MAC lue sur la puce, jamais écrite dans le dépôt), commit `1069571` (`main`, version lue `jalon-1a-tab5-st7121-354-g10695`), binaire « avant » compilé à 13:09:01 (3 298 320 o, `-O2`, une unité), binaire « après » compilé à 13:12:07 (3 299 872 o, seule différence : `CONFIG_LV_OS_FREERTOS=y` et `CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=2` dans `sdkconfig.defaults`) ; 09/11/2026 ; 6 démarrages par binaire par la commande série `redemarrer`, `memoire` à 40 s ; sans Wi-Fi configuré. Le firmware trouvé sur la carte (compilé à 13:36:36) a été sauvegardé, puis réécrit à la fin ; version et heure de compilation relues identiques.

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

## Ce qui a été lu dans le code de LVGL 9.5.0 (pas constaté sur carte)

- Les caches de TinyTTF (glyphes, données, crénage) et le décodeur d'images ont chacun un verrou, actif seulement avec un OS ; la création d'une entrée de cache se fait sous verrou.
- L'allocateur de `main/lvgl_memoire.cpp` (`heap_caps`) est utilisable depuis plusieurs tâches.
- Les masques d'icônes A8 de `main/ui_meteo_icones.cpp` sont `static const` ; `main/ui_meteo_dessin.h` pose `text_local = 1` ; les `sur_dessin` tournent dans `taskLVGL`.
- Chaque unité ajoute un fil de dessin de 8192 o de pile (`LV_DRAW_THREAD_STACK_SIZE`), priorité 3 (`LV_DRAW_THREAD_PRIO`) ; l'écart de mémoire interne mesuré ci-dessus est supérieur à ces deux piles (cause du reste non analysée : **non vérifié**).

## Non vérifié

Cause de la lenteur (concurrence sur la PSRAM ou le cache, coût des verrous, démarrage des fils : non séparées) ; marge de pile des fils de dessin ; la 7B ; l'usage avec un doigt.

Conclusion : pas de PR de code. Le réglage reste à une unité (`LV_OS_NONE`).
