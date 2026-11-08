"""Mutations du pilote du mode automatique de l'accueil (components/core/include/ecran/accueil_pilote.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_ACCUEIL_AUTO : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test qui
doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou « détectée par
autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle écrite dans l'en-tête et l'ADR-0023 : un branchement d'entrée, la remise à zéro au changement de lieu, le gel
sous le doigt (sa borne, sa remise à zéro, ses exceptions), le filet d'un profil que l'appareil ne dessine pas, la clé du lieu, les phrases.
Ce sont des mutations choisies à la main, pas un balayage : elles ne disent pas que « tout » est testé, seulement que ces règles-là le sont.

NE FIGURENT PAS (le code change, le comportement non, il n'y a rien à détecter) :
  - `if (m.toucher && heure_sure)` sans `heure_sure` : sans heure, `toucher_depuis_s_` reste à 0 (l'instant du moment), donc « pas de gel » ;
  - `if (lc.situation == Normal)` : hors du cas normal `lever_s` et `coucher_s` valent déjà 0 (soleil.hpp) ;
  - `soleil_valide_ &&` : le cache part d'une année 0 qui ne vaut jamais celle d'un jour réel ;
  - `e.epingle = epingle` au lieu de `m.epingle` : le filet de sécurité redonne le même écran pour un profil que l'appareil ne dessine pas ;
  - `etat_ = d.etat` : aujourd'hui seul Météo est dessiné, l'état n'évolue pas (la mutation sera à écrire avec les profils de mer) ;
  - la garde « jamais nulle » de `cle_du_lieu` elle-même : aucune entrée connue ne donne 0 (la garde est testée en direct, `cle_non_nulle`).
"""

T = "test_accueil_pilote.cpp"
A = "accueil_pilote.hpp"

