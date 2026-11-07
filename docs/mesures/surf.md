# Surf : ce que les données d'Open-Meteo donnent au noyau, et ce qui reste à prouver — PC seulement, aucune carte

> **Summary (EN)** — The pure core of the future "surf" page was run on **15 real Open-Meteo answers** (Marine API and forecast
> API, captured 2026-11-07 about 12:51 UTC, 30 files kept in `test/host/donnees/`), on the PC only.
> - **Measured**: all 15 answers are read (54 hours each); the off-sea position (Paris) comes back with every sea variable `null` and
>   the core keeps it as "no data", never 0; the lake (Michigan) has waves but no sea temperature and no sea level; the 11 ocean
>   coasts have a tide that passes the 30 cm filter (the weakest, Oahu, 68 cm), the Baltic (20 cm) and the Mediterranean (27 cm)
>   do not. The **margin is thin** (3 cm above Marseille) and rests on **one day** of capture. The wave cell the API returns is
>   1.3 to 16.2 km from the place asked, so heights are open-sea estimates. 164 host tests (AddressSanitizer + UBSan) pass; 142
>   targeted mutations are all detected; the 15 places agree with an independent Python reading.
> - **Not verified**: how well the score matches the real quality of any real spot (a comparison with the owner's own spots is
>   proposed in section 7), the beach orientations of the test places (taken from a map), the tide filter on other days, the
>   Beaufort figures behind the calm threshold, the licence wording of each wave-model provider, **everything on a board**.

**Vu sur** : PC seulement (Windows 11, clang++ 23.1.5 avec AddressSanitizer + UBSan via `env_hote.ps1`, Python 3), aucune carte ; réponses réelles capturées le 07/11/2026 de 12:51:27 à 12:52:00 UTC (UNIX 1 794 055 887 à 1 794 055 920) depuis le PC d'Axel, première heure de chaque réponse : 06:00 UTC ; branche `claude/surf-noyau-c4002d`, base `4b36c71`, code essayé : commit `0125743` (données de test : `43e4a84`). Aucune MAC ni adresse IP ici.

Règle de lecture : « mesuré » = relu sur une sortie d'outil de cette session ; tout le reste est une lecture de code ou **non vérifié**. Le détail des règles est dans [l'ADR-0013](../decisions/0013-surf-donnees-note-et-limites.md).

## 1. Question

Les réponses réelles de l'API Marine et de l'API de prévision d'Open-Meteo, lues par le noyau pur du surf, donnent-elles des valeurs exploitables partout dans le monde (y compris là où il n'y a pas de mer), et que valent les règles de vent, de marée, de note et de créneau sur ces données ?

## 2. Mesuré : quinze lieux, deux réponses chacun

Orientation de plage = choix du test, tiré d'une carte, **jamais vérifié sur le terrain**. Fourchette de hauteur = celle par défaut (1,00 à 2,50 m). « Maintenant » = l'instant de capture (12:51 UTC). Valeurs calculées par la lecture indépendante en Python (`controle_python_surf.py`) ; le noyau C++ donne les mêmes (test `test_surf_reponses.cpp`).

