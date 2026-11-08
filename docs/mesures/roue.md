# La roue d'appui long (LVGL) — ce qui est mesuré, ce qui ne l'est pas

> **Summary (EN)** — The press-and-hold wheel drawn on the main screen ([ADR-0021](../decisions/0021-roue-d-appui-long-sur-l-ecran-principal.md)),
> written on 2026-11-08 and **checked on the PC only**. The open wheel is **1 LVGL object** and 1 timer (the main screen goes from
> 10 to 9 objects: the Settings button is gone), 3 TinyTTF fonts kept in PSRAM, **1 436 bytes** of static RAM for its state (`.data`, RISC-V 32-bit).
> Compiled for both boards (ESP-IDF 5.5.5): the firmware grows by 32 186 bytes of flash and 1 536 bytes of internal RAM (`DIRAM`) on the Tab5, 32 166 and 1 472 on the 7B (about 19 KB of it is the Details page, which the wheel now reaches, so the linker no longer drops it). The hub sits 0 to 162 px from the finger on the Tab5 (0 to 123 px on the 7B) with the three first-ring entries (Settings, Details, Alerts; 86 and 69 px before Details); a
> family that does not fit moves it much further (220 px in a corner, 434 px at the right edge, Tab5, test scene). Running RAM,
> drawing time and looks on the real panel: **not measured** (no board was flashed).

Branche `claude/roue-lvgl-6352a9` (tirée de `main` au commit `4dd7b2c`). Règle de lecture : tout ce qui n'est pas sous « Vu sur » avec une carte est **non constaté sur carte**.

## 1. Ce que la roue coûte

