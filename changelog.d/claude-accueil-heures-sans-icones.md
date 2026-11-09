### Changed

- **Home screen, hours card**: the twelve weather icons side by side are gone. An icon now appears at the first hour and again
  each time the weather changes from one hour to the next (a day turning to night counts: the moon replaces the sun); a weather
  that lasts is not repeated. To follow one hour by eye, from its icon down to its point on the curve and its label, **every
  column is tinted**, like the current hour already was: the first (now) in the accent colour, the others in three colours of the
  theme (cyan, green, violet) that come back in that order, so that two neighbouring columns never share one. New in the pure core
  (`ecran/accueil.hpp`, section 9): `a_une_icone_heure`, `couleur_de_colonne`; tested with mutation tests. Seen on: PC renders
  (LVGL 9.5.0, 1280x720, 1024x600, 720x1280); not tested on board.
- **Home screen, date**: the year is no longer shown and the day and the month start with a capital letter: in French "Lundi 9
  Novembre" (before: "lundi 9 novembre 2026"). The other languages lose the year too, and their day and month
  names start with a capital when they did not (Dutch, Spanish, Italian; English, German and Turkish already did, and the Spanish
  "de" stays lower case). The date of the alert page ("samedi 8 novembre · 05:59") is unchanged. Seen on: PC renders (all
  sizes, French); not tested on board.
