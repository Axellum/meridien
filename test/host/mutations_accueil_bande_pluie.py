"""Mutations de la bande de pluie de l'accueil (components/core/include/ecran/accueil_bande_pluie.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_ACCUEIL_CARTES : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test qui
doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou « détectée par
autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle écrite dans l'en-tête du fichier et dans l'ADR-0027 : le pas d'un quart d'heure, le seuil de la bruine, le
quart où la bande commence, ce qui compte comme « pas de bande », la force, la fin de la pluie, les repères et la hauteur d'une barre.
Choisies à la main : elles ne disent pas que « tout » est testé, seulement que ces règles-là le sont.
"""

T = "test_accueil_bande_pluie.cpp"
A = "accueil_bande_pluie.hpp"

MUTATIONS_ACCUEIL_BANDE_PLUIE = [
    # --- les constantes ---
    (A, "QUARTS_BANDE_MAX = 8;", "QUARTS_BANDE_MAX = 9;", T, "les_constantes_de_la_bande", "la bande montre neuf quarts d'heure au lieu de huit"),
    (A, "QUART_S = 900;", "QUART_S = 600;", T, "les_constantes_de_la_bande", "le pas de l'API passe à dix minutes"),
    (A, "SEUIL_QUART_DIX = 1;", "SEUIL_QUART_DIX = 2;", T, "les_constantes_de_la_bande", "la bruine de 0,1 mm ne compte plus comme de la pluie"),
    (A, "PLANCHER_ECHELLE_QUART_DIX_H = 30;", "PLANCHER_ECHELLE_QUART_DIX_H = 20;", T, "les_constantes_de_la_bande", "le plancher de l'échelle passe à 2 mm/h"),

    # --- le quart où la bande commence, et ce qui n'est pas une bande ---
    (A, " || maintenant_s < debut_quarts_s)", ")", T, "des_quarts_qui_ne_couvrent_pas_le_moment_ne_donnent_jamais_de_bande",
     "des quarts pas encore commencés donnent une bande"),
    (A, "if (courant_64 >= nb)", "if (courant_64 > nb)", T, "des_quarts_qui_ne_couvrent_pas_le_moment_ne_donnent_jamais_de_bande",
     "des quarts tous passés donnent encore une bande"),
    (A, "    const std::int64_t courant_64 = (maintenant_s - debut_quarts_s) / QUART_S;",
     "    const std::int64_t courant_64 = (maintenant_s - debut_quarts_s) / (QUART_S + 1);",
     T, "le_quart_change_pile_au_quart_d_heure", "le quart en cours change une seconde par quart d'heure trop tard"),
    (A, "    if (n > QUARTS_BANDE_MAX)\n        n = QUARTS_BANDE_MAX;", "    if (false)\n        n = QUARTS_BANDE_MAX;",
     T, "la_bande_ne_depasse_jamais_deux_heures", "la bande dépasse deux heures"),
    (A, "    if (b.premier < 0)\n        return b; // pas de pluie dans la bande : pas de bande", "    if (false)\n        return b; // pas de pluie dans la bande : pas de bande",
     T, "sans_pluie_dans_la_bande_il_n_y_a_pas_de_bande", "une bande apparaît sans aucune pluie"),
    (A, "    b.en_cours = b.barre[0].pluvieux;", "    b.en_cours = b.premier >= 0;",
     T, "une_pluie_qui_commence_plus_tard_prend_la_force_du_premier_quart_pluvieux", "une pluie qui commence plus tard est dite « en cours »"),

    # --- la pluie d'un quart d'heure : valeur, niveau, inconnu ---
    (A, "        if (dix >= 0) { // une valeur négative", "        if (dix > 0) { // une valeur négative",
     T, "la_bande_commence_au_quart_en_cours_pas_au_quart_ecoule", "un quart sec (0 mm) devient un quart sans valeur"),
    (A, "            q.dix_mm_h = dix * 4;", "            q.dix_mm_h = dix * 3;",
     T, "le_niveau_est_celui_de_la_pluie_ramenee_a_l_heure", "la pluie ramenée à l'heure est calculée sur trois quarts"),
    (A, "            q.pluvieux = dix >= SEUIL_QUART_DIX;", "            q.pluvieux = dix > SEUIL_QUART_DIX;",
     T, "sans_pluie_dans_la_bande_il_n_y_a_pas_de_bande", "0,1 mm sur un quart d'heure n'est plus de la pluie"),
    (A, "            if (q.dix_mm_h > maxi_h)\n                maxi_h = q.dix_mm_h;", "            if (false)\n                maxi_h = q.dix_mm_h;",
     T, "l_echelle_a_un_plancher_de_trois_millimetres_et_suit_la_plus_forte_barre", "l'échelle ne suit plus la plus forte pluie"),

    # --- les repères ---
    (A, "(maintenant_s - b.barre[0].debut_s) * 1000 / largeur_s", "(maintenant_s - b.barre[0].debut_s) * 100 / largeur_s",
     T, "la_pluie_en_cours_a_sa_force_sa_fin_et_ses_reperes", "le repère « maintenant » est en centièmes au lieu de millièmes"),
    (A, "maintenant_s + HEURE_S - b.barre[0].debut_s;", "maintenant_s + HEURE_S;",
     T, "la_pluie_en_cours_a_sa_force_sa_fin_et_ses_reperes", "le repère « dans une heure » oublie le début de la bande"),
    (A, "dans_une_heure_s < largeur_s ?", "dans_une_heure_s <= largeur_s ?",
     T, "le_repere_dans_une_heure_sort_de_la_bande_quand_elle_est_courte", "le repère « dans une heure » tombe pile au bord de la bande"),

    # --- la fin de la pluie ---
    (A, "            if (b.barre[i].dix_mm < 0)\n                break; // un quart inconnu", "            if (false)\n                break; // un quart inconnu",
     T, "un_quart_inconnu_coupe_la_fin_de_la_pluie_il_ne_dit_pas_qu_elle_finit", "un quart inconnu est pris pour la fin de la pluie"),
    (A, "static_cast<int>((reste_s + 59) / 60) : 0;", "static_cast<int>(reste_s / 60) : 0;",
     T, "la_fin_est_arrondie_a_la_minute_au_dessus", "la fin de la pluie est arrondie vers le bas"),

    # --- la hauteur d'une barre ---
    (A, "    if (dix_mm_h <= 0 || echelle_dix_h <= 0 || hauteur <= 0)", "    if (dix_mm_h < 0 || echelle_dix_h <= 0 || hauteur <= 0)",
     T, "la_hauteur_est_en_racine_avec_un_plancher_et_un_plafond", "une pluie nulle a une barre de plancher"),
    (A, "    if (plancher > hauteur)\n        plancher = hauteur;", "    if (false)\n        plancher = hauteur;",
     T, "la_hauteur_tient_dans_les_cas_limites", "le plancher d'une barre dépasse la place"),
    (A, "(hauteur - plancher) * racine_pm / 1000);", "(hauteur - plancher) * racine_pm / 1024);",
     T, "la_hauteur_est_en_racine_avec_un_plancher_et_un_plafond", "la racine carrée est ramenée à 1024 au lieu de 1000"),
]