| Lieu | Décalage | Houle ou vagues (m) | Période (s) | Vent (km/h, d'où) | Classe | Points | Marée (extrema / marnage) | Créneau du jour (heure locale) |
|---|---|---|---|---|---|---|---|---|
| Hossegor | +1 h | 0,60 | 9,6 | 10 (122°) | terre | 62 | 9 / 332 cm | 15 h–21 h (63 pts) |
| Pipeline (Oahu) | −10 h | 1,62 | 10,1 | 19 (62°) | travers | 71 | 4 / 68 cm | 06 h–11 h (68 pts) |
| Snapper Rocks | +11 h | 0,58 | 4,0 | 15 (326°) | terre | 47 | 9 / 162 cm | aucun (23:51 locale) |
| Nazaré | 0 h | 0,56 | 8,9 | 18 (145°) | terre | 58 | 9 / 266 cm | 14 h–20 h (62 pts) |
| Jeffreys Bay | +2 h | 1,44 | 9,0 | 26 (221°) | travers | 59 | 8 / 184 cm | 18 h–21 h (74 pts) |
| Uluwatu | +8 h | 1,70 | 10,7 | 6 (170°) | calme | 93 | 8 / 239 cm | aucun (20:51 locale) |
| Muizenberg | +2 h | 2,44 | 9,9 | 20 (152°) | mer | 55 | 8 / 148 cm | 14 h–20 h (55 pts) |
| Punta de Lobos | −3 h | 2,34 | 8,7 | 16 (169°) | travers | 72 | 8 / 130 cm | 19 h–21 h (75 pts) |
| Macaronis | +7 h | 1,34 | 12,7 | 18 (124°) | travers | 88 | 9 / 93 cm | 19 h–21 h (94 pts) |
| Lahinch | 0 h | 0,48 | 8,7 | 3 (94°) | calme | 52 | 9 / 370 cm | 12 h–18 h (54 pts) |
| Warnemünde (Baltique) | +1 h | 0,08 | 2,2 | 16 (129°) | terre | 35 | non notable | 13 h–19 h (33 pts) |
| Marseille | +1 h | 0,06 | 3,1 | 10 (272°) | mer | 29 | non notable | 15 h–21 h (32 pts) |
| Saint-Malo | +1 h | 0,14 | 8,8 | 10 (165°) | terre | 47 | 8 / 992 cm | 13 h–19 h (47 pts) |
| Chicago (lac Michigan) | −6 h | 0,76 | 4,6 | 23 (293°) | terre | 59 | non notable (pas de niveau de mer) | 06 h–11 h (54 pts) |
| Paris (hors mer) | +1 h | — | — | 4 (185°) | calme | — | non notable | aucun |

Lecture :
- **Une valeur absente reste absente.** À Paris les onze variables de mer sont `null` (mesuré) : le noyau ne rend aucune note (« — »), jamais un 0. À Chicago la température de l'eau et le niveau de mer sont `null` mais les vagues sont là : la note existe, la marée n'est pas dite.
- **La note dit « bon jour / mauvais jour » avec des nuances raisonnables, mais cela ne prouve rien sur la qualité réelle.** Uluwatu à 93 points = mer à 1,70 m, période de 10,7 s, vent de 6 km/h (calme) ; la Baltique à 35 = clapot de 8 cm, période de 2,2 s. Ce sont des cas où la règle donne un résultat que l'on devine ; **aucun n'a été comparé à ce qu'on voit sur place** (voir section 7).
- **Le créneau suit l'heure locale du lieu** : à Snapper Rocks il est 23:51 locale et à Uluwatu 20:51, il n'y a plus d'heure à noter aujourd'hui (le créneau « demain » existe : test `a_gold_coast_et_uluwatu_il_est_trop_tard_pour_aujourd_hui`).
- **Open-Meteo a rendu `Australia/Sydney` (+11 h, heure d'été) pour Snapper Rocks** (champ `timezone` des deux réponses, mesuré). Que ce lieu, près de la frontière du Queensland, doive être en +11 h ou en +10 h n'est **pas vérifié** : si la zone rendue est celle de l'autre côté de la frontière, la fenêtre « 6 h à 21 h » serait décalée d'une heure.

## 3. Mesuré : la marée et le seuil de 30 cm

Variation du niveau de mer (plus haut moins plus bas de la série de 54 heures, `sea_level_height_msl`) :

| Lieu | Variation sur 54 h | Marée « notable » (filtre 30 cm) |
|---|---|---|
| Warnemünde (Baltique) | 20 cm | non |
| Marseille (Méditerranée) | 27 cm | non |
| Pipeline (Oahu) | 68 cm (marnage des extrema gardés : 68 cm) | oui |
| Dix autres littoraux océaniques | 93 cm (Macaronis) à 992 cm (Saint-Malo) de marnage | oui |

Le seuil de **30 cm** tombe entre 27 et 68. **Marge de 3 cm au-dessus de Marseille, une seule journée de capture** : le seuil est un choix d'ingénieur pour ces données (le test `le_seuil_de_30_cm_separe_les_mers_sans_maree_des_marees_mesurees` le rejoue), pas une propriété de l'océan. Une marée de morte-eau sur un littoral à marée faible peut passer dessous, et une mer fermée peut passer dessus un jour de tempête : **non vérifié**. Le niveau est rapporté au niveau moyen global de la mer, pas au zéro des cartes marines : la page d'Open-Meteo écrit qu'il ne remplace pas un annuaire des marées (page lue le 07/11/2026). Les instants sont à l'heure près (série horaire, aucune interpolation). Toute phrase affichée doit dire « indicatif ».

## 4. Mesuré : la case lue n'est pas le spot

Distance entre la position demandée et la position que l'API rend dans la réponse (haversine) :

| | Plus proche | Plus loin |
|---|---|---|
| API Marine (maille des vagues) | 1,3 km (Macaronis) | 16,2 km (Snapper Rocks) ; 15,8 km Saint-Malo, 14,1 km Pipeline, 12,7 km Warnemünde, 12,0 km Nazaré |
| API de prévision (vent) | 0,0 km (Saint-Malo) | 7,9 km (Uluwatu) |

Les hauteurs sont donc celles d'une case de mer qui peut être à plus de dix kilomètres du spot : des **estimations du large**, qui ne voient ni le banc de sable ni le récif. C'est écrit dans l'ADR et dans les en-têtes du noyau.

## 5. Mesuré : tailles

- Adresse de la mer : 385 caractères à Hossegor, 389 octets au pire (latitude −90 et longitude −180, zéro final compris) ; adresse du vent : 216 caractères, 220 octets au pire. Les deux restent sous 512 octets, borne que le noyau s'impose (test à la compilation) ; ce que le client HTTP de la carte accepte vraiment n'est **pas vérifié ici**.
- Réponses : mer de 4 079 à 4 309 octets, vent de 1 614 à 1 726 octets (texte décompressé, 54 heures). La mémoire et le temps de lecture **sur le P4 ne sont pas mesurés**.
- Tests : la comparaison des quinze réponses avec les valeurs attendues est exécutée, pas évaluée à la compilation : en `static_assert` le fichier de test mettait environ 406 s à compiler contre 43 s à l'exécution (clang++ 23.1.5 avec les assainisseurs, même PC).

## 6. Mesuré : les tests

| Fichier | Tests | Sujet |
|---|---|---|
| `test_surf_spot.cpp` | 15 | validité d'un spot, briques de `surf_base.hpp` |
| `test_surf_requete.cpp` | 11 | adresses exactes, bornes, tampons |
| `test_surf_vent.cpp` | 19 | cosinus, classes, composante, calme, rafales |
| `test_surf_maree.cpp` | 20 | extrema, paliers, trous, filtre 30 cm, tendance |
| `test_surf_note.cpp` | 50 | barème, verdicts, classement, créneau |
| `test_surf_lecture.cpp` | 18 | lecture de réponses fabriquées (refus, arrondis, unités) |
| `test_surf_reponses.cpp` | 31 | les quinze réponses réelles et des variantes altérées |
| **Total** | **164** | `python test/host/lancer.py --seulement test_surf` : « complet (compilé + exécuté) », 0 échec |

**Mutations** : 142 mutations ciblées sur les règles écrites dans les en-têtes (un arrondi, une borne incluse ou exclue, un seuil, une unité, l'ordre d'un tri, une valeur absente) dans `test/host/mutations_surf.py` ; `python test/host/lancer.py --mutations --seulement test_surf` : **142 détectées, 0 problème**, chacune par le test visé. Elles ne disent pas que « tout » est testé : seulement que ces règles-là le sont. Quatre ont d'abord posé problème : trois révélaient un test trop faible (voisine de gauche du créneau à exactement 10 points de moins, « aujourd'hui » compté en UTC et non en heure locale, fin de série de marée) et une portait sur une ligne redondante du tri des spots (mutant équivalent : la ligne a été supprimée). Les tests ont été renforcés et la campagne relancée.

**Seconde lecture en Python** : `python test/host/controle_python_surf.py --verifier` : « tout est à jour ». Elle est indépendante pour la lecture du JSON (module `decimal`), le cosinus (`math.cos`) et la marée (autre méthode) ; **pas** pour le barème et le créneau, récrits à partir du même texte par la même personne.

## 7. Non vérifié, et comment le vérifier

- **La qualité réelle d'un spot.** Rien ne compare la note à ce qu'on voit. *Proposition* : pendant une à deux semaines, sur deux ou trois spots qu'Axel connaît bien, noter chaque session de 0 à 10 et relever les conditions réelles (hauteur vue, vent, marée) ; comparer à la note du jour et au créneau proposé ; ajuster la fourchette de hauteur, les seuils de période et les classes de vent là où l'écart se répète. Tant que ce n'est pas fait, l'écran doit parler de « conditions estimées ».
- **La direction de la houle par rapport à la plage** n'est pas modélisée (limite écrite, fixée par le test `la_direction_de_la_houle_ne_change_pas_la_note_limite_ecrite`).
- **Les orientations de plage** des quinze lieux viennent d'une carte, jamais du terrain.
- **Une seule journée** de capture : pas de tempête, pas de vent de 80 km/h, pas de vive-eau ciblée (ces cas sont fabriqués à la main dans les tests, pas mesurés).
- **Les chiffres de l'échelle de Beaufort** derrière le seuil de calme de 10 km/h n'ont pas été relus à la source ; c'est un choix maison.
- **La licence de chaque fournisseur de modèles de vagues** derrière l'API Marine n'a pas été examinée (seule la CC BY 4.0 de l'API est citée).
- **Tout ce qui touche la carte** : client HTTP, mémoire et temps de lecture sur le P4, affichage. Rien n'est compilé pour une carte ni flashé.

## 8. Reproduire

```powershell
# Tests du noyau (outils : docs/outils.md), mode « complet » exigé
. .\tools\trouver_outils.ps1; . "$(Trouver-Outils)\env_hote.ps1" -Sanitizers
python test/host/lancer.py --seulement test_surf
python test/host/lancer.py --mutations --seulement test_surf
python test/host/controle_python_surf.py --verifier
```

La capture (`python test/host/capturer_surf.py`) appelle le réseau : à lancer à la main ; elle écrase les réponses gardées et change les valeurs attendues.
