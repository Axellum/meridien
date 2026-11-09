# Ciel de l'accueil en mosaïque — ce qui est mesuré, ce qui ne l'est pas

> **Summary (EN)** — On the Waveshare 7B, coming back to the main screen takes 235 ms (Sky theme). Most of the repaint is not the sky
> itself (a full-screen image: 27 ms alone, with LVGL's gradient under it) but what it does to everything else: the zones alone take
> 143 ms on a flat background, so zones plus sky should take about 164 ms; with the 1.2 MB image in PSRAM they take 235 ms. The sky is a vertical gradient dithered with an 8-column pattern, so
> it only needs a 64-column tile that LVGL repeats (`bg_image_tiled`): same pixels, 77 KB instead of 1.2 MB, and LVGL's own
> gradient is no longer drawn under it. Return to the main screen: **235 → 178 ms (−57 ms, −24 %)** with equivalent test binaries, then
> **239.0 → 183.3 ms (−55.7 ms, −23 %)** with this commit's code plus a temporary timer, measured on the board the same afternoon, with
> 1.17 to 1.19 MB more free PSRAM. Pixel-for-pixel identical on the PC bench (575 of 575 renders at 50, 28 and 25 lines). The exact
> binary of this commit has booted on the board, but nobody has looked at the sky on the panel (the 7B is set to the Dark theme, which
> has no sky). The mechanism (the image sweeping the 128 KB L2 cache) is a hypothesis, not a finding.

Branche `claude/ciel-en-mosaique-6ac1a8` (tirée de `main` au commit `1069571`). Règle de lecture : tout ce qui n'est pas sous « Vu sur » avec une carte est **non constaté sur carte**.

## 1. Ce qui change

`appliquer_ciel()` (`main/ui_principal.cpp`) construisait une image RGB565 tramée de la taille de l'écran (1 843 200 octets à 1280 × 720, 1 228 800 à 1024 × 600). La trame de Bayer ne dépend que de la colonne modulo 8 et de la ligne : l'image n'est donc qu'une **tuile de 64 colonnes sur toute la hauteur** (92 160 octets à 720 lignes, 76 800 à 600), que LVGL répète (`bg_image_tiled`) à partir du coin de l'écran. Le dégradé vertical de LVGL, que l'écran dessinait **sous** l'image à chaque bande, est coupé tant que l'image est là ; il ne sert plus que de repli si l'allocation échoue. Aucune couleur, aucune géométrie ne change.

## 2. Mesure sur la 7B

Méthode : un chronomètre **temporaire, jamais commité** note, par les événements de l'affichage de LVGL, le temps entre le chargement de l'accueil (`SCREEN_LOAD_START`) et la fin du rafraîchissement (`REFR_READY`), et par bande de 25 lignes le rendu, l'attente du transfert précédent et la durée du rappel de transfert. Une minuterie alterne l'accueil et l'écran de diagnostic toutes les secondes ; on mesure le **retour du diagnostic vers l'accueil**. Thème **Ciel forcé** (la NVS de la carte disait « sombre », qui n'a ni ciel ni verre). Pour chaque variante : 2 tours de 5 retours (6 pour la série des mosaïques), le premier de chaque tour écarté, soit 8 retours (10 pour la série des mosaïques). Les variantes sont choisies par le binaire d'essai en masquant des morceaux de l'accueil au moment du retour. Le Wi-Fi et la météo tournent comme en usage normal.

| Retour → accueil (ms) | Moyenne ± écart-type | Min – max |
| --- | --- | --- |
| Aujourd'hui (image pleine taille + dégradé dessous) | **235,2** (3 séries : 235,3 ± 1,9, 234,7 ± 1,4 et 235,6 ± 3,6) | 233,0 – 244,6 |
| Image pleine taille, sans le dégradé dessous | 227,2 ± 3,6 (2<sup>e</sup> série : 226,4 ± 1,0) | 223,1 – 235,0 |
| Mosaïque de 8 px, dégradé conservé dessous | 192,2 ± 1,2 | 190,8 – 194,9 |
| Mosaïque de 8 px, sans dégradé | 185,6 ± 0,6 | 185,0 – 186,5 |
| **Mosaïque de 64 px, sans dégradé (cette PR)** | **178,4 ± 1,4** (autre série : 178,5 ± 0,7) | 177,0 – 181,8 |
| Mosaïque de 128 px, sans dégradé | 180,0 ± 0,4 | 179,6 – 181,0 |

**Écart : −56,8 ms (−24 %)** (235,2 → 178,4), très au-dessus de la dispersion (≤ 3,6 ms). Le rendu fait tout le temps : 229 ms de rendu sur 235 ; l'attente du transfert vers l'écran est de 0,3 ms et le rappel de transfert pèse 5,4 ms en tout. Agrandir les bandes ne rapporterait rien : une bande vide coûte 0,1 ms.

| Ce qui est dessiné (ms) | Retour |
| --- | --- |
| Fond uni, aucune zone (plancher) | 5,5 |
| Dégradé de LVGL seul / image pleine taille seule / les deux (aujourd'hui, sans zone) | 10,9 / 19,8 / 26,6 |
| Ciel en mosaïque de 64 px seul, sans zone | 10,7 |
| Une zone seule sur fond uni : barre / horloge / météo du moment / heures / bandeau / jours | 12,5 / 9,4 / 9,9 / 51,2 / 12,6 / 77,6 |
| Les six zones sur fond uni | 143,3 – 144,8 |
| Tout : les six zones et le ciel d'aujourd'hui | 235,2 |

Les zones seules s'additionnent (≈ 143 ms) et le ciel seul coûte 27 ms (plancher compris) : la somme attendue est d'environ 164 ms, le mesuré 235 ms. C'est l'image pleine taille derrière les zones qui coûte les ≈ 71 ms de trop. Ce qui reste à dessiner avec la mosaïque (on retire une classe d'éléments à la fois, gain en ms sur l'image d'aujourd'hui / sur la mosaïque) : rectangles, disques et anneaux (cartes de verre, colonnes, gélules) −55 / −50 ; voiles (rectangles à dégradé) −33 / −29 ; traits des courbes −26 / −21 ; textes −27 / −20 ; icônes −7 / −2 ; arcs et triangles −3 / −1.

**Vu sur** : Waveshare 7B (1024 × 600), MAC lue sur la puce et non écrite ici, 09/11/2026 entre 13 h 55 et 14 h 20. Base `main` `1069571` plus le chronomètre, flashés application seule (0x10000) et vérifiés par la puce ; la 7B a été remise ensuite sur le binaire d'origine (`roue-sans-voile-1069571`, SHA-256 `35680da5…`, lu : version `354-g10695`, compilé à 13:04:01). Binaires d'essai non commités (SHA-256) : `aa40de58…` (zones, 3 278 544 o), `883c4cb7…` (mosaïques, 3 278 848 o), `981db2fb…` (classes d'éléments, 3 281 248 o). Les valeurs de la ligne en gras viennent du binaire de mosaïque, qui pose la même tuile de 64 px et coupe le même dégradé que le commit : voir la reprise ci-dessous, faite avec le code du commit lui-même.

### Reprise avec le code du commit `44f61a5` (même jour, après l'ouverture de la PR)

Même méthode (chronomètre temporaire non commité, thème Ciel forcé, retour du diagnostic vers l'accueil, accueil et diagnostic alternés toutes les 3 s), mais sur le **code du commit** auquel on ajoute seulement le chronomètre, comparé à `main` (`1069571`) avec le même chronomètre. Deux démarrages par binaire, 8 retours par démarrage, le premier de chaque démarrage écarté : 14 retours chacun.

| | Retour → accueil (ms) | Min – max | PSRAM libre à ≈ 65 s (octets) |
| --- | --- | --- | --- |
| `main` + chronomètre | **239,0 ± 1,3** | 236,6 – 241,9 | 30 709 968 / 30 726 348 |
| PR (`44f61a5`) + chronomètre | **183,3 ± 1,1** | 182,2 – 185,4 | 31 904 264 / 31 894 148 |
| Écart | **−55,7 ms (−23 %)** | | **+1 194 296 / +1 167 800** |

- La PSRAM gagnée (1,17 à 1,19 Mo) est proche de celle attendue (1 228 800 − 76 800 = 1 152 000 octets) ; l'excédent, 16 à 42 Ko, n'est pas expliqué (la valeur varie d'environ 16 Ko d'un démarrage à l'autre).
- `main` mesure 239,0 ms ici contre 235,2 ms le matin : la carte, le Wi-Fi et la météo ne sont pas dans le même état. Seul l'écart entre deux binaires mesurés l'un après l'autre compte.
- **Le binaire exact du commit** (sans chronomètre, SHA-256 `6d0917e0…`, 3 276 512 octets) a été écrit, comparé à la flash par la puce (identique), puis a démarré : version `jalon-1a-tab5-st7121-355-g44f61`, écran principal 1024 × 600 chargé, SNTP lancé, aucune panique ni redémarrage dans les 60 s de journal. Comme la NVS de la 7B dit « thème sombre », **il n'affiche pas de ciel** : ce démarrage prouve qu'il tourne, pas le dessin du ciel ni le temps de retour. Remarque : la version et l'heure de compilation lues sur la puce (`14:40:25`) sont les mêmes que celles de la variante chronométrée (compilation incrémentale), elles ne distinguent pas les deux binaires ; c'est la comparaison par la puce avec le fichier qui le fait.

**Vu sur** : Waveshare 7B (1024 × 600), MAC lue sur la puce et non écrite ici, 09/11/2026 de 14 h 55 à 15 h 17, commit `44f61a5`. Binaires (SHA-256) : `main` + chronomètre `8faeb15f…` (3 276 896 octets), PR + chronomètre `be35f0e9…` (3 276 944 octets), commit exact `6d0917e0…`. La 7B a ensuite été remise sur le firmware d'origine (`roue-sans-voile-1069571`, SHA-256 `35680da5…`, relu par la puce : version `354-g10695`, compilé à 13:04:01, écran principal chargé, SNTP lancé). Sauvegarde de la flash faite avant d'écrire (début de flash, NVS, application en service).

## 3. L'image est la même : rendu du banc du PC, pixel pour pixel

`.\tools\rendre.ps1` (même LVGL 9.5.0, mêmes `main/ui_*.cpp`), scènes `principal`, `principal-themes`, `auto`, `fuseaux` et `alertes`, 1280×720, 1024×600 et 720×1280, langues par défaut de chaque scène (fr, de, tr), rendues **avant** (`main` `1069571`) et **après**, puis comparées pixel pour pixel.

| Hauteur du tampon | Images identiques |
| --- | --- |
| 50 lignes (réglage du banc) | **575 sur 575** |
| 28 lignes (le Tab5) | **575 sur 575** |
| 25 lignes (la 7B) | **575 sur 575** |

- **Contre-épreuve** : avec une tuile de 60 colonnes (pas un multiple de 8), **0 sur 108** rendus de `principal` reste identique (13 000 à 22 000 pixels différents) : la comparaison voit bien une trame qui ne se raccorde pas.
- À part, et sans rapport avec ce changement : le banc donne déjà des images qui diffèrent de quelques pixels entre un tampon de 50 lignes et un tampon de 28 lignes (269 images sur 575, sur `main`). La comparaison se fait donc toujours à hauteur de tampon égale.

## 4. Ce qui n'est pas prouvé

- **Le binaire exact du commit, avec un ciel à l'écran.** Il a démarré sur la 7B, mais en thème sombre (celui de la NVS) donc sans ciel ; le temps de retour et la PSRAM sont mesurés sur le code du commit **plus** un chronomètre temporaire et le thème Ciel forcé, pas sur le binaire exact. La PSRAM libérée sur le Tab5 (+1,75 Mo attendus) n'est pas relevée.
- **L'image sur la dalle**, regardée à l'œil : seule la preuve du banc PC existe.
- **La cause.** Mesuré : l'image pleine taille fait perdre ≈ 71 ms au dessin du reste. Non prouvé : pourquoi. L'hypothèse est qu'elle balaie le cache L2 de 128 Ko (`CONFIG_CACHE_L2_CACHE_128KB`), où vivent aussi le code, venu de la flash, et tout ce que LVGL alloue en PSRAM (`main/lvgl_memoire.cpp`) ; la mosaïque, qui ne relit que 3 Ko par bande, la corrobore. Un cache L2 de 256 Ko n'a pas été essayé (il coûterait 128 Ko de mémoire interne).
- **Le Tab5** (1280 × 720) n'a pas été mesuré pour ce changement. Le profil du 08/11 (385 ms de repeinte, voir [accueil-zone-de-dessin.md](accueil-zone-de-dessin.md)) a la même forme, mais rien ne dit que le gain y soit le même.
- Thème « Ciel » seulement, retour depuis le diagnostic et non depuis les Réglages, sans doigt.
- **La page « Détails »** a sa propre copie du ciel (`main/ui_details.cpp`, 1,8 Mo, pleine taille) : le même levier y existe, non essayé.

## 5. Rapport avec la PR 26 (verre cuit dans l'image du ciel)

Les deux changements s'excluent : la PR 26 dessine le verre des cartes dans l'image du ciel, qui n'est alors plus périodique, donc plus une tuile. Sur la 7B, avec la même méthode de mesure : `main` + PR 26 200,7 ms contre 235,4 ms pour `main` (−34,6 ms) ; mosaïque 178,4 ms (−56,8 ms). C'est à Axel de trancher ; si la PR 26 est fusionnée la première, celle-ci est à reprendre.
