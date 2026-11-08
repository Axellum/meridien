# Main screen — design directions, criteria and choice / Écran principal — pistes, critères et choix

## English · [Français](#version-française)

> **Status.** Everything below is judged on **PC renders** (LVGL 9.5.0 software drawing, clang). On 2026-11-07 the screen was
> also flashed on a Tab5 and its serial log read (cost table below), but nobody has **looked at the panel**: the look, the
> colours as the panel shows them and the legibility at 2 m are **not verified by eye**.
> The scoring is the agent's reading of the renders and measurements; the decision is the owner's, by eye, from about 2 m.

**What was asked.** A main screen that is very legible, clear, modern and complete, read mostly by sight: large time and
date; current weather (big icon, temperature, feels-like, label, min and max); rain; wind with a direction arrow;
humidity; pressure and its trend; UV; sunrise and sunset on an arc with the sun's position; moon phase; an hourly strip;
five to seven days; city; a discreet stale-data mark; the Settings button; a sky that follows the time and the weather;
visual alerts. It has to stay right at 1024 × 600 (Waveshare 7B), in portrait, and in seven languages.

**Four directions, built and rendered** (one screen code, four themes of the pure core `visuel_themes.hpp`):

| Direction | Idea | Composition | Sheet |
|---|---|---|---|
| **Ciel** (the default) | « glass on a sky »: a gradient background that follows the time of day and the weather, translucent cards | complete: clock, weather now, seven quantity tiles, hourly strip (13 columns in landscape, 7 in portrait), 7 days | [esquisse-ciel](esquisses/esquisse-ciel-1280x720.png) |
| **Tableau sombre** | « dashboard »: flat charcoal background, solid cards | same as Ciel | [esquisse-tableau](esquisses/esquisse-tableau-1280x720.png) |
| **Horloge** | « clock first »: a giant time, the weather now, the week; no quantities, no hourly strip | **different**: three stacked blocks | [esquisse-horloge](esquisses/esquisse-horloge-1280x720.png) |
| **Clair** | « day »: light background, dark text, for a very bright room | same as Ciel | [esquisse-clair](esquisses/esquisse-clair-1280x720.png) |

**Honest limit.** Only *Horloge* differs in structure from *Ciel*; *Tableau sombre* and *Clair* are other palettes on the
same composition. Three really different compositions were asked for; two are built. A third one (weather first, with a
giant temperature) was not, to spend the night on checking the chosen one rather than on a variant nobody asked to keep.
This is a deviation from the brief, listed in the report for the owner.

**Written criteria** (set before the scoring; each one says how it is checked):

| # | Criterion | How it is checked | Weight |
|---|---|---|---|
| 1 | Legibility from 2 m | size of the capitals relative to the screen height: time ≥ 18 %, temperature ≥ 12 %, titles ≥ 4 %, nothing essential below 2.8 % (20 px of 720); icons ≥ 100 px | 30 |
| 2 | Visual hierarchy | the first read is the time, the second the temperature and the icon, the rest is detail; successive sizes at least 1.4 times apart | 20 |
| 3 | Contrast, **measured** | WCAG 2.x ratio: text ≥ 4.5:1 on its card and on the sky, icons, bars and big digits ≥ 3:1, for the 28 skies (4 moments × 7 weathers) | 15 |
| 4 | Visual load | number of distinct items on the screen (text runs, icons, bars), counted from the layout, not instrumented | 10 |
| 5 | Holds in seven languages | renders in German and Turkish with the longest texts: nothing cut, nothing overlapping | 15 |
| 6 | Memory and CPU cost | LVGL objects, fonts, image memory, flash, redraw granularity | 10 |

**Measured** (`test/host/test_visuel_themes.cpp` fails if a threshold is missed; the minima below come from the same formula,
over the 56 backgrounds of *Ciel* and the single background of the other themes):

