# Voiles de l'accueil cuits en image : mesure au banc du PC

> **Summary (EN)** — The two translucent veils of the home screen (under the hourly curve, between the five-day highs and lows) used to be drawn as one vertical-gradient rectangle every 8 px, which LVGL fills line by line: about 13,900 of the ~15,900 blend calls of a full repaint on the Tab5 (profile of 08/11/2026). They are now baked once into an image (A8 for the one-colour hourly veil, RGB565A8 for the five days) whenever what they show changes, and laid down with one image draw per screen band. The sky gradient hidden under the opaque sky image is no longer computed either. On the PC bench (28-line buffers like the Tab5) a full repaint of the home screen makes 64–67 % fewer blend calls and takes 22–29 % less time; 326 of 326 renders are identical pixel for pixel, and a one-bit error in the baked colour, or an 8-level error in its mask, breaks 45 and 48 of 54. **Not measured on a board.**

## Ce qui change

- `voile_entre` (`main/ui_principal.cpp`) calcule les bandes du voile comme avant, en fait une empreinte (positions, couleurs, opacités) et, si elle a changé, **cuit** le voile dans une image en PSRAM ; puis il pose cette image d'un seul `lv_draw_image` par bande d'écran. Avant : ≈ 144 rectangles à dégradé par bande d'écran, chacun rempli ligne par ligne (`lv_draw_sw_fill.c`).
- Même pixel, par construction : chaque ligne de l'image reçoit la couleur et l'opacité que donne `lv_draw_sw_grad_color_calculate` (la fonction du remplissage, sur la même hauteur de rectangle) ; LVGL fond une image RGB565 + masque, ou A8 + couleur, avec le même `lv_color_16_16_mix` qu'un remplissage translucide ; une ligne d'opacité ≤ `LV_OPA_MIN`, que le remplissage saute, a un masque nul.
- Formats : **A8 + couleur** pour le voile des heures (une seule teinte), **RGB565A8** pour celui des jours (deux teintes). Une image par voile, réutilisée si sa taille et son format conviennent, sinon rendue avant d'en créer une autre ; `ui_principal_creer` les rend toutes. Si l'allocation échoue, le voile est dessiné en bandes comme avant.
- R3 : quand l'image du ciel (opaque, de la taille de l'écran) est posée, le fond de l'écran n'a plus de dégradé dessous (`bg_grad_dir` = `NONE`) : un remplissage simple par bande au lieu d'un mélange par ligne. Le dégradé reste le repli si l'image ne peut pas être créée.

Mémoire (déduite des boîtes vues au banc sur `principal-soleil` 1280 × 720, non mesurée sur carte) : voile des heures ≈ 522 × 153 px en A8 ≈ 80 Ko, voile des jours ≈ 931 × 53 px en RGB565A8 (3 o par pixel) ≈ 148 Ko : **≈ 0,23 Mo de PSRAM en plus** dans ce cas (plus avec des courbes plus hautes), rien en mémoire interne.

## Résultats au banc (constatés)

`.\tools\rendre.ps1 -Scene <scène> -Langue fr` avec `BANC_CHRONO=1`, `tools/rendu/banc.cpp` passé **temporairement** à `kLignesParTampon = 28` (les bandes du Tab5), `main` 7254283 contre cette branche. « Repeint » = repeint complet de l'écran tel qu'il est (médiane de 15, µs du PC) ; « mélanges » = appels à `lv_draw_sw_blend` d'un repeint. Médiane des variantes de chaque scène.

