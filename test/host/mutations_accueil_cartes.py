"""Mutations des cartes du mode automatique de l'accueil (components/core/include/ecran/accueil_cartes.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_ACCUEIL_AUTO : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test qui
doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou « détectée par
autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle écrite dans l'en-tête et l'ADR-0023 : un rectangle d'une composition (position, taille, écart), le choix de
la composition, ce que dit la carte pluie (début de l'épisode, pic, durée, cumul, barres, échelle) ou le dessin d'une barre.
Choisies à la main : elles ne disent pas que « tout » est testé, seulement que ces règles-là le sont.

NE FIGURENT PAS (le code change, le comportement non) :
  - `if (b.dix_mm > fort)` en `>=` : seul le maximum compte, pas la première barre qui l'atteint ;
  - `!visuel::connu(dix_mm) ||` dans `hauteur_de_barre` : l'inconnu (-32768) est déjà négatif, `dix_mm <= 0` le couvre ;
  - `visuel::connu(episode.cumul_dix_mm) &&` et `episode.cumul_dix_mm > 0` : un cumul connu d'un épisode est toujours positif (chaque heure
    compte au moins 0,1 mm) et l'inconnu est négatif ;
  - `episode.fin_s > episode.debut_s` : toute pluie qui a une durée a une fin après son début (fin_s = 0 signifie « pas de cumul »).
"""

T = "test_accueil_cartes.cpp"
A = "accueil_cartes.hpp"

