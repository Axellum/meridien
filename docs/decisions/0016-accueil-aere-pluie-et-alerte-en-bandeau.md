# ADR-0016 — Accueil aéré : la courbe des 12 heures, un bandeau pluie et alerte, cinq jours

> **English summary.** The home screen of the Sky, Dark board and Light themes is redone after the "airy sketch" the owner chose
> (Weather profile, lot 1 only). Left: the place and the Settings button, the clock, the date, the current weather. Right: a glass
> card with the next 12 hours (monotone curve, weather icons, rain bars, maximum and minimum). Under them one **banner** with two
> halves that keep their place: the rain to come ("Rain from 1 pm · 6.5 mm") and the official alert ("Strong wind · 2–10 pm",
> still the touch target that opens the alerts page). Below, a card with **five days** (name and icon, high / low, a ribbon of the
> temperatures, wind with its arrow, a rain capsule whose number is written only if it fits). Still **one LVGL object per
> zone**, redrawn by fingerprint; the geometry, the curve, the rain episode, the banner content and the greying of stale data are in the
> **pure core** (`ecran/accueil.hpp`, tested on the PC with mutation tests). The Clock theme keeps its composition (rendered
> pixel for pixel identical to before). Left out on purpose: the seven quantity tiles, the pressure curve, the "Auto · Weather"
> chip, any sea data, the long-press wheel, the automatic mode (other lots). Status: **proposed**, seen on PC renders only, never on a board.

- **Statut** : **proposée**, écrite le 08/11/2026 ; à valider par Axel. **Vu sur** : banc de rendu du PC (LVGL 9.5.0, dessin logiciel,
  clang), branche `claude/accueil-aere-18e1ff`, 08/11/2026 ; compilé pour le Tab5 et la 7B ; **aucune carte** : le rendu sur le
  panneau, le goût, la lisibilité à 2 m et la RAM en marche ne sont **pas constatés**.
- **Contexte** : Axel a retenu l'esquisse « croquis aéré » (profil Météo, `00ProjetTab/.claude/esquisses-accueil-meteo`, images
  `accueil-1280`, `accueil-7b`, `etat-perime`, `etat-calme`). L'ancien accueil empilait l'heure, la météo du moment, sept tuiles de
  grandeurs, une bande heure par heure et sept jours : dense, sans hiérarchie, et la pluie ou l'alerte à venir n'y étaient dites
  que dans la barre du haut. Lot 1 : le dessin de l'accueil seulement. Hors lot : le mode automatique (ADR-0015, branche `claude/mode-auto-noyau-ec5fa6`,
  pas encore fusionnée), la roue d'appui long, les profils Surf, Côte, Vent·Bateau et leurs données de mer.
