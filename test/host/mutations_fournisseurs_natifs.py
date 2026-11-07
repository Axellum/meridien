"""Mutations des lecteurs de MET Norway et du NWS et de leurs pièces (ADR-0014), lues par lancer.py --mutations.

Même format que MUTATIONS_FOURNISSEURS : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement,
fichier de test qui doit échouer, nom de la fonction de test qui doit être citée (None : n'importe quelle erreur de compilation),
ce que la mutation simule). Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Fichiers couverts : iso8601.hpp (dates, durées, dates HTTP), parcours_json.hpp, meteo_rythme_natif.hpp,
meteo_requete_fournisseurs.hpp, soleil_calcule.hpp, meteo_natif_commun.hpp, meteo_metno.hpp, meteo_nws.hpp, plus quatre
mutations de meteo_openmeteo.hpp et meteo_riche.hpp (le lecteur d'Open-Meteo refuse un « inconnu » écrit dans sa réponse). Les huit « identique à
Python » de chaque lecteur voient n'importe quelle valeur copiée au mauvais endroit : les mutations de champs sont attendues d'eux
sans nommer lequel (None) ; les autres visent un test précis.

Écartées volontairement (mutations équivalentes : aucun comportement observable ne change) : le `std::max` du coucher du soleil
(`e.coucher_s > e.lever_s ? … : …`, le calcul ne donne jamais un coucher avant le lever hors des pôles), la remise de `p_` à la fin
du tableau après le dernier élément (parcours_json.hpp : `suivant` s'arrête déjà sur le `]`).
"""

ISO = "test_iso8601.cpp"
PARCOURS = "test_parcours_json.cpp"
RYTHME = "test_rythme_natif.cpp"
REQUETE = "test_requete_fournisseurs.cpp"
SOLEIL = "test_soleil_calcule.cpp"
METNO = "test_meteo_metno.cpp"
NWS = "test_meteo_nws.cpp"
OPENMETEO = "test_meteo_openmeteo.cpp"
RICHE = "test_meteo_riche.cpp"

C_ISO = "iso8601.hpp"
P = "parcours_json.hpp"
R = "meteo_rythme_natif.hpp"
Q = "meteo_requete_fournisseurs.hpp"
S = "soleil_calcule.hpp"
C = "meteo_natif_commun.hpp"
M = "meteo_metno.hpp"
N = "meteo_nws.hpp"
OM = "meteo_openmeteo.hpp"
RH = "meteo_riche.hpp"