MUTATIONS_ACCUEIL_CARTES = [
    # --- les constantes ---
    (A, "ECART_CARTES = 16;", "ECART_CARTES = 12;", T, "les_constantes_des_cartes", "l'écart entre les cartes passe à 12 px"),
    (A, "BANDE_JOURS_H = 92;", "BANDE_JOURS_H = 80;", T, "les_constantes_des_cartes", "la bande des cinq jours perd 12 px"),
    (A, "TUILE_PLUIE_W = 360;", "TUILE_PLUIE_W = 300;", T, "les_constantes_des_cartes", "la tuile de pluie perd 60 px de large"),
    (A, "TUILE_PLUIE_H = 88;", "TUILE_PLUIE_H = 70;", T, "les_constantes_des_cartes", "la tuile de pluie (portrait) perd 18 px de haut"),
    # clang-format met la valeur sur la ligne d'après (`ALERTE_H_MIN =` / `    150; // …`) : on ne mute que la ligne de la valeur.
    (A, "    150; // sous cette hauteur", "    100; // sous cette hauteur", T, "les_constantes_des_cartes", "la carte d'alerte peut descendre à 100 px"),
    (A, "    12; // l'écran en montre neuf", "    9; // l'écran en montre neuf", T, "les_constantes_des_cartes", "la carte ne montre plus que 9 barres au plus"),

    # --- la composition ---
    (A, "    if (p.bandeau_agrandi)\n        return Composition::AlerteAgrandie;\n    if (p.carte_pluie)\n        return Composition::CartePluie;",
     "    if (p.carte_pluie)\n        return Composition::CartePluie;\n    if (p.bandeau_agrandi)\n        return Composition::AlerteAgrandie;",
     T, "l_alerte_passe_avant_la_pluie_dans_la_composition", "la pluie prend la place de l'alerte officielle"),
    (A, "    if (p.carte_pluie)\n        return Composition::CartePluie;", "    if (false)\n        return Composition::CartePluie;",
     T, "l_alerte_passe_avant_la_pluie_dans_la_composition", "la carte pluie n'est jamais choisie"),

    # --- la composition normale et la carte pluie ---
    (A, "        a.bandeau = z.bandeau;\n        a.bas = z.jours;\n        return a;",
     "        a.bandeau = z.bandeau;\n        a.bas = z.heures;\n        return a;",
     T, "la_composition_normale_est_l_accueil_d_avant_pixel_pour_pixel", "la composition normale ne rend plus l'accueil d'avant"),
    (A, "z.jours.y + z.jours.h - z.bandeau.y}", "z.jours.h}",
     T, "la_carte_pluie_sans_alerte_monte_prendre_la_place_du_bandeau", "la carte pluie sans alerte laisse un trou à la place du bandeau"),
    (A, "            a.alerte = z.bandeau;", "            a.alerte = Rect{};",
     T, "la_carte_pluie_avec_alerte_garde_le_bandeau_et_prend_la_place_des_jours", "la carte pluie efface l'alerte officielle"),
    (A, "        a.bas_en_carte_pluie = true;", "        a.bas_en_carte_pluie = false;",
     T, "la_carte_pluie_sans_alerte_monte_prendre_la_place_du_bandeau", "la zone du bas ne sait pas qu'elle porte la carte pluie"),

    # --- la composition d'alerte ---
    (A, "    const int bas_ecran = z.jours.y + z.jours.h;", "    const int bas_ecran = z.jours.y + z.jours.h - 10;",
     T, "la_composition_d_alerte_tient_la_bande_des_jours_au_bas_de_l_ecran_tab5", "la bande des jours remonte de 10 px"),
    (A, "a.bas = {z.jours.x, bas_ecran - BANDE_JOURS_H, z.jours.w, BANDE_JOURS_H};",
     "a.bas = {z.jours.x, bas_ecran - BANDE_JOURS_H - 4, z.jours.w, BANDE_JOURS_H};",
     T, "la_composition_d_alerte_tient_la_bande_des_jours_au_bas_de_l_ecran_tab5", "la bande des jours est décalée de 4 px"),
    (A, "    a.bas_en_bande = true;", "    a.bas_en_bande = false;",
     T, "la_composition_d_alerte_tient_la_bande_des_jours_au_bas_de_l_ecran_tab5", "les cinq jours ne savent pas qu'ils sont en bande"),
    (A, "    const int haut = z.bandeau.y;", "    const int haut = z.bandeau.y + 8;",
     T, "les_rectangles_d_alerte_ne_se_chevauchent_pas_et_tiennent_dans_l_ecran", "la carte d'alerte démarre 8 px trop bas"),
    (A, "    const int zone_h = a.bas.y - ECART_CARTES - haut;", "    const int zone_h = a.bas.y - haut;",
     T, "la_composition_d_alerte_tient_la_bande_des_jours_au_bas_de_l_ecran_tab5", "pas d'écart entre la carte d'alerte et la bande des jours"),
    (A, "        const int alerte_w = tuile ? z.bandeau.w - TUILE_PLUIE_W - ECART_CARTES : z.bandeau.w;",
     "        const int alerte_w = tuile ? z.bandeau.w - TUILE_PLUIE_W : z.bandeau.w;",
     T, "la_composition_d_alerte_tient_la_bande_des_jours_au_bas_de_l_ecran_tab5", "pas d'écart entre la carte d'alerte et la tuile de pluie"),
    (A, "        const int alerte_w = tuile ? z.bandeau.w - TUILE_PLUIE_W - ECART_CARTES : z.bandeau.w;",
     "        const int alerte_w = tuile ? z.bandeau.w - TUILE_PLUIE_W - ECART_CARTES : z.bandeau.w - TUILE_PLUIE_W;",
     T, "sans_rien_a_dire_de_la_pluie_la_carte_d_alerte_prend_toute_la_largeur", "sans pluie à dire, la carte d'alerte laisse la place de la tuile"),
    (A, "a.tuile = {z.bandeau.x + alerte_w + ECART_CARTES, haut, TUILE_PLUIE_W, zone_h};",
     "a.tuile = {z.bandeau.x + alerte_w, haut, TUILE_PLUIE_W, zone_h};",
     T, "la_composition_d_alerte_tient_la_bande_des_jours_au_bas_de_l_ecran_tab5", "la tuile de pluie touche la carte d'alerte"),
    (A, "        const int alerte_h = tuile ? zone_h - TUILE_PLUIE_H - ECART_CARTES : zone_h;",
     "        const int alerte_h = tuile ? zone_h - TUILE_PLUIE_H : zone_h;",
     T, "la_composition_d_alerte_en_portrait_met_la_tuile_sous_la_carte", "portrait : pas d'écart entre la carte d'alerte et la tuile"),
    (A, "a.tuile = {z.bandeau.x, haut + alerte_h + ECART_CARTES, z.bandeau.w, TUILE_PLUIE_H};",
     "a.tuile = {z.bandeau.x, haut + alerte_h, z.bandeau.w, TUILE_PLUIE_H};",
     T, "la_composition_d_alerte_en_portrait_met_la_tuile_sous_la_carte", "portrait : la tuile touche la carte d'alerte"),
    (A, "    if (!portrait) {\n        const int alerte_w", "    if (portrait) {\n        const int alerte_w",
     T, "la_composition_d_alerte_tient_la_bande_des_jours_au_bas_de_l_ecran_tab5", "les deux orientations sont échangées"),

    # --- la carte pluie : ce qu'elle dit ---
    (A, "    if (episode.genre != GenrePluie::EN_COURS && episode.genre != GenrePluie::BIENTOT)",
     "    if (episode.genre == GenrePluie::INCONNU)",
     T, "pas_de_carte_sans_pluie_proche", "une carte pour « aucune pluie » ou « pluie plus tard »"),
    (A, "    if (h == nullptr || nh <= 0 || maintenant_s <= 0)", "    if (h == nullptr || nh <= 0)",
     T, "sans_heures_ou_sans_heure_la_carte_dit_ce_qu_elle_sait_sans_barres", "des barres sont dessinées sans connaître l'heure"),
    (A, "    if (colonnes > BARRES_MAX)\n        colonnes = BARRES_MAX;", "",
     T, "les_barres_commencent_a_l_heure_en_cours_et_se_bornent_aux_colonnes", "plus de barres que la carte n'en peut porter"),
    (A, "        if (h[i].instant_s + 3600 > maintenant_s) {\n            premiere = i;",
     "        if (h[i].instant_s + 3600 >= maintenant_s) {\n            premiere = i;",
     T, "les_barres_commencent_a_l_heure_en_cours_et_se_bornent_aux_colonnes", "à l'heure pile, la barre de l'heure finie est encore dessinée"),
    (A, "i < nh && c.n < colonnes; ++i) {", "i < nh && c.n <= colonnes; ++i) {",
     T, "les_barres_commencent_a_l_heure_en_cours_et_se_bornent_aux_colonnes", "une barre de trop"),
    (A, "visuel::connu(h[i].pluie_dix_mm) ? h[i].pluie_dix_mm : INCONNU_DIX;", "visuel::connu(h[i].pluie_dix_mm) ? h[i].pluie_dix_mm : 0;",
     T, "une_heure_sans_valeur_est_inconnue_jamais_zero", "une heure sans valeur se dessine comme une heure sèche"),
    (A, "c.echelle_dix = echelle_pluie(fort, PLANCHER_ECHELLE_HEURE_DIX);", "c.echelle_dix = fort;",
     T, "l_echelle_a_un_plancher_de_trois_millimetres_et_suit_la_plus_forte_barre", "l'échelle n'a plus de plancher : une bruine remplit la carte"),
    (A, "        if (h[i].instant_s + 3600 > episode.debut_s && detail::heure_pluvieuse(h[i])) {",
     "        if (h[i].instant_s + 3600 > episode.debut_s) {",
     T, "la_force_est_celle_de_la_premiere_heure_pluvieuse_pas_d_une_heure_seche", "la force est celle d'une heure sèche avant la pluie"),
    (A, "            if (h[i].instant_s < episode.debut_s + 3600)\n                debut = i;", "            debut = i;",
     T, "une_heure_pluvieuse_trop_loin_du_debut_annonce_ne_donne_ni_force_ni_cumul", "la force d'une pluie trois heures plus tard est prêtée au début annoncé"),
    (A, "h[i].instant_s == h[i - 1].instant_s + 3600 && ", "",
     T, "un_trou_dans_les_heures_coupe_l_episode", "un trou dans les heures ne coupe pas l'épisode"),
    (A, "        for (int i = debut + 1; i < nh && h[i].instant_s", "        for (int i = debut + 1; i < nh - 1 && h[i].instant_s",
     T, "la_duree_n_est_dite_que_si_la_pluie_finit_dans_les_donnees", "la dernière heure connue n'est jamais lue"),
    (A, "            fin = i;\n", "",
     T, "la_duree_n_est_dite_que_si_la_pluie_finit_dans_les_donnees", "la fin de l'épisode n'avance pas : la durée est toujours dite"),
    (A, "            if (h[i].pluie_dix_mm > h[maxi].pluie_dix_mm)", "            if (h[i].pluie_dix_mm >= h[maxi].pluie_dix_mm)",
     T, "le_pic_est_la_premiere_heure_la_plus_forte", "le pic est la dernière heure la plus forte"),
    (A, "        if (niveau_pluie(h[maxi].pluie_dix_mm) > c.force) {", "        if (niveau_pluie(h[maxi].pluie_dix_mm) >= c.force) {",
     T, "la_pluie_plus_forte_plus_tard_ne_se_dit_que_si_le_niveau_monte", "« plus forte vers » sans que la pluie soit plus forte"),
    (A, "        const bool coupee_par_les_donnees = fin == nh - 1;", "        const bool coupee_par_les_donnees = false;",
     T, "la_duree_n_est_dite_que_si_la_pluie_finit_dans_les_donnees", "une durée est dite pour une pluie qui touche la dernière heure connue"),
    (A, "c.duree_h = (minutes + 30) / 60 < 1 ? 1 : (minutes + 30) / 60;", "c.duree_h = (minutes + 30) / 60;",
     T, "la_duree_ne_tombe_jamais_sous_une_heure", "une pluie de 10 minutes dure « 0 h »"),
    (A, "c.duree_h = (minutes + 30) / 60 < 1 ? 1 : (minutes + 30) / 60;", "c.duree_h = minutes / 60 < 1 ? 1 : minutes / 60;",
     T, "une_pluie_en_cours_est_dite_en_cours", "la durée est tronquée au lieu d'être arrondie"),

    # --- le dessin des barres ---
    (A, "    if (!visuel::connu(dix_mm) || dix_mm <= 0 ||", "    if (!visuel::connu(dix_mm) || dix_mm < 0 ||",
     T, "la_hauteur_d_une_barre_est_proportionnelle_avec_un_plancher_et_un_plafond", "une heure sèche a une barre de la hauteur du plancher"),
    (A, "    const int borne = h < plancher ? plancher : h;", "    const int borne = h < plancher ? h : plancher;",
     T, "la_hauteur_d_une_barre_est_proportionnelle_avec_un_plancher_et_un_plafond", "le plancher devient un plafond"),
    (A, "    return borne > hauteur ? hauteur : borne;", "    return borne;",
     T, "la_hauteur_d_une_barre_est_proportionnelle_avec_un_plancher_et_un_plafond", "une barre dépasse le graphique"),
    (A, "    return h > hauteur ? hauteur : h;", "    return h;",
     T, "les_reperes_moyen_et_fort_sont_sur_la_meme_echelle_que_les_barres", "un repère dépasse le graphique"),
    (A, "return echelle_dix >= seuil_dix;", "return echelle_dix > seuil_dix;",
     T, "les_reperes_moyen_et_fort_sont_sur_la_meme_echelle_que_les_barres", "le repère « fort » disparaît sur l'échelle plancher de 3 mm"),
]
