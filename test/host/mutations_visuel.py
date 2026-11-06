"""Mutations du noyau « écran principal » (visuel_icones, visuel_astres, visuel_echelles), lues par lancer.py --mutations.

Même format que MUTATIONS dans lancer.py : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —,
remplacement, test qui doit échouer, nom de la fonction de test qui doit être citée (None : n'importe laquelle), ce
que la mutation simule). Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Non couvert : l'arrondi de `melanger` (+ 500) : l'écart d'une unité sur un canal de couleur ne se voit pas et le test
l'accepte exprès (0x7F ou 0x80).
"""

ICONES = "test_visuel_icones.cpp"
ASTRES = "test_visuel_astres.cpp"
ECHELLES = "test_visuel_echelles.cpp"

MUTATIONS_VISUEL = [
    # --- visuel_icones.hpp ---
    ("visuel_icones.hpp", "return jour ? IdIcone::averses_jour : IdIcone::averses_nuit;", "return IdIcone::averses_jour;", ICONES,
     "le_jour_et_la_nuit_different_pour_le_ciel_degage_et_les_averses", "les averses de nuit montrent le soleil"),
    ("visuel_icones.hpp", "case meteo::Icone::OrageGrele:\n        return IdIcone::orage_grele;",
     "case meteo::Icone::OrageGrele:\n        return IdIcone::orage;", ICONES, "aucune_icone_n_est_orpheline",
     "l'orage avec grêle prend l'icône d'orage (l'icône de grêle n'est plus jamais montrée)"),
    ("visuel_icones.hpp", "if (tailles[i] <= cible)", "if (tailles[i] <= cible + 1)", ICONES,
     "la_taille_choisie_ne_depasse_jamais_la_place", "une image d'un pixel trop grande déborde de sa place"),
    ("visuel_icones.hpp", "const int i = ((indice % NB_PHASES_LUNE) + NB_PHASES_LUNE) % NB_PHASES_LUNE;",
     "const int i = indice % NB_PHASES_LUNE;", ICONES, "les_phases_de_lune_sont_huit_pieces_consecutives",
     "un indice de lune négatif sort du tableau"),
    # --- visuel_astres.hpp ---
    ("visuel_astres.hpp", "LUNE_MOIS_SYNODIQUE_S = 2551443;", "LUNE_MOIS_SYNODIQUE_S = 2591443;", ASTRES, None,
     "le mois lunaire dure 0,5 jour de trop"),
    ("visuel_astres.hpp", "LUNE_NOUVELLE_REFERENCE_S = 947182440;", "LUNE_NOUVELLE_REFERENCE_S = 947614440;", ASTRES, None,
     "la nouvelle lune de référence est décalée de 5 jours"),
    ("visuel_astres.hpp", "(1000 - detail::cos_tour_milliemes(milliemes_de_tour) + 10) / 20",
     "(1000 + detail::cos_tour_milliemes(milliemes_de_tour) + 10) / 20", ASTRES, None,
     "la nouvelle lune est éclairée à 100 %"),
    ("visuel_astres.hpp", "(age_s * LUNE_NB_PHASES * 2 + LUNE_MOIS_SYNODIQUE_S)", "(age_s * LUNE_NB_PHASES * 2)", ASTRES, None,
     "l'indice de phase est tronqué au lieu d'être arrondi"),
    ("visuel_astres.hpp", "if (m > 500)\n        m -= 1000;", "if (false)\n        m -= 1000;", ASTRES,
     "la_serie_de_taylor_est_assez_juste", "le cosinus n'est plus ramené entre -pi et pi"),
    ("visuel_astres.hpp", "DUREE_AUBE_S = 40 * 60;", "DUREE_AUBE_S = 5 * 60;", ASTRES,
     "le_moment_de_la_journee_suit_le_lever_et_le_coucher", "l'aube ne dure plus que 5 minutes"),
    ("visuel_astres.hpp", "(maintenant_s - lever_s) * 1000 / a.duree_jour_s", "(maintenant_s - lever_s) * 100 / a.duree_jour_s",
     ASTRES, "la_position_sur_l_arc_va_de_0_a_1000", "le soleil n'atteint que le dixième de l'arc"),
    ("visuel_astres.hpp", "if (lever_s <= 0 || coucher_s <= lever_s) {", "if (lever_s <= 0) {", ASTRES,
     "sans_lever_ni_coucher_on_se_fie_a_is_day", "nuit polaire (lever = coucher) prise pour un jour normal"),
    # --- visuel_echelles.hpp ---
    ("visuel_echelles.hpp",
     "    case meteo::Icone::Orage:\n    case meteo::Icone::OrageGrele:\n        return Ambiance::ORAGE;",
     "    case meteo::Icone::Orage:\n    case meteo::Icone::OrageGrele:\n        return Ambiance::PLUIE;", ECHELLES,
     "chaque_code_a_une_ambiance_et_les_familles_sont_les_bonnes", "le ciel d'orage est celui de la pluie"),
    ("visuel_echelles.hpp", "if (uv_dix < 60)", "if (uv_dix < 70)", ECHELLES, "l_indice_uv_suit_l_echelle_de_l_oms",
     "l'indice UV 6 reste « modéré »"),
    ("visuel_echelles.hpp", "1030, 1180}", "1030, 1190}", ECHELLES, "le_vent_suit_l_echelle_de_beaufort",
     "l'ouragan commence à 119 km/h"),
    ("visuel_echelles.hpp", "if (variation_dix_hpa_3h >= 10)", "if (variation_dix_hpa_3h >= 11)", ECHELLES,
     "la_tendance_de_pression_a_un_seuil_de_un_hectopascal", "une hausse de 1,0 hPa n'est plus une tendance"),
    ("visuel_echelles.hpp", "return ((degres * 2 + 45) / 90) % 8;", "return ((degres * 2 + 45) / 90);", ECHELLES,
     "le_vent_se_range_en_secteurs_de_45_degres", "un vent de 350° tombe sur un secteur 8 qui n'existe pas"),
    ("visuel_echelles.hpp", "(degres_d_ou_il_vient + 180) % 360", "degres_d_ou_il_vient % 360", ECHELLES,
     "le_vent_se_range_en_secteurs_de_45_degres", "la flèche du vent pointe d'où il vient"),
    ("visuel_echelles.hpp", "SEUIL_CHALEUR_DIX = 320;", "SEUIL_CHALEUR_DIX = 330;", ECHELLES,
     "les_alertes_ont_une_priorite_et_ne_s_inventent_pas", "la chaleur s'alerte à 33 °C"),
    ("visuel_echelles.hpp", "e.proba_pluie_pct >= 60", "e.proba_pluie_pct >= 50", ECHELLES,
     "les_alertes_ont_une_priorite_et_ne_s_inventent_pas", "la pluie forte s'alerte avec une probabilité de 50 %"),
    ("visuel_echelles.hpp", "if (orage)\n        return Alerte::ORAGE;", "if (false)\n        return Alerte::ORAGE;", ECHELLES,
     "les_alertes_ont_une_priorite_et_ne_s_inventent_pas", "l'orage ne déclenche plus d'alerte"),
    ("visuel_echelles.hpp", "if (minutes > 0)\n        return PluieProche::BIENTOT;", "if (minutes > 1)\n        return PluieProche::BIENTOT;",
     ECHELLES, "la_pluie_proche_distingue_les_quatre_cas", "la pluie dans 1 minute n'est pas annoncée"),
    ("visuel_echelles.hpp", "int h = (haut >= 0 ? (haut + 9) / 10 : -((-haut) / 10)) * 10;",
     "int h = (haut >= 0 ? haut / 10 : -((-haut) / 10)) * 10;", ECHELLES,
     "l_echelle_de_la_semaine_arrondit_vers_l_exterieur_et_n_est_jamais_nulle", "le maximum de la semaine dépasse de l'échelle"),
    ("visuel_echelles.hpp", "if (h - b < ETENDUE_MIN_DIX) {", "if (false) {", ECHELLES,
     "l_echelle_de_la_semaine_arrondit_vers_l_exterieur_et_n_est_jamais_nulle", "une semaine plate donne des barres pleines"),
    ("visuel_echelles.hpp", "borner(valeur_dix, e.mini_dix, e.maxi_dix) - e.mini_dix", "valeur_dix - e.mini_dix", ECHELLES,
     "une_valeur_est_placee_sur_l_echelle_et_bornee", "une température hors échelle sort de la barre"),
    ("visuel_echelles.hpp", "return h < 2 ? 2 : h;", "return h;", ECHELLES, "les_barres_de_pluie_se_voient_des_5_pour_cent",
     "une pluie à 1 % ne laisse aucune barre visible"),
    ("visuel_echelles.hpp", "return r.teinte[2];", "return r.teinte[0];", ECHELLES, "la_couleur_d_une_temperature_suit_la_rampe",
     "une température inconnue s'affiche en bleu glacial"),
    ("visuel_echelles.hpp", "return (a >= 0 ? a / b : -((-a + b - 1) / b));", "return a / b;", ECHELLES,
     "l_heure_et_le_jour_locaux_suivent_le_decalage", "la division arrondit vers zéro pour les instants négatifs"),
    ("visuel_echelles.hpp", "+ 4 + 7) % 7", "+ 3 + 7) % 7", ECHELLES, "l_heure_et_le_jour_locaux_suivent_le_decalage",
     "tous les jours de la semaine sont décalés d'un cran"),
    ("visuel_echelles.hpp", "sortie[n++] = static_cast<char>(0x92);", "sortie[n++] = static_cast<char>(0x91);", ECHELLES,
     "les_formats_de_nombres_utilisent_le_vrai_signe_moins", "le signe moins est un autre caractère (U+2211)"),
    ("visuel_echelles.hpp", "return dixiemes >= 0 ? (dixiemes + 5) / 10 : -((-dixiemes + 5) / 10);", "return dixiemes / 10;",
     ECHELLES, "les_formats_de_nombres_utilisent_le_vrai_signe_moins", "la température est tronquée au lieu d'être arrondie"),
]
