"""Mutations du noyau « Wi-Fi » (essai_wifi.hpp, points_acces.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS dans lancer.py : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —,
remplacement, test qui doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule).
Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Chaque mutation simule une erreur qui couperait l'appareil de son réseau qui marchait (pas de retour arrière, retour arrière
qui ne vient pas, rappel qui arrive deux fois) ou qui afficherait une mauvaise liste de réseaux (doublons, réseau faible en tête).
Non couvert : tout ce qui se passe hors du noyau (esp_wifi, le C6, les minuteurs, la boucle d'événements : main/reseau.cpp,
essayé seulement sur la carte, voir docs/mesures/wifi-retour-arriere.md).
"""

ESSAI = "essai_wifi.hpp"
POINTS = "points_acces.hpp"
T_ESSAI = "test_essai_wifi.cpp"
T_POINTS = "test_points_acces.cpp"

MUTATIONS_WIFI = [
    # --- essai_wifi.hpp : constantes et table des causes ---
    (ESSAI, "inline constexpr std::uint8_t SEUIL_PREUVES = 2;", "inline constexpr std::uint8_t SEUIL_PREUVES = 1;", T_ESSAI,
     "mot_de_passe_faux_deux_fois_retour_arriere", "un seul échec isolé condamne l'essai (un bon mot de passe est abandonné)"),
    (ESSAI, "inline constexpr std::int64_t DELAI_ESSAI_MS = DELAI_CONNEXION_MS;", "inline constexpr std::int64_t DELAI_ESSAI_MS = 20000;",
     T_ESSAI, "delais_alignes_sur_ceux_de_l_ecran", "le délai de l'essai n'est plus celui que l'écran affiche"),
    (ESSAI, "inline constexpr std::int64_t DELAI_RETOUR_MS = 20000;", "inline constexpr std::int64_t DELAI_RETOUR_MS = 40000;", T_ESSAI,
     "delais_alignes_sur_ceux_de_l_ecran", "le retour arrière peut durer plus que l'essai lui-même"),
    (ESSAI, "inline constexpr int FIN_D_ASSOCIATION = 8;", "inline constexpr int FIN_D_ASSOCIATION = 9;", T_ESSAI,
     "code_de_chaque_cause", "le code « nous avons quitté le réseau » n'est plus le bon"),
    (ESSAI, "    case raison::POIGNEE_DE_MAIN_DELAI:\n", "", T_ESSAI, "code_de_chaque_cause",
     "le cas classique du mot de passe faux (poignée de main expirée) n'est plus reconnu"),
    (ESSAI, "    case raison::AUTHENTIFICATION:\n", "", T_ESSAI, "code_de_chaque_cause", "l'échec d'authentification n'est plus reconnu"),
    (ESSAI, "    case raison::AUCUN_POINT_D_ACCES:\n", "", T_ESSAI, "code_de_chaque_cause", "un réseau introuvable n'est plus reconnu"),
    (ESSAI, "        return Resultat::ReseauIntrouvable;", "        return Resultat::Autre;", T_ESSAI, "code_de_chaque_cause",
     "les réseaux introuvables sont dits « autre »"),
    # --- essai_wifi.hpp : décisions ---
    (ESSAI, "    case raison::MIC_ECHEC:\n", "    case raison::MIC_ECHEC:\n    case raison::FIN_D_ASSOCIATION:\n", T_ESSAI,
     "la_deconnexion_provoquee_par_nous_est_ignoree", "quitter l'ancien réseau compte comme un échec du nouveau"),
    (ESSAI, "    if (e.phase != Phase::Essai) {\n        return s;\n    }\n    const Resultat cause", "    if (false) {\n        return s;\n    }\n    const Resultat cause",
     T_ESSAI, "evenements_hors_phase_sans_effet", "une déconnexion hors essai est prise pour un échec d'essai"),
    (ESSAI, "if (cause == Resultat::Autre) {", "if (false) {", T_ESSAI, "cause_autre_n_arrete_jamais_l_essai",
     "des balises perdues à la chaîne arrêtent l'essai avant le délai"),
    (ESSAI, "        return s; // ni preuve ni démenti", "        s.etat.cause_connue = false;\n        s.etat.preuves = 0;\n        return s;", T_ESSAI,
     "une_cause_autre_ne_casse_pas_une_serie_de_preuves", "une cause « autre » efface la preuve déjà vue (mot de passe refusé perdu)"),
    (ESSAI, "if (e.cause_connue && e.derniere_cause == cause) {", "if (e.cause_connue) {", T_ESSAI,
     "causes_differentes_ne_s_additionnent_pas", "deux causes différentes comptent comme deux preuves de la même"),
    (ESSAI, "e.preuves = 1;", "e.preuves = 0;", T_ESSAI, "mot_de_passe_faux_deux_fois_retour_arriere",
     "la première preuve n'est pas comptée"),
    (ESSAI, "    if (e.avait_ancien) {\n        e.phase = Phase::Retour;\n        e.attendu = r;",
     "    if (false) {\n        e.phase = Phase::Retour;\n        e.attendu = r;", T_ESSAI, "mot_de_passe_faux_deux_fois_retour_arriere",
     "un échec ne reprend jamais l'ancien réseau"),
    (ESSAI, "        e.attendu = r;\n        s.appliquer_ancien = true;", "        e.attendu = r;", T_ESSAI,
     "mot_de_passe_faux_deux_fois_retour_arriere", "l'échec ne demande pas de reconfigurer l'ancien réseau"),
    (ESSAI, "        s.rappeler = !e.muet;\n        s.resultat = r;", "        s.rappeler = false;\n        s.resultat = r;", T_ESSAI,
     "sans_ancien_reseau_l_echec_est_rendu_tout_de_suite", "sans ancien réseau, l'écran n'est jamais prévenu de l'échec"),
    (ESSAI, "        s.adopter_nouveau = true;", "        s.adopter_nouveau = false;", T_ESSAI, "essai_reussi",
     "le nouveau réseau réussi n'est pas retenu comme réseau courant"),
    (ESSAI, "    } else if (e.phase == Phase::Retour) {\n        e.phase = Phase::Repos;\n        s.rappeler = !e.muet;",
     "    } else if (e.phase == Phase::Retour) {\n        e.phase = Phase::Repos;\n        s.rappeler = true;", T_ESSAI,
     "abandon_pendant_l_essai_reprend_l_ancien_sans_rappel", "le rappel arrive après un abandon"),
    (ESSAI, "    Sortie s;\n    if (e.phase == Phase::Retour) {\n        e.phase = Phase::Repos;",
     "    Sortie s;\n    if (false) {\n        e.phase = Phase::Repos;", T_ESSAI, "delai_du_retour_rend_la_main_quand_meme",
     "si l'ancien réseau ne revient pas, l'écran attend pour toujours"),
    (ESSAI, "    if (e.phase != Phase::Repos) {\n        s.etat = e;", "    if (false) {\n        s.etat = e;", T_ESSAI,
     "un_seul_essai_a_la_fois", "un deuxième essai écrase le premier en cours"),
    (ESSAI, "    if (e.phase != Phase::Essai) {\n        Sortie s;\n        s.etat = e;\n        return s;\n    }\n    return detail::echec(",
     "    if (false) {\n        Sortie s;\n        s.etat = e;\n        return s;\n    }\n    return detail::echec(", T_ESSAI,
     "un_resultat_tardif_ne_rappelle_pas_deux_fois", "un minuteur d'essai qui arrive après la fin relance un retour arrière"),
    (ESSAI, "e.cause_connue ? e.derniere_cause : Resultat::Autre", "Resultat::Autre", T_ESSAI, "delai_garde_la_derniere_cause_vue",
     "le délai oublie ce que la radio avait dit (mot de passe refusé…)"),
    (ESSAI, "    if (e.phase == Phase::Essai) {\n        e.muet = true;", "    if (e.phase == Phase::Essai) {\n        e.muet = false;",
     T_ESSAI, "abandon_pendant_l_essai_reprend_l_ancien_sans_rappel", "un abandon pendant l'essai laisse le rappel actif"),
    (ESSAI, "    } else if (e.phase == Phase::Retour) {\n        e.muet = true;", "    } else if (e.phase == Phase::Retour) {\n        e.muet = false;",
     T_ESSAI, "abandon_pendant_le_retour_supprime_le_rappel", "un abandon pendant le retour laisse le rappel actif"),
    (ESSAI, "            e.phase = Phase::Retour;\n            s.appliquer_ancien = true;", "            e.phase = Phase::Retour;",
     T_ESSAI, "abandon_pendant_l_essai_reprend_l_ancien_sans_rappel", "un abandon ne remet pas l'ancien réseau"),
    # --- points_acces.hpp : classement ---
    (POINTS, "if (!(p.rssi_dbm > sortie[existant].rssi_dbm)) {", "if (false) {", T_POINTS, "un_ssid_vu_plusieurs_fois_garde_le_plus_fort",
     "un point d'accès plus faible remplace le plus fort du même réseau"),
    (POINTS, "if (!(p.rssi_dbm > sortie[existant].rssi_dbm)) {", "if (!(p.rssi_dbm >= sortie[existant].rssi_dbm)) {", T_POINTS,
     "le_plus_fort_arrive_en_premier_et_les_autres_sont_ignores", "à force égale, le dernier point d'accès vu remplace le premier"),
    (POINTS, "if (existant < compte) {", "if (false) {", T_POINTS, "un_ssid_vu_plusieurs_fois_garde_le_plus_fort",
     "le même réseau est listé autant de fois que de bornes"),
    (POINTS, "if (detail::vide(p.ssid)) {", "if (false) {", T_POINTS, "reseau_cache_n_est_pas_liste",
     "les réseaux cachés (SSID vide) sont listés"),
    (POINTS, "return a.rssi_dbm > b.rssi_dbm;", "return a.rssi_dbm < b.rssi_dbm;", T_POINTS, "du_plus_fort_au_plus_faible",
     "la liste commence par le réseau le plus faible"),
    (POINTS, "return ca < cb;", "return ca > cb;", T_POINTS, "force_egale_ordre_des_octets_du_ssid",
     "à force égale, l'ordre change d'une recherche à l'autre"),
    (POINTS, "        const unsigned char ca = static_cast<unsigned char>(a.ssid[i]);\n        const unsigned char cb = static_cast<unsigned char>(b.ssid[i]);",
     "        const signed char ca = static_cast<signed char>(a.ssid[i]);\n        const signed char cb = static_cast<signed char>(b.ssid[i]);",
     T_POINTS, "octets_au_dela_de_127_comparent_sans_signe", "les SSID accentués passent avant les SSID ASCII"),
    (POINTS, "vers.ssid[32] = '\\0';", "(void)0;", T_POINTS, "ssid_sans_zero_final_est_borne",
     "un SSID sans zéro final est copié sans être terminé"),
    (POINTS, "} else if (compte == max && !detail::avant(p, sortie[max - 1])) {", "} else if (false) {", T_POINTS,
     "liste_pleine_un_reseau_plus_faible_que_le_dernier_est_ignore", "écriture hors du tableau quand la liste est pleine"),
    (POINTS, "std::size_t fin = compte < max ? compte : max - 1;", "std::size_t fin = compte;", T_POINTS, "liste_pleine_garde_les_plus_forts",
     "décalage hors du tableau quand la liste est pleine"),
    (POINTS, "            detail::copier(sortie[j], sortie[j - 1]);", "            (void)j;", T_POINTS, "du_plus_fort_au_plus_faible",
     "l'insertion triée n'écarte plus les lignes (un réseau en écrase un autre)"),
    (POINTS, "for (std::size_t j = existant; j + 1 < compte; ++j) {", "for (std::size_t j = existant; j + 2 < compte; ++j) {", T_POINTS,
     "un_doublon_du_milieu_plus_fort_garde_les_autres", "retirer le doublon du milieu laisse un trou ou un double"),
]
