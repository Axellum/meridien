"""Mutations du mode automatique de l'accueil (components/core/include/ecran/accueil_auto.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_SURF : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test qui
doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou « détectée
par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle écrite dans l'en-tête et l'ADR-0015 : un seuil, une borne incluse ou exclue, une priorité, un
garde-fou, la façon de traiter une valeur inconnue. Ce sont des mutations choisies à la main sur les règles de ce noyau, pas un
balayage automatique : elles ne disent pas que « tout » est testé, seulement que ces règles-là le sont. Les mutations qui
changeraient seulement le CODE sans changer le comportement (un contrôle redondant) n'y figurent pas : il n'y a rien à détecter.
"""

T = "test_accueil_auto.cpp"
A = "accueil_auto.hpp"

# Les trois blocs « règle 3 », « règle 4 », « règle 5 » de detail::choisir_profil, tels qu'ils sont écrits (pour en échanger l'ordre).
BLOC_SURF = (
    "    if (e.spot_surf && e.activite_surf &&\n"
    "        creneau_de_sortie(e.maintenant_s, e.decalage_local_s, e.lever_s, e.coucher_s)) {\n"
    "        if (e.fraicheur_mer == Fraicheur::Fraiche)\n"
    "            return {Profil::Surf, Raison::Surf};\n"
    "        perimee = perimee || e.fraicheur_mer == Fraicheur::Perimee;\n"
    "    }\n"
)
BLOC_VENT = (
    "    if (e.activite_vent_bateau && vent_au_dessus_du_seuil(e.vent_dixiemes_kmh, e.seuil_vent_dixiemes_kmh)) {\n"
    "        if (e.fraicheur_meteo == Fraicheur::Fraiche)\n"
    "            return {Profil::VentBateau, Raison::VentBateau};\n"
    "        perimee = perimee || e.fraicheur_meteo == Fraicheur::Perimee;\n"
    "    }\n"
)
BLOC_COTE = (
    "    if (e.spot_peche_a_pied && basse_mer_proche(e.basse_mer_dans_min)) {\n"
    "        if (e.fraicheur_mer == Fraicheur::Fraiche)\n"
    "            return {Profil::Cote, Raison::Cote};\n"
    "        perimee = perimee || e.fraicheur_mer == Fraicheur::Perimee;\n"
    "    }\n"
)

BLOC_ALERTE_PASTILLE = (
    "    if (d.bandeau_agrandi)\n"
    "        d.raison = e.niveau_alerte >= vigie::Niveau::Rouge ? Raison::AlerteRouge : Raison::AlerteOrange;\n"
    "    else if (d.carte_pluie)\n"
    "        d.raison = Raison::PluieProche;\n"
)

BLOC_PERMIS = (
    "        const bool permis = !e.toucher_en_cours && e.maintenant_s > 0 &&\n"
    "                            (voulu.raison == Raison::DonneesPerimees || delai_ecoule(avant, e.maintenant_s));\n"
)

