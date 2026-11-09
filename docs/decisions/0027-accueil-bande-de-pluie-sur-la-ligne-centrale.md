# ADR-0027 — Accueil : la bande de pluie sur la ligne centrale, une barre par quart d'heure

> **English summary.** The home screen's central line (between the clock / hour-curve card and the five days) now shows the rain of
> the **next two hours as one bar per quarter of an hour**, the step of the Open-Meteo `minutely_15` data the device already keeps
> (the last elapsed quarter plus the next two hours). It replaces the big « rain card » that used to cover the five days when rain was
> less than 60 minutes away, so **the five days stay on screen**. A pure piece, `components/core/include/ecran/accueil_bande_pluie.hpp`
> (C++17, `constexpr`, tested on the PC), turns the quarters and the time into the bars to draw: it starts at the quarter that
> **contains the moment**, colours each bar by the rain brought back to the hour (under 1 mm/h weak, under 3 mm/h moderate, else strong:
> the same thresholds as the hour bars), keeps a **3 mm/h floor** on the scale so a drizzle stays a small bar, marks « now » and
> « in one hour » (the automatic mode's threshold), and gives the three facts the sentence needs (is it raining, when does it stop,
> how strong). A quarter **without a value** is an empty bar, never a dry one, and it never says « the rain stops ». The sentence
> (« Moderate rain · ends in 42 min », « Rain in 25 min », « Rain from 3 pm ») and the drawing are in `main/ui_principal.cpp`; the
> times under the bars are every half hour when they fit, every hour otherwise, none if neither fits. **The strip always takes the left
> half of the line** (the size of the old pill), never the full width: the right half stays for the official alert, as before (Axel's
> request of 09/11/2026 after seeing the first full-width render). In that half the sentence drops « ends in N min » first; the bars
> and the times say when the rain stops. One exception, for a half that is too narrow for the bars (the portrait 720 x 1280, about 320
> reference pixels): the strip takes the full width when no alert needs the right half, and otherwise the old pill stays. **No bar is drawn without rain**: the old pill (« Nothing to report », « Rain from… ») is
> unchanged. Status: **proposed**, seen on **PC renders only** (LVGL 9.5.0, 1280x720 and 1024x600), compiled for both boards, **never
> flashed, never run on a board**. Which weather providers fill the quarters is **not verified** (only Open-Meteo's rich read was read).

- **Statut** : **proposée**, écrite le 09/11/2026 ; à valider par Axel sur une carte. Constaté (PC et compilation, aucune carte) : voir
  [docs/mesures/bande-pluie.md](../mesures/bande-pluie.md). **Non constaté** : le dessin sur les vraies dalles (lisibilité à un mètre,
  hauteur des barres sur la 7B), la durée de dessin, le comportement sur une vraie journée de pluie.
- **Contexte** : le 09/11/2026 il pleuvait et Axel n'a rien vu changer sur ses écrans. La cause : la carte pluie du mode automatique
  (ADR-0023) n'était fusionnée que depuis le matin et n'était flashée sur aucune carte ; les données, elles, étaient justes (Open-Meteo
  donnait de la pluie). Axel a ensuite demandé : « j'ai juste la phrase, pas visuel comme sur l'autre projet », puis de l'intégrer **sur
  la ligne centrale**, « avec les crans (horaire) qui vont avec l'API, en plus beau ». Quatre esquisses lui ont été montrées (A barres,
  B frise, C pastilles, D aire lissée) ; il a choisi **« A · Barres »**. Les esquisses sont des dessins Python, pas des rendus de
  l'écran : le rendu LVGL du PC est la première vue de la vraie composition.
