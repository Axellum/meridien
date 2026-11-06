# Orientation paysage du Tab5 — rotation logicielle de LVGL par le PPA

> **Summary (EN)** — Landscape orientation on the M5Stack Tab5 (ST7121 display): the interface is now 1280 × 720 on the 720 × 1280 portrait panel. Same principle as the author's ESPHome firmware (`rotation: 270`): LVGL renders landscape, each dirty area is rotated by the ESP32-P4's PPA accelerator and copied to the single DSI frame buffer; touch is rotated in the touch layer. Done through `esp_lvgl_port` (`sw_rotate` + `CONFIG_LVGL_PORT_ENABLE_PPA`), `lv_display_set_rotation(90)` in LVGL's convention = ESPHome's 270. A working document in French; every "Vu sur" line gives board, screen revision, commit and date.
> - Hardware rotation (panel swap/mirror) is not an option: `esp_lcd_panel_swap_xy` answers "not supported" on the DSI panel (seen in the 1a log) and espp reports it non-functional on the ST7121.
> - Status of this page: **sources read, formulas derived and measured on the Tab5 ST7121 on 06/11/2026** (§ 5: PPA 1.8 times faster than the CPU on a 1280 × 14 band, no redraw on a static screen, touch correct after a double-rotation fix; tearing, fluency under a long drag, the 7B and portrait are **not** judged). § 6: the main screen was rendered on the PC (LVGL 9.5.0, x86-64): it fits at 1280 × 720 and, after a scale fix, at 1024 × 600; that render is not a photo of the board.

Piste ecran-principal, branche `claude/ecran-principal-f0f722`, tirée du jalon 1b (`97520e2`). Carte : M5Stack Tab5 (écran ST7121).

Règle de lecture : tout ce qui est écrit sans la mention « constaté » est une décision ou une lecture de source, pas une mesure. Ce qui n'a pas été observé est écrit **non vérifié**.

## 1. Comment ESPHome obtient la rotation 270 sur cette carte (sources lues le 06/11/2026)

| Fait | Source |
| --- | --- |
| La dalle est portrait natif 720 × 1280 (`carte_dalle_l` / `carte_dalle_h`), l'interface est paysage 1280 × 720 (`carte_ecran_l` / `carte_ecran_h`), `carte_rotation: "270"` | `00ProjetTab/Tab5/paquets/geometrie-tab5.yaml` (lecture seule) |
| `lvgl: rotation: ${carte_rotation}` ; ESPHome ne compile le support de rotation (`ROTATION_SOFTWARE`, tampon de rotation, client PPA) que si cette clé existe | `tab5-styles.yaml`, bloc `lvgl:` |
| La rotation est **logicielle** : à chaque flush, `LvglComponent::draw_buffer_` fait tourner la zone avec le PPA du P4 (`ppa_do_scale_rotate_mirror`, mode bloquant, RGB565) et, si le PPA échoue, avec une boucle du processeur ; puis `draw_pixels_at` la copie dans le tampon d'image de la dalle | `esphome/components/lvgl/lvgl_esphome.cpp` de la version 2026.9.1 installée (`ppa_rotate_`, `draw_buffer_`) |
| Pour `rotation: 270` : angle PPA `PPA_SRM_ROTATION_ANGLE_90` (« 270° horaire = 90° anti-horaire »), position de la zone `x1 = y1`, `y1 = hauteur_native − x1 − largeur` | idem |
| Le tactile n'est pas touché par le pilote : `LVTouchListener` appelle `rotate_coordinates` à chaque lecture. Pour 270 : `x_logique = hauteur_native − y_brut − 1`, `y_logique = x_brut` | idem ; ADR-0012 du dépôt Tab5, point 3 |
| Coût mesuré par le dépôt Tab5 : envoyer un écran entier (1,8 Mo tourné puis copié en PSRAM) ≈ 59 ms fixes ; une image du panneau tournant (petite zone) ≈ 8 ms ; en portrait sans rotation 1,2 ms | `00ProjetTab/docs/performance.md` (29/09/2026, une tablette ST7123) |
| Tampon : `buffer_size: 100%` (un seul tampon plein écran en PSRAM), un seul tampon d'image DSI | `tab5-styles.yaml`, `performance.md` (« single buffer ») |

