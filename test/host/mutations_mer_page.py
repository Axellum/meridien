"""Mutations du noyau « page mer composée » (components/core/include/ecran/mer_page.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_MER : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test qui doit
échouer, nom de la fonction de test qui doit être citée (None : n'importe laquelle, y compris une erreur de compilation voulue), ce que la
mutation simule). Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle de ce que la page décide de montrer : quand un verdict se montre (jamais pour une mer absente ou trop
vieille), ce qu'une carte jugée ou non jugée dit, la teinte d'une barre (neutre la nuit), le repère « maintenant », l'axe, les critères d'une
carte, les blocs du détail, la courbe de marée (jamais de pont au-dessus d'un trou, jamais étirée à tort), les prochaines marées, le
coefficient et le style du bandeau (une vigilance illisible n'est jamais peinte comme un calme). Ce sont des mutations choisies à la main
sur les règles de ce fichier, pas un balayage automatique : elles ne disent pas que « tout » est testé, seulement que ces règles-là le sont.
Les couleurs choisies par la page (main/ui_mer.cpp) ne sont pas dans le noyau : le rendu PC les montre, aucun test ne les compare.
"""

TP = "test_mer_page.cpp"
PG = "mer_page.hpp"

MONTRES = "les_verdicts_ne_se_montrent_que_pour_une_mer_fraiche_ou_de_quelques_minutes"
OUVRABLE = "une_carte_jugee_montre_son_verdict_et_s_ouvre_si_elle_a_des_criteres"
TERNE = "des_donnees_de_quelques_minutes_ternissent_la_carte"
INDISPO = "une_activite_qu_on_ne_juge_pas_ici_n_a_jamais_de_verdict"
ABSENTE = "une_mer_absente_ou_trop_vieille_ne_montre_aucun_verdict"
SANS_VERDICT = "une_ligne_absente_ou_sans_verdict_n_est_pas_jugee"
POURQUOI = "la_ligne_de_la_carte_dit_pourquoi"
TEINTE_JOUR = "une_barre_prend_la_teinte_de_son_verdict_le_jour_et_reste_neutre_la_nuit"
BARRE_NEUTRE = "une_barre_au_verdict_inconnu_ou_hors_plage_est_neutre"
TEINTES = "chaque_verdict_a_sa_teinte"
REPERE_HEURE = "le_repere_maintenant_suit_l_heure_et_disparait_avec_les_verdicts"
AXE = "l_axe_demande_des_barres_montrees_et_une_heure_de_depart"
CRITERES = "une_carte_montre_les_premiers_criteres_connus_sans_la_vigilance"
CRITERES_MAX = "une_carte_ne_montre_pas_plus_de_criteres_qu_on_en_demande"
CRITERE_INCONNU = "un_critere_inconnu_ou_d_unite_inconnue_n_est_pas_montre"
MARQUES = "le_detail_dessine_des_blocs_une_pastille_ou_rien"
POINTS_INCOHERENTS = "des_points_incoherents_ne_donnent_pas_de_blocs"
BLOCS_MAX = "les_blocs_ne_depassent_pas_le_maximum_dessine"
COURBE = "la_courbe_couvre_ce_que_la_trame_donne_et_va_de_la_basse_a_la_haute"
REPERE_COURBE = "le_repere_est_sur_la_courbe_a_l_heure_de_maintenant"
HORS_24H = "pas_de_repere_hors_des_24_heures_ni_hors_de_la_couverture"
SANS_COURBE = "sans_assez_de_marees_ni_heure_de_depart_il_n_y_a_pas_de_courbe"
TROU = "une_maree_sans_hauteur_coupe_la_courbe_sans_pont"
PLATE = "une_maree_plate_ne_s_etire_pas_sur_toute_la_hauteur"
PROCHAINES = "les_prochaines_marees_sont_les_deux_premieres_qui_ne_sont_pas_passees"
COEFFICIENT = "le_coefficient_est_celui_de_la_prochaine_pleine_mer"
BANDEAU = "le_style_du_bandeau_ne_peint_jamais_des_vigilances_illisibles_comme_un_calme"