MUTATIONS_FOURNISSEURS_NATIFS = [
    # ====================================================================== iso8601.hpp
    (C_ISO, "decalage = (dh * 3600 + dm * 60) * (t[19] == '-' ? -1 : 1);", "decalage = (dh * 3600 + dm * 60) * (t[19] == '-' ? 1 : -1);",
     ISO, "un_decalage_est_retranche", "le décalage d'un instant est ajouté au lieu d'être retranché"),
    (C_ISO, "dm > 59 || dh * 60 + dm > 14 * 60)", "dm > 59 || dh * 60 + dm > 15 * 60)",
     ISO, "des_instants_invalides_sont_refuses_sans_toucher_la_sortie", "un décalage de plus de 14 h est accepté"),
    (C_ISO, "    if (t.size() == 20 && t[19] == 'Z') {", "    if (t.size() >= 20 && t[19] == 'Z') {",
     ISO, "des_instants_invalides_sont_refuses_sans_toucher_la_sortie", "du texte après le « Z » est accepté"),
    (C_ISO, "total += n * 86400;", "total += n * 86401;",
     ISO, "des_durees_valides", "un jour dure 86 401 secondes"),
    (C_ISO, "} else if (u == 'M' && apres_t && etape <= 2) {", "} else if (u == 'M' && etape <= 2) {",
     ISO, "des_durees_invalides_sont_refusees_sans_toucher_la_sortie", "« P3M » (trois mois) est lu comme trois minutes"),
    (C_ISO, "if (!au_moins_un || total > 366LL * 86400)", "if (!au_moins_un || total > 367LL * 86400)",
     ISO, "des_durees_invalides_sont_refusees_sans_toucher_la_sortie", "le plafond d'une durée passe de 366 à 367 jours"),
    (C_ISO, "if (!instant(t.substr(0, barre), d) || !duree(t.substr(barre + 1), l) || l <= 0)",
     "if (!instant(t.substr(0, barre), d) || !duree(t.substr(barre + 1), l) || l < 0)",
     ISO, "un_intervalle_invalide_est_refuse_sans_toucher_les_sorties", "un intervalle de durée nulle est accepté"),
    (C_ISO, "if (heure::detail::jour_semaine_de(jours) != nom_du_jour)", "if (heure::detail::jour_semaine_de(jours) == nom_du_jour)",
     ISO, "des_dates_http_valides", "une date HTTP est refusée quand son jour de semaine est le bon"),
    # (`t.size() != 29` devenu `< 29` serait un mutant ÉQUIVALENT : une date trop longue est déjà refusée par `t.substr(25) != " GMT"`.)
    (C_ISO, "if (t.size() != 29 || t[3] != ','", "if (t[3] != ','",
     ISO, "des_dates_http_invalides_sont_refusees_sans_toucher_la_sortie", "la longueur d'une date HTTP n'est plus contrôlée : lecture hors de la chaîne"),
    (C_ISO, "t[22] != ':' || t.substr(25) != \" GMT\")", "t[22] != ':')",
     ISO, "des_dates_http_invalides_sont_refusees_sans_toucher_la_sortie", "un fuseau autre que GMT est accepté"),
    (C_ISO, "sortie = jours * 86400 + h * 3600 + mn * 60 + s;", "sortie = jours * 86400 + h * 3600 + mn * 60;",
     ISO, "des_dates_http_valides", "les secondes d'une date HTTP sont perdues"),
    # ====================================================================== parcours_json.hpp
    (P, "if (p_ >= brut_.size() || brut_[p_] == ']') {", "if (p_ >= brut_.size()) {",
     PARCOURS, "un_tableau_vide_ou_d_un_element", "le crochet fermant est pris pour un élément"),
    (P, "tableau.genre() == Genre::Tableau ? 1 : tableau.brut().size()", "tableau.genre() == Genre::Tableau ? 0 : tableau.brut().size()",
     PARCOURS, "des_nombres", "le parcours commence sur le crochet ouvrant"),
    (P, "detail::genre_de(brut_[p_]));", "Genre::Chaine);",
     PARCOURS, "le_parcours_rend_les_genres_attendus", "tous les éléments sont des chaînes"),
    # ====================================================================== meteo_rythme_natif.hpp
    (R, "inline constexpr std::int64_t PLANCHER_NWS_S = 60 * 60;", "inline constexpr std::int64_t PLANCHER_NWS_S = 30 * 60;",
     RYTHME, "les_planchers_sont_ceux_de_chaque_service", "le NWS est interrogé toutes les 30 minutes"),
    (R, "return PLANCHER_METNO_S;", "return PLANCHER_NWS_S;",
     RYTHME, "les_planchers_sont_ceux_de_chaque_service", "MET Norway reçoit le plancher du NWS"),
    (R, "inline constexpr std::int64_t PLAFOND_S = 90 * 60;", "inline constexpr std::int64_t PLAFOND_S = 120 * 60;",
     RYTHME, "un_expires_lointain_est_plafonne_a_quatre_vingt_dix_minutes", "un Expires lointain laisse la météo vieillir deux heures"),
    (R, "if (expires_s > 0 && jusqu_a_expires > base)", "if (expires_s > 0 && jusqu_a_expires < base)",
     RYTHME, "le_delai_suit_expires_quand_il_est_plus_loin_que_le_plancher", "Expires raccourcit le délai au lieu de le rallonger"),
    (R, "std::int64_t delai = base + (base / 10) * alea / 255;", "std::int64_t delai = base + (base / 5) * alea / 255;",
     RYTHME, "sans_expires_c_est_le_plancher_plus_au_plus_un_dixieme", "l'étalement du délai passe d'un dixième à un cinquième"),
    (R, "return reste >= 3600 - AVANT_HEURE_RONDE_S || reste < APRES_HEURE_RONDE_S;",
     "return reste >= 3600 - AVANT_HEURE_RONDE_S && reste < APRES_HEURE_RONDE_S;",
     RYTHME, "l_heure_ronde_est_evitee_avec_un_etalement", "l'heure ronde n'est plus jamais évitée"),
    (R, "delai += jusqu_a_la_sortie + (alea & 0x3FU);", "delai += jusqu_a_la_sortie;",
     RYTHME, "l_heure_ronde_est_evitee_avec_un_etalement", "plus d'étalement après l'heure ronde : tous les appareils retombent ensemble"),
    (R, "3600 - reste + APRES_HEURE_RONDE_S", "3600 - reste",
     RYTHME, "l_heure_ronde_est_evitee_avec_un_etalement", "on sort de la zone avant l'heure ronde seulement jusqu'à l'heure pile"),
    (R, "return obtenue_s > 0 && age >= 0 && age < DUREE_MAILLE_NWS_S;", "return obtenue_s > 0 && age >= 0 && age <= DUREE_MAILLE_NWS_S;",
     RYTHME, "la_maille_du_nws_vaut_vingt_quatre_heures", "la maille du NWS sert encore à l'instant exact de ses 24 heures"),
    (R, "return obtenue_s > 0 && age >= 0 && age < DUREE_MAILLE_NWS_S;", "return obtenue_s > 0 && age < DUREE_MAILLE_NWS_S;",
     RYTHME, "la_maille_du_nws_vaut_vingt_quatre_heures", "une horloge revenue en arrière garde la maille"),
    # ====================================================================== meteo_requete_fournisseurs.hpp
    (Q, "const std::int64_t frac = (a % 100000) / 10; // quatre chiffres", "const std::int64_t frac = (a % 100000) / 100; // quatre chiffres",
     REQUETE, "metno_quatre_decimales", "les décimales de la position sont tronquées à deux chiffres"),
    (Q, "if (negatif && (entier != 0 || frac != 0))", "if (negatif)",
     REQUETE, "jamais_de_signe_devant_un_zero", "un « -0.0000 » part dans l'adresse"),
    (Q, "longitude_e5 <= LONGITUDE_E5_MAX;", "longitude_e5 <= LONGITUDE_E5_MAX + 1;",
     REQUETE, "hors_du_globe_rien_n_est_construit", "une longitude un peu au-delà de 180° est acceptée"),
    (Q, "if (bureau.size() != 3)", "if (bureau.size() < 3)",
     REQUETE, "le_bureau_est_trois_lettres_capitales", "un bureau de plus de trois lettres entre dans l'adresse"),
    (Q, "if (c < 'A' || c > 'Z')", "if (c > 'Z')",
     REQUETE, "le_bureau_est_trois_lettres_capitales", "un bureau avec un signe ou un chiffre entre dans l'adresse"),
    (Q, "|| x < 0 || x > MAILLE_NWS_MAX || y < 0 ||", "|| x > MAILLE_NWS_MAX || y < 0 ||",
     REQUETE, "la_grille_refuse_un_bureau_ou_une_maille_qui_ne_va_pas_dans_une_adresse", "un x négatif entre dans l'adresse de la grille"),
    (Q, 'inline constexpr std::string_view USER_AGENT = "Meridien/ESP32-P4 github.com/Axellum/meridien";',
     'inline constexpr std::string_view USER_AGENT = "Meridien/ESP32-P4";',
     REQUETE, "l_identification_est_celle_de_l_application", "l'identification n'a plus de contact : MET Norway répond 403"),
    (Q, "inline constexpr std::size_t TAILLE_REPONSE_NWS_GRILLE_MAX = 384 * 1024;", "inline constexpr std::size_t TAILLE_REPONSE_NWS_GRILLE_MAX = 200 * 1024;",
     REQUETE, "les_bornes_de_reponse_ont_de_la_marge_sur_les_reponses_reelles", "la borne de la grille du NWS est plus petite que la réponse de San Juan"),
    # ====================================================================== soleil_calcule.hpp
    (S, "inline constexpr double HAUTEUR_LEVER_DEG = -0.833;", "inline constexpr double HAUTEUR_LEVER_DEG = -0.5;",
     SOLEIL, "contre_open_meteo_lyon", "la réfraction et le demi-disque du soleil sont oubliés"),
    (S, "const double temps_solaire_vrai_min = minutes_utc + p.equation_du_temps_min + 4.0 * lon;",
     "const double temps_solaire_vrai_min = minutes_utc + p.equation_du_temps_min - 4.0 * lon;",
     SOLEIL, "hauteur_du_soleil", "la longitude est comptée dans le mauvais sens"),
    (S, "const double demi_jour_s = detail::vers_deg(detail::arc_cosinus(ratio)) * 240.0;", "const double demi_jour_s = detail::vers_deg(detail::arc_cosinus(ratio)) * 120.0;",
     SOLEIL, "contre_open_meteo_lyon", "une minute par demi-degré au lieu de quatre par degré"),
    (S, "p.equation_du_temps_min = 4.0 * vers_deg(eq);", "p.equation_du_temps_min = 3.0 * vers_deg(eq);",
     SOLEIL, "contre_open_meteo_lyon", "l'équation du temps est fausse d'un quart"),
    (S, "        r.situation = Situation::NuitPolaire;", "        r.situation = Situation::JourPolaire;",
     SOLEIL, "contre_open_meteo_longyearbyen_nuit_polaire", "la nuit polaire est prise pour un jour polaire"),
    (S, "const double lever = midi - demi_jour_s;", "const double lever = midi + demi_jour_s;",
     SOLEIL, "contre_open_meteo_lyon", "le lever est après le midi solaire"),
    (S, "const long long k = static_cast<long long>(n >= 0.0 ? n + 0.5 : n - 0.5);", "const long long k = static_cast<long long>(n + 0.5);",
     SOLEIL, "fonctions_trigonometriques", "la réduction d'un angle négatif arrondit du mauvais côté"),
    (S, "for (int n = 1; n <= 12; ++n) {\n        terme *= -x * x / (static_cast<double>(2 * n) * static_cast<double>(2 * n + 1));",
     "for (int n = 1; n <= 3; ++n) {\n        terme *= -x * x / (static_cast<double>(2 * n) * static_cast<double>(2 * n + 1));",
     SOLEIL, "fonctions_trigonometriques", "le sinus n'a que trois termes"),
    # ====================================================================== meteo_natif_commun.hpp
    (C, "if (n > 100) {", "if (n > 1000) {",
     METNO, "une_humidite_au_dela_de_100_est_refusee", "un pourcentage de 306 est rangé dans un octet : il deviendrait 50"),
    (C, "if (n < 0 || n > INT16_MAX) {", "if (n > INT16_MAX) {",
     METNO, "une_humidite_au_dela_de_100_est_refusee", "un entier négatif (« -1 ») est pris pour « inconnu »"),
    (C, "constexpr std::int64_t kmh_dix_depuis_ms_dix(std::int64_t ms_dix) noexcept { return (ms_dix * 36 + 5) / 10; }",
     "constexpr std::int64_t kmh_dix_depuis_ms_dix(std::int64_t ms_dix) noexcept { return (ms_dix * 36) / 10; }",
     METNO, "le_vent_passe_de_m_s_a_km_h", "la conversion m/s -> km/h tronque au lieu d'arrondir"),
    (C, "        if (v == INCONNU_DIX)\n            return;", "        if (v == 0)\n            return;",
     METNO, "les_extremes_d_un_jour_gardent_un_zero_et_ignorent_l_inconnu", "un zéro est ignoré et l'inconnu est compté dans le plus petit et le plus grand"),
    (C, "j.coucher_s = j.date_s;", "j.coucher_s = fin_s;",
     METNO, "la_nuit_polaire_de_longyearbyen_a_lever_et_coucher_a_minuit", "la nuit polaire a son coucher à la fin du jour"),
    (C, "if (iana.empty() || iana.size() >= FUSEAU_MAX)", "if (iana.empty() || iana.size() > FUSEAU_MAX)",
     METNO, "un_nom_de_fuseau_qui_ne_tient_pas_dans_le_releve_est_refuse", "un nom de fuseau déborde du relevé d'un caractère"),
    # ====================================================================== meteo_metno.hpp
    (M, '{"cloudy", 3}, {"fog", 45},', '{"cloudy", 3}, {"fog", 48},',
     METNO, "les_41_symboles_officiels_et_leurs_variantes", "le brouillard de MET Norway est un brouillard givrant"),
    (M, 'std::string_view("_polartwilight")}) {', 'std::string_view("_polartwilightx")}) {',
     METNO, "les_41_symboles_officiels_et_leurs_variantes", "la variante « _polartwilight » n'est plus retirée du symbole"),
    (M, "if (ms_dix < 0 || kmh > INT16_MAX) {", "if (kmh > INT16_MAX) {",
     METNO, "un_vent_negatif_ou_enorme_est_refuse", "un vent négatif est accepté"),
    (M, "if (s.n > 0 && e.t <= s.e[s.n - 1].t) {", "if (s.n > 0 && e.t < s.e[s.n - 1].t) {",
     METNO, "des_instants_qui_ne_croissent_pas_sont_refuses", "deux éléments au même instant sont acceptés"),
    (M, "r.jour = astro::soleil_leve(c.t, latitude_e5, longitude_e5);", "r.jour = !astro::soleil_leve(c.t, latitude_e5, longitude_e5);",
     METNO, None, "le jour et la nuit sont inversés pour le moment"),
    (M, "j.uv_max_dixiemes = INCONNU_DIX;", "j.uv_max_dixiemes = 0;",
     METNO, "ce_que_la_source_ne_donne_pas_reste_inconnu", "l'indice UV du jour vaut zéro au lieu d'être inconnu"),
    (M, "q.precip_courante_dixiemes = INCONNU_DIX; // « le dernier quart d'heure écoulé »", "q.precip_courante_dixiemes = 0; // « le dernier quart d'heure écoulé »",
     METNO, "ce_que_la_source_ne_donne_pas_reste_inconnu", "la pluie du dernier quart d'heure vaut zéro au lieu d'être inconnue"),
    (M, "h.uv_dixiemes = INCONNU_DIX; // MET Norway ne donne que l'UV par ciel dégagé", "h.uv_dixiemes = 0; // MET Norway ne donne que l'UV par ciel dégagé",
     METNO, "ce_que_la_source_ne_donne_pas_reste_inconnu", "l'indice UV des heures vaut zéro au lieu d'être inconnu"),
    (M, "cj.precip_dixiemes_mm = (complet && total <= INT16_MAX) ?", "cj.precip_dixiemes_mm = (total <= INT16_MAX) ?",
     METNO, None, "le cumul d'un jour dont des heures manquent est donné comme s'il était complet"),
    (M, "cj.heures_precip = (complet && que_des_heures) ?", "cj.heures_precip = (complet) ?",
     METNO, None, "des heures de pluie sont comptées sur un cumul fait de blocs de six heures"),
    (M, "const bool bloc6_dans_le_jour = e.h6.presente && e.t + 6 * 3600 <= d1;", "const bool bloc6_dans_le_jour = e.h6.presente;",
     METNO, None, "le minimum et le maximum d'un jour reprennent ceux d'un bloc qui déborde sur le lendemain"),
    (M, "if (temp.vide() || code < 0)", "if (temp.vide())",
     METNO, "un_jour_sans_symbole_n_est_pas_garde", "un jour sans symbole est gardé"),
    (M, "e.h1.presente && e.h1.code >= 0;", "e.h1.presente;",
     METNO, "vingt_quatre_heures_de_suite_avec_chacune_un_symbole_connu", "une heure sans symbole connu entre dans les 24 heures"),
    # ====================================================================== meteo_nws.hpp
    (N, "return force == Force::Legere ? 56 : 57;", "return force == Force::Legere ? 57 : 56;",
     NWS, "chaque_phenomene_donne_son_code", "la bruine verglaçante légère et forte sont échangées"),
    (N, "return 77; // « ice pellets »", "return 71; // « ice pellets »",
     NWS, "chaque_phenomene_donne_son_code", "les grains de glace sont de la neige faible"),
    (N, "return grosse_grele ? 99 : 96;", "return grosse_grele ? 96 : 99;",
     NWS, "chaque_phenomene_donne_son_code", "la grosse grêle et la petite sont échangées"),
    (N, "return trois(51, 53, 55);", "return trois(51, 53, 53);",
     NWS, "chaque_phenomene_donne_son_code", "la bruine forte est une bruine modérée"),
    (N, 'return c == "slight_chance" || c == "isolated" || c == "few" || c == "brief";', 'return c == "slight_chance" || c == "isolated" || c == "few";',
     NWS, "chaque_phenomene_donne_son_code", "une averse brève compte comme un phénomène prévu"),
    (N, "if (pourcent <= 12)", "if (pourcent <= 13)",
     NWS, "les_seuils_du_ciel", "le ciel dégagé va jusqu'à 13 %"),
    (N, "if (pourcent <= 87)", "if (pourcent <= 88)",
     NWS, "les_seuils_du_ciel", "le ciel nuageux va jusqu'à 88 %"),
    (N, "return temps->v > c ? temps->v : c;", "return c;",
     NWS, "le_plus_fort_du_ciel_et_du_phenomene_l_emporte", "le phénomène (pluie, orage) n'emporte jamais sur le ciel"),
    (N, "if (i == nullptr || i->fin - i->deb > 6 * 3600)", "if (i == nullptr || i->fin - i->deb > 12 * 3600)",
     NWS, "la_probabilite_d_une_heure_est_celle_d_un_intervalle_de_six_heures_au_plus", "la probabilité d'un intervalle de douze heures est celle d'une heure"),
    (N, "if (trouve == nullptr || trouve->v == INCONNU_DIX || trouve->fin > rel1)", "if (trouve == nullptr || trouve->v == INCONNU_DIX)",
     NWS, "un_intervalle_qui_deborde_le_jour_ou_un_trou_rend_le_cumul_inconnu", "un intervalle qui déborde le jour est compté en entier dans le cumul"),
    (N, "(c.n > 0 && rel < fin_precedente)", "(c.n > 0 && false)",
     NWS, "des_intervalles_qui_se_chevauchent_ou_a_l_envers_font_refuser_la_reponse", "des intervalles qui se chevauchent sont acceptés"),
    (N, "case Mode::NeigeMm:\n            it.v = natif::fixe_ou_inconnu(v, 0, nom, diag);", "case Mode::NeigeMm:\n            it.v = natif::fixe_ou_inconnu(v, 1, nom, diag);",
     NWS, "la_neige_est_en_millimetres_dans_la_source_et_en_centimetres_dans_le_noyau", "la neige reste en millimètres dans le noyau"),
    (N, "if (tronque || !fournisseurs::bureau_nws_valide(std::string_view(bureau, n)))", "if (tronque)",
     NWS, "un_bureau_mal_forme_est_refuse", "un bureau mal formé de /points entre dans l'adresse suivante"),
    (N, "|| y < 0 || y > fournisseurs::MAILLE_NWS_MAX)", "|| y > fournisseurs::MAILLE_NWS_MAX)",
     NWS, "une_maille_hors_plage_ou_mal_typee_est_refusee", "un y négatif de /points est accepté"),
    (N, "|| x != maille.x || y != maille.y) {", "|| x != maille.x) {",
     NWS, "la_grille_doit_etre_celle_de_la_maille_demandee", "une grille d'un autre y est lue comme celle de la maille"),
    (N, "x.noter(c.e[i].v);", "x.noter(INCONNU_DIX);",
     NWS, None, "le plus petit et le plus grand d'un jour ne voient plus aucune valeur"),
    (N, "mode != Mode::Temps && !unite_attendue(couche, unite)", "mode != Mode::Temps && false",
     NWS, "une_unite_inattendue_fait_refuser_la_reponse", "une couche en degrés Fahrenheit est lue comme des Celsius"),
    # --- Open-Meteo ne produit jamais d'« inconnu » : un « -1 » ou un « -3276,8 » dans sa réponse reste une valeur fausse ---
    (OM, "detail::verifier_champs(r, false);", "detail::verifier_champs(r, true);",
     OPENMETEO, "valeurs_invraisemblables_et_bornes", "la réponse d'Open-Meteo admet un « inconnu » (direction du vent -1)"),
    (RH, "diag = verifier_heures_riche(essai, false);", "diag = verifier_heures_riche(essai, true);",
     RICHE, "une_valeur_inconnue_ecrite_dans_une_reponse_d_open_meteo_est_refusee", "les heures d'Open-Meteo admettent un « inconnu » (humidité -1)"),
    (RH, "diag = verifier_jours_riche(base, essai, false);", "diag = verifier_jours_riche(base, essai, true);",
     RICHE, "une_valeur_inconnue_ecrite_dans_une_reponse_d_open_meteo_est_refusee", "les jours d'Open-Meteo admettent un « inconnu » (heures de pluie -1)"),
    (RH, "a.diag = detail::verifier_courant_riche(a.meteo.riche, false);", "a.diag = detail::verifier_courant_riche(a.meteo.riche, true);",
     RICHE, "une_valeur_inconnue_ecrite_dans_une_reponse_d_open_meteo_est_refusee", "le courant d'Open-Meteo admet un « inconnu » (nuages -1)"),
]
