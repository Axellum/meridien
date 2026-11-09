# Ciel de la page « Détails » en mosaïque — ce qui est prouvé, ce qui ne l'est pas

> **Summary (EN)** — The *Details* page keeps its own copy of the dithered sky (`main/ui_details.cpp`), a full-screen RGB565 image
> (1.84 MB at 1280 × 720) in PSRAM with LVGL's gradient still drawn under it. It is now the same 64-column tile that LVGL repeats
> (`bg_image_tiled`) as on the main screen (PR 27): same pixels, 92 KB, and the gradient under it is switched off. **Pixel-for-pixel
> identical on the PC bench: 180 of 180 renders of the scene `details` at 50, 28 and 25 lines of draw buffer**; with a 60-column tile
> (not a multiple of 8) 117 of 180 change, which proves the comparison sees a broken seam. **Nothing was run on a board and this page
> was never timed**: the time it takes to open or repaint is not known, the gain measured on the main screen is not transferable by
> itself. Both boards build without a warning.

Branche `claude/ciel-details-tuile-6ac1a8` (tirée de `main` au commit `1069571`). Règle de lecture : tout ce qui n'est pas sous « Vu sur » avec une carte est **non constaté sur carte**.

## 1. Ce qui change

`appliquer_ciel()` de `main/ui_details.cpp` construisait une image RGB565 tramée de la taille de l'écran (1 843 200 octets à 1280 × 720, 1 228 800 à 1024 × 600), posée en image de fond de la page, avec le dégradé vertical de LVGL réglé dessous. La trame de Bayer ne dépend que de la colonne modulo 8 et de la ligne : l'image devient une **tuile de 64 colonnes sur toute la hauteur** (92 160 octets à 720 lignes, 76 800 à 600) que LVGL répète (`bg_image_tiled`) à partir du coin de l'écran, et le dégradé de LVGL est coupé tant que l'image est là (il redevient le repli si l'allocation échoue). Aucune couleur, aucune géométrie ne change. C'est le même changement que celui de l'accueil, qui a sa propre copie de la fonction ([accueil-ciel-en-mosaique.md](accueil-ciel-en-mosaique.md), PR 27 : ce fichier n'existe qu'une fois cette PR fusionnée) ; les deux PR ne touchent pas le même fichier et se fusionnent dans n'importe quel ordre, la PR 26 (verre cuit de l'accueil) ne touche que `main/ui_principal.cpp`.

## 2. L'image est la même : rendu du banc du PC, pixel pour pixel

`.\tools\rendre.ps1 -Scene details` (même LVGL 9.5.0, mêmes `main/ui_*.cpp`) : 20 variantes (thèmes Ciel, sombre et clair, pluie, nuit, orage, chaleur, valeurs les plus larges, grandeurs absentes, météo périmée, mise à jour à chaud, réouverture…), 3 langues (fr, de, tr) et 3 tailles (1280 × 720, 1024 × 600, 720 × 1280), soit 180 PNG, rendus **avant** (`main` `1069571`) et **après**, puis comparés pixel pour pixel.

| Hauteur du tampon de dessin | Images identiques |
| --- | --- |
| 50 lignes (réglage du banc) | **180 sur 180** |
| 28 lignes (le Tab5) | **180 sur 180** |
| 25 lignes (la 7B) | **180 sur 180** |

**Vu sur** : le PC (Windows, clang 23.1.5 llvm-mingw, LVGL 9.5.0), aucune carte, 09/11/2026, base `main` `1069571`.

- **Contre-épreuve** : avec une tuile de 60 colonnes (pas un multiple de 8), **117 rendus sur 180 changent** (les 63 autres sont les variantes sans ciel : thèmes sombre et clair) : la comparaison voit bien une trame qui ne se raccorde pas, et le ciel est bien dessiné dans les variantes comparées.
- Les variantes `mise-a-jour` (le ciel est remplacé page ouverte) et `reouverture` (la page est rouverte avant la suppression de la première) passent par `appliquer_ciel` plus d'une fois : elles sont identiques aussi.
- Comme pour l'accueil, le banc donne par ailleurs des images qui diffèrent de quelques pixels entre deux hauteurs de tampon ; la comparaison se fait donc toujours à hauteur de tampon égale.

## 3. Compilation

Tab5 et Waveshare 7B compilent depuis cette branche, 0 avertissement (`.\tools\idf.ps1 build`, `CMAKE_BUILD_PARALLEL_LEVEL=4`, 09/11/2026) : binaire 7B 3 276 512 octets, SHA-256 `5482b648…` (celui de `main` : 3 276 496 octets, `35680da5…`).

## 4. Ce qui n'est pas prouvé

- **Aucune carte** : ni flash, ni démarrage, ni ouverture de la page sur l'appareil. Commit marqué « non testé sur carte ».
- **Le temps d'ouverture et de repeinte de la page « Détails »** n'a jamais été chronométré, ni avant ni après. Sur l'accueil de la 7B, la tuile a fait passer le retour de 239 à 183 ms (PR 27), avec une cause non prouvée (l'image pleine taille qui balaie le cache L2) ; rien ne dit que le gain soit le même ici : le contenu à dessiner n'est pas le même.
- **La mémoire** : PSRAM libre attendue pendant l'ouverture de la page, +1,15 Mo à la 7B et +1,75 Mo au Tab5 (d'après la taille de l'image), non relevée.
- **L'image sur la dalle**, regardée à l'œil : seule la preuve du banc PC existe.
