# Ciel de la page « Détails » en mosaïque — ce qui est prouvé, ce qui ne l'est pas

> **Summary (EN)** — The *Details* page keeps its own copy of the dithered sky (`main/ui_details.cpp`), a full-screen RGB565 image
> (1.84 MB at 1280 × 720) in PSRAM with LVGL's gradient still drawn under it. It is now the same 64-column tile that LVGL repeats
> (`bg_image_tiled`) as on the main screen (PR 27): same pixels, 92 KB, and the gradient under it is switched off. **Pixel-for-pixel
> identical on the PC bench: 180 of 180 renders of the scene `details` at 50, 28 and 25 lines of draw buffer**; with a 60-column tile
> (not a multiple of 8) 117 of 180 change, which proves the comparison sees a broken seam. **Measured on the Waveshare 7B (Sky theme
> forced, temporary timer, 16 openings per binary): opening the page takes 187.4 → 102.1 ms (−85.3 ms, −46 %)**, building the page
> goes from 51.7 to 7.6 ms, and the open page takes about 1.17 MB less PSRAM. Not seen: the sky on the panel by eye, the exact binary
> of the commit (the measured one adds the timer), the Tab5. Both boards build without a warning.

Branche `claude/ciel-details-tuile-6ac1a8` (tirée de `main` au commit `1069571`). Règle de lecture : tout ce qui n'est pas sous « Vu sur » avec une carte est **non constaté sur carte**.

## 1. Ce qui change

`appliquer_ciel()` de `main/ui_details.cpp` construisait une image RGB565 tramée de la taille de l'écran (1 843 200 octets à 1280 × 720, 1 228 800 à 1024 × 600), posée en image de fond de la page, avec le dégradé vertical de LVGL réglé dessous. La trame de Bayer ne dépend que de la colonne modulo 8 et de la ligne : l'image devient une **tuile de 64 colonnes sur toute la hauteur** (92 160 octets à 720 lignes, 76 800 à 600) que LVGL répète (`bg_image_tiled`) à partir du coin de l'écran, et le dégradé de LVGL est coupé tant que l'image est là (il redevient le repli si l'allocation échoue). Aucune couleur, aucune géométrie ne change. C'est le même changement que celui de l'accueil, qui a sa propre copie de la fonction (`accueil-ciel-en-mosaique.md`, PR 27 : ce fichier n'existe qu'une fois cette PR fusionnée ; note du 12/11/2026 : la PR 27 a été fermée sans fusion le 09/11/2026, le fichier n'est que dans son historique, commit `448b8b4`) ; les deux PR ne touchent pas le même fichier et se fusionnent dans n'importe quel ordre, la PR 26 (verre cuit de l'accueil) ne touche que `main/ui_principal.cpp`.

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

## 4. Mesure sur la 7B : ouverture de la page

Méthode : un chronomètre **temporaire, jamais commité**, identique pour les deux binaires, appliqué à `main` (`1069571`) et au commit `f810681`. Le thème **Ciel est forcé** (la NVS de la carte dit « sombre », qui n'a pas de ciel), sur l'accueil et sur la page. Une minuterie ouvre la page « Détails » (`ui_details_ouvrir()`) puis la ferme (`ui_details_fermer()`) toutes les 3 s. On note, par les événements de l'affichage de LVGL : le temps entre l'appel d'ouverture et la fin du premier rafraîchissement (la page est alors à l'écran), le temps passé dans `ui_details_ouvrir()` seule, la PSRAM libre juste avant et juste après l'appel, et le temps du retour à l'accueil depuis la page (même mesure que pour la PR 27). Deux démarrages par binaire, 9 ouvertures et 8 retours par démarrage, la première de chaque démarrage écartée (les polices de la page sont créées à la première ouverture) : 16 ouvertures et 14 retours par binaire. Le Wi-Fi et la météo tournent comme en usage normal. Aucun doigt sur l'écran.

| | `main` + chronomètre | PR 28 + chronomètre | Écart |
| --- | --- | --- | --- |
| **Ouverture de la page** (appel → fin du premier rafraîchissement) | **187,4 ± 3,6** ms (184,7 – 198,5) | **102,1 ± 1,7** ms (101,1 – 108,0) | **−85,3 ms (−46 %)** |
| dont la construction, dans `ui_details_ouvrir()` (ciel, page) | 51,7 ± 1,3 ms | 7,6 ± 0,1 ms | −44,1 ms |
| dont le premier rafraîchissement qui suit (le reste) | 135,7 ms | 94,5 ms | −41,2 ms |
| 1<sup>re</sup> ouverture de chaque démarrage (polices créées) | 208,8 et 216,2 ms | 117,7 et 120,2 ms | |
| PSRAM prise par la page ouverte | ≈ 1 250 000 octets | ≈ 82 700 octets | **≈ −1 167 300 octets** |
| Retour à l'accueil depuis la page (contrôle) | 235,3 ± 1,4 ms | 235,4 ± 1,4 ms | +0,1 ms |

- Le retour à l'accueil n'est pas touché par cette PR (l'accueil garde son image pleine taille dans les deux binaires) : les deux mesures coïncident, ce qui montre que la carte était dans le même état pour les deux séries.
- La PSRAM gagnée (≈ 1,167 Mo) est proche de celle attendue (1 228 800 − 76 800 = 1 152 000 octets) ; l'excédent, ≈ 15 Ko, n'est pas expliqué. La valeur est stable d'une ouverture à l'autre (à quelques dizaines d'octets près, hors les deux premières ouvertures : +1,3 Ko).

**Vu sur** : Waveshare 7B (1024 × 600), MAC lue sur la puce et non écrite ici, 09/11/2026 de 15 h 47 à 15 h 58. Binaires d'essai non commités (SHA-256) : `main` + chronomètre `95b064f3…` (3 276 656 octets, version lue sur la puce `jalon-1a-tab5-st7121-354-g10695`) et PR 28 + chronomètre `bf3fc991…` (3 276 720 octets, version `jalon-1a-tab5-st7121-355-gf8106`), chacun écrit à partir de 0x10000 et comparé à la flash par la puce (identique). La 7B a ensuite été remise sur le firmware d'origine (`roue-sans-voile-1069571`, SHA-256 `35680da5…`, comparé à la flash par la puce, puis version `354-g10695` compilée à 13:04:01 relue sur deux démarrages propres, SNTP lancé). La flash était identique à la sauvegarde avant d'écrire.

## 5. Ce qui n'est pas prouvé

- **Le binaire exact du commit** (sans chronomètre) n'a pas été flashé pour cette PR : les temps viennent du code du commit **plus** le chronomètre temporaire et le thème Ciel forcé.
- **L'image sur la dalle**, regardée à l'œil : seule la preuve du banc PC existe. La NVS de la 7B est en thème sombre, il faut passer la carte en thème Ciel (ou « auto ») pour voir le ciel de la page.
- **La cause** du gain n'est pas prouvée ; l'hypothèse (l'image pleine taille qui balaie le cache L2 de 128 Ko, et le remplissage de 1,2 Mo) est la même que pour l'accueil. La construction gagne 44 ms et le premier rafraîchissement 41 ms (mesurés) ; que la première vienne du remplissage d'une image de 1,2 Mo au lieu de 77 Ko est l'explication la plus simple, non isolée.
- **Le Tab5** (1280 × 720) n'a pas été mesuré pour ce changement.
- Thème « Ciel » seulement, page ouverte par une minuterie et non par la roue, sans doigt, première ouverture de chaque démarrage écartée.