Donc : **pas de rotation matérielle, pas de double tampon d'image, pas de synchronisation verticale** — ESPHome écrit chaque zone tournée directement dans l'unique tampon d'image pendant que le DSI le balaie. C'est ce rendu-là (sans déchirure vue par Axel sur ses tablettes en paysage) qui sert de référence.

## 2. Ce que fait `esp_lvgl_port` 2.9.0 (sources lues dans `managed_components/`)

- `lvgl_port_display_cfg_t.flags.sw_rotate = 1` : le port ne touche plus à la dalle (`lvgl_port_disp_rotation_update` rend la main avant `esp_lcd_panel_swap_xy`, ce qui supprime aussi l'erreur « swap_xy is not supported » vue en 1a) et fait la rotation dans son flush.
- Avec `CONFIG_LVGL_PORT_ENABLE_PPA=y` (`Kconfig` du composant), la rotation passe par `lvgl_port_ppa_rotate` (`ppa_do_scale_rotate_mirror`, bloquant, tampon de sortie aligné sur la ligne de cache, de la taille d'un tampon de dessin). Sans cette option, le port tourne les zones avec `lv_draw_sw_rotate` (processeur).
- Le PPA n'impose d'alignement qu'au tampon de **sortie** (adresse et taille multiples de la ligne de cache) ; l'entrée (tampon de dessin LVGL, aligné sur 4 octets seulement ici : `CONFIG_LV_DRAW_BUF_ALIGN=4`) est synchronisée avec `ESP_CACHE_MSYNC_FLAG_UNALIGNED` (`esp_driver_ppa/src/ppa_srm.c`, lu). Pas de contrainte de parité en RGB565.
- **Piège repéré dans le code du port** : si le PPA répond une erreur, le port **n'écrit rien** dans le journal et envoie quand même la zone non tournée au mauvais endroit (`if (err == ESP_OK) {…}` sans `else`). Un PPA en panne se verrait donc comme une image brouillée, pas comme une ligne d'erreur du port (le pilote PPA, lui, écrit sa propre erreur `PPA` dans le journal).

## 3. Formules (dérivées des sources ci-dessus, pas mesurées)

Notation : `(x, y)` repère logique 1280 × 720 de LVGL ; `(X, Y)` repère natif de la dalle 720 × 1280.

| Élément | ESPHome `rotation: 270` | esp_lvgl_port `LV_DISPLAY_ROTATION_90` | Accord |
| --- | --- | --- | --- |
| Zone envoyée à la dalle | `X1 = y1`, `Y1 = 1280 − x1 − largeur` | `lcd_ppa.c` : `X1 = y1`, `Y2 = hres_logique − x1 − 1` | oui (mêmes pixels) |
| Rotation des pixels | PPA `ANGLE_90` (anti-horaire) | `rotation` = valeur LVGL 90 = `PPA_SRM_ROTATION_ANGLE_90` | oui |
| Tactile | `x = 1279 − Y`, `y = X` (fait par ESPHome lui-même) | **fait par LVGL**, pas par `esp_lcd_touch` : `lv_indev.c` (`indev_pointer_proc`) appelle `lv_display_rotate_point`, qui pour 90° donne `x = ver_res − Y − 1 = 1279 − Y`, `y = X`. `esp_lcd_touch` doit donc rendre le repère **natif**, sans miroir ni échange | oui (source lue) ; **voir l'erreur du 06/11 ci-dessous** |

**Erreur corrigée le 06/11/2026.** La première version de ce document et du firmware (commit `6a0e6cc`) disait que LVGL ne tourne pas le tactile et appliquait un miroir Y + un échange X/Y dans `esp_lcd_touch` : le point était alors tourné **deux fois**. Constaté par Axel sur le Tab5 avec le binaire du commit `102077b` : l'anneau n'était « pas au bon endroit, totalement décalé par rapport au doigt ». Calcul : un doigt en `(x, y)` logique donnait un anneau en `(1279 − y, x)`, hors écran dès que `x > 719`. La rotation du tactile n'est plus faite à l'étage `esp_lcd_touch`.

Conséquence : `lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90)` dans la convention de LVGL donne le **même sens** que le `rotation: 270` d'ESPHome. Le haut de l'image logique est le bord **gauche** natif de la dalle ; pour qui tient la tablette en paysage, c'est la tablette tournée d'un quart de tour dans le sens horaire depuis sa position portrait. **Le sens a été constaté sur la carte par Axel** (« c'est droit », § 5) ; si une autre carte montrait l'image à l'envers, il faudrait passer `TAB5_UI_ROTATION_DEGREES` à 270 (un seul réglage, tactile compris).

## 4. Choix à faire par la mesure

Voir le § 5 pour ce qui sera relevé. Candidats :

| Approche | Pour | Contre |
| --- | --- | --- |
| **A. LVGL partiel + PPA + un seul tampon d'image (retenu au départ)** | c'est ce qu'ESPHome fait déjà sur cette tablette, sans déchirure vue ; seules les zones qui changent sont envoyées (une horloge qui change à la minute envoie quelques centaines de pixels) | une grande zone qui change (écran entier) peut montrer une déchirure : le balayage DSI lit le tampon pendant qu'on y écrit |
| B. Même chose avec rotation au processeur (sans PPA) | aucune dépendance au PPA | plus lent et occupe un cœur ; le banc de démarrage le chiffre |
| C. Rotation matérielle de la dalle | gratuite | non supportée sur ST7121 (journal 1a, espp) |
| D. Double tampon DSI + rendu plein écran (`avoid_tearing`) | pas de déchirure | tampons de 1,8 Mo × 3 en PSRAM, rendu et rotation de l'écran entier à chaque changement (≈ 59 ms d'après ESPHome), et la combinaison avec la rotation du port n'est pas prévue : **non essayé** |

## 5. Mesures

**Vu sur** Tab5 (écran ST7121), journal série relevé sur la carte, 06/11/2026. Binaires (commit propre sauf mention) : `102077b` (21:43), `557eb44` (22:25), `c1fa7f6` (22:27), `3b68f42` en version d'essai « autotest du tactile » (22:31) et `9f82623` (22:38, version courante). La MAC n'est pas écrite dans le dépôt. Chaque ligne dit ce qui a été vu et par qui ; ce qui n'a pas été vu est écrit tel quel.

| Mesure | Constaté | Source |
| --- | --- | --- |
| Banc de rotation d'une bande de 1280 × 14 px (moyenne de 20) | PPA **362 à 363 µs**, processeur **649 à 652 µs** : le PPA est environ 1,8 fois plus rapide. La bande de 28 lignes ne tient pas dans la mémoire utilisable par le DMA (le banc l'avait écrit « mémoire insuffisante ») ; le banc essaie donc 28, 14 puis 7 lignes. | ligne `banc de rotation` du journal, démarrages de 22:34 et 22:38 |
| Premier affichage de l'écran principal | 2 images, 34 zones de 35 418 px en moyenne ; rendu par image 44 à 47 ms (max 61 ms) ; envoi d'une zone (rotation PPA + copie) 0,72 ms en moyenne, 1,3 ms au pire | ligne `affichage sur … ms`, six démarrages |
| Écran fixe | **aucune image dessinée** entre les changements de minute : 1 image en 47 s (22:38:13 → 22:39:00) puis 1 image en 60 s (22:40:00). Avant le correctif (`102077b`) : 21 images toutes les 10 s, soit un redessin deux fois par seconde (cause : la couleur du libellé était réécrite à chaque tick, ce qui invalide l'objet même si la valeur ne change pas). | journal, `102077b` contre `9f82623` |
| Changement de minute | 1 image, 8 zones de 35 328 px ; rendu 20 à 26 ms ; envoi 0,7 à 0,75 ms par zone (max 1,2 ms) | journal, `9f82623`, 22:39:00 et 22:40:00 |
| Pendant des appuis (essai d'Axel) | 77 à 94 images en 10 s (8 à 9 images par seconde), rendu moyen 5,8 à 8,3 ms, **max 94 ms**, envoi 0,54 ms par zone. Ce n'est pas un glissement lent soutenu : la fluidité d'un doigt qui glisse pendant 10 s **n'est pas mesurée**. | journal, `3b68f42` autotest, 22:32 |
| Sens de l'image | « Oui, c'est droit » : le haut de l'interface est du bon côté. Vu par Axel sur l'écran, relayé par la session principale vers 21:53. | Axel, `102077b` |
| Tactile avant correctif | « pas au bon endroit, totalement décalé par rapport au doigt » (Axel, ~21:58) ; anneau à gauche quand le doigt est à droite (~22:02). Cause : point tourné deux fois (voir § 3). | Axel, `102077b` |
| Tactile après correctif, sans doigt | **autotest 6/6 « OK »** : six appuis simulés (quatre coins, centre, bouton Réglages) injectés en coordonnées brutes de la dalle ressortent au bon point logique (par exemple brut (0, 1279) → logique (0, 0) ; brut (719, 0) → logique (1279, 719)). | journal, `3b68f42` autotest, 22:31:40 à 22:31:44 |
| Tactile après correctif, avec le doigt | 27 appuis réels journalisés sans plantage ; la formule x = 1279 − Y brut, y = X brut se vérifie sur toutes les lignes lues. Axel : « les doigts étaient bons ». | journal et Axel, `3b68f42` autotest, 22:32 |
| Appui sur l'écran | `557eb44` redémarrait à chaque appui : le journal par appui lisait l'objet touché à la place de l'indev (cible et paramètre inversés dans `lv_indev_send_event`). Corrigé dans `c1fa7f6`, exercé ensuite par l'autotest et par les appuis réels. | journal, voir `docs/depannage.md` |

Non constaté à cette date :

- **Déchirure** : à juger par Axel sur l'écran réel (un changement entier : appui long qui bascule vers l'écran de diagnostic ; un glissement du doigt de 10 s). Le journal ne la voit pas.
- **Bouton « Réglages »** : le chemin d'événement est exécuté (appui n° 6 de l'autotest et deux appuis réels dans le bouton, sans plantage) ; la bannière « Réglages : bientôt » n'a été vue par personne.
- **Waveshare 7B** et **orientation portrait** : jamais essayées sur la carte.
- Le bloc météo avec `minmax_connus` à faux n'est pas exercé sur la carte (le bouchon met toujours la valeur à vrai).

## 6. Rendu PC de l'écran principal (avant carte)

Ce rendu est le dessin logiciel de LVGL 9.5.0 sur x86-64, compilé avec la configuration du firmware par l'outil de la session `rendu-pc` (sa branche, non fusionnée au moment de l'essai ; une scène `principal` écrite pour l'occasion, avec des données inventées). **Ce n'est pas une photo du Tab5** : la dalle, la rotation par le PPA, le tactile et les temps d'affichage ne sont pas couverts.

**Vu sur** PC de l'auteur, rendu LVGL 9.5.0 (configuration identique au `sdkconfig`, 106 options), commit `44149ff`, 06/11/2026.

- 1280 × 720 : l'heure, la date et le bloc météo tiennent, y compris dans les pires cas essayés (heure à chiffres larges « 20:08 », date la plus longue « mercredi 30 septembre 2026 », libellé de 39 octets sur trois lignes, températures négatives, « Données périmées » et « Exemple » affichés ensemble).
- 1024 × 600 (Waveshare 7B, **non essayée sur la carte**) : avant le correctif d'échelle l'heure de 270 px était **coupée** (« 1:4( ») ; après la mise à l'échelle des cotes (facteur 0,8) elle tient. Constaté sur le rendu PC seulement.
- Libellé météo rempli jusqu'au bout sans zéro final (hors contrat) : l'écran dit « Météo indisponible » pour le libellé au lieu de lire au-delà du champ.
- Un premier rendu « pires cas » montrait une mention « Exemple » parasite : cause **dans la scène de rendu** (un libellé de 45 octets écrasait le champ voisin de la structure), pas dans l'écran ; scène corrigée.

Ce qui reste **non vérifié** : ce qui dépend de la carte et n’est pas dans le tableau du § 5 (déchirure, glissement soutenu, 7B, portrait).
