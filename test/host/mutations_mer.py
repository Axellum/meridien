"""Mutations du noyau « mer » (components/core/include/ecran/mer_trame.hpp et mer_vue.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_SURF : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test qui doit
échouer, nom de la fonction de test qui doit être citée (None : n'importe laquelle, y compris une erreur de compilation voulue), ce que la
mutation simule). Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle écrite dans un des deux en-têtes : une constante du CRC, une borne incluse ou exclue, l'indice d'un champ de
la trame, une table de noms, la façon de traiter une valeur absente ou des vigilances illisibles, l'ordre du temps d'une courbe de marée.
Ce sont des mutations choisies à la main sur les règles de ce noyau, pas un balayage automatique : elles ne disent pas que « tout » est
testé, seulement que ces règles-là le sont.
"""

T = "test_mer_trame.cpp"
V = "test_mer_variantes.cpp"
U = "test_mer_vue.cpp"

TR = "mer_trame.hpp"
VU = "mer_vue.hpp"

# noms des tests (pour que la mutation soit attribuée au bon)
CRC32 = "le_crc32_est_celui_de_zlib"
CRC_REEL = "le_crc_de_chaque_trame_reelle_est_celui_que_python_a_calcule"
SANS_END = "une_trame_sans_ligne_end_ou_coupee_est_refusee"
HOSSEGOR = "hossegor_est_lue_comme_python_la_lit"
DECIMALES = "les_decimales_sont_lues_sur_le_texte_et_arrondies_loin_de_zero"
ILLISIBLE = "un_nombre_illisible_est_refuse_et_laisse_la_sortie_intacte"
INSTANT = "un_instant_est_un_entier_sur_32_bits_sinon_zero"
POINTS = "les_points_vont_de_0_a_100_sinon_inconnus"
CHAMPS = "une_ligne_est_coupee_en_gardant_les_champs_vides"
TEXTE = "un_texte_trop_long_est_coupe_sur_une_frontiere_de_caractere"
SLUG = "un_identifiant_de_spot_est_un_slug"
NOMS_A = "les_activites_les_verdicts_et_les_unites_ont_les_noms_du_contrat"
NOMS_C = "les_criteres_ont_les_noms_du_contrat"
NOMS_E = "les_etats_ont_les_noms_du_contrat"
RAISON = "la_raison_est_un_critere_un_point_et_un_etat"

NEUF = "un_crc_de_neuf_chiffres_est_refuse_et_les_minuscules_sont_acceptees"
SANS_BLOC = "une_trame_sans_le_bloc_mer_garde_l_ancienne_mer"
SEAX = "une_ligne_dont_la_balise_ressemble_a_sea_n_est_pas_le_bloc"
REMPLACE = "une_nouvelle_trame_remplace_toute_la_mer"
ACT_AVANT = "une_ligne_act_avant_la_ligne_sea_est_ignoree"
DEUX_SEA = "avec_deux_lignes_sea_la_premiere_gagne"
SEA_KO = "une_ligne_sea_inutilisable_refuse_la_trame_et_garde_l_ancienne_mer"
CRIT_AUTRE = "une_ligne_crit_d_une_autre_activite_que_celle_du_dessus_est_ignoree"
CRIT_NOM = "un_critere_au_nom_inconnu_n_est_pas_compte_mais_un_etat_ou_une_unite_inconnus_gardent_la_valeur"
SIX = "au_dela_de_six_criteres_les_suivants_sont_laisses"
VIDE = "un_champ_vide_reste_inconnu_jamais_zero"
ORANGE = "un_avertissement_orange_est_lu_avec_sa_fin_et_sa_source"
NIVEAUX = "les_quatre_niveaux_sont_lus"
ILLISIBLES = "sans_avertissement_lu_est_different_de_avertissements_illisibles"
BANDE = "une_bande_de_la_mauvaise_longueur_ou_avec_un_tiret_laisse_l_inconnu"
LUMIERES = "les_lumieres_de_la_bande_sont_jour_nuit_ou_inconnues"
CRENEAU = "un_creneau_qui_finit_avant_de_commencer_n_est_pas_un_creneau"
INDISPO = "une_activite_indisponible_n_a_ni_verdict_ni_bande_ni_critere"
COURTE = "une_ligne_act_ou_crit_trop_courte_est_ignoree"
HUIT = "plus_de_huit_marees_les_suivantes_sont_laissees"
MAREE_KO = "un_element_de_maree_mal_forme_est_saute_et_une_hauteur_vide_reste_inconnue"
COEF = "un_coefficient_hors_de_1_a_200_est_saute_et_il_en_reste_quatre_au_plus"
CRLF = "les_fins_de_ligne_windows_sont_acceptees_et_le_crc_les_couvre"
APRES_END = "une_trame_apres_la_ligne_end_est_ignoree"
CORROMPUE = "une_trame_dont_un_octet_a_change_est_refusee_et_la_mer_d_avant_est_gardee"
BALISE = "une_balise_inconnue_est_sautee_et_ne_coupe_pas_le_lien_entre_une_activite_et_ses_criteres"