MUTATIONS_MER_PAGE = [
    # ---- quand un verdict se montre ------------------------------------------------------------------------------------------------------
    (
        PG,
        "return f == Fraicheur::Fraiche || f == Fraicheur::Vieillie; }",
        "return f != Fraicheur::Rien; }",
        TP,
        MONTRES,
        "un verdict se montre aussi pour une mer trop vieille : on conseille d'aller à l'eau sur des données d'hier",
    ),
    (
        PG,
        "return montres && a.presente && a.disponible && a.verdict != Verdict::Inconnu;",
        "return montres && a.presente && a.verdict != Verdict::Inconnu;",
        TP,
        INDISPO,
        "la pêche à pied sans modèle de marée est jugée quand même : un « Bon » par défaut sur une activité qu'on ne sait pas juger",
    ),
    (
        PG,
        "return montres && a.presente && a.disponible && a.verdict != Verdict::Inconnu;",
        "return montres && a.disponible && a.verdict != Verdict::Inconnu;",
        TP,
        SANS_VERDICT,
        "une activité absente de la trame est jugée : on montre un verdict pour une ligne qui n'existe pas",
    ),
    (
        PG,
        "return montres && a.presente && a.disponible && a.verdict != Verdict::Inconnu;",
        "return montres && a.presente && a.disponible;",
        TP,
        SANS_VERDICT,
        "un verdict inconnu est montré comme une carte jugée : la page choisit une couleur de verdict qui n'existe pas",
    ),
    (
        PG,
        "v.terne = f == Fraicheur::Vieillie;",
        "v.terne = f == Fraicheur::Fraiche;",
        TP,
        TERNE,
        "ce sont les données fraîches qui sont atténuées, pas celles de quelques minutes",
    ),
    (
        PG,
        "v.ouvrable = a.nb_criteres > 0;",
        "v.ouvrable = true;",
        TP,
        OUVRABLE,
        "une carte sans critère s'ouvre : le détail serait un panneau vide",
    ),
    (
        PG,
        "} else if (montres && a.presente && !a.disponible) {",
        "} else if (a.presente && !a.disponible) {",
        TP,
        ABSENTE,
        "« Indisponible ici » s'affiche même quand la mer est trop vieille : on dit une chose précise sur une mer périmée",
    ),
    (
        PG,
        "p.texte(texte(Texte::DonneesAnciennes, langue));",
        "p.texte(texte(Texte::MerIndisponible, langue));",
        TP,
        POURQUOI,
        "une mer trop vieille est annoncée comme absente : on ne dit plus qu'elle existe mais qu'elle est périmée",
    ),
    (
        PG,
        "if (a.presente && !a.disponible) {",
        "if (a.presente && a.disponible) {",
        TP,
        POURQUOI,
        "c'est l'activité jugée qui dit « Indisponible ici » et celle qu'on ne juge pas qui dit sa raison",
    ),
    # ---- les barres, le repère et l'axe --------------------------------------------------------------------------------------------------
    (
        PG,
        "if (a.lumiere[h] == Lumiere::Nuit)",
        "if (a.lumiere[h] == Lumiere::Jour)",
        TP,
        TEINTE_JOUR,
        "la nuit prend la couleur du verdict et le jour reste neutre : un surfeur croit pouvoir y aller à 3 h du matin",
    ),
    (
        PG,
        "if (h < 0 || h >= HEURES)",
        "if (h < 0 || h > HEURES)",
        TP,
        BARRE_NEUTRE,
        "l'heure 24 (hors des barres) lit au-delà du tableau : une couleur au hasard",
    ),
    (
        PG,
        "return Teinte::Possible;",
        "return Teinte::Bon;",
        TP,
        TEINTES,
        "« Possible » prend la teinte de « Bon » : deux verdicts qu'on ne distingue plus",
    ),
    (
        PG,
        "return Teinte::Neutre; // inconnu",
        "return Teinte::Deconseille; // inconnu",
        TP,
        TEINTES,
        "un verdict inconnu prend la teinte « déconseillé » : une alerte sans raison",
    ),
    (
        PG,
        "return verdicts_montres(f) ? indice_heure(mer, maintenant_s) : -1;",
        "return indice_heure(mer, maintenant_s);",
        TP,
        REPERE_HEURE,
        "le trait « maintenant » se dessine sur une mer trop vieille : il donne l'air d'une prévision à jour",
    ),
    (
        PG,
        "return verdicts_montres(f) && mer.premiere_heure_s != 0;",
        "return verdicts_montres(f);",
        TP,
        AXE,
        "l'axe des heures s'écrit sans heure de départ : des heures inventées sous les barres",
    ),
    # ---- les critères d'une carte et du détail ----------------------------------------------------------------------------------------------
    (
        PG,
        "return c.valeur_d != INCONNU && c.unite != Unite::Inconnue;",
        "return c.valeur_d != INCONNU;",
        TP,
        CRITERE_INCONNU,
        "un critère d'une unité que ce firmware ne connaît pas est montré : un nombre sans unité",
    ),
    (
        PG,
        "if (c.id == Critere::Vigilance || c.id == Critere::Inconnu || !valeur_montrable(c))",
        "if (c.id == Critere::Inconnu || !valeur_montrable(c))",
        TP,
        CRITERES,
        "la vigilance côtière prend une rangée de la carte : le bandeau en parle déjà",
    ),
    (
        PG,
        "for (int k = 0; k < a.nb_criteres && k < CRITERES_MAX && n < max; ++k) {",
        "for (int k = 0; k < a.nb_criteres && k < CRITERES_MAX; ++k) {",
        TP,
        CRITERES_MAX,
        "une carte montre plus de critères qu'elle n'a de rangées : le tableau des indices déborde",
    ),
    (
        PG,
        "const bool points = c.points >= 0 && c.points_max > 0 && c.points <= c.points_max;",
        "const bool points = c.points >= 0 && c.points_max > 0 && c.points < c.points_max;",
        TP,
        MARQUES,
        "les points au maximum ne dessinent plus de blocs : le meilleur score se montre comme une pastille",
    ),
    (
        PG,
        "const bool points = c.points >= 0 && c.points_max > 0 && c.points <= c.points_max;",
        "const bool points = c.points >= 0 && c.points_max > 0;",
        TP,
        POINTS_INCOHERENTS,
        "des points au-delà du maximum dessinent des blocs : « 5 sur 2 »",
    ),
    (
        PG,
        "m.total = c.points_max < BLOCS_MAX ? c.points_max : BLOCS_MAX;",
        "m.total = c.points_max;",
        TP,
        BLOCS_MAX,
        "le nombre de blocs n'est plus borné : une rangée de blocs qui déborde de sa cellule",
    ),
    (
        PG,
        "m.plein = c.points < m.total ? c.points : m.total;",
        "m.plein = c.points;",
        TP,
        BLOCS_MAX,
        "plus de blocs pleins que de blocs dessinés",
    ),
    (
        PG,
        "} else if (c.verdict != Verdict::Inconnu) {",
        "} else {",
        TP,
        MARQUES,
        "un critère sans verdict dessine une pastille d'un verdict qui n'existe pas",
    ),
    # ---- la courbe de marée ----------------------------------------------------------------------------------------------------------------
    (
        PG,
        "if (!mer.valide || t0 == 0)",
        "if (!mer.valide)",
        TP,
        SANS_COURBE,
        "sans heure de départ la courbe est calculée quand même : des marées placées depuis l'instant 0",
    ),
    (
        PG,
        "break; // le premier",
        "continue; // le premier",
        TP,
        TROU,
        "la courbe fait un pont au-dessus d'un trou de la trame : elle montre une marée qu'on n'a pas",
    ),
    (
        PG,
        "const std::int32_t amplitude = haut - bas < 10 ? 10 : haut - bas;",
        "const std::int32_t amplitude = haut - bas < 1 ? 10 : haut - bas;",
        TP,
        PLATE,
        "une marée presque plate s'étire sur toute la hauteur : un marnage de 5 cm paraît aussi grand que 5 m",
    ),
    (
        PG,
        "c.x_pm = static_cast<std::int32_t>((maintenant_s - t0) * 1000 / 86400);",
        "c.x_pm = static_cast<std::int32_t>((maintenant_s - t0) * 1000 / 86000);",
        TP,
        REPERE_COURBE,
        "le repère « maintenant » est décalé de quelques minutes sur la courbe",
    ),
    (
        PG,
        "if (maintenant_s >= t0 && maintenant_s <= t0 + 86400 && hauteur_maree_cm(mer, maintenant_s, h_maintenant)) {",
        "if (maintenant_s >= t0 && maintenant_s <= t0 + 2 * 86400 && hauteur_maree_cm(mer, maintenant_s, h_maintenant)) {",
        TP,
        HORS_24H,
        "le repère se pose au-delà des 24 heures de la courbe : à droite du cadre",
    ),
    (
        PG,
        "if (!hauteur_maree_cm(mer, t0 + k * PAS_MAREE_S, h)) {",
        "if (!hauteur_maree_cm(mer, t0 + k * 3600, h)) {",
        TP,
        COURBE,
        "la courbe est échantillonnée toutes les heures au lieu de toutes les demi-heures : elle n'arrive plus à 24 heures",
    ),
    # ---- prochaines marées, coefficient, bandeau -------------------------------------------------------------------------------------------
    (
        PG,
        "if (maintenant_s == 0)",
        "if (maintenant_s < 0)",
        TP,
        PROCHAINES,
        "sans horloge on annonce quand même des « prochaines » marées : celles de 1970",
    ),
    (
        PG,
        "for (int j = 0; j < 2 && premiere + j < mer.nb_marees && premiere + j < MAREES_MAX; ++j)",
        "for (int j = 0; j < 3 && premiere + j < mer.nb_marees && premiere + j < MAREES_MAX; ++j)",
        TP,
        PROCHAINES,
        "trois marées au lieu de deux : la page n'a que deux emplacements",
    ),
    (
        PG,
        "return maintenant_s == 0 ? -1 : coefficient_pour(mer, maintenant_s);",
        "return coefficient_pour(mer, maintenant_s);",
        TP,
        COEFFICIENT,
        "sans horloge on montre le coefficient du premier jour de la trame : peut-être celui d'hier",
    ),
    (
        PG,
        "s.teinte = true;",
        "s.teinte = false;",
        TP,
        BANDEAU,
        "une vigilance lue n'est plus teintée : le bandeau ne se remarque plus",
    ),
    (
        PG,
        "s.niveau = static_cast<int>(vigie::Niveau::Jaune);",
        "s.niveau = static_cast<int>(niveau);",
        TP,
        BANDEAU,
        "des vigilances illisibles prennent la couleur du dernier niveau connu : peut-être le vert, un faux calme",
    ),
]