- **Objets LVGL** : roue **fermée 0, ouverte 1** (le voile plein écran qui dessine tout), lu dans `ui_roue_cout()` (`main/ui_roue.cpp`) et écrit dans le journal du banc à chaque rendu. La roue du Tab5 en pose **64** (`tab5_roue.h:37-52`, comptés à l'ADR-0017). **L'écran principal passe de 10 à 9 objets** (le bouton Réglages disparaît), lu dans `ui_principal_cout()`.
- **Minuteurs** : 1 quand la roue est ouverte (la fermeture après 15 s sans toucher), 0 fermée.
- **Polices** : 3 TinyTTF (mot 24 px, icône 40 px, moyeu 52 px de référence, mises à l'échelle de l'écran), en PSRAM, créées à la première ouverture et gardées. **Non mesuré** : la mémoire de leurs glyphes en cache.
- **RAM statique** : l'état de la roue pèse **1 436 octets** dans le firmware (section `.data._ZN5ecran12_GLOBAL__N_11gE` de `ui_roue.cpp.obj`, `riscv32-esp-elf-size -A`, 7B) ; son `sizeof` sur le PC est **2 192 octets** (pointeurs de 8 octets au lieu de 4), c'est ce que `ui_roue_cout().ram_statique` écrit dans le journal du banc : **ne pas le lire comme la valeur de la carte**. (Relevé au commit `b034aac` ; depuis le réglage « Profil » (`ChoixRoue::actif`) : **1 628 octets** sur la carte et 2 576 sur le PC, voir [accueil-profil.md](accueil-profil.md).) Plus une constante `ANNEAU_VIDE` en lecture seule. `.bss` de l'objet : 1 octet.
- **Taille du firmware** : compilé avec ESP-IDF 5.5.5 (GCC 14.2.0), `idf.py size`, au commit `b034aac` (tout le firmware de ce lot, « Détails » compris), sans aucun avertissement du compilateur ; « avant » = `main` au commit `4dd7b2c` dans un worktree jetable (supprimé), compilé de la même façon (ses chiffres sont ceux de `mesures/accueil.md`, retrouvés à l'octet).

  | Carte | Mesure | Avant | Après | Écart |
  | --- | --- | --- | --- | --- |
  | Tab5 | `DIRAM` (RAM interne prise par l'image) | 144 670 o | 146 206 o | +1 536 o |
  | Tab5 | Flash utilisée | 2 855 348 o | 2 887 534 o | +32 186 o |
  | Tab5 | `meridien.bin` | 2 961 104 o | 2 994 768 o | +33 664 o (29 % de l'emplacement libre) |
  | 7B | `DIRAM` | 144 730 o | 146 202 o | +1 472 o |
  | 7B | Flash utilisée | 2 835 166 o | 2 867 332 o | +32 166 o |
  | 7B | `meridien.bin` | 2 941 664 o | 2 975 232 o | +33 568 o (29 % de l'emplacement libre) |

  **La roue seule** (avant l'entrée « Détails ») coûtait +13 062 o de flash et +1 472 o de `DIRAM` au Tab5, +13 002 et +1 408 à la 7B ; **les ~19 KB de plus** (+19 124 o au Tab5, +19 164 o à la 7B, et 64 o de `DIRAM`) sont la page Détails (ADR-0020), qui était dans l'arbre mais non référencée, donc écartée par l'éditeur de liens, et que « Détails » atteint maintenant. Le `DIRAM` gagne l'état de la roue (1 436 o de `.data`, mesurés avant cet ajout, `ui_roue.cpp` n'ayant pas changé depuis, soit l'essentiel des +1 536 o) ; la répartition de la flash par fichier n'a pas été mesurée (le sous-ensemble d'icônes y pèse 1 380 o).
- **Flash** : le sous-ensemble d'icônes `mdi_roue.ttf` pèse **1 380 octets** (4 glyphes) ; sha256 `2265e9fc…dc85` (relu par `python tools/icones/generer_police_roue.py --verifier`).
- **Non mesuré sur carte** : la RAM libre en marche (avant, pendant, après plusieurs ouvertures), le temps de dessin d'une ouverture, le temps entre le toucher et l'apparition de la roue.

## 2. Où la roue se pose : l'éloignement du moyeu

Mesuré sur le banc (premier anneau seul = « Réglages », « Détails » et « Alertes » ; `EXIGENCE_A_L_OUVERTURE = PremierAnneauSeul`). « Écart » = distance entre le doigt et le centre du moyeu, rendue par `ui_roue_position()`.

| Appui | Tab5 1280 × 720 : moyeu, écart | 7B 1024 × 600 : moyeu, écart |
| --- | --- | --- |
| Centre | au doigt, 0 px | au doigt, 0 px |
| Coin haut gauche | boutons dessous, 162 px | boutons dessous, 123 px |
| Coin bas droit | boutons dessus, 49 px | boutons dessus, 38 px |
| Bord droit | boutons dessous, 62 px | boutons dessus, 49 px |
| Bord haut | boutons dessous, 43 px | boutons dessous, 33 px |
| Bord bas | boutons dessus, 42 px | boutons dessus, 33 px |

Avec une famille dépliée (scène d'essai : quatre entrées au premier anneau dont la famille, et huit choix, **qui n'existent pas dans le produit**) : Tab5 — 16 px au centre, **220 px** au coin haut gauche, **434 px** au bord droit, 42 px au bord bas ; 7B — 0 px au centre, **216 px** au coin, **347 px** au bord droit, 33 px au bord bas. **« Détails » a un prix** : avec trois boutons l'éventail est plus large, et au coin haut gauche le moyeu s'éloigne du doigt de 162 px (Tab5) et 123 px (7B) au lieu de 86 et 69 ; les cinq autres appuis du tableau ne changent pas. Ce sont ces sauts que l'on paie avec `PremierAnneauSeul` ; `TousLesAnneaux` les ferait à chaque ouverture, pour 92 % des appuis (264 px en moyenne : mesure de l'ADR-0017, 1280 × 720).

## 3. Tests du noyau (PC)

- `components/core/include/ecran/roue_commande.hpp` : **33 tests** dans `test/host/test_roue_commande.cpp` (échelle de la 7B et du Tab5, seuil de glissement, demande, dépliement, chaque geste de `decider`), évalués à la compilation puis exécutés ; compilés avec AddressSanitizer et UBSan (clang++ 23.1.5, mode « complet (compilé + exécuté) »). **Toute la suite du noyau passe** (1 629 tests exécutés, 0 échec ; relevé par le crochet de commit sur le commit « Détails », après la fusion de la page Détails, ADR-0020 : le décompte n'est plus 1 580 d'avant la fusion).
- **Mutations** (`test/host/mutations_roue_commande.py`) : **32 sur 32 détectées** par le test attendu. Un mutant équivalent n'est pas retenu (les deux diamètres de boutons valent 72 px dans `roue_geometrie.hpp`) ; il est noté en tête du fichier.
- Les tests de la géométrie (ADR-0017) et leurs 64 mutations ne sont pas touchés : `roue_geometrie.hpp` est inchangé.
- **Police d'icônes** : `python tools/icones/generer_police_roue.py --verifier` (sans la police source) constate que la `cmap` du fichier contient exactement les 4 glyphes du manifeste et que chacun a des contours (cloche 3, roue dentée 4, grille de tuiles 8, rose des vents 9) : une icône n'est jamais un rectangle vide. Il ne dit pas que l'icône ressemble à ce qu'on attend : cela se regarde sur les rendus (§ 4) puis sur la carte.

## 4. Rendus (banc de rendu du PC)

LVGL 9.5.0, dessin logiciel, RGB565, clang. `.\tools\rendre.ps1 -Scene roue -Taille 1280x720` puis `-Taille 1024x600` (la liste des tailles se donne en deux lancements : `-File` ne coupe pas une liste à virgules). Variantes : `centre`, `centre-sans-alertes`, `coin-haut-gauche`, `coin-bas-droit`, `bord-droit`, `bord-haut`, `bord-bas`, `famille-centre` (huit choix), `famille-quatre-centre`, `famille-coin`, `famille-bord-droit`, `famille-bord-bas`, et trois parcours. Les PNG gardés dans `docs/captures/ecran/` : `roue-centre`, `roue-coin-haut-gauche`, `roue-bord-droit`, `roue-famille-quatre-centre` en 1280 × 720 et `roue-centre`, `roue-coin-haut-gauche`, `roue-bord-droit` en 1024 × 600 ; le reste se refait en quelques secondes.

**Comparé aux cibles** (`roue-finale.png`, `roue-sans-mer.png`, `roue-affichage.png`, `roue-vignettes.png` de l'esquisse du Tab5) :

1. Même composition : voile sombre sur l'accueil, bande de verre sous chaque anneau, boutons ronds sombres à bord clair, icône blanche, mot blanc en gras à côté du bouton (côté choisi par la géométrie), moyeu au centre avec un anneau clair et un halo, point blanc sur un bouton de famille.
2. **Le premier anneau n'a que trois boutons** (Réglages, Détails, Alertes ; deux sans alertes) : la bande de verre est donc **courte et arrondie** (une gélule), pas l'arc de six boutons de la cible. Les boutons suivent l'arc autour du moyeu : celui du milieu (« Détails ») et son mot sont les plus éloignés du moyeu.
3. Le moyeu porte la rose des vents en **couleur d'accent du thème** (or sur les thèmes d'Axel) ; la cible l'a en cyan. Le halo est un disque translucide plat, **pas un flou**.
4. **Quatre glyphes seulement** (roue dentée, cloche, grille de tuiles, rose des vents) : la cible a une icône par entrée (Profil, Affichage, Pages, Assistant, Décision, Bureau...). Dans les scènes à famille, ces glyphes sont **répétés** (données d'essai) : un « Profil » avec une rose des vents n'est pas le produit. La grille de tuiles de « Détails » se reconnaît nettement sur le fond sombre ; qu'elle dise « les sept grandeurs » est un jugement pour l'écran réel.
5. **Les mots du premier anneau peuvent être recouverts par un bouton du second** : dans la scène à huit choix (`famille-centre`), le mot « Affichage » de la famille est coupé par un bouton du second anneau et « Détails » est touché par un autre ; `ecran::roue` ne vérifie pas la collision entre anneaux (ADR-0021, limites). **Sans effet aujourd'hui** : trois entrées, aucune famille.
6. La bande du second anneau est teintée de l'accent du thème, celle du premier du texte du thème, toutes deux à 14 % d'opacité.
7. À 1024 × 600 (échelle 0,8, `ecran_de`) la roue est la même mise à l'échelle ; dans les six appuis du tableau du § 2, le journal du banc dit « tient » (la géométrie juge que tout tient dans l'écran).

## 5. Parcours simulés (banc)

Le banc envoie `LV_EVENT_LONG_PRESSED` à l'écran principal réel (`ui_principal`) et un rappel qui rejoue le câblage de `ecran_principal.cpp` (le fichier lui-même ne tourne pas sur le PC) : appui long → la roue s'ouvre (1 objet LVGL) → toucher « Réglages » → les réglages s'ouvrent exactement une fois, la roue est fermée (0 objet) ; idem « Alertes » ; toucher à vide → la roue se ferme, rien ne s'ouvre ; une quatrième ouverture puis fermeture ne laisse aucun objet ; famille dépliée → toucher un choix → son action une fois, et « Réglages » reste atteignable famille dépliée. **« Détails »** (`parcours-details`) : appui long → la roue → toucher « Détails » → la page Détails est l'écran actif, ouverte exactement une fois, la roue fermée (0 objet) ; « Fermer » (un `LV_EVENT_CLICKED` envoyé à son seul objet cliquable, pas un toucher) ramène à l'accueil et supprime la page ; la roue se rouvre et « Réglages » s'ouvre une fois. La colle de l'appareil (`ui_details_ouvrir.cpp`) **ne tourne pas** sur le PC : le banc rejoue ses appels (écran neuf, `ui_details_creer`, écran précédent gardé, chargement ; « Fermer » recharge le précédent), la page elle-même (`ui_details.cpp`) est celle de l'appareil ; le vrai rappel et la vraie zone tactile de « Fermer » ne sont **pas constatés**. **Le diagnostic** : le parcours `diagnostic-depuis-les-reglages` constate que la ligne « Diagnostic » est dans Réglages > Appareil, qu'elle se touche et qu'elle demande l'écran de diagnostic une fois ; l'écran de diagnostic et son retour **ne tournent pas sur le banc** : non vérifiés. Le point d'appui du banc est (0, 0) (pas de doigt) : l'ouverture **au point réel du doigt** n'est pas constatée.

**Vu sur :** banc de rendu du PC (clang 23.1.5, LVGL 9.5.0), ESP-IDF 5.5.5 pour la compilation, branche `claude/roue-lvgl-6352a9`, 08/11/2026. **Aucune carte** : le dessin sur le panneau (Tab5 ST7121, ST7123, ILI9881C, Waveshare 7B), l'ouverture sous un vrai doigt, le seuil de glissement, la lisibilité à 2 m, la vitesse et la RAM libre en marche ne sont **pas constatés**.
