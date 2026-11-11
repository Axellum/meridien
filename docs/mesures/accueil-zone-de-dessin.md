# Test de zone du dessin de l'accueil — ce qui est mesuré, ce qui ne l'est pas

> **Summary (EN)** — LVGL replays the main screen's draw callback once per render band (26 bands of 28 lines on the Tab5). Every
> element used to become a draw task in every band, and LVGL discarded it afterwards. The drawing helpers now return early when the
> element's rectangle (plus a small margin) does not touch the band being repainted. Measured on the Tab5 ST7121, by a board
> self-test with no finger, 4 runs each: closing the settings back to the main screen goes from **429 ms** (423.9 to 431.5) to **390 ms**
> (384.1 to 403.1; 385 ms once warm), about **−10 %**. Opening the settings does not move (94 ms). The main screen still takes
> 385 ms to repaint: this change is a first step, not the fix.

Branche `claude/accueil-test-de-zone` (tirée de `main` au commit `c3fd97b`). Règle de lecture : tout ce qui n'est pas sous « Vu sur » avec une carte est **non constaté sur carte**.

## 1. Ce qui change

LVGL repeint l'accueil par bandes et rejoue à chaque bande le rappel de dessin de l'objet (`sur_dessin`, `main/ui_principal.cpp`). Sans test, chacun des ≈ 630 éléments (traits, rectangles, libellés, icônes ; 26 bandes de 28 lignes, d'après le profil du 08/11, non refait ici) devenait une tâche de dessin dans toutes les bandes, puis LVGL l'écartait après coup. `dessin::hors_zone()` (`main/ui_meteo_dessin.h`) compare le rectangle de l'élément, agrandi de 4 px (la hauteur de ligne pour un texte), à `couche->_clip_area` et renvoie sans rien poser s'il ne touche pas la bande. Les helpers `texte`, `rect`, `rect_degrade_h`, `rect_degrade_v`, `rect_bord`, `anneau`, `trait`, `triangle`, `arc` et les icônes météo (`main/ui_meteo_icones.cpp`) l'utilisent. Aucune géométrie ni couleur ne change.

## 2. Mesure sur la carte

Méthode : l'autotest des réglages (`MERIDIEN_AUTOTEST_REGLAGES=ON`, [reglages.md](reglages.md)), qui rejoue sans doigt « ouvrir les Réglages puis Retour » sur le vrai LVGL. Un chronomètre **temporaire, jamais commité** note l'appel de `fermer()` (ou de l'ouverture), la première bande peinte (`LV_EVENT_RENDER_START`) et la fin de la peinture (`LV_EVENT_REFR_READY`), au journal série. Deux binaires compilés dans le même dossier de travail, à la seule différence du test de zone (+368 octets). Le Wi-Fi, la météo et le reste tournent comme en usage normal.

| Retour → accueil (ms) | Essai 1 | 2 | 3 | 4 | Moyenne |
| --- | --- | --- | --- | --- | --- |
| Sans le test (`main`, `c3fd97b`) | 430,1 | 431,5 | 431,1 | 423,9 | **429,2** |
| Avec le test de zone | 403,1 | 386,0 | 384,1 | 385,0 | **389,6** |

- **Écart : −39,6 ms (−9,2 %)** sur les quatre essais ; −43,8 ms (−10,2 %) en ne gardant que les trois essais « à chaud ». L'écart est très au-dessus de la dispersion (± 4 ms).
- L'appel part en 2 à 3 ms : tout le temps est dans la peinture (422 à 430 ms avant, 382 à 400 ms après).
- Ouverture des Réglages : 94,2 / 94,7 / 94,0 / 94,3 ms avant, 94,8 / 97,8 / 94,6 / 94,2 ms après (la première ouverture, à froid, 120-122 ms dans les deux cas) : **inchangée**.
- Une session précédente avait annoncé 431 → 374 ms (−13 %) avec un binaire d'essai et le profileur LVGL. Cette mesure-ci, sans profileur, donne −10 %.

**Vu sur** : Tab5 révision d'écran ST7121, base `c3fd97b` plus le chronomètre (avant : binaire de 3 103 440 octets, SHA-256 `22301232…`) et plus le test de zone (après : 3 103 808 octets, SHA-256 `5EE6FA4F…`), flashés et lus le 09/11/2026 entre 08 h 25 et 08 h 37.

## 2 bis. L'image est la même : rendu du banc du PC, pixel pour pixel

Le banc de rendu du PC (`.\tools\rendre.ps1`, [outils.md](../outils.md)) repeint l'écran par tampons partiels avec le même LVGL 9.5 et les mêmes `main/ui_*.cpp` : il exerce donc le test de zone. Scène `principal`, 18 variantes × 2 tailles (1280×720 et 1024×600), langue `fr`, rendues **avec** et **sans** le test de zone (même arbre, le patch retiré puis remis), puis comparées pixel pour pixel :

| Hauteur du tampon | Images identiques (avant / après) |
| --- | --- |
| 50 lignes (réglage du banc) | **36 sur 36** |
| 28 lignes (la bande de la carte) | **36 sur 36** |

- **Les autres écrans qui utilisent ces fonctions de dessin** (`ui_details.cpp`, `ui_mer.cpp`, `ui_roue.cpp`, et l'accueil dans ses thèmes, fuseaux et alertes) : scènes `details`, `mer`, `roue`, `principal-themes`, `fuseaux` et `alertes`, 1280×720 et 1024×600, langue `fr`, tampons de 28 lignes : **230 images sur 230 identiques**.
- **Contre-épreuve** : avec un test de zone volontairement faux (marge de −30 px au lieu de +4), **0 image sur 36** reste identique (jusqu'à ≈ 140 000 pixels différents). La comparaison voit donc bien un élément coupé à une frontière de bande.
- À part : sans le test de zone, les rendus à 50 et à 28 lignes diffèrent déjà de 1 à 5 pixels sur 9 images (1280×720, vers le bandeau d'alerte et les courbes). Cela vient de la découpe en bandes, pas de ce changement ; non exploré ici.

## 3. Ce qui n'est pas prouvé

- **L'image sur la dalle** : non comparée pixel à pixel sur la carte. Une image de la vidéo de l'accueil au repos (build corrigé, filmé de biais avec un téléphone) ne montre pas de coupure aux frontières de bande, mais elle est trop floue pour le prouver. La preuve est celle du banc (§2 bis) ; à regarder une fois sur la carte.
- Le banc n'est pas la carte : clang sur le PC en `-O1`, données d'exemple (pas la météo réelle), 1280×720 et 1024×600, langue `fr` seulement (les autres langues et la taille 720×1280 ne sont pas rendues avant/après).
- Un seul appui à la main, fait pendant une première capture « après » (avant la recompilation avec l'autotest), a donné 438 ms pour le même geste. Essai unique, non contrôlé, non expliqué.
- La 7B (1024 × 600) n'a pas été mesurée. Le Tab5 seul.
- Le test PC du noyau n'exerce pas ce code (LVGL, carte) ; il ne détecterait aucun écart. Rien dans `components/` ni `test/` ne compile les deux fichiers modifiés : la vérification complète du dépôt (mutations du noyau, plus d'une demi-heure) a été **arrêtée avant la fin** après 37 minutes ; seul le contrôle du crochet de commit (`.\tools\verifier.ps1 -Rapide` : secrets, tests du noyau, ruff, mise en forme) fait foi pour ce commit, puis la CI de la PR.

## 4. Ce qui reste : l'accueil met encore 385 ms

D'après le profil par bandes du 08/11 (session précédente, binaire de profil, non refait ici) le coût restant est de ≈ 15 900 petits `lv_draw_sw_blend`, concentrés dans les bandes 5-9 et 19-20. Pistes, non mesurées ici : réduire ces blends (courbes, cartes des jours), garder une image de l'accueil pour le retour, agrandir les bandes.
