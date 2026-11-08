"""Mutations du noyau « roue » (components/core/include/ecran/roue_geometrie.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_SURF : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test qui
doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou
« détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle de la roue : un rayon, un pas d'angle, le sens du pivotement au bord, un arrondi, une borne incluse
ou exclue, le côté d'un mot, la zone de toucher. Ce sont des mutations choisies à la main sur les règles de ce noyau, pas un
balayage automatique : elles ne disent pas que « tout » est testé, seulement que ces règles-là le sont.
"""

GEO = "test_roue_geometrie.cpp"
BAL = "test_roue_balayage.cpp"

H = "roue_geometrie.hpp"

MUTATIONS_ROUE_GEOMETRIE = [
    # --- les mesures du Tab5 : un rayon, un pas, une marge, un diamètre ---
    (H, "inline constexpr int RAYON_1 = 180;", "inline constexpr int RAYON_1 = 181;",
     GEO, "premier_anneau_valeurs_du_tab5", "le premier anneau est à 181 px du moyeu au lieu de 180"),
    (H, "inline constexpr int RAYON_2 = 290;", "inline constexpr int RAYON_2 = 291;",
     GEO, "second_anneau_valeurs_du_tab5", "le second anneau est à 291 px du moyeu au lieu de 290"),
    (H, "inline constexpr int PAS_1 = 30;", "inline constexpr int PAS_1 = 29;",
     GEO, "premier_anneau_valeurs_du_tab5", "30° entre deux boutons du premier anneau deviennent 29°"),
    (H, "inline constexpr int PAS_2 = 20;", "inline constexpr int PAS_2 = 21;",
     GEO, "second_anneau_valeurs_du_tab5", "20° entre deux choix du second anneau deviennent 21°"),
    (H, "inline constexpr int MARGE = 12;", "inline constexpr int MARGE = 8;",
     GEO, "premier_anneau_valeurs_du_tab5", "la marge aux bords est de 8 px au lieu de 12"),
    (H, "inline constexpr int DIAMETRE_1 = 72;", "inline constexpr int DIAMETRE_1 = 70;",
     GEO, "premier_anneau_valeurs_du_tab5", "les boutons du premier anneau font 70 px au lieu de 72"),
    (H, "inline constexpr int MOT_ECART = 4;", "inline constexpr int MOT_ECART = 6;",
     GEO, "mots_valeurs_du_tab5", "le mot est à 6 px de son bouton au lieu de 4"),
    (H, "inline constexpr int ECART_MOTS = 8;", "inline constexpr int ECART_MOTS = 0;",
     GEO, "mots_valeurs_du_tab5", "deux mots voisins ne gardent plus d'écart entre eux"),
    (H, "inline constexpr int MOT_MIN = 40;", "inline constexpr int MOT_MIN = 20;",
     GEO, "deux_mots_voisins_valeurs_du_tab5", "un mot serré peut descendre à 20 px au lieu de 40"),
    (H, "inline constexpr int TOUCHER_MARGE = 8;", "inline constexpr int TOUCHER_MARGE = 0;",
     GEO, "toucher_la_frontiere_d_un_bouton_pixel_dedans_pixel_dehors", "la zone de toucher n'est plus élargie de 8 px"),

    # --- l'échelle ---
    (H, "return (valeur * echelle_pm + 500) / 1000;", "return valeur * echelle_pm / 1000;",
     GEO, "la_7b_a_l_echelle_0_8", "l'échelle tronque au lieu d'arrondir (72 x 0,8 = 57,6 donne 57)"),
    (H, "mettre_a_l_echelle(RAYON_1, k),", "RAYON_1,",
     GEO, "la_7b_a_l_echelle_0_8", "le rayon du premier anneau ne suit pas l'échelle de la 7B"),

    # --- le pivotement au bord ---
    (H, "sens = -PIVOT;", "sens = PIVOT;",
     GEO, "le_pivotement_va_du_cote_oppose_au_bord", "un éventail qui sort à gauche pivote vers la gauche au lieu de la droite"),
    (H, "sens = PIVOT;", "sens = -PIVOT;",
     GEO, "le_pivotement_va_du_cote_oppose_au_bord", "un éventail qui sort à droite pivote vers la droite au lieu de la gauche"),
    (H, "sens = a.bouton[i].angle < 90 ? PIVOT : -PIVOT;", "sens = a.bouton[i].angle < 90 ? -PIVOT : PIVOT;",
     GEO, "le_pivotement_va_du_cote_oppose_au_bord", "un éventail qui sort en haut ou en bas pivote en s'éloignant de la verticale"),
    (H, "if (gauche - rayon_bouton < m.marge) {", "if (gauche - rayon_bouton <= m.marge) {",
     GEO, "le_pivotement_va_du_cote_oppose_au_bord", "un bouton exactement à la marge à gauche fait déjà pivoter l'éventail"),
    (H, "} else if (droite + rayon_bouton > m.largeur - m.marge) {", "} else if (droite + rayon_bouton >= m.largeur - m.marge) {",
     GEO, "premier_anneau_valeurs_du_tab5", "un bouton exactement à la marge à droite fait déjà pivoter l'éventail"),
    (H, "if (a.bouton[i].y - rayon_bouton < m.marge || a.bouton[i].y + rayon_bouton > m.hauteur - m.marge) {",
     "if (a.bouton[i].y + rayon_bouton > m.hauteur - m.marge) {",
     GEO, "premier_anneau_valeurs_du_tab5", "un bouton qui sort par le haut ne fait plus pivoter l'éventail"),
    (H, "const int ang = centre + (n - 1) * pas / 2 - i * pas + decalage;", "const int ang = centre + (n - 1) * pas / 2 - i * pas - decalage;",
     GEO, "premier_anneau_valeurs_du_tab5", "le pivotement tourne l'éventail dans le sens contraire de celui qu'il a calculé"),
    (H, "const int ang = centre + (n - 1) * pas / 2 - i * pas + decalage;", "const int ang = centre + n * pas / 2 - i * pas + decalage;",
     GEO, "premier_anneau_valeurs_du_tab5", "l'éventail n'est plus centré (n au lieu de n - 1)"),
    (H, "a.bouton[i].x = moyeu.x + projeter(rayon, cosinus5(ang));", "a.bouton[i].x = moyeu.x - projeter(rayon, cosinus5(ang));",
     GEO, "premier_anneau_valeurs_du_tab5", "les boutons sont posés en miroir (gauche et droite échangés)"),
    (H, "a.bouton[i].y = sous ? moyeu.y + dy : moyeu.y - dy;", "a.bouton[i].y = sous ? moyeu.y - dy : moyeu.y + dy;",
     GEO, "premier_anneau_valeurs_du_tab5", "les boutons passent sous le moyeu quand ils devraient être au-dessus, et inversement"),
    (H, "const int x = detail::limiter(a.bouton[i].x, m.marge + rayon_bouton, m.largeur - m.marge - rayon_bouton);",
     "const int x = detail::limiter(a.bouton[i].x, m.marge + rayon_bouton, m.largeur - rayon_bouton);",
     GEO, "premier_anneau_valeurs_du_tab5", "un bouton ramené dans l'écran garde le bord droit sans marge"),
    (H, "a.rogne = a.rogne || x != a.bouton[i].x || y != a.bouton[i].y;", "a.rogne = false;",
     GEO, "un_eventail_trop_large_pour_le_coin_empile_ses_boutons_chez_le_tab5", "un bouton ramené dans l'écran n'est plus signalé"),

    # --- le sinus et l'arrondi ---
    (H, "signe = -1;", "signe = 1;",
     GEO, "la_table_des_sinus_est_celle_du_tab5", "le sinus d'un angle au-delà de 180° est positif"),
    (H, "constexpr int cosinus5(int deg) noexcept { return sinus5(deg + 90); }",
     "constexpr int cosinus5(int deg) noexcept { return sinus5(deg - 90); }",
     GEO, "la_table_des_sinus_est_celle_du_tab5", "le cosinus a le signe contraire (sin(a - 90°) au lieu de sin(a + 90°))"),
    (H, "return p >= 0 ? (p + 16383) / 32767 : -((-p + 16383) / 32767);", "return p >= 0 ? p / 32767 : -((-p + 16383) / 32767);",
     GEO, "la_projection_arrondit_au_plus_proche_loin_de_zero", "la projection tronque les positifs au lieu de les arrondir"),
    (H, "return p >= 0 ? (p + 16383) / 32767 : -((-p + 16383) / 32767);", "return p >= 0 ? (p + 16383) / 32767 : -(-p / 32767);",
     GEO, "la_projection_arrondit_au_plus_proche_loin_de_zero", "la projection tronque les négatifs au lieu de les arrondir"),

    # --- dessus ou dessous ---
    (H, "return y_moyeu - (m.rayon_2 + m.legende_2 + m.legende_h / 2) < 0;", "return y_moyeu - (m.rayon_2 + m.legende_2 + m.legende_h / 2) <= 0;",
     GEO, "le_seuil_du_dessous_est_celui_du_tab5", "un moyeu exactement au seuil passe déjà sous le moyeu"),
    (H, "return y_moyeu - (m.rayon_2 + m.legende_2 + m.legende_h / 2) < 0;", "return y_moyeu - (m.rayon_2 + m.legende_h / 2) < 0;",
     GEO, "le_seuil_du_dessous_est_celui_du_tab5", "le seuil du dessous oublie la place du mot au-dessus d'un choix"),

    # --- les mots ---
    (H, "return diametre / 2 + m.mot_ecart + projeter(l / 2, detail::abs_int(cosinus5(angle))) +",
     "return diametre / 2 + projeter(l / 2, detail::abs_int(cosinus5(angle))) +",
     GEO, "mots_valeurs_du_tab5", "le mot colle à son bouton (l'écart de 4 px a disparu)"),
    (H, "projeter(m.legende_h / 2, detail::abs_int(sinus5(angle)));", "0;",
     GEO, "mots_valeurs_du_tab5", "le mot ne tient pas compte de sa hauteur sur l'axe du bouton"),
    (H, "if (detail::abs_int(cosinus5(angle)) >= 16383) {", "if (detail::abs_int(cosinus5(angle)) > 16383) {",
     GEO, "les_cotes_des_mots_comme_sur_l_esquisse", "à 60° exactement le mot passe au-dessus au lieu de se mettre sur le côté"),
    (H, "return cosinus5(angle) > 0 ? Cote::Droite : Cote::Gauche;", "return cosinus5(angle) > 0 ? Cote::Gauche : Cote::Droite;",
     GEO, "les_cotes_des_mots_comme_sur_l_esquisse", "un mot à droite du bouton est déclaré à sa gauche"),
    (H, "return (sinus5(angle) > 0) != sous ? Cote::Haut : Cote::Bas;", "return (sinus5(angle) > 0) == sous ? Cote::Haut : Cote::Bas;",
     GEO, "les_cotes_des_mots_comme_sur_l_esquisse", "un mot au-dessus du bouton est déclaré au-dessous"),
    (H, "const int l = detail::min_int(largeur, m.legende_l);", "const int l = largeur;",
     GEO, "un_mot_trop_large_est_borne_a_la_boite", "la place d'un mot plus large que la boîte ne tient pas compte du plafond de 150 px"),
    (H, "detail::abs_int(a.bouton[i].mot.y - a.bouton[j].mot.y) >= m.legende_h) {",
     "detail::abs_int(a.bouton[i].mot.y - a.bouton[j].mot.y) > m.legende_h) {",
     GEO, "deux_mots_voisins_valeurs_du_tab5", "deux mots exactement à une hauteur d'écart sont encore serrés l'un contre l'autre"),
    (H, "detail::abs_int(a.bouton[i].mot.x - a.bouton[j].mot.x) - m.ecart_mots);", "detail::abs_int(a.bouton[i].mot.x - a.bouton[j].mot.x));",
     GEO, "mots_valeurs_du_tab5", "deux mots serrés se touchent (l'écart de 8 px a disparu)"),
    (H, "libre = detail::max_int(libre, m.mot_min);", "libre = detail::max_int(libre, 0);",
     GEO, "deux_mots_voisins_valeurs_du_tab5", "un mot serré n'a plus de largeur minimale"),
    (H, "if (largeur == nullptr || largeur[i] <= 0) {", "if (largeur == nullptr) {",
     GEO, "un_mot_absent_ne_laisse_aucune_boite", "un mot de largeur 0 laisse tout de même une boîte"),

    # --- ce qui tient ---
    (H, "if (a.rogne) {", "if (false) {",
     GEO, "anneau_complet_refuse_ce_qui_ne_tient_pas", "un anneau dont un bouton a été ramené dans l'écran est déclaré tenir"),
    (H, "if (dx * dx + dy * dy < diametre * diametre) {", "if (dx * dx + dy * dy < diametre * diametre / 4) {",
     GEO, "anneau_complet_refuse_ce_qui_ne_tient_pas", "des boutons qui se recouvrent sont déclarés tenir"),
    (H, "if (gauche < m.marge || gauche + mot.largeur > m.largeur - m.marge || haut < m.marge ||",
     "if (gauche + mot.largeur > m.largeur - m.marge || haut < m.marge ||",
     GEO, "les_quatre_bords_avec_six_boutons_tiennent", "un mot qui sort à gauche de l'écran est déclaré tenir"),
    (H, "if (disque_sur_boite(a.bouton[j].x, a.bouton[j].y, rb, mot.x, mot.y, mot.largeur, m.legende_h)) {",
     "if (disque_sur_boite(a.bouton[j].x, a.bouton[j].y, rb, mot.x, mot.y, mot.largeur, m.legende_h) && false) {",
     GEO, "anneau_complet_refuse_ce_qui_ne_tient_pas", "un mot posé sur un bouton est déclaré tenir"),
    (H, "if (!anneau_tient(m, r.premier, m.diametre_1)) {", "if (false) {",
     GEO, "quand_rien_ne_tient_la_roue_est_au_centre_et_le_dit", "le premier anneau n'est plus contrôlé quand on déclare que la roue tient"),
    (H, "if (!anneau_tient(m, deplier(m, r, f, d.choix[f], largeur), m.diametre_2)) {", "if (false) {",
     GEO, "les_quatre_bords_avec_six_boutons_tiennent", "le second anneau n'est plus contrôlé quand on déclare que la roue tient"),

    (H, "return detail::anneau_tient(m, a, diametre);", "return true;",
     GEO, "sans_exiger_les_seconds_anneaux_le_moyeu_reste_plus_pres_du_doigt", "anneau_complet répond toujours oui, même pour un mot qui sort de l'écran"),
    (H, "largeur[j] = d.mot_choix;", "largeur[j] = 0;",
     GEO, "sans_exiger_les_seconds_anneaux_le_moyeu_reste_plus_pres_du_doigt", "les mots du second anneau ne comptent plus quand on décide que la roue tient"),

    # --- le second anneau ---
    (H, "if (famille < 0 || famille >= roue.premier.n) {", "if (famille < 0) {",
     GEO, "une_famille_hors_du_premier_anneau_donne_un_anneau_vide", "une famille qui n'existe pas donne un anneau au lieu d'un anneau vide"),
    (H, "Anneau a = disposer(m, roue.moyeu, n, m.rayon_2, m.pas_2, roue.premier.bouton[famille].angle, m.diametre_2 / 2,",
     "Anneau a = disposer(m, roue.moyeu, n, m.rayon_2, m.pas_2, 90, m.diametre_2 / 2,",
     GEO, "le_second_anneau_est_centre_sur_sa_famille", "le second anneau est centré sur la verticale au lieu de l'angle de sa famille"),
    (H, "placer_mots(m, a, roue.moyeu, roue.dessous, m.rayon_2, m.diametre_2, largeur);",
     "placer_mots(m, a, roue.moyeu, roue.dessous, m.rayon_1, m.diametre_2, largeur);",
     GEO, "les_quatre_bords_avec_six_boutons_tiennent", "les mots du second anneau sont posés comme s'il était à 180 px du moyeu"),

    # --- ouvrir : où est le moyeu ---
    (H, "r.dessous = passe_dessous(m, moyeu.y);", "r.dessous = false;",
     GEO, "ouvrir_garde_le_moyeu_sous_le_doigt_quand_la_roue_tient", "les anneaux restent toujours au-dessus du moyeu"),
    (H, "r.moyeu_deplace = !(appui == moyeu);", "r.moyeu_deplace = false;",
     GEO, "dans_un_coin_le_moyeu_avance_vers_le_centre_et_tout_tient", "un moyeu éloigné du doigt n'est pas signalé"),
    (H, "const int bord = m.marge + m.jauge / 2;", "const int bord = m.marge;",
     BAL, "toute_la_roue_courante_tient_partout_sur_le_tab5", "le cercle autour du moyeu peut sortir de l'écran"),
    (H, "const Point centre{m.largeur / 2, m.hauteur / 2};", "const Point centre{m.largeur / 2, m.hauteur / 3};",
     GEO, "dans_un_coin_le_moyeu_avance_vers_le_centre_et_tout_tient", "le chemin du moyeu vise le tiers de la hauteur au lieu du centre"),
    (H, "const Point p{depart.x + (centre.x - depart.x) * k / ETAPES_MOYEU,", "const Point p{depart.x,",
     GEO, "les_quatre_bords_avec_six_boutons_tiennent", "le moyeu ne s'écarte jamais du bord latéral"),
    (H, "const int decalage = sens * j * PAS_VERTICAL;", "const int decalage = 0;",
     GEO, "les_quatre_bords_avec_six_boutons_tiennent", "le chemin n'est jamais décalé de la bande morte à mi-hauteur"),
    (H, "for (int j = 0; j <= DECALAGES_VERTICAUX; j++) {", "for (int j = 0; j <= 0; j++) {",
     GEO, "les_quatre_bords_avec_six_boutons_tiennent", "aucun décalage vertical n'est essayé"),
    (H, "return detail::composer(m, appui, centre, d); // rien ne tient : le centre, `tient` faux",
     "return detail::composer(m, appui, appui, d); // rien ne tient : le centre, `tient` faux",
     GEO, "quand_rien_ne_tient_la_roue_est_au_centre_et_le_dit", "si rien ne tient, la roue reste là où le doigt s'est posé"),

    # --- le toucher ---
    (H, "constexpr int rayon_tactile(const Mesures &m, int diametre) noexcept { return diametre / 2 + m.toucher_marge; }",
     "constexpr int rayon_tactile(const Mesures &m, int diametre) noexcept { return diametre / 2; }",
     GEO, "toucher_la_frontiere_d_un_bouton_pixel_dedans_pixel_dehors", "la zone de toucher se réduit au bouton lui-même"),
    (H, "if (d2 <= rayon * rayon && d2 < meilleur) {", "if (d2 < rayon * rayon && d2 < meilleur) {",
     GEO, "toucher_la_frontiere_d_un_bouton_pixel_dedans_pixel_dehors", "le pixel à la distance exacte du rayon est dehors"),
    (H, "int meilleur = rayon * rayon + 1;", "int meilleur = rayon * rayon;",
     GEO, "toucher_la_frontiere_d_un_bouton_pixel_dedans_pixel_dehors", "le pixel à la distance exacte du rayon est dehors (borne de départ du plus proche)"),
    (H, "if (d2 <= rayon * rayon && d2 < meilleur) {", "if (d2 <= rayon * rayon && d2 <= meilleur) {",
     GEO, "deux_zones_qui_se_recouvrent_le_centre_le_plus_proche_gagne", "à égalité de distance le dernier bouton gagne au lieu du premier"),
    (H, "if (d2 <= rayon * rayon && d2 < meilleur) {", "if (d2 <= rayon * rayon && d2 < meilleur + rayon * rayon) {",
     GEO, "deux_zones_qui_se_recouvrent_le_centre_le_plus_proche_gagne", "dans deux zones qui se recouvrent, le dernier bouton gagne au lieu du plus proche"),
]