| Theme | Text on cards (min of 3 levels) | Time and date on the sky | Icons | Quantity colours |
|---|---|---|---|---|
| Ciel | 6.01:1 | 4.62:1 | 3.10:1 | 3.01:1 |
| Tableau sombre | 7.27:1 | 12.74:1 | 5.78:1 | 7.11:1 |
| Horloge | 8.53:1 | 14.34:1 | 6.88:1 | 8.47:1 |
| Clair | 6.84:1 | 9.03:1 | 3.17:1 | 3.77:1 |

Letter sizes (Roboto Bold capital height = 0.711 em, reference screen 720 px high): time 135 px (18.8 %), giant time 192 px,
temperature 88 px (12.2 %), title 30 px (4.1 %), body 24 px (3.4 %), small 20 px (2.8 %), mini 16 px (2.3 %), nano 13.5 px
(1.9 %). **Honest reading:** from 2 m only the time, the temperature, the icons and the day names are read; the nano and mini
sizes (units, « Rafales 26 », « En hausse ») are detail for someone who steps closer. The physical size of the panel is not
recorded in this repository, so no size in millimetres is claimed (not verified).

**Choice: *Ciel* as the default screen**, because it is the only direction that gives everything asked at once (all
quantities, hourly strip, seven days) while keeping the three levels of reading; its sky also tells the time of day and the
weather at a glance, which no flat theme does. Its weak points are the low contrast margin of the icons (3.10:1, above the
3:1 threshold but the lowest of the four) and the heaviest load (about 130 items). *Horloge* is the calmer alternative for a
bedroom or a hallway (about 50 items, the best contrast); *Tableau sombre* and *Clair* serve a dark or a very bright room.

**What is reachable on the device.** The existing setting « thème » (auto, clair, sombre; page of settings) picks: auto →
*Ciel*, clair → *Clair*, sombre → *Tableau sombre*, and the switch is made live
(`ui_principal_changer_theme`; checked on the PC: the switched screen is pixel for pixel the one built directly with the new
theme, 10 pairs, `tools/rendu/verifier_bascule.py`). *Horloge* has no value in that setting: a fourth value in the settings
registry (owned by the `reglages-ecran` session) is needed to reach it.

**Cost** (compiled for ESP32-P4 with ESP-IDF 5.5.5; the rows marked *board* were read from the serial log of a Tab5):

| Item | Value | Source |
|---|---|---|
| LVGL objects of the screen | 9 (screen, 6 drawing zones, Settings button, touch ring) | `ui_principal_cout()` read in the code |
| Fonts | 7 TinyTTF sizes from the one embedded Roboto Bold | code |
| Icon set | 825,032 bytes in flash, 0 bytes of RAM for the pixels | `main/assets/icones_meteo.bin` |
| Sky image | 1,843,200 bytes at 1280 × 720 (RGB565), in PSRAM, rebuilt only when the moment or weather class changes; gradient fallback if the allocation fails | computed from the size |
| Firmware | 2,722,192 bytes, 35 % of the 4 MB app partition free | `build/meridien.bin` |
| PC redraw (relative, **not** the board) | construction 2–4 ms, first full render 2–4 ms at 1280 × 720 | `BANC_CHRONO=1`, 9800X3D |
| Internal RAM taken by building the screen (*board*) | 4.9 KB (253,571 → 248,643 bytes free) | log line of `ecran_principal.cpp` |
| Internal RAM free before the first weather request (*board*) | 154.4 KB with the memory settings of jalon 1b; 44.9 KB without them, and the board then reboots (see [depannage.md](../depannage.md)) | serial log |
| Internal RAM free after 154 s (*board*) | 105.9 KB, minimum since start 96.0 KB, stable between 64 s and 154 s | `memoire` command |
| Redraw on the board | first 10 s: 3 images at 215 ms on average (empty glyph cache); a minute change: 1 image, 10 zones, 82 ms. The old screen: about 24 ms | serial log, `tab5_orientation` line |