- **Décision** :
  1. **Un noyau pur** `ecran::accueil::composer_bande_pluie()` (`accueil_bande_pluie.hpp`) : entrées = début du premier quart gardé,
     pluie de chaque quart en dixièmes de mm, instant du moment ; sorties = jusqu'à huit barres (le quart en cours puis deux heures),
     leur niveau, l'échelle, les repères « maintenant » et « dans une heure » en ‰ de largeur, et `en_cours`, `premier`,
     `fin_dans_min`, `force`. Aucune écriture de texte, aucun include ESP-IDF ni LVGL, aucune allocation. Les règles sont écrites en
     tête du fichier pour que l'écran ne les contredise pas.
  2. **Le pas est celui de l'API** : Open-Meteo donne des quarts d'heure (son horodatage est la **fin** du quart), pas des pas de cinq
     minutes. L'appareil gardait déjà ces quarts (`MeteoDetail.quarts_*`) ; `vue_depuis_detail.h` les recopie dans `VueMeteo`
     (`nb_quarts`, `quarts_debut_s`, `quarts_dix_mm[9]`) sans rien inventer : un quart sans valeur reste `INCONNU_DIX`.
  3. **Un quart d'heure est « pluvieux » dès 0,1 mm** (la bruine compte, comme pour « pluie proche » de `meteo_etat.h`). Le **niveau**
     est celui de la pluie ramenée à l'heure (x4) : moins de 1 mm/h faible, moins de 3 mm/h moyen, sinon fort — les seuils des barres
     des heures (`niveau_pluie`). La hauteur suit la **racine carrée** de la pluie sur une échelle qui ne descend pas sous 3 mm/h, avec
     un plancher de quelques pixels pour qu'une goutte se voie.
  4. **Une bande n'existe que s'il pleut** dans l'un de ses quarts. Sans pluie, l'écran garde la pastille d'avant (« Rien à signaler »,
     « Pluie dès 15 h »). Des quarts périmés ou qui ne couvrent pas le moment ne font jamais de bande.
  5. **Le dessin** (`dessiner_bande_pluie`, `main/ui_principal.cpp`) : l'icône et la phrase à gauche (au plus 45 % de la largeur), puis
     les barres, une ligne de base, l'heure sous chaque demi-heure (chaque heure si la place manque, aucune sinon), un triangle
     « maintenant » en haut et un trait pointillé à « dans une heure ». **Toujours la moitié gauche du bandeau** (la taille de
     l'ancienne pastille), jamais toute la largeur : la moitié droite reste à l'alerte officielle, comme avant (demande d'Axel du
     09/11/2026 après le premier rendu pleine largeur). La phrase perd d'abord « fin dans N min » avant d'être coupée ; dans une
     moitié, c'est presque toujours le cas, et ce sont les barres et les heures qui disent quand la pluie s'arrête. **Exception** : une
     moitié de moins de 450 px de référence (le portrait 720 x 1280 : environ 320 ; le paysage : environ 600) ne tient ni la phrase ni
     les barres. La bande prend alors toute la largeur si aucune alerte ne réclame la moitié droite ; sinon la pastille d'avant
     reste (la phrase seule). Choix de l'auteur, pas d'Axel, qui n'a vu que le paysage.
  6. **La grande carte « Pluie en cours » n'est plus choisie** par la composition : les cinq jours restent visibles. L'alerte agrandie
     (orange ou rouge, ADR-0023) ne change pas : la pluie y reste dite par sa petite tuile.
  7. **Une phrase de plus** (« fin dans %d min ») dans la table des sept langues ; l'allemand, le néerlandais, l'espagnol, l'italien et
     le turc sont **écrits par l'IA, sans relecteur de langue maternelle**.
- **Limites écrites** :
  - La ligne centrale ne fait que **62 px de référence** (environ 50 px sur la 7B) : les barres y sont basses (environ 28 px de
     référence au plus). Plus haut obligerait à reprendre l'espacement de la carte des jours, dont les rangées sont calées sur sa
     hauteur ; je ne l'ai pas fait sans l'avis d'Axel.
  - Le code de la grande carte pluie (calcul, rectangles, dessin) **n'est pas encore retiré** : il n'est plus atteint, il reste le
     temps que la bande soit validée sur une carte. À retirer ensuite, avec ses tests.
  - La bande ne dit pas la **probabilité** de la pluie, seulement les cumuls par quart d'heure du modèle.
  - La bande n'a que la **moitié gauche** du bandeau : la phrase et les barres y sont serrées. Au rendu PC (1024x600 et 1280x720),
     huit barres fines, quatre heures étiquetées (chaque demi-heure) et « Pluie modérée » sans « fin dans N min » ; la fin de la pluie
     se lit aux barres. Au rendu PC en 1024x600, les phrases tiennent en allemand, néerlandais, espagnol et turc (le turc, « Orta
     şiddetli yağmur », passe à la petite police et aux heures pleines seulement) ; sur une carte, non vu.
- **Alternatives écartées** : **B frise** (une seule bande colorée, sans hauteur : on perd la quantité) ; **C pastilles** (une
  pastille par quart, la couleur seule dit la force : lisible mais sans repère de volume) ; **D aire lissée** (une courbe qui adoucit
  des quarts d'heure qui ne sont pas continus : elle inventerait des valeurs entre deux quarts). Une **bande plus haute** (voir les
  limites). **Garder la grande carte** et y ajouter les barres : elle cachait les cinq jours, alors qu'Axel a demandé la ligne centrale.
- **À valider par Axel** (choix de l'auteur, pas de la consigne) : la largeur maximale de la phrase (45 %), la largeur et la hauteur des
  barres, le plancher de l'échelle à 3 mm/h, le repère « dans une heure » en pointillés, l'heure sous chaque demi-heure, le choix de
  laisser la pastille d'avant quand il ne pleut pas.
- **À prouver (non vérifié)** : sur une carte (la 7B, puis le Tab5), la lisibilité à un mètre, la hauteur réelle des barres, que la bande
  apparaît et disparaît avec la vraie pluie, qu'elle ne redessine que quand sa clé change (une fois par minute au plus), la
  mémoire interne libre pendant l'affichage.
- **Ce qui la remettrait en cause** : des barres trop basses pour se lire sur la 7B (alors : une ligne centrale plus haute, avec la
  carte des jours reprise). Quand le relevé n'a pas de quarts d'heure (`nb_quarts` à 0), la bande ne s'affiche pas et la pastille
  d'avant reste ; **non vérifié** : quels fournisseurs météo remplissent ces quarts (la lecture riche que j'ai lue est celle
  d'Open-Meteo, `meteo_riche.hpp`).