MUTATIONS_ACCUEIL_AUTO = [
    # --- règle 1 : l'alerte officielle ---
    (A, "return n >= ALERTE_NIVEAU_MIN;", "return n > ALERTE_NIVEAU_MIN;",
     T, "alerte_orange_et_rouge_agrandissent_le_bandeau_pas_en_dessous", "une alerte orange n'agrandit plus le bandeau"),
    (A, "ALERTE_NIVEAU_MIN = vigie::Niveau::Orange;", "ALERTE_NIVEAU_MIN = vigie::Niveau::Jaune;",
     T, "alerte_orange_et_rouge_agrandissent_le_bandeau_pas_en_dessous", "une alerte jaune agrandit le bandeau"),
    (A, "ALERTE_NIVEAU_MIN = vigie::Niveau::Orange;", "ALERTE_NIVEAU_MIN = vigie::Niveau::Rouge;",
     T, "alerte_orange_et_rouge_agrandissent_le_bandeau_pas_en_dessous", "seule une alerte rouge agrandit le bandeau"),
    (A, "d.raison = e.niveau_alerte >= vigie::Niveau::Rouge ? Raison::AlerteRouge : Raison::AlerteOrange;",
     "d.raison = e.niveau_alerte >= vigie::Niveau::Rouge ? Raison::AlerteOrange : Raison::AlerteRouge;",
     T, "alerte_orange_et_rouge_agrandissent_le_bandeau_pas_en_dessous", "la pastille dit orange pour une alerte rouge"),
    (A, "d.bandeau_agrandi = alerte_agrandit_le_bandeau(e.niveau_alerte);",
     "d.bandeau_agrandi = alerte_agrandit_le_bandeau(e.niveau_alerte) && !e.epingle;",
     T, "l_alerte_agrandit_meme_epinglee_retenue_ou_pendant_un_toucher", "un profil épinglé empêche le bandeau agrandi"),
    (A, "d.bandeau_agrandi = alerte_agrandit_le_bandeau(e.niveau_alerte);",
     "d.bandeau_agrandi = alerte_agrandit_le_bandeau(e.niveau_alerte) && !e.toucher_en_cours;",
     T, "l_alerte_agrandit_meme_epinglee_retenue_ou_pendant_un_toucher", "un toucher en cours empêche le bandeau agrandi"),
    (A, "d.bandeau_agrandi = alerte_agrandit_le_bandeau(e.niveau_alerte);",
     "d.bandeau_agrandi = alerte_agrandit_le_bandeau(e.niveau_alerte) && e.etat.profil == Profil::Meteo;",
     T, "l_alerte_agrandit_meme_epinglee_retenue_ou_pendant_un_toucher", "le bandeau dépend du profil montré"),
    (A, "d.bandeau_agrandi = alerte_agrandit_le_bandeau(e.niveau_alerte);",
     "d.bandeau_agrandi = alerte_agrandit_le_bandeau(e.niveau_alerte);\n"
     "    if (d.bandeau_agrandi) {\n        d.raison = Raison::AlerteOrange;\n        return d;\n    }",
     T, "une_alerte_n_est_pas_un_changement_de_profil", "une alerte coupe court : plus de profil choisi par les règles 3 à 6"),
    (A, BLOC_ALERTE_PASTILLE,
     "    if (d.carte_pluie)\n        d.raison = Raison::PluieProche;\n    else if (d.bandeau_agrandi)\n"
     "        d.raison = e.niveau_alerte >= vigie::Niveau::Rouge ? Raison::AlerteRouge : Raison::AlerteOrange;\n",
     T, "l_alerte_passe_avant_la_pluie_dans_la_pastille", "la pluie passe avant l'alerte dans la pastille"),

    # --- règle 2 : la pluie proche ---
    (A, "pluie_dans_min < PLUIE_PROCHE_MAX_MIN;", "pluie_dans_min <= PLUIE_PROCHE_MAX_MIN;",
     T, "pluie_a_59_minutes_oui_a_60_et_61_non", "la carte pluie apparaît à 60 minutes"),
    (A, "PLUIE_PROCHE_MAX_MIN = 60;", "PLUIE_PROCHE_MAX_MIN = 59;",
     T, "pluie_a_59_minutes_oui_a_60_et_61_non", "la carte pluie n'apparaît plus à 59 minutes"),
    (A, "PLUIE_PROCHE_MAX_MIN = 60;", "PLUIE_PROCHE_MAX_MIN = 61;",
     T, "pluie_a_59_minutes_oui_a_60_et_61_non", "la carte pluie apparaît à 60 minutes (seuil à 61)"),
    (A, "meteo == Fraicheur::Fraiche && pluie_dans_min >= 0", "meteo != Fraicheur::Perimee && pluie_dans_min >= 0",
     T, "la_carte_pluie_veut_des_previsions_fraiches", "une prévision de fraîcheur inconnue montre la carte pluie"),
    (A, "meteo == Fraicheur::Fraiche && pluie_dans_min >= 0", "meteo != Fraicheur::Inconnue && pluie_dans_min >= 0",
     T, "la_carte_pluie_veut_des_previsions_fraiches", "une prévision périmée montre la carte pluie"),
    (A, "pluie_dans_min >= 0 && pluie_dans_min <", "pluie_dans_min > 0 && pluie_dans_min <",
     T, "pluie_a_59_minutes_oui_a_60_et_61_non", "la pluie déjà en cours (0 minute) n'a plus de carte"),
    (A, "pluie_dans_min >= 0 && pluie_dans_min <", "pluie_dans_min <",
     T, "pluie_a_59_minutes_oui_a_60_et_61_non", "« aucune pluie » (-1) montre la carte pluie"),
    (A, "        d.etat.profil = e.profil_epingle;\n        return d;",
     "        d.etat.profil = e.profil_epingle;\n        d.carte_pluie = pluie_proche(e.fraicheur_meteo, e.pluie_dans_min);\n        return d;",
     T, "un_profil_epingle_n_a_pas_de_carte_pluie", "la carte pluie s'ajoute à un profil épinglé"),
    (A, "const detail::Choix voulu = detail::choisir_profil(e);",
     "const detail::Choix voulu = d.carte_pluie ? detail::Choix{} : detail::choisir_profil(e);",
     T, "la_carte_pluie_recouvre_le_profil_sans_le_changer", "la carte pluie change le profil (Météo) au lieu de le recouvrir"),
    (A, "        d.raison = Raison::PluieProche;", "        d.raison = d.raison_profil;",
     T, "la_carte_pluie_recouvre_le_profil_sans_le_changer", "la pastille ne dit pas « pluie proche »"),

    # --- règle 3 : le créneau de sortie ---
    (A, "seconde_du_jour < CRENEAU_MATIN_FIN_S;", "seconde_du_jour <= CRENEAU_MATIN_FIN_S;",
     T, "le_creneau_du_matin_va_du_lever_a_10_h_bornes_a_la_seconde", "10 h pile est encore dans le créneau du matin"),
    (A, "CRENEAU_MATIN_FIN_S = 10 * 3600;", "CRENEAU_MATIN_FIN_S = 11 * 3600;",
     T, "le_creneau_du_matin_va_du_lever_a_10_h_bornes_a_la_seconde", "le créneau du matin finit à 11 h"),
    (A, "maintenant_s >= lever_s && seconde_du_jour", "maintenant_s > lever_s && seconde_du_jour",
     T, "le_creneau_du_matin_va_du_lever_a_10_h_bornes_a_la_seconde", "le créneau du matin ne commence qu'une seconde après le lever"),
    (A, "const bool matin = maintenant_s >= lever_s && seconde_du_jour", "const bool matin = seconde_du_jour",
     T, "hors_creneau_midi_nuit_et_avant_le_lever", "le créneau du matin commence à minuit, avant le lever"),
    (A, "seconde_du_jour >= CRENEAU_SOIR_DEBUT_S &&", "seconde_du_jour > CRENEAU_SOIR_DEBUT_S &&",
     T, "le_creneau_du_soir_va_de_16_h_au_coucher_bornes_a_la_seconde", "16 h pile n'est pas dans le créneau du soir"),
    (A, "CRENEAU_SOIR_DEBUT_S = 16 * 3600;", "CRENEAU_SOIR_DEBUT_S = 15 * 3600;",
     T, "le_creneau_du_soir_va_de_16_h_au_coucher_bornes_a_la_seconde", "le créneau du soir commence à 15 h"),
    (A, "maintenant_s < coucher_s;", "maintenant_s <= coucher_s;",
     T, "le_creneau_du_soir_va_de_16_h_au_coucher_bornes_a_la_seconde", "le coucher du soleil est encore dans le créneau"),
    (A, "const bool soir = seconde_du_jour >= CRENEAU_SOIR_DEBUT_S && maintenant_s < coucher_s;",
     "const bool soir = seconde_du_jour >= CRENEAU_SOIR_DEBUT_S;",
     T, "hors_creneau_midi_nuit_et_avant_le_lever", "le créneau du soir ne s'arrête pas au coucher du soleil"),
    (A, "return matin || soir;", "return matin && soir;",
     T, "le_creneau_du_matin_va_du_lever_a_10_h_bornes_a_la_seconde", "le créneau demande le matin ET le soir"),
    (A, "if (lever_s < minuit_local_s || lever_s >= minuit_local_s + JOUR_S)", "if (lever_s >= minuit_local_s + JOUR_S)",
     T, "le_creneau_veut_un_lever_et_un_coucher_du_jour_local", "un lever d'hier ouvre le créneau"),
    (A, "if (lever_s < minuit_local_s || lever_s >= minuit_local_s + JOUR_S)", "if (lever_s < minuit_local_s)",
     T, "le_creneau_veut_un_lever_et_un_coucher_du_jour_local", "un lever de demain ouvre le créneau"),
    (A, "if (lever_s < minuit_local_s ||", "if (lever_s <= minuit_local_s ||",
     T, "le_creneau_veut_un_lever_et_un_coucher_du_jour_local", "un lever à minuit pile n'est plus du jour"),
    (A, "if (coucher_s < minuit_local_s || coucher_s >= minuit_local_s + JOUR_S)", "if (coucher_s < minuit_local_s)",
     T, "le_creneau_veut_un_lever_et_un_coucher_du_jour_local", "un coucher de demain est accepté"),
    (A, "if (coucher_s < minuit_local_s || coucher_s >= minuit_local_s + JOUR_S)", "if (coucher_s >= minuit_local_s + JOUR_S)",
     T, "le_creneau_veut_un_lever_et_un_coucher_du_jour_local", "un coucher d'hier est accepté"),
    (A, "coucher_s >= minuit_local_s + JOUR_S)", "coucher_s > minuit_local_s + JOUR_S)",
     T, "le_creneau_veut_un_lever_et_un_coucher_du_jour_local", "un coucher à minuit pile du lendemain est du jour"),
    (A, "if (seconde_du_jour < 0)\n        seconde_du_jour += JOUR_S;", "if (false)\n        seconde_du_jour += JOUR_S;",
     T, "le_creneau_lit_les_heures_dans_le_decalage_du_lieu", "le reste négatif d'une division n'est pas ramené dans le jour"),
    (A, "(maintenant_s + decalage_local_s) % JOUR_S;", "maintenant_s % JOUR_S;",
     T, "le_creneau_lit_les_heures_dans_le_decalage_du_lieu", "le décalage du lieu est ignoré (heures lues en UTC)"),
    (A, "(maintenant_s + decalage_local_s) % JOUR_S;", "(maintenant_s - decalage_local_s) % JOUR_S;",
     T, "le_creneau_lit_les_heures_dans_le_decalage_du_lieu", "le décalage du lieu est soustrait au lieu d'être ajouté"),
    (A, "if (maintenant_s <= 0 || lever_s <= 0 || coucher_s <= 0)", "if (lever_s <= 0 || coucher_s <= 0)",
     T, "le_creneau_lit_les_heures_dans_le_decalage_du_lieu", "une heure négative ouvre un créneau"),
    (A, "if (maintenant_s <= 0 || lever_s <= 0 || coucher_s <= 0)", "if (maintenant_s <= 0 || coucher_s <= 0)",
     T, "le_creneau_lit_les_heures_dans_le_decalage_du_lieu", "un lever inconnu (0) ouvre un créneau"),
    (A, "if (maintenant_s <= 0 || lever_s <= 0 || coucher_s <= 0)", "if (maintenant_s <= 0 || lever_s <= 0)",
     T, "le_creneau_lit_les_heures_dans_le_decalage_du_lieu", "un coucher inconnu (0) ouvre un créneau"),
    (A, "JOUR_S = 24 * 3600;", "JOUR_S = 23 * 3600;",
     T, "les_seuils_publics_sont_ceux_de_l_adr", "un jour dure 23 h"),
    (A, "if (e.spot_surf && e.activite_surf &&\n", "if (e.activite_surf &&\n",
     T, "surf_demande_spot_activite_donnees_fraiches_et_creneau", "Surf sans spot de surf enregistré"),
    (A, "if (e.spot_surf && e.activite_surf &&\n", "if (e.spot_surf &&\n",
     T, "surf_demande_spot_activite_donnees_fraiches_et_creneau", "Surf sans l'activité Surf cochée"),
    (A, "        creneau_de_sortie(e.maintenant_s, e.decalage_local_s, e.lever_s, e.coucher_s)) {\n",
     "        true) {\n",
     T, "surf_demande_spot_activite_donnees_fraiches_et_creneau", "Surf sans créneau de sortie"),
    (A, "if (e.fraicheur_mer == Fraicheur::Fraiche)\n            return {Profil::Surf, Raison::Surf};",
     "if (e.fraicheur_mer != Fraicheur::Perimee)\n            return {Profil::Surf, Raison::Surf};",
     T, "rien_d_inconnu_ne_declenche_une_bascule", "Surf avec des données de mer de fraîcheur inconnue"),
    (A, "if (e.fraicheur_mer == Fraicheur::Fraiche)\n            return {Profil::Surf, Raison::Surf};",
     "if (e.fraicheur_mer != Fraicheur::Inconnue)\n            return {Profil::Surf, Raison::Surf};",
     T, "une_donnee_de_mer_perimee_ramene_a_meteo_tout_de_suite", "Surf avec des données de mer périmées"),

    # --- règle 4 : vent et bateau ---
    (A, "seuil_dixiemes_kmh > 0 && vent_dixiemes_kmh > seuil_dixiemes_kmh;",
     "seuil_dixiemes_kmh > 0 && vent_dixiemes_kmh >= seuil_dixiemes_kmh;",
     T, "vent_bateau_veut_un_vent_strictement_au_dessus_du_seuil", "un vent égal au seuil est « au-dessus »"),
    (A, "seuil_dixiemes_kmh > 0 && vent_dixiemes_kmh > seuil_dixiemes_kmh;", "vent_dixiemes_kmh > seuil_dixiemes_kmh;",
     T, "vent_bateau_veut_l_activite_cochee_et_un_seuil_regle", "un seuil non réglé (0) laisse passer tout vent"),
    (A, "seuil_dixiemes_kmh > 0 && vent_dixiemes_kmh > seuil_dixiemes_kmh;",
     "seuil_dixiemes_kmh >= 0 && vent_dixiemes_kmh > seuil_dixiemes_kmh;",
     T, "vent_bateau_veut_l_activite_cochee_et_un_seuil_regle", "un seuil de 0 compte comme réglé"),
    (A, "if (e.activite_vent_bateau && vent_au_dessus_du_seuil(e.vent_dixiemes_kmh, e.seuil_vent_dixiemes_kmh)) {",
     "if (vent_au_dessus_du_seuil(e.vent_dixiemes_kmh, e.seuil_vent_dixiemes_kmh)) {",
     T, "vent_bateau_veut_l_activite_cochee_et_un_seuil_regle", "Vent·Bateau sans l'activité cochée"),
    (A, "if (e.activite_vent_bateau && vent_au_dessus_du_seuil(e.vent_dixiemes_kmh, e.seuil_vent_dixiemes_kmh)) {",
     "if (e.spot_surf && e.activite_vent_bateau && vent_au_dessus_du_seuil(e.vent_dixiemes_kmh, e.seuil_vent_dixiemes_kmh)) {",
     T, "une_maison_sans_spot_ne_bascule_jamais_vers_surf_ni_cote", "Vent·Bateau exige un spot de surf"),
    (A, "if (e.fraicheur_meteo == Fraicheur::Fraiche)\n            return {Profil::VentBateau, Raison::VentBateau};",
     "if (e.fraicheur_meteo != Fraicheur::Perimee)\n            return {Profil::VentBateau, Raison::VentBateau};",
     T, "rien_d_inconnu_ne_declenche_une_bascule", "Vent·Bateau avec des prévisions de fraîcheur inconnue"),

    # --- règle 5 : côte et basse mer ---
    (A, "basse_mer_dans_min <= BASSE_MER_MAX_MIN;", "basse_mer_dans_min < BASSE_MER_MAX_MIN;",
     T, "cote_veut_une_basse_mer_dans_les_trois_heures_bornes_comprises", "une basse mer dans 3 h pile ne compte plus"),
    (A, "BASSE_MER_MAX_MIN = 180;", "BASSE_MER_MAX_MIN = 240;",
     T, "cote_veut_une_basse_mer_dans_les_trois_heures_bornes_comprises", "la basse mer est « proche » jusqu'à 4 h"),
    (A, "BASSE_MER_MAX_MIN = 180;", "BASSE_MER_MAX_MIN = 120;",
     T, "cote_veut_une_basse_mer_dans_les_trois_heures_bornes_comprises", "la basse mer est « proche » jusqu'à 2 h"),
    (A, "basse_mer_dans_min >= 0 && basse_mer_dans_min <=", "basse_mer_dans_min > 0 && basse_mer_dans_min <=",
     T, "cote_veut_une_basse_mer_dans_les_trois_heures_bornes_comprises", "une basse mer à l'instant même ne compte plus"),
    (A, "basse_mer_dans_min >= 0 && basse_mer_dans_min <=", "basse_mer_dans_min <=",
     T, "cote_veut_une_basse_mer_dans_les_trois_heures_bornes_comprises", "une basse mer « inconnue » (-1) compte comme proche"),
    (A, "if (e.spot_peche_a_pied && basse_mer_proche(e.basse_mer_dans_min)) {",
     "if (basse_mer_proche(e.basse_mer_dans_min)) {",
     T, "cote_veut_un_spot_de_peche_a_pied_pas_d_activite", "Côte sans spot de pêche à pied"),
    (A, "if (e.fraicheur_mer == Fraicheur::Fraiche)\n            return {Profil::Cote, Raison::Cote};",
     "if (e.fraicheur_mer != Fraicheur::Perimee)\n            return {Profil::Cote, Raison::Cote};",
     T, "rien_d_inconnu_ne_declenche_une_bascule", "Côte avec des données de mer de fraîcheur inconnue"),

    # --- l'ordre des règles 3 à 6 ---
    (A, BLOC_SURF + BLOC_VENT, BLOC_VENT + BLOC_SURF,
     T, "surf_puis_vent_bateau_puis_cote_puis_meteo", "Vent·Bateau passe avant Surf"),
    (A, BLOC_VENT + BLOC_COTE, BLOC_COTE + BLOC_VENT,
     T, "surf_passe_avant_cote_et_vent_bateau_passe_avant_cote_deux_a_deux", "Côte passe avant Vent·Bateau"),
    (A, BLOC_SURF + BLOC_VENT + BLOC_COTE, BLOC_COTE + BLOC_SURF + BLOC_VENT,
     T, "surf_passe_avant_cote_et_vent_bateau_passe_avant_cote_deux_a_deux", "Côte passe avant Surf et Vent·Bateau"),

    # --- donnée périmée ---
    (A, "return {Profil::Surf, Raison::Surf};\n        perimee = perimee || e.fraicheur_mer == Fraicheur::Perimee;",
     "return {Profil::Surf, Raison::Surf};\n        perimee = perimee;",
     T, "une_donnee_de_mer_perimee_ramene_a_meteo_tout_de_suite", "une houle périmée n'est pas notée « périmée »"),
    (A, "perimee = perimee || e.fraicheur_meteo == Fraicheur::Perimee;", "perimee = perimee;",
     T, "une_donnee_meteo_perimee_ramene_vent_bateau_a_meteo", "un vent périmé n'est pas noté « périmé »"),
    (A, "return {Profil::Cote, Raison::Cote};\n        perimee = perimee || e.fraicheur_mer == Fraicheur::Perimee;",
     "return {Profil::Cote, Raison::Cote};\n        perimee = perimee;",
     T, "une_donnee_de_mer_perimee_ramene_cote_a_meteo", "une marée périmée n'est pas notée « périmée »"),
    (A, "return {Profil::Surf, Raison::Surf};\n        perimee = perimee || e.fraicheur_mer == Fraicheur::Perimee;",
     "return {Profil::Surf, Raison::Surf};\n        perimee = perimee || e.fraicheur_mer != Fraicheur::Fraiche;",
     T, "inconnue_n_est_pas_perimee", "une donnée inconnue est prise pour périmée"),
    (A, "return {Profil::Meteo, perimee ? Raison::DonneesPerimees : Raison::Defaut};",
     "return {Profil::Meteo, Raison::Defaut};",
     T, "une_donnee_de_mer_perimee_ramene_a_meteo_tout_de_suite", "le retour à Météo ne dit pas « données périmées »"),
    (A, "return {Profil::Meteo, perimee ? Raison::DonneesPerimees : Raison::Defaut};",
     "return {Profil::Meteo, perimee ? Raison::Defaut : Raison::DonneesPerimees};",
     T, "perimee_sans_les_autres_conditions_n_est_qu_un_defaut", "« données périmées » dit à l'envers"),
    (A, "return {Profil::Surf, Raison::Surf};\n        perimee = perimee || e.fraicheur_mer == Fraicheur::Perimee;\n    }\n",
     "return {Profil::Surf, Raison::Surf};\n        perimee = perimee || e.fraicheur_mer == Fraicheur::Perimee;\n    }\n"
     "    if (perimee)\n        return {Profil::Meteo, Raison::DonneesPerimees};\n",
     T, "une_donnee_perimee_laisse_la_place_au_profil_suivant_dont_la_donnee_est_fraiche",
     "une donnée périmée ramène à Météo sans laisser la place au profil suivant"),
    (A, "(voulu.raison == Raison::DonneesPerimees || delai_ecoule(avant, e.maintenant_s));",
     "delai_ecoule(avant, e.maintenant_s);",
     T, "une_donnee_de_mer_perimee_ramene_a_meteo_tout_de_suite", "une donnée périmée attend les 30 minutes"),
    (A, BLOC_PERMIS,
     "        const bool permis = voulu.raison == Raison::DonneesPerimees ||\n"
     "                            (!e.toucher_en_cours && e.maintenant_s > 0 && delai_ecoule(avant, e.maintenant_s));\n",
     T, "la_peremption_attend_la_fin_du_toucher", "une donnée périmée change le profil sous le doigt"),

    # --- garde-fou : 30 minutes ---
    (A, "return ecoule < 0 || ecoule >= CHANGEMENT_ECART_MIN_S;", "return ecoule < 0 || ecoule > CHANGEMENT_ECART_MIN_S;",
     T, "pas_de_changement_a_29_min_59_s_changement_a_30_min", "30 minutes pile ne suffisent pas"),
    (A, "CHANGEMENT_ECART_MIN_S = 30 * 60;", "CHANGEMENT_ECART_MIN_S = 29 * 60;",
     T, "pas_de_changement_a_29_min_59_s_changement_a_30_min", "29 minutes suffisent pour changer"),
    (A, "CHANGEMENT_ECART_MIN_S = 30 * 60;", "CHANGEMENT_ECART_MIN_S = 31 * 60;",
     T, "pas_de_changement_a_29_min_59_s_changement_a_30_min", "il faut 31 minutes pour changer"),
    (A, "return ecoule < 0 || ecoule >= CHANGEMENT_ECART_MIN_S;", "return ecoule >= CHANGEMENT_ECART_MIN_S;",
     T, "une_horloge_revenue_en_arriere_ne_bloque_pas_les_changements", "une horloge revenue en arrière bloque les changements"),
    (A, "if (!etat.changement_connu)\n        return true;", "if (!etat.changement_connu)\n        return false;",
     T, "le_premier_choix_est_libre", "le premier choix attend lui aussi 30 minutes"),
    (A, "if (!etat.changement_connu)\n        return true;", "if (false)\n        return true;",
     T, "une_horloge_revenue_en_arriere_ne_bloque_pas_les_changements", "le « premier choix libre » est jugé sur un horodatage à 0"),
    (A, "d.etat = EtatAuto{voulu.profil, true, e.maintenant_s};", "d.etat = EtatAuto{voulu.profil, true, avant.change_a_s};",
     T, "pas_de_changement_a_29_min_59_s_changement_a_30_min", "l'heure du changement n'est pas renouvelée"),
    (A, "d.etat = EtatAuto{voulu.profil, true, e.maintenant_s};", "d.etat = EtatAuto{voulu.profil, false, e.maintenant_s};",
     T, "pas_de_changement_a_29_min_59_s_changement_a_30_min", "le changement n'est pas noté comme connu"),
    (A, "    if (voulu.profil == avant.profil) {\n        d.raison_profil = voulu.raison;",
     "    if (voulu.profil == avant.profil) {\n        d.raison_profil = voulu.raison;\n"
     "        d.etat = EtatAuto{avant.profil, true, e.maintenant_s};",
     T, "le_meme_profil_ne_renouvelle_pas_l_horodatage", "garder le même profil renouvelle l'heure du changement"),
    (A, "        } else {\n            d.raison_profil = Raison::ChangementDiffere;",
     "        } else {\n            d.raison_profil = voulu.raison;",
     T, "pas_de_changement_a_29_min_59_s_changement_a_30_min", "un profil gardé dit la raison du profil refusé"),
    (A, "        d.raison_profil = voulu.raison;\n    } else {", "        d.raison_profil = Raison::ChangementDiffere;\n    } else {",
     T, "le_meme_profil_ne_renouvelle_pas_l_horodatage", "le profil déjà montré se dit « différé »"),

    (A, "return ecoule < 0 || ecoule >= CHANGEMENT_ECART_MIN_S;", "return ecoule <= 0 || ecoule >= CHANGEMENT_ECART_MIN_S;",
     T, "pas_de_changement_a_29_min_59_s_changement_a_30_min", "un changement à la même seconde que le précédent est permis"),
    (A, "(voulu.raison == Raison::DonneesPerimees || delai_ecoule(avant, e.maintenant_s));",
     "(voulu.profil == Profil::Meteo || delai_ecoule(avant, e.maintenant_s));",
     T, "un_retour_a_meteo_ordinaire_attend_les_30_minutes_comme_tout_changement", "tout retour à Météo passe devant le délai, pas seulement la péremption"),
    (A, "return static_cast<std::size_t>(p) < NOMBRE_PROFILS;", "return static_cast<std::size_t>(p) <= NOMBRE_PROFILS;",
     T, "une_valeur_hors_table_est_ignoree_jamais_une_erreur", "un profil juste hors de la table est valide"),

    # --- garde-fou : jamais pendant un toucher, jamais sans heure ---
    (A, "const bool permis = !e.toucher_en_cours && e.maintenant_s > 0 &&", "const bool permis = e.maintenant_s > 0 &&",
     T, "le_profil_ne_change_jamais_pendant_un_toucher", "le profil change sous le doigt"),
    (A, "const bool permis = !e.toucher_en_cours && e.maintenant_s > 0 &&", "const bool permis = !e.toucher_en_cours &&",
     T, "sans_heure_le_profil_ne_change_pas", "le profil change sans heure"),

    # --- l'épinglage ---
    (A, "if (e.epingle && profil_valide(e.profil_epingle)) {", "if (e.epingle) {",
     T, "une_valeur_hors_table_est_ignoree_jamais_une_erreur", "un profil épinglé hors table est montré"),
    (A, "if (e.epingle && profil_valide(e.profil_epingle)) {", "if (profil_valide(e.profil_epingle)) {",
     T, "entrees_vides_donnent_meteo_sans_rien_d_autre", "le profil épinglé vaut même sans épinglage"),
    (A, "        d.raison = Raison::Epingle;\n",
     "        d.raison = d.bandeau_agrandi ? Raison::AlerteOrange : Raison::Epingle;\n",
     T, "l_alerte_agrandit_meme_epinglee_retenue_ou_pendant_un_toucher", "l'alerte remplace « épinglé » dans la pastille"),
    (A, "        d.raison_profil = Raison::Epingle;\n", "        d.raison_profil = Raison::Defaut;\n",
     T, "un_profil_epingle_gagne_sur_toutes_les_regles", "le panneau ne dit pas que le profil est épinglé"),
    (A, "        d.etat.profil = e.profil_epingle;\n        return d;",
     "        d.etat = EtatAuto{e.profil_epingle, true, e.maintenant_s};\n        return d;",
     T, "un_profil_epingle_gagne_sur_toutes_les_regles", "l'épinglage compte comme un changement automatique (30 min d'attente au désépinglage)"),
    (A, "        d.etat.profil = e.profil_epingle;\n        return d;", "        d.etat = e.etat;\n        return d;",
     T, "un_profil_epingle_gagne_sur_toutes_les_regles", "l'état rendu ne retient pas le profil épinglé"),
    (A, "if (!profil_valide(avant.profil))\n        avant = EtatAuto{};", "if (false)\n        avant = EtatAuto{};",
     T, "une_valeur_hors_table_est_ignoree_jamais_une_erreur", "un état précédent hors table est repris tel quel"),
    (A, "NOMBRE_PROFILS = 4;", "NOMBRE_PROFILS = 3;",
     T, "les_seuils_publics_sont_ceux_de_l_adr", "Côte n'est plus un profil valide"),
    (A, "VALEUR_INCONNUE = -1;", "VALEUR_INCONNUE = 0;",
     T, "les_seuils_publics_sont_ceux_de_l_adr", "« inconnu » vaut 0 minute ou 0 km/h"),
    (A, "NOMBRE_RAISONS = 10;", "NOMBRE_RAISONS = 9;",
     T, "les_seuils_publics_sont_ceux_de_l_adr", "une raison de plus que la table de traduction n'en prévoit"),
    (A, "    d.etat = avant;\n    if (voulu.profil == avant.profil) {", "    d.etat = EtatAuto{};\n    if (voulu.profil == avant.profil) {",
     T, "le_meme_profil_ne_renouvelle_pas_l_horodatage", "l'état rendu oublie le dernier changement quand rien ne change"),
]