The P4 is far slower than the PC: a minute change costs 82 ms on the board, about three times the old screen, which is
acceptable for a screen that changes once a minute (the touch response of the Settings button, which redraws only its own
zone, is **not measured**). The design limits the damage: one drawing object per zone, redrawn only when its content changed (a
hash per zone), so the clock alone is redrawn each minute.

> **Update 2026-11-08 — the home screen was redone** (themes Sky, Dark board, Light; [ADR-0016](../decisions/0016-accueil-aere-pluie-et-alerte-en-bandeau.md),
> numbers in [mesures/accueil.md](../mesures/accueil.md)). The seven quantity tiles and the old hourly strip are gone: the zones are now the
> bar (place, Settings button), the clock, the current weather, the 12-hour curve, the banner (rain to come and official alert) and
> five days. **LVGL objects: still 10** (the banner zone replaces the tiles zone), **fonts: 6** (the clock 168 px, the temperature 54 px, the
> date 36 px, the day texts 32 and 28 px, the small text 24 px; the table above counts the old composition's 7). The Clock theme
> is unchanged. The cost rows marked *board* above describe the old home screen: the new one has **not been measured on a board**.

> **Update 2026-11-08 — no more Settings button** ([ADR-0021](../decisions/0021-roue-d-appui-long-sur-l-ecran-principal.md), numbers in [mesures/roue.md](../mesures/roue.md)). The round button at the right end of the place bar is gone, and so is its reserved space: the place now has the whole bar. **LVGL objects of the home screen: 9** (screen, six zones, alert chip, touch ring). A long press (400 ms) anywhere on the screen opens the wheel **at the finger**: « Settings » and, when the alerts page exists, « Alerts » (one more LVGL object while it is open, none otherwise). The diagnostic screen is reached through Settings > Device > Diagnostic. Seen on PC renders only; **not seen on a board**.

**Seen on:** PC render (clang 23.1.5, LVGL 9.5.0), commits of branch `claude/ecran-visuel-ba722a`, 2026-11-07; the *board* rows:
Tab5 with the ST7121 screen (MAC read on the chip, not copied here), ESP32-P4, commit `cd362eb` built with the memory settings of
jalon 1b added in a separate build directory (never committed), 2026-11-07 from 01:57 to 02:02.

**Reproduce:**

```powershell
. .\tools\trouver_outils.ps1; . "$(Trouver-Outils)\env_hote.ps1"
.\tools\rendre.ps1 -Scene principal -Planche            # 13 variants, fr de tr, three sizes + sheets -> docs\captures\ecran\
.\tools\rendre.ps1 -Scene principal-themes -Langue fr -Planche -Variante ciel-soleil,ciel-pluie,ciel-neige,ciel-orage,ciel-brouillard,ciel-nuit,ciel-chaleur,ciel-pires
python test\host\lancer.py --seulement visuel --mutations   # pure core tests and mutations
```

---

## Version française

> **Statut.** Tout ce qui suit est jugé sur des **rendus PC** (dessin logiciel LVGL 9.5.0, clang). Le 07/11/2026 l'écran a aussi
> été flashé sur un Tab5 et son journal série lu (tableau du coût plus bas), mais personne n'a **regardé la dalle** : l'aspect,
> les couleurs telles que la dalle les montre et la lisibilité à 2 m sont **non vérifiés à l'œil**. La
> notation est la lecture que l'agent fait des rendus et des mesures ; la décision est celle d'Axel, à l'œil, à 2 m environ.

**Ce qui était demandé.** Un écran principal très lisible, clair, moderne et complet, lu surtout du regard : grande heure et
date ; météo du moment (grande icône, température, ressenti, libellé, minimum et maximum) ; pluie ; vent avec une flèche de
direction ; humidité ; pression et sa tendance ; UV ; lever et coucher du soleil sur un arc avec la position du soleil ; phase
de lune ; bande heure par heure ; cinq à sept jours ; ville ; une marque discrète de données périmées ; le bouton Réglages ;
un ciel qui suit l'heure et le temps ; des alertes visuelles. Il doit rester juste en 1024 × 600 (Waveshare 7B), en portrait
et en sept langues.

**Quatre pistes, construites et rendues** (un seul code d'écran, quatre thèmes du noyau pur `visuel_themes.hpp`) :

| Piste | Idée | Composition | Planche |
|---|---|---|---|
| **Ciel** (par défaut) | « verre sur un ciel » : fond en dégradé qui suit l'heure et le temps, cartes translucides | complète : heure, météo du moment, sept tuiles de grandeurs, bande heure par heure (13 colonnes en paysage, 7 en portrait), 7 jours | [esquisse-ciel](esquisses/esquisse-ciel-1280x720.png) |
| **Tableau sombre** | « tableau de bord » : fond anthracite uni, cartes pleines | identique à Ciel | [esquisse-tableau](esquisses/esquisse-tableau-1280x720.png) |
| **Horloge** | « l'heure d'abord » : heure géante, météo du moment, semaine ; ni grandeurs ni heure par heure | **différente** : trois blocs empilés | [esquisse-horloge](esquisses/esquisse-horloge-1280x720.png) |
| **Clair** | « jour » : fond clair, texte sombre, pour une pièce très éclairée | identique à Ciel | [esquisse-clair](esquisses/esquisse-clair-1280x720.png) |

**Limite assumée.** Seule *Horloge* diffère de *Ciel* par sa structure ; *Tableau sombre* et *Clair* sont d'autres palettes sur
la même composition. Trois compositions vraiment différentes étaient demandées ; deux sont construites. Une troisième (météo
d'abord, température géante) ne l'a pas été : la nuit a été employée à vérifier l'écran choisi plutôt qu'une variante que
personne n'a demandé de garder. C'est un écart au cahier des charges, listé dans le rapport pour Axel.

**Critères écrits** (posés avant la notation ; chacun dit comment il est vérifié) :

| N° | Critère | Vérification | Poids |
|---|---|---|---|
| 1 | Lisibilité à 2 m | hauteur des capitales rapportée à la hauteur d'écran : heure ≥ 18 %, température ≥ 12 %, titres ≥ 4 %, rien d'essentiel sous 2,8 % (20 px sur 720) ; icônes ≥ 100 px | 30 |
| 2 | Hiérarchie visuelle | la première lecture est l'heure, la deuxième la température et l'icône, le reste est du détail ; tailles successives distantes d'au moins 1,4 | 20 |
| 3 | Contraste, **mesuré** | rapport WCAG 2.x : texte ≥ 4,5:1 sur sa carte et sur le ciel, icônes, barres et gros chiffres ≥ 3:1, pour les 28 ciels (4 moments × 7 temps) | 15 |
| 4 | Charge visuelle | nombre d'éléments distincts à l'écran (textes, icônes, barres), compté d'après la disposition, sans instrument | 10 |
| 5 | Tenue en sept langues | rendus en allemand et en turc avec les textes les plus longs : rien de coupé, rien qui se chevauche | 15 |
| 6 | Coût mémoire et processeur | objets LVGL, polices, mémoire d'image, flash, finesse du redessin | 10 |

**Mesuré** (`test/host/test_visuel_themes.cpp` échoue si un seuil n'est pas tenu ; les minima ci-dessous viennent de la même
formule, sur les 56 fonds de *Ciel* et le fond unique des autres thèmes) :

| Thème | Texte sur les cartes (minimum des 3 niveaux) | Heure et date sur le ciel | Icônes | Couleurs des grandeurs |
|---|---|---|---|---|
| Ciel | 6,01:1 | 4,62:1 | 3,10:1 | 3,01:1 |
| Tableau sombre | 7,27:1 | 12,74:1 | 5,78:1 | 7,11:1 |
| Horloge | 8,53:1 | 14,34:1 | 6,88:1 | 8,47:1 |
| Clair | 6,84:1 | 9,03:1 | 3,17:1 | 3,77:1 |

Tailles de lettres (hauteur de capitale de Roboto Bold = 0,711 em, écran de référence de 720 px de haut) : heure 135 px (18,8 %),
heure géante 192 px, température 88 px (12,2 %), titre 30 px (4,1 %), corps 24 px (3,4 %), petit 20 px (2,8 %), mini 16 px
(2,3 %), nano 13,5 px (1,9 %). **Lecture honnête :** à 2 m on lit l'heure, la température, les icônes et les noms de jour ;
les tailles nano et mini (unités, « Rafales 26 », « En hausse ») sont du détail pour qui s'approche. La taille physique de la
dalle n'est pas consignée dans ce dépôt : aucune taille en millimètres n'est avancée (non vérifié).

**Choix : *Ciel* comme écran par défaut**, parce que c'est la seule piste qui donne tout ce qui est demandé à la fois (toutes les
grandeurs, l'heure par heure, les sept jours) en gardant les trois niveaux de lecture ; son ciel dit aussi l'heure de la journée
et le temps d'un coup d'œil, ce qu'aucun thème uni ne fait. Ses points faibles : la marge de contraste des icônes (3,10:1, au-dessus
du seuil de 3:1 mais la plus basse des quatre) et la charge la plus lourde (environ 130 éléments). *Horloge* est l'alternative plus
calme pour une chambre ou un couloir (environ 50 éléments, les meilleurs contrastes) ; *Tableau sombre* et *Clair* servent une
pièce sombre ou très éclairée.

**Ce qui est atteignable sur l'appareil.** Le réglage « thème » existant (auto, clair, sombre ; page de réglages) choisit : auto →
*Ciel*, clair → *Clair*, sombre → *Tableau sombre*, et la bascule se fait à chaud (`ui_principal_changer_theme` ; vérifié sur le PC :
l'écran basculé est identique pixel pour pixel à celui construit directement avec le nouveau thème, 10 paires,
`tools/rendu/verifier_bascule.py`). *Horloge* n'a pas de valeur dans ce réglage : il faut une quatrième valeur au registre des
réglages (propriété de la session `reglages-ecran`) pour l'atteindre.

**Coût** (compilé pour l'ESP32-P4 avec ESP-IDF 5.5.5 ; les lignes marquées *carte* sont lues dans le journal série d'un Tab5) :

| Poste | Valeur | Source |
|---|---|---|
| Objets LVGL de l'écran | 9 (écran, 6 zones de dessin, bouton Réglages, anneau tactile) | `ui_principal_cout()`, lu dans le code |
| Polices | 7 tailles TinyTTF du seul Roboto Bold embarqué | code |
| Jeu d'icônes | 825 032 octets en flash, 0 octet de RAM pour les pixels | `main/assets/icones_meteo.bin` |
| Image du ciel | 1 843 200 octets en 1280 × 720 (RGB565), en PSRAM, refaite seulement quand le moment ou la classe de temps change ; repli sur un dégradé si l'allocation échoue | calculé d'après la taille |
| Firmware | 2 722 192 octets, 35 % de la partition d'application de 4 Mo libre | `build/meridien.bin` |
| Redessin sur le PC (relatif, **pas** la carte) | construction 2 à 4 ms, premier rendu complet 2 à 4 ms en 1280 × 720 | `BANC_CHRONO=1`, 9800X3D |
| RAM interne prise par la construction de l'écran (*carte*) | 4,9 Ko (253 571 → 248 643 octets libres) | ligne de journal de `ecran_principal.cpp` |
| RAM interne libre avant la première requête météo (*carte*) | 154,4 Ko avec les réglages mémoire du jalon 1b ; 44,9 Ko sans eux, et la carte redémarre alors (voir [depannage.md](../depannage.md)) | journal série |
| RAM interne libre après 154 s (*carte*) | 105,9 Ko, minimum depuis le démarrage 96,0 Ko, stable entre 64 s et 154 s | commande `memoire` |
| Redessin sur la carte | 10 premières secondes : 3 images à 215 ms en moyenne (cache de glyphes vide) ; un changement de minute : 1 image, 10 zones, 82 ms. L'ancien écran : environ 24 ms | journal série, ligne `tab5_orientation` |

Le P4 est bien plus lent que le PC : un changement de minute coûte 82 ms sur la carte, environ trois fois l'ancien écran, ce qui
est acceptable pour un écran qui change une fois par minute (la réponse au toucher du bouton Réglages, qui ne redessine que sa
propre zone, n'est **pas mesurée**). La conception limite les dégâts : un objet de dessin par zone, redessiné seulement si son
contenu a changé (une empreinte par zone), donc seule l'heure est redessinée chaque minute.

> **Mise à jour du 08/11/2026 — l'accueil est refait** (thèmes Ciel, Tableau sombre, Clair ; [ADR-0016](../decisions/0016-accueil-aere-pluie-et-alerte-en-bandeau.md),
> chiffres dans [mesures/accueil.md](../mesures/accueil.md)). Les sept tuiles de grandeurs et l'ancienne bande heure par heure ont disparu :
> les zones sont maintenant la barre (lieu, bouton Réglages), l'heure, la météo du moment, la courbe des 12 heures, le bandeau (pluie à
> venir et alerte officielle) et cinq jours. **Objets LVGL : toujours 10** (la zone du bandeau remplace celle des tuiles), **polices : 6**
> (l'heure 168 px, la température 54 px, la date 36 px, les textes des jours 32 et 28 px, le petit texte 24 px ; le tableau ci-dessus
> compte 7 polices pour l'ancienne composition). Le thème Horloge ne change pas. Les lignes *carte* du coût ci-dessus décrivent
> l'ancien accueil : le nouveau n'a **pas été mesuré sur une carte**.

> **Mise à jour du 08/11/2026 — plus de bouton Réglages** ([ADR-0021](../decisions/0021-roue-d-appui-long-sur-l-ecran-principal.md), chiffres dans [mesures/roue.md](../mesures/roue.md)). Le bouton rond au bout droit de la barre du lieu a disparu, et la place qu'il réservait aussi : le lieu a toute la barre. **Objets LVGL de l'accueil : 9** (écran, six zones, pastille d'alerte, anneau tactile). Un appui long (400 ms) n'importe où sur l'écran ouvre la roue **au doigt** : « Réglages » et, quand la page des alertes existe, « Alertes » (un objet LVGL de plus tant qu'elle est ouverte, aucun sinon). Le diagnostic s'atteint par Réglages > Appareil > Diagnostic. Vu sur des rendus du PC seulement ; **pas vu sur une carte**.

**Vu sur :** rendu PC (clang 23.1.5, LVGL 9.5.0), commits de la branche `claude/ecran-visuel-ba722a`, 07/11/2026 ; les lignes
*carte* : Tab5 avec l'écran ST7121 (MAC lue sur la puce, non recopiée ici), ESP32-P4, commit `cd362eb` compilé avec les réglages
mémoire du jalon 1b ajoutés dans un dossier de compilation à part (jamais commités), 07/11/2026 de 01 h 57 à 02 h 02.

**Reproduire :**

```powershell
. .\tools\trouver_outils.ps1; . "$(Trouver-Outils)\env_hote.ps1"
.\tools\rendre.ps1 -Scene principal -Planche            # 13 variantes, fr de tr, trois tailles + planches -> docs\captures\ecran\
.\tools\rendre.ps1 -Scene principal-themes -Langue fr -Planche -Variante ciel-soleil,ciel-pluie,ciel-neige,ciel-orage,ciel-brouillard,ciel-nuit,ciel-chaleur,ciel-pires
python test\host\lancer.py --seulement visuel --mutations   # tests du noyau pur et mutations
```