- **Décision** :
  1. **Cinq zones sur verre**, chacune UN objet LVGL qui dessine tout son contenu et n'est redessiné que si son empreinte change
     (comme [ADR-0008](0008-ecran-principal-par-zones-themes-et-ciel-trame.md)) : barre (lieu, bouton Réglages), horloge, météo du
     moment, courbe des 12 heures, bandeau, cinq jours. La zone des tuiles de grandeurs disparaît, celle du bandeau prend sa
     place : **le compte d'objets LVGL ne change pas** (10, racine, six zones, bouton, pastille et anneau compris).
  2. **Le noyau pur porte la logique** (`components/core/include/ecran/accueil.hpp`, C++17 sans include ESP-IDF) : la courbe
     monotone (Fritsch–Carlson en entiers, Hermite, sans dépassement ni recul entre deux points), l'ordonnée d'une valeur,
     **l'épisode de pluie** (premier relevé pluvieux des cumuls horaires, recalé sur la pluie proche au quart d'heure, annoncé
     seulement dans les **12 heures**), la phrase d'heure (« 13 h » en français, « 13:00 » ailleurs) et la fenêtre d'alerte
     (« 14–22 h », seulement dans un même jour), le contenu du bandeau, la gélule de pluie d'un jour (largeur en racine carrée,
     **échelle plancher** 6 mm par jour et 3 mm par heure pour que la bruine reste petite), le maximum et le minimum de la courbe,
     la disposition des zones pour une taille d'écran, et le **grisé** d'un thème (60 % de gris) pour la météo périmée.
  3. **Le bandeau a deux moitiés qui gardent leur place** : la pluie à gauche, l'alerte à droite ; l'une apparaît ou disparaît
     sans déplacer l'autre. **L'alerte officielle de Vigie** est toujours l'objet `g.pastille` (appui = page des alertes, cible
     tactile conservée) posé sur la moitié droite, avec son niveau, le nombre d'autres alertes (« +2 ») et la fenêtre horaire quand
     l'alerte tient dans un jour ; l'alerte déduite de la météo (sans appui) prend la même place quand Vigie n'a rien. Sans pluie
     ni alerte : une seule puce **« Rien à signaler dans les prochaines heures »**, centrée. Le **cumul** de pluie part avant que la
     phrase soit coupée par « … ».
  4. **Données périmées** : la puce de gauche dit « Données de 07:52 · périmées » et tout ce qui vient de la météo est dessiné
     avec le thème grisé ; l'heure, la date, le lieu et **l'alerte officielle** gardent leurs couleurs (un avis en vigueur reste un
     avis).
  5. **Le thème Horloge ne change pas** : sa composition (heure géante, météo du moment, sept jours) garde son code ; rendu avant
     et après sur cinq cas (trois tailles d'écran, trois langues), les PNG sont **identiques au pixel**.
  6. **Les données** : `JourPrevu` reçoit le vent maximal du jour et sa direction, `vigie::Bandeau` l'instant de début et de fin de
     l'alerte ; rien d'autre n'est demandé aux clients météo.
- **Écarts voulus avec l'esquisse** (tous sur décision d'Axel ou par contrainte du produit) : pas de courbe de pression ; **pas de
  pastille « Auto · Météo »** (la place reste libre à droite du lieu) ; le **bouton Réglages et la pastille d'alerte restent**
  (l'esquisse retirait les boutons au profit de l'appui long, qui n'est pas dans ce lot) ; l'appui long ouvre toujours le
  diagnostic ; la gélule de pluie n'écrit son chiffre que s'il tient dedans.
- **Alternatives écartées** : garder les sept tuiles de grandeurs sous l'accueil (le principe de l'esquisse est de les ranger dans
  des profils ou sous l'appui long : à décider avec le lot suivant) ; un objet LVGL par puce ou par colonne (RAM interne) ;
  une pastille d'état du mode automatique (hors lot, décision d'Axel) ; le dégradé natif de LVGL pour les voiles sous les
  courbes (il ne suit pas une courbe : les voiles sont des bandes étroites, dont les bords se voient en RGB565 parce que
  LVGL 9.5 ne tramerait pas ce dégradé) ; une mise à jour de l'esp_hosted du C6 (jamais proposée).
- **Constaté (PC)** : tests du noyau compilés et exécutés avec AddressSanitizer et UBSan (clang++), **mutations toutes détectées**
  ([docs/mesures/accueil.md](../mesures/accueil.md)) ; rendus 1280×720, 1024×600 et 720×1280, sept langues, quatre thèmes ; bascule de
  thème à chaud identique au thème construit directement ; Horloge identique à l'avant.
- **Non constaté** : tout ce qui touche une carte (Tab5 ST7121, ST7123, ILI9881C, Waveshare 7B), la lisibilité à 2 m, l'effet des
  bandes dans les voiles sur le panneau réel, la RAM libre en marche (mesurée statiquement seulement, voir la page de mesures),
  la **nuit** (aucun mécanisme n'existe : l'état « nuit » de l'esquisse n'est pas dans ce lot).
- **Ce qui la remettrait en cause** : un jugement d'Axel sur l'écran réel ; une RAM interne trop basse en marche (les tâches de
  dessin des zones « heures » et « jours » sont plus nombreuses) ; le choix du lot suivant sur la place des grandeurs retirées.