| Scène, taille | Variantes | Repeint avant → après | Mélanges avant → après |
| --- | --- | --- | --- |
| `principal` 1280×720 | 18 | 2 721 → 2 064 µs (**−24 %**) | 19 840 → 7 146 (**−64 %**) |
| `principal` 1024×600 | 18 | 2 149 → 1 675 µs (−22 %) | 17 628 → 5 927 (−66 %) |
| `principal` 720×1280 | 18 | 3 216 → 2 267 µs (−29 %) | 19 617 → 6 404 (−67 %) |
| `principal-themes` 1280×720 | 37 | 2 550 → 2 103 µs (−18 %) | 19 605 → 7 200 (−63 %) |
| `fuseaux` 1280×720 | 7 | 2 693 → 2 082 µs (−23 %) | 19 011 → 6 930 (−64 %) |
| `alertes` 1280×720 | 9 | 2 367 → 2 105 µs (−11 %) | 15 639 → 6 962 (−55 %) |

Sans météo (`principal-attente`), il n'y a pas de voile : les mélanges passent de 971 à 277, c'est R3 seul (26 bandes × 27 lignes de dégradé en moins). Les durées sont celles d'un PC partagé avec d'autres compilations : bruitées de ±10 à 20 % d'une variante à l'autre ; les comptes de mélanges, eux, sont exacts. **Le rapport PC / carte n'est pas connu** : ces chiffres ne disent pas le gain sur le Tab5.

## Rendu identique (banc du PC)

Tampons de 28 lignes, langue `fr` : **326 rendus sur 326 identiques au pixel** (`ImageChops.difference(...).getbbox()` vide) sur les six scènes qui compilent `main/ui_principal.cpp` : `principal` (18 variantes), `principal-themes` (37), `fuseaux` (7), `alertes` (9), `auto` et `roue`, en 1280×720, 1024×600 et 720×1280 (deux premières seulement pour `fuseaux` et `roue`).

Contre-épreuves (scène `principal`, 54 rendus, fichier restauré après) :

- couleur du voile RGB565A8 décalée d'un bit (`lv_color_to_u16(c) ^ 1`) : **45 rendus sur 54 diffèrent** (zone des cinq jours ; les 9 autres n'ont pas de voile) ;
- masque décalé de 8 (`a ^ 8`) : **48 sur 54 diffèrent**, dans la zone des heures (A8) et dans celle des jours ;
- un masque décalé d'un seul bit (`a ^ 1`) ne change **aucun** pixel : `lv_color_16_16_mix` arrondit l'opacité par `(a + 4) >> 3`, et `a` et `a ^ 1` y tombent toujours sur la même valeur. Ce n'est donc pas une épreuve utile, et le RGB565 ne peut pas montrer une erreur de masque plus fine que 8 niveaux.

## Pas de fuite des images

`BANC_VOILES=1 .\tools\rendre.ps1 -Scene principal` : chaque variante rejoue 40 mises à jour de la météo (sept temps différents, donc des voiles recuits à d'autres tailles et formats), un repeint après chacune et deux reconstructions de l'écran, puis remet l'état de la variante ; la scène sort en code 4 si plus de deux images vivent à la fois (`ui_principal_images_voile()`). Constaté : les 21 rendus concernés (les sept variantes qui passent par `depuis`, trois tailles) écrivent « au plus 2 images vivantes sur 40 mises à jour, 2 à la fin », et les 54 PNG de la scène sont identiques à ceux rendus sans `BANC_VOILES`.

## Ce qui n'est pas vérifié

- **Temps de repeint de l'accueil sur la carte** et **PSRAM utilisée en plus** : non mesurés. Protocole proposé (accord d'Axel nécessaire pour le flash) : binaire jetable avec le profileur intégré de LVGL (`LV_USE_PROFILER_BUILTIN`), comme le 08/11 ; 4 retours des Réglages vers l'accueil, ligne `affichage sur … ms` du journal série, avant (`main`) et après ; `python analyser_profil.py` et `par_bande.py` : les bandes 9 et 20 (voiles) devraient tomber de ≈ 2 400 / 4 100 mélanges à quelques centaines ; commande série `memoire` à 40 s pour la PSRAM libre.
- **L'image sur la dalle** : non regardée ; la preuve est celle du banc (même LVGL, même format RGB565).
- La 7B n'a pas été essayée ; autres langues que `fr` non comparées.