REEL = "les_vues_des_trames_reelles_sont_celles_que_python_a_calculees"
HAUTEUR = "la_hauteur_de_la_mer_reste_a_moins_de_8_mm_du_calcul_a_virgule"
HORS = "avant_la_premiere_maree_et_apres_la_derniere_il_n_y_a_pas_de_hauteur"
MONTE = "la_mer_monte_sans_redescendre_de_la_basse_a_la_pleine_mer"
PROCHAINE = "la_prochaine_maree_d_une_trame_reelle_est_la_premiere_qui_n_est_pas_passee"
HEURE = "l_heure_courante_va_de_0_a_23_et_sort_des_bandes_aux_bornes"
AGE = "l_age_de_la_mer_passe_de_fraiche_a_vieillie_a_expiree_aux_bornes"
HORLOGE = "sans_mer_ou_sans_horloge_rien_n_est_affirme_frais"
GARDEE = "une_mer_gardee_par_l_api_est_vieillie_meme_toute_neuve"
VINGT_QUATRE = "les_24_heures_passees_la_mer_est_expiree_meme_si_elle_est_jeune"
MEILLEURE = "la_meilleure_activite_est_celle_du_meilleur_verdict_la_premiere_en_cas_d_egalite"
ABSENTE = "une_activite_absente_ou_indisponible_ne_peut_pas_etre_la_meilleure"
COEFFICIENT = "le_coefficient_est_celui_de_la_premiere_pleine_mer_a_venir"
EXTREMES = "la_hauteur_aux_extremes_au_milieu_et_au_quart"
DESCEND = "une_mer_qui_descend_et_des_hauteurs_negatives_sont_arrondies_loin_de_zero"
RACCORD = "la_courbe_se_raccorde_au_passage_d_une_maree_a_la_suivante"
ORDRE = "on_ne_dessine_pas_une_courbe_dont_les_marees_ne_sont_pas_dans_l_ordre_du_temps"
INCONNUE = "une_hauteur_inconnue_ou_une_seule_maree_ne_donne_pas_de_courbe"