MUTATIONS_ACCUEIL_PILOTE = [
    # --- les constantes ---
    (A, "PROFILS_DISPONIBLES = 1U << static_cast<unsigned>(Profil::Meteo);", "PROFILS_DISPONIBLES = 0x0FU;",
     T, "les_constantes_du_pilote", "l'appareil croit savoir dessiner Surf, Vent·Bateau et Côte"),
    (A, "GEL_TOUCHER_MAX_S = 10;", "GEL_TOUCHER_MAX_S = 60;",
     T, "les_constantes_du_pilote", "le gel sous le doigt dure une minute"),

    # --- la clé du lieu ---
    (A, "        mele_octet(static_cast<unsigned char>(*c));", "        (void)c;",
     T, "la_cle_du_lieu_suit_le_nom_et_la_position", "le nom du lieu n'entre plus dans la clé"),
    (A, "    mele_entier(dix_milliemes(latitude_deg));", "    mele_entier(0);",
     T, "la_cle_du_lieu_suit_le_nom_et_la_position", "la latitude n'entre plus dans la clé"),
    (A, "    mele_entier(dix_milliemes(longitude_deg));", "    mele_entier(0);",
     T, "la_cle_du_lieu_suit_le_nom_et_la_position", "la longitude n'entre plus dans la clé"),
    (A, "const double x = v * 10000.0;", "const double x = v * 100.0;",
     T, "la_cle_du_lieu_suit_le_nom_et_la_position", "la clé ne distingue plus deux positions à 11 m"),
    (A, "return h == 0 ? 1U : h;", "return h;",
     T, "la_cle_du_lieu_n_est_jamais_nulle", "une clé peut valoir 0, réservé à « pas de lieu »"),

    # --- les phrases de la pastille ---
    (A, r'return "Auto \xC2\xB7 alerte orange";', r'return "Auto \xC2\xB7 alerte rouge";',
     T, "une_phrase_par_raison", "une alerte orange se lit « alerte rouge »"),
    (A, r'return "Auto \xC2\xB7 pluie proche";', r'return "Auto \xC2\xB7 M\xC3\xA9t\xC3\xA9o";',
     T, "une_phrase_par_raison", "la pluie proche se lit « Auto · Météo »"),
    # Une seule ligne du source (clang-format coupe la phrase sur trois) : le reste devient une instruction morte après le `return`.
    (A, r'return "Auto \xC2\xB7 donn\xC3\xA9"', r'return "Auto \xC2\xB7 M\xC3\xA9t\xC3\xA9o"; (void)"',
     T, "une_phrase_par_raison", "des données périmées se lisent « Auto · Météo »"),
    (A, 'return profil == Profil::Meteo ? ', 'return true ? ',
     T, "la_phrase_d_un_epinglage_ne_dit_que_le_profil", "tout épinglage se lit « Météo · épinglé », même celui d'un autre profil"),
    (A, 'return "Auto \\xC2\\xB7 M\\xC3\\xA9t\\xC3\\xA9o"; // une `Raison` hors table', 'return ""; // une `Raison` hors table',
     T, "toutes_les_phrases_sont_differentes_et_une_raison_inconnue_se_lit_par_defaut", "une raison hors table se lit comme un texte vide"),
    (A, 'inline constexpr const char *PHRASE_AUTO_EN_PAUSE = "Auto en pause";', 'inline constexpr const char *PHRASE_AUTO_EN_PAUSE = "Auto";',
     T, "une_phrase_par_raison", "« Auto en pause » se lit « Auto »"),
    (A, "    case Raison::PluieProche:\n        return Ton::Pluie;", "    case Raison::PluieProche:\n        return Ton::Neutre;",
     T, "le_ton_de_la_pastille", "la pastille de la pluie proche n'a plus sa teinte"),
    (A, "    case Raison::AlerteOrange:\n    case Raison::AlerteRouge:\n        return Ton::Alerte;",
     "    case Raison::AlerteOrange:\n        return Ton::Alerte;\n    case Raison::AlerteRouge:\n        return Ton::Neutre;",
     T, "le_ton_de_la_pastille", "la pastille d'une alerte rouge n'a plus la teinte d'alerte"),
    (A, "    case Raison::Epingle:\n        return Ton::Epingle;", "    case Raison::Epingle:\n        return Ton::Neutre;",
     T, "le_ton_de_la_pastille", "la pastille épinglée n'a plus la teinte d'épinglage"),

    # --- l'égalité de deux présentations (l'écran ne redessine que sur un changement) ---
    (A, "a.raison_profil == b.raison_profil &&", "",
     T, "l_egalite_de_deux_presentations_compare_chaque_champ", "deux présentations de raison de profil différente sont égales"),
    (A, "           a.epingle == b.epingle && a.bandeau_agrandi", "           a.bandeau_agrandi",
     T, "l_egalite_de_deux_presentations_compare_chaque_champ", "épinglée ou non, c'est la même présentation"),
    (A, " && a.carte_pluie == b.carte_pluie;", ";",
     T, "l_egalite_de_deux_presentations_compare_chaque_champ", "avec ou sans carte pluie, c'est la même présentation"),

    # --- ce qui entre dans la décision ---
    (A, "const bool epingle = m.epingle && profil_disponible(m.profil_epingle);", "const bool epingle = m.epingle;",
     T, "un_epinglage_que_l_appareil_ne_sait_pas_montrer_vaut_pas_d_epinglage", "un profil que l'appareil n'a pas peut être épinglé"),
    (A, "        if (heure_sure && m.fuseau_connu) {", "        if (heure_sure) {",
     T, "sans_position_ou_sans_fuseau_ni_lever_ni_coucher", "le lever du soleil est calculé sans fuseau connu"),
    (A, "            if (m.position_connue) {", "            if (true) {",
     T, "sans_position_ou_sans_fuseau_ni_lever_ni_coucher", "le lever du soleil est calculé sans position (0°, 0°)"),
    (A, "            e.decalage_local_s = local.decalage_s;", "",
     T, "les_mesures_vont_dans_les_entrees_de_la_decision", "le décalage du lieu n'est plus transmis"),
    (A, "        e.toucher_en_cours = m.toucher;", "",
     T, "au_relachement_l_ecran_se_met_a_jour_tout_de_suite", "la décision ne sait plus qu'un doigt est posé"),
    (A, "        e.etat = etat_;", "",
     T, "un_profil_que_l_appareil_ne_dessine_pas_n_est_jamais_rendu", "l'état de la décision n'est pas redonné d'un appel au suivant"),
    (A, "        e.niveau_alerte = m.niveau_alerte;", "",
     T, "une_alerte_orange_ou_rouge_agrandit_le_bandeau_et_donne_sa_pastille", "l'alerte n'est plus transmise"),
    (A, "e.pluie_dans_min = m.pluie_dans_min >= 0 ?", "e.pluie_dans_min = m.pluie_dans_min > 0 ?",
     T, "la_pluie_inconnue_ou_absente_reste_inconnue_pour_la_decision", "« il pleut » (0 minute) devient une pluie inconnue"),
    (A, "!m.meteo_connue ? Fraicheur::Inconnue : (m.meteo_perimee ? Fraicheur::Perimee : Fraicheur::Fraiche);",
     "!m.meteo_connue ? Fraicheur::Fraiche : (m.meteo_perimee ? Fraicheur::Perimee : Fraicheur::Fraiche);",
     T, "la_fraicheur_de_la_meteo_est_inconnue_fraiche_ou_perimee", "une météo jamais reçue passe pour fraîche"),
    (A, "(m.meteo_perimee ? Fraicheur::Perimee : Fraicheur::Fraiche);", "(m.meteo_perimee ? Fraicheur::Fraiche : Fraicheur::Perimee);",
     T, "la_fraicheur_de_la_meteo_est_inconnue_fraiche_ou_perimee", "une météo périmée passe pour fraîche, et inversement"),

    # --- le lever et le coucher du soleil, calculés une fois par jour et par lieu ---
    (A, "soleil_an_ == local.an && ", "",
     T, "le_lever_suit_le_jour_le_mois_l_annee_et_le_lieu", "le lever d'un an plus tôt est gardé le même jour du même mois"),
    (A, "soleil_mois_ == local.mois && ", "",
     T, "le_lever_suit_le_jour_le_mois_l_annee_et_le_lieu", "le lever du mois d'avant est gardé le même quantième"),
    (A, "soleil_jour_ == local.jour &&", "",
     T, "le_lever_suit_le_jour_le_mois_l_annee_et_le_lieu", "le lever de la veille est gardé le lendemain"),
    (A, " && soleil_lat_ == m.latitude_deg", "",
     T, "le_lever_suit_le_jour_le_mois_l_annee_et_le_lieu", "le lever de l'ancienne latitude est gardé"),
    (A, " && soleil_lon_ == m.longitude_deg", "",
     T, "le_lever_suit_le_jour_le_mois_l_annee_et_le_lieu", "le lever de l'ancienne longitude est gardé"),

    # --- ce que l'écran montre ---
    (A, "    p.epingle = epingle;", "    p.epingle = false;",
     T, "un_profil_epingle_coupe_la_carte_pluie_mais_pas_le_bandeau_agrandi", "un épinglage n'est jamais montré"),
    (A, "    p.bandeau_agrandi = d.bandeau_agrandi;", "    p.bandeau_agrandi = false;",
     T, "une_alerte_orange_ou_rouge_agrandit_le_bandeau_et_donne_sa_pastille", "le bandeau n'est jamais agrandi"),
    (A, "    p.carte_pluie = d.carte_pluie;", "    p.carte_pluie = false;",
     T, "la_pluie_proche_donne_la_carte_a_59_minutes_pas_a_60", "la carte pluie n'est jamais montrée"),
    (A, "    p.raison_profil = d.raison_profil;", "    p.raison_profil = Raison::Defaut;",
     T, "un_profil_epingle_coupe_la_carte_pluie_mais_pas_le_bandeau_agrandi", "la raison du profil est perdue"),
    (A, "    p.raison = d.raison;", "    p.raison = Raison::Defaut;",
     T, "une_alerte_orange_ou_rouge_agrandit_le_bandeau_et_donne_sa_pastille", "la pastille ne dit jamais pourquoi"),

    # --- un profil que l'appareil ne dessine pas ---
    # `if (!profil_disponible(` finit la ligne dans le source (clang-format reporte `neuve.profil)) {` à la suivante).
    (A, "        if (!profil_disponible(\n                neuve.profil)) {", "        if (false) {",
     T, "un_profil_que_l_appareil_ne_dessine_pas_n_est_jamais_rendu", "un profil que l'appareil ne dessine pas peut être rendu"),
    (A, "            neuve.raison_profil = Raison::Defaut;\n            if (", "            if (",
     T, "un_profil_que_l_appareil_ne_dessine_pas_n_est_jamais_rendu", "la raison « changement différé » d'un profil retenu reste affichée"),
    (A, "            if (!neuve.epingle && !neuve.bandeau_agrandi && !neuve.carte_pluie)\n                neuve.raison = Raison::Defaut;",
     "            neuve.raison = Raison::Defaut;",
     T, "un_profil_que_l_appareil_ne_dessine_pas_n_est_jamais_rendu", "le filet efface la pastille d'une alerte"),
    (A, "            etat_.profil = Profil::Meteo;", "",
     T, "un_profil_que_l_appareil_ne_dessine_pas_n_est_jamais_rendu", "l'état garde un profil que l'appareil ne dessine pas"),

    # --- le lieu change ---
    (A, "const bool lieu_change = lieu_connu_ && m.lieu != lieu_;", "const bool lieu_change = m.lieu != lieu_;",
     T, "le_premier_lieu_vu_ne_remet_rien_a_zero", "le premier appel passe pour un changement de lieu"),
    (A, "        if (lieu_change)\n            etat_ = EtatAuto{};", "        if (false)\n            etat_ = EtatAuto{};",
     T, "changer_de_lieu_remet_l_etat_de_la_decision_a_zero", "un autre lieu garde le dernier changement de profil de l'ancien"),
    (A, "    lieu_ = m.lieu;\n", "",
     T, "changer_de_lieu_remet_l_etat_de_la_decision_a_zero", "le lieu n'est pas retenu : chaque appel passe pour un changement de lieu"),
    (A, "    lieu_connu_ = true;\n", "",
     T, "changer_de_lieu_remet_l_etat_de_la_decision_a_zero", "aucun changement de lieu n'est jamais vu"),

    # --- pas de redessin sous le doigt ---
    (A, "depuis < GEL_TOUCHER_MAX_S", "depuis <= GEL_TOUCHER_MAX_S",
     T, "sous_le_doigt_la_presentation_ne_change_pas_pendant_dix_secondes_au_plus", "le gel dure 10 s de plus d'une seconde"),
    (A, "        if (toucher_depuis_s_ <= 0)\n                toucher_depuis_s_ = m.maintenant_s;",
     "        toucher_depuis_s_ = m.maintenant_s;",
     T, "sous_le_doigt_la_presentation_ne_change_pas_pendant_dix_secondes_au_plus", "le compte repart à chaque pas : un doigt posé gèle l'écran sans fin"),
    (A, "        } else {\n            toucher_depuis_s_ = 0;\n        }", "        } else {\n        }",
     T, "un_nouveau_toucher_recommence_le_compte_a_dix_secondes", "un nouveau toucher continue le compte du précédent"),
    (A, "derniere_valide_ && depuis >= 0", "depuis >= 0",
     T, "le_premier_ecran_n_est_jamais_gele", "le premier écran peut être gelé sur une présentation vide"),
    (A, "depuis >= 0 && ", "",
     T, "sans_heure_ou_avec_une_horloge_a_l_envers_rien_n_est_gele", "une horloge revenue en arrière gèle l'écran"),
    (A, " && !lieu_change &&", " &&",
     T, "changer_de_lieu_sous_le_doigt_se_voit_tout_de_suite", "un autre lieu reste gelé sous le doigt"),
    (A, " && epingle == epingle_vu_;", " && true;",
     T, "epingler_ou_desepingler_sous_le_doigt_se_voit_tout_de_suite", "épingler sous le doigt n'est visible qu'après 10 s"),
    (A, "        if (!geler) {", "        if (true) {",
     T, "sous_le_doigt_la_presentation_ne_change_pas_pendant_dix_secondes_au_plus", "l'écran n'est jamais gelé"),
    (A, "m.maintenant_s - toucher_depuis_s_ : -1;", "m.maintenant_s - toucher_depuis_s_ : 0;",
     T, "au_relachement_l_ecran_se_met_a_jour_tout_de_suite", "sans doigt posé, l'écran reste gelé"),
]