MUTATIONS_MER = [
    # --- le CRC32 du contrat ---
    (TR, "c = (c & 1u) ? (c >> 1) ^ 0xEDB88320u : (c >> 1);", "c = (c & 1u) ? (c >> 1) ^ 0xEDB88321u : (c >> 1);",
     T, CRC32, "un autre polynôme"),
    (TR, "std::uint32_t c = 0xFFFFFFFFu;", "std::uint32_t c = 0u;", T, CRC32, "le CRC part de zéro au lieu de 0xFFFFFFFF"),
    (TR, "return ~c;", "return c;", T, CRC32, "le CRC n'est pas inversé à la fin"),
    (TR, "return crc32(t.substr(0, pos)) == annonce;", "return crc32(t.substr(0, pos + 4)) == annonce;",
     T, CRC_REEL, "le CRC couvre aussi les quatre premiers octets de la ligne END"),
    (TR, "return false; // pas de ligne END", "return true; // pas de ligne END", T, SANS_END,
     "une trame sans ligne END (coupée en route) est acceptée"),
    (TR, "return false; // plus de 8 chiffres : ce n'est pas un CRC", "{} // plus de 8 chiffres : ce n'est pas un CRC", V, NEUF,
     "un CRC de neuf chiffres est accepté"),
    (TR, "return c - '0';", "return c - '0' + 1;", T, CRC_REEL, "un chiffre décimal du CRC compte un de trop"),
    (TR, "return c - 'A' + 10;", "return c - 'A' + 11;", T, CRC_REEL, "une lettre majuscule du CRC compte un de trop"),
    (TR, "return c - 'a' + 10;", "return c - 'a' + 11;", V, NEUF, "une lettre minuscule du CRC compte un de trop"),

    # --- lecture des champs ---
    (TR, "if (c.n >= CHAMPS_MAX)", "if (c.n >= CHAMPS_MAX - 20)", T, CHAMPS, "une ligne est coupée à 12 champs"),
    (TR, "c.v[c.n++] = s.substr(debut, p - debut);", "c.v[c.n++] = s.substr(debut, p - debut - 1);", T, CHAMPS,
     "chaque champ perd son dernier caractère"),
    (TR, "debut = p + 1;\n    }\n}", "debut = p + 2;\n    }\n}", T, CHAMPS, "le séparateur compte deux caractères"),
    (TR, "if (v > 4294967295LL)", "if (v > 4294967296LL)", T, INSTANT, "un instant peut dépasser 32 bits"),
    (TR, "v = v * 10 + (c - '0');", "v = v * 10 + (c - '0') + 1;", T, INSTANT, "un chiffre d'un instant compte un de trop"),
    (TR, "        if (c < '0' || c > '9')\n            return 0;\n", "        if (c < '0' || c > '9')\n            continue;\n", T, INSTANT,
     "un instant « 12a » est lu 12"),
    (TR, "v = -v;", "v = +v;", T, DECIMALES, "un nombre négatif est lu positif"),
    (TR, "arrondir = t[i] >= '5';", "arrondir = t[i] > '5';", T, DECIMALES, "5 arrondit vers le bas (0,05 donne 0,0)"),
    (TR, "else if (lus == decimales)", "else if (lus == decimales + 1)", T, DECIMALES, "le premier chiffre perdu n'est plus celui qui arrondit"),
    (TR, "if (++nb_entier > 6)", "if (++nb_entier > 7)", T, ILLISIBLE, "sept chiffres avant la virgule sont acceptés"),
    (TR, "frac *= 10; // « 1.6 » avec 2 décimales vaut 160", "frac *= 1; // « 1.6 » avec 2 décimales vaut 160", T, DECIMALES,
     "des décimales manquantes ne sont pas complétées (12 vaut 12 au lieu de 120)"),
    (TR, "return false; // « 3. »", "{} // « 3. »", T, ILLISIBLE, "« 5. » est lu 5"),
    (TR, "if (nb_entier == 0)", "if (nb_entier == 1000)", T, ILLISIBLE, "« .5 » est lu 0,5"),
    (TR, "(arrondir ? 1 : 0)", "(arrondir ? 2 : 0)", T, DECIMALES, "l'arrondi ajoute deux"),
    (TR, "if (v > 100 || (v == 0 && t != \"0\"))", "if (v > 101 || (v == 0 && t != \"0\"))", T, POINTS, "101 points sont acceptés"),
    (TR, "if (v > 100 || (v == 0 && t != \"0\"))", "if (v > 100)", T, POINTS, "« 00 » ou « abc » donnent 0 point au lieu de l'inconnu"),
    (TR, "0xC0u) == 0x80u)", "0xF0u) == 0x80u)", T, TEXTE, "un nom coupé garde la moitié d'un caractère"),
    (TR, "std::size_t n = src.size() < N - 1 ? src.size() : N - 1;", "std::size_t n = src.size() < N - 1 ? src.size() : N - 2;",
     T, TEXTE, "un texte perd un caractère de place"),
    (TR, "if (t[i] == 'G')", "if (t[i] == 'H')", T, HOSSEGOR, "la lettre G n'est plus « bon »"),
    (TR, "else if (t[i] == 'P')", "else if (t[i] == 'Q')", V, BANDE, "la lettre P n'est plus « possible »"),
    (TR, "else if (t[i] == 'X')", "else if (t[i] == 'Y')", T, HOSSEGOR, "la lettre X n'est plus « déconseillé »"),
    (TR, "if (t[i] == '1')", "if (t[i] == '2')", T, HOSSEGOR, "le chiffre 1 n'est plus « jour »"),
    (TR, "else if (t[i] == '0')", "else if (t[i] == '9')", T, HOSSEGOR, "le chiffre 0 n'est plus « nuit »"),
    (TR, "constexpr void lire_heures(std::string_view t, Verdict (&sortie)[HEURES]) noexcept {\n"
         "    const bool entier = t.size() == static_cast<std::size_t>(HEURES);",
     "constexpr void lire_heures(std::string_view t, Verdict (&sortie)[HEURES]) noexcept {\n"
     "    const bool entier = t.size() >= static_cast<std::size_t>(HEURES);",
     V, BANDE, "une bande de 25 lettres est lue"),
    (TR, "constexpr void lire_lumiere(std::string_view t, Lumiere (&sortie)[HEURES]) noexcept {\n"
         "    const bool entier = t.size() == static_cast<std::size_t>(HEURES);",
     "constexpr void lire_lumiere(std::string_view t, Lumiere (&sortie)[HEURES]) noexcept {\n"
     "    const bool entier = true;",
     V, LUMIERES, "une bande de lumière de 23 caractères est lue"),

    # --- marées, coefficients, raison ---
    (TR, "if (p.n != 3 || (p.v[0] != \"H\" && p.v[0] != \"L\"))", "if (p.n != 3 || (p.v[0] != \"H\" || p.v[0] != \"L\"))",
     T, HOSSEGOR, "aucune marée n'est lue"),
    (TR, "if (instant == 0)", "if (instant == 7)", V, MAREE_KO, "une marée sans instant lisible est gardée"),
    (TR, "e.haute = p.v[0] == \"H\";", "e.haute = p.v[0] == \"L\";", T, HOSSEGOR, "les pleines mers sont des basses mers"),
    (TR, "if (lire_fixe(p.v[2], 2, cm))", "if (lire_fixe(p.v[2], 1, cm))", T, HOSSEGOR, "la hauteur d'une marée est lue en dixièmes"),
    (TR, "m.nb_marees < MAREES_MAX) {", "m.nb_marees < MAREES_MAX - 1) {", V, HUIT, "la huitième marée est laissée"),
    (TR, "m.nb_coefficients < COEFFICIENTS_MAX) {", "m.nb_coefficients < COEFFICIENTS_MAX - 1) {", V, COEF, "le quatrième coefficient est laissé"),
    (TR, "if (instant == 0 || valeur == 0 || valeur > 200)", "if (instant == 0 || valeur > 200)", V, COEF, "un coefficient de 0 est gardé"),
    (TR, "if (instant == 0 || valeur == 0 || valeur > 200)", "if (instant == 0 || valeur == 0 || valeur > 201)", V, COEF,
     "un coefficient de 201 est gardé"),
    (TR, "if (p.n != 2)", "if (p.n != 3)", T, HOSSEGOR, "aucun coefficient n'est lu"),
    (TR, "if (c == Critere::Inconnu || e == Etat::Inconnu)", "if (c == Critere::Inconnu && e == Etat::Inconnu)", T, RAISON,
     "une raison dont l'état est inconnu garde son critère"),
    (TR, "const std::size_t point = t.find('.');", "const std::size_t point = t.find(':');", T, RAISON, "la raison est coupée sur « : »"),
    (TR, "inline constexpr int MAREES_MAX = 8;", "inline constexpr int MAREES_MAX = 4;", T, HOSSEGOR, "quatre marées au plus"),
    (TR, "inline constexpr int COEFFICIENTS_MAX = 4;", "inline constexpr int COEFFICIENTS_MAX = 2;", T, HOSSEGOR, "deux coefficients au plus"),
    (TR, "inline constexpr int CRITERES_MAX = 6;", "inline constexpr int CRITERES_MAX = 4;", T, HOSSEGOR, "quatre critères au plus"),

    # --- la ligne SEA ---
    (TR, "if (f.n < 18 || f.v[1].empty())", "if (f.n < 12 || f.v[1].empty())", V, SEA_KO, "une ligne SEA de 15 champs est lue"),
    (TR, "if (f.n < 18 || f.v[1].empty())", "if (f.n < 18)", V, SEA_KO, "une ligne SEA sans nom est lue"),
    (TR, "if (genere == 0)", "if (genere == 7)", V, SEA_KO, "une ligne SEA sans date de génération est lue"),
    (TR, "m.donnees_gardees = f.v[5] == \"1\";", "m.donnees_gardees = f.v[5] != \"1\";", T, HOSSEGOR, "le drapeau « données gardées » est inversé"),
    (TR, "copier_texte(f.v[1], m.nom);", "copier_texte(f.v[2], m.nom);", T, HOSSEGOR, "le nom du spot est celui de la commune"),
    (TR, "m.premiere_heure_s = lire_instant(f.v[4]);", "m.premiere_heure_s = lire_instant(f.v[3]);", T, HOSSEGOR,
     "la première heure des bandes est la date de génération"),
    (TR, "lire_fixe(f.v[6], 1, d)", "lire_fixe(f.v[6], 2, d)", T, HOSSEGOR, "le vent minimal est lu en centièmes"),
    (TR, "lire_fixe(f.v[7], 1, d)", "lire_fixe(f.v[7], 2, d)", T, HOSSEGOR, "le vent maximal est lu en centièmes"),
    (TR, "m.lever_s = lire_instant(f.v[8]);", "m.lever_s = lire_instant(f.v[9]);", T, HOSSEGOR, "le lever du soleil est le coucher"),
    (TR, "m.coucher2_s = lire_instant(f.v[11]);", "m.coucher2_s = lire_instant(f.v[10]);", T, HOSSEGOR, "le second coucher est le second lever"),
    (TR, "lire_marees(f.v[12], m);", "lire_marees(f.v[13], m);", T, HOSSEGOR, "les marées sont lues dans le champ des coefficients"),
    (TR, "lire_coefficients(f.v[13], m);", "lire_coefficients(f.v[12], m);", T, HOSSEGOR, "les coefficients sont lus dans le champ des marées"),
    (TR, "m.vigilances_lues = f.v[14] == \"ok\";", "m.vigilances_lues = f.v[14] != \"unknown\";", V, ILLISIBLES,
     "tout état autre que « unknown » vaut « vigilances lues »"),
    (TR, "m.vigilances_lues = false;", "m.vigilances_lues = true;", V, ILLISIBLES,
     "un niveau inconnu de ce firmware est pris pour « rien à craindre »"),
    (TR, "if (m.vigilances_lues && !f.v[15].empty()) {", "if (!f.v[15].empty()) {", V, ILLISIBLES,
     "un niveau est lu même quand les vigilances n'ont pas pu l'être"),
    (TR, "m.vigilance_presente = m.vigilances_lues;", "m.vigilance_presente = true;", V, ILLISIBLES,
     "une vigilance illisible est déclarée présente"),
    (TR, "if (f.v[15] == \"GREEN\")", "if (f.v[15] == \"GREENX\")", V, NIVEAUX, "le niveau vert n'est pas lu"),
    (TR, "m.vigilance_niveau = vigie::Niveau::Jaune;", "m.vigilance_niveau = vigie::Niveau::Vert;", V, NIVEAUX, "jaune est lu vert"),
    (TR, "m.vigilance_niveau = vigie::Niveau::Orange;", "m.vigilance_niveau = vigie::Niveau::Rouge;", V, ORANGE, "orange est lu rouge"),
    (TR, "m.vigilance_niveau = vigie::Niveau::Rouge;", "m.vigilance_niveau = vigie::Niveau::Orange;", V, NIVEAUX, "rouge est lu orange"),
    (TR, "m.vigilance_fin_s = lire_instant(f.v[16]);", "m.vigilance_fin_s = lire_instant(f.v[15]);", V, ORANGE, "la fin de la vigilance est lue dans le niveau"),
    (TR, "copier_texte(f.v[17], m.vigilance_source);", "copier_texte(f.v[16], m.vigilance_source);", V, ORANGE, "la source est lue dans la fin"),

    # --- la ligne ACT ---
    (TR, "if (f.n < 12)", "if (f.n < 4)", V, COURTE, "une ligne ACT de 5 champs est lue"),
    (TR, "a.disponible = f.v[2] == \"1\";", "a.disponible = f.v[2] != \"1\";", T, HOSSEGOR, "une activité jugeable est déclarée indisponible"),
    (TR, "return indice; // les champs 3 à 11 sont vides alors", "{} // les champs 3 à 11 sont vides alors", V, INDISPO,
     "une activité indisponible garde les champs qu'elle porte"),
    (TR, "a.verdict = verdict_de_nom(f.v[3]);", "a.verdict = verdict_de_nom(f.v[4]);", T, HOSSEGOR, "le verdict est lu dans la raison"),
    (TR, "a.creneau_debut_s = lire_instant(f.v[5]);", "a.creneau_debut_s = lire_instant(f.v[6]);", T, HOSSEGOR, "le début du créneau est sa fin"),
    (TR, "a.creneau_fin_s <= a.creneau_debut_s", "a.creneau_fin_s < a.creneau_debut_s", V, CRENEAU, "un créneau vide est gardé"),
    (TR, "a.creneau_debut_s == 0 || a.creneau_fin_s", "a.creneau_debut_s == 0 && a.creneau_fin_s", V, CRENEAU, "un créneau sans début est gardé"),
    (TR, "lire_heures(f.v[7], a.heures);", "lire_heures(f.v[8], a.heures);", T, HOSSEGOR, "les heures sont lues dans la bande de lumière"),
    (TR, "lire_lumiere(f.v[8], a.lumiere);", "lire_lumiere(f.v[7], a.lumiere);", T, HOSSEGOR, "la lumière est lue dans la bande des heures"),
    (TR, "a.detail_s = lire_instant(f.v[9]);", "a.detail_s = lire_instant(f.v[8]);", T, HOSSEGOR, "l'heure du détail est lue dans la lumière"),
    (TR, "a.points = lire_points(f.v[10]);", "a.points = lire_points(f.v[9]);", T, HOSSEGOR, "les points sont lus dans l'heure du détail"),
    (TR, "a.sur = lire_points(f.v[11]);", "a.sur = lire_points(f.v[10]);", T, HOSSEGOR, "le maximum est lu dans les points"),
    (TR, "a.raison_etat);\n", "a.raison_critere);\n", T, HOSSEGOR, "l'état de la raison est écrit dans le critère"),

    # --- la ligne CRIT ---
    (TR, "if (f.n < 9 || courant < 0)", "if (f.n < 4 || courant < 0)", V, COURTE, "une ligne CRIT de 5 champs est lue"),
    (TR, "if (activite_de_nom(f.v[1]) != static_cast<Activite>(courant))", "if (activite_de_nom(f.v[1]) == Activite::Nombre)",
     V, CRIT_AUTRE, "un CRIT d'une autre activité se range sous l'ACT du dessus"),
    (TR, "if (a.nb_criteres >= CRITERES_MAX)", "if (a.nb_criteres >= CRITERES_MAX + 8)", V, SIX, "le septième critère est gardé"),
    (TR, "if (c.id == Critere::Inconnu)", "if (false)", V, CRIT_NOM, "un critère de nom inconnu est compté"),
    (TR, "c.unite = unite_de_nom(f.v[4]);", "c.unite = unite_de_nom(f.v[3]);", T, HOSSEGOR, "l'unité est lue dans la valeur"),
    (TR, "c.etat = etat_de_nom(f.v[5]);", "c.etat = etat_de_nom(f.v[6]);", T, HOSSEGOR, "l'état est lu dans le verdict"),
    (TR, "c.verdict = verdict_de_nom(f.v[6]);", "c.verdict = verdict_de_nom(f.v[5]);", T, HOSSEGOR, "le verdict est lu dans l'état"),
    (TR, "c.points = lire_points(f.v[7]);", "c.points = lire_points(f.v[6]);", T, HOSSEGOR, "les points sont lus dans le verdict"),
    (TR, "c.points_max = lire_points(f.v[8]);", "c.points_max = lire_points(f.v[7]);", T, HOSSEGOR, "le maximum est lu dans les points"),
    (TR, "if (lire_fixe(f.v[3], 1, d))", "if (lire_fixe(f.v[3], 2, d))", T, HOSSEGOR, "la valeur d'un critère est lue en centièmes"),

    # --- la trame entière ---
    (TR, "constexpr bool decoder(std::string_view trame, Mer &sortie) noexcept {\n    if (trame.empty() || !detail::crc_valide(trame))",
     "constexpr bool decoder(std::string_view trame, Mer &sortie) noexcept {\n    if (false)", V, CORROMPUE,
     "le décodeur ne vérifie plus le CRC"),
    (TR, "continue; // une seule ligne SEA par trame : la première gagne", "a_sea = false; // une seule ligne SEA par trame : la première gagne",
     V, DEUX_SEA, "avec deux lignes SEA, la seconde gagne"),
    (TR, "continue; // ACT et CRIT n'ont de sens qu'après leur ligne SEA", "{} // ACT et CRIT n'ont de sens qu'après leur ligne SEA",
     V, ACT_AVANT, "une ligne ACT avant la ligne SEA est gardée"),
    (TR, "if (!ligne.empty() && ligne.back() == '\\r')", "if (false)", V, CRLF, "le retour chariot reste dans le dernier champ"),
    (TR, "if (f.v[0] == \"END\")", "if (f.v[0] == \"ENDX\")", V, APRES_END, "ce qui suit la ligne END est lu"),
    (TR, "constexpr MiseAJour mettre_a_jour(std::string_view trame, Mer &mer) noexcept {\n    if (trame.empty() || !detail::crc_valide(trame))",
     "constexpr MiseAJour mettre_a_jour(std::string_view trame, Mer &mer) noexcept {\n    if (trame.empty())", V, SANS_BLOC,
     "une trame sans bloc et au CRC faux est dite « sans bloc »"),
    (TR, "return MiseAJour::SansBloc;", "return MiseAJour::Refusee;", V, SANS_BLOC, "une trame sans bloc est refusée"),
    (TR, "return decoder(trame, mer) ? MiseAJour::Remplacee : MiseAJour::Refusee;", "return decoder(trame, mer) ? MiseAJour::Remplacee : MiseAJour::SansBloc;",
     V, SEA_KO, "une ligne SEA illisible est prise pour une trame sans bloc"),
    (TR, "if (!detail::a_une_ligne(trame, \"SEA\"))", "if (!detail::a_une_ligne(trame, \"SE\"))", V, REMPLACE, "la ligne SEA n'est plus reconnue"),
    (TR, "&& reste[balise.size()] == '|')", "&& true)", V, SEAX, "SEAX|… est pris pour la ligne SEA"),
    (TR, "if (spot.empty() || spot.size() > 64)", "if (spot.empty() || spot.size() > 65)", T, SLUG, "un identifiant de 65 caractères est accepté"),
    (TR, "c == '-' || c == '_'", "c == '-' || c == '.'", T, SLUG, "le tiret bas n'est plus permis"),
    (TR, "(c >= '0' && c <= '9') || c == '-'", "(c >= '0' && c <= '8') || c == '-'", T, SLUG, "le chiffre 9 n'est plus permis"),
    (TR, "(c >= 'a' && c <= 'z')", "(c >= 'a' && c <= 'y')", T, SLUG, "la lettre z n'est plus permise"),

    # --- les noms du contrat ---
    (TR, "if (nom == \"poor\")", "if (nom == \"bad\")", T, NOMS_A, "« poor » n'est plus « déconseillé »"),
    (TR, "if (nom == \"good\")", "if (nom == \"goood\")", T, NOMS_A, "« good » n'est plus « bon »"),
    (TR, "if (nom == \"possible\")", "if (nom == \"possibles\")", T, NOMS_A, "« possible » n'est plus « possible »"),
    (TR, "if (nom == \"km/h\")", "if (nom == \"kmh\")", T, NOMS_A, "l'unité km/h n'est plus lue"),
    (TR, "if (nom == \"Beaufort\")", "if (nom == \"beaufort\")", T, NOMS_A, "l'unité Beaufort n'est plus lue"),
    (TR, "if (nom == \"min\")", "if (nom == \"mn\")", T, NOMS_A, "l'unité des minutes n'est plus lue"),
    (TR, "if (nom == \"kn\")", "if (nom == \"knots\")", T, NOMS_A, "l'unité des nœuds n'est plus lue"),
    (TR, "{\"surf\", \"kite\", \"boat\", \"shore_fishing\"}", "{\"kite\", \"surf\", \"boat\", \"shore_fishing\"}", T, NOMS_A,
     "le surf et le kite sont échangés"),
    (TR, "\"swell_height\", \"swell_period\",", "\"swell_period\", \"swell_height\",", T, NOMS_C, "hauteur et période de houle échangées"),
    (TR, '    "too_small",\n    "small",\n', '    "small",\n    "too_small",\n', T, NOMS_E, "« too_small » et « small » échangés"),
    (TR, '    "right_stage",\n    "wrong_stage",\n', '    "wrong_stage",\n    "right_stage",\n', T, NOMS_E,
     "« right_stage » et « wrong_stage » échangés"),
    (TR, '    "orange",\n    "red",\n', '    "red",\n    "orange",\n', T, NOMS_E, "« orange » et « red » échangés"),
    (TR, '    "in_window",\n    "outside_window",\n', '    "outside_window",\n    "in_window",\n', T, NOMS_E,
     "« in_window » et « outside_window » échangés"),

    # --- la vue : âge, heure, meilleure activité, marée, hauteur, coefficient ---
    (VU, "inline constexpr std::int64_t VIEILLIE_APRES_S = 600;", "inline constexpr std::int64_t VIEILLIE_APRES_S = 601;", U, AGE,
     "la mer reste fraîche une seconde de plus"),
    (VU, "inline constexpr std::int64_t EXPIREE_APRES_S = 21600;", "inline constexpr std::int64_t EXPIREE_APRES_S = 21601;", U, AGE,
     "la mer expire une seconde plus tard"),
    (VU, "if (age > EXPIREE_APRES_S)", "if (age >= EXPIREE_APRES_S)", U, AGE, "la mer expire à six heures pile"),
    (VU, "mer.donnees_gardees || age > VIEILLIE_APRES_S", "age > VIEILLIE_APRES_S", U, GARDEE, "une mer gardée par l'API est dite fraîche"),
    (VU, "age > VIEILLIE_APRES_S", "age >= VIEILLIE_APRES_S", U, AGE, "la mer vieillit à dix minutes pile"),
    (VU, "mer.premiere_heure_s != 0 && maintenant_s >=", "maintenant_s >=", U, AGE,
     "sans heure de départ, les 24 heures sont comptées depuis 0 et la mer expire aussitôt"),
    (VU, "maintenant_s >= mer.premiere_heure_s + static_cast<std::int64_t>(HEURES) * 3600)",
     "maintenant_s > mer.premiere_heure_s + static_cast<std::int64_t>(HEURES) * 3600)", U, VINGT_QUATRE,
     "la mer n'expire qu'une seconde après la fin des 24 heures"),
    (VU, "return Fraicheur::Vieillie; // pas d'horloge", "return Fraicheur::Fraiche; // pas d'horloge", U, HORLOGE,
     "sans horloge, la mer est dite fraîche"),
    (VU, "return Fraicheur::Rien;", "return Fraicheur::Fraiche;", U, HORLOGE, "sans mer, il y a une mer fraîche"),
    (VU, "mer.premiere_heure_s == 0 || maintenant_s < mer.premiere_heure_s", "maintenant_s < mer.premiere_heure_s", U, HEURE,
     "sans heure de départ, on est à l'heure 0"),
    (VU, "maintenant_s < mer.premiere_heure_s)", "maintenant_s <= mer.premiere_heure_s)", U, HEURE, "la première seconde n'est pas dans l'heure 0"),
    (VU, "/ 3600;", "/ 3601;", U, HEURE, "une heure dure 3 601 secondes"),
    (VU, "return indice < HEURES ?", "return indice <= HEURES ?", U, HEURE, "l'heure 24 existe"),
    (VU, "if (!mer.valide ||", "if (", U, HEURE, "une mer invalide a une heure"),
    (VU, "return 3;", "return 1;", U, MEILLEURE, "« bon » ne vaut pas plus que « déconseillé »"),
    (VU, "return 2;", "return 0;", U, MEILLEURE, "« possible » ne vaut rien"),
    (VU, "return 1;", "return 0;", U, MEILLEURE, "« déconseillé » ne vaut pas plus qu'inconnu"),
    (VU, "if (rang > meilleur_rang)", "if (rang >= meilleur_rang)", U, MEILLEURE, "en cas d'égalité, la dernière activité gagne"),
    (VU, "if (!a.presente || !a.disponible)", "if (!a.presente && !a.disponible)", U, ABSENTE, "une activité indisponible peut être la meilleure"),
    (VU, "if (mer.marees[i].instant_s >= maintenant_s)", "if (mer.marees[i].instant_s > maintenant_s)", U, PROCHAINE, "une marée à la seconde même est déjà passée"),
    (VU, "if (a.instant_s == 0 || b.instant_s <= a.instant_s)", "if (a.instant_s == 0 || b.instant_s < a.instant_s)", U, ORDRE,
     "deux marées au même instant divisent par zéro"),
    (VU, "if (instant_s < a.instant_s || instant_s > b.instant_s)", "if (instant_s <= a.instant_s || instant_s > b.instant_s)", U, EXTREMES,
     "la hauteur à l'instant de la première marée est inconnue"),
    (VU, "180 * ecoule", "90 * ecoule", U, EXTREMES, "la courbe ne fait qu'un quart de période"),
    (VU, "surf::cosinus_pm(deg + 1)", "surf::cosinus_pm(deg)", U, HAUTEUR, "le cosinus n'est pas interpolé entre deux degrés"),
    (VU, "(c1 - c0) * reste", "(c1 - c0) * (duree - reste)", U, HAUTEUR, "l'interpolation du cosinus va à l'envers"),
    (VU, "1000 * duree - cosinus_x_duree", "1000 * duree + cosinus_x_duree", U, EXTREMES, "la courbe monte quand elle devrait descendre"),
    (VU, "surf::division_arrondie(ecart * (1000 * duree - cosinus_x_duree), 2000 * duree)", "(ecart * (1000 * duree - cosinus_x_duree)) / (2000 * duree)",
     U, DESCEND, "la hauteur est tronquée au lieu d'être arrondie"),
    (VU, "a.hauteur_cm == INCONNU || b.hauteur_cm == INCONNU", "a.hauteur_cm == INCONNU && b.hauteur_cm == INCONNU", U, INCONNUE,
     "une hauteur de marée inconnue donne une courbe"),
    (VU, "mer.coefficients[i].valeur != 0 && mer.coefficients[i].pleine_mer_s >= maintenant_s", "mer.coefficients[i].pleine_mer_s >= maintenant_s",
     U, COEFFICIENT, "un coefficient de 0 est choisi"),
    (VU, "mer.coefficients[i].pleine_mer_s >= maintenant_s", "mer.coefficients[i].pleine_mer_s > maintenant_s", U, COEFFICIENT,
     "le coefficient d'une pleine mer à la seconde même est déjà passé"),
]
