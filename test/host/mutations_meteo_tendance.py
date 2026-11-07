"""Mutations de la tendance de pression sur 3 h (meteo_tendance.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_METEO_RICHE : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —,
remplacement, test qui doit échouer, nom de la fonction de test qui doit être citée (None : n'importe laquelle), ce que la
mutation simule). Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Chaque mutation vise une règle écrite en tête de meteo_tendance.hpp : fenêtre de 2 h 30 à 3 h 30 bornes comprises, mesure la plus
proche (à égalité la plus ancienne), même mesure reçue deux fois, mesure plus ancienne, historique borné.
"""

TENDANCE = "test_meteo_tendance.cpp"
H = "meteo_tendance.hpp"

MUTATIONS_METEO_TENDANCE = [
    # --- la différence elle-même ---
    (H, "t.dixiemes_hpa = pression_dixiemes_hpa - h.dixiemes_hpa[i];",
     "t.dixiemes_hpa = h.dixiemes_hpa[i] - pression_dixiemes_hpa;",
     TENDANCE, "trois_heures_pile_donnent_la_difference", "la tendance est le contraire de ce qu'elle doit être (la pression qui monte descend)"),
    (H, "t.connue = true;", "t.connue = false;",
     TENDANCE, "trois_heures_pile_donnent_la_difference", "une tendance trouvée n'est jamais « connue »"),
    (H, "const std::int64_t cible = instant_s - TENDANCE_DUREE_S;", "const std::int64_t cible = instant_s - 2 * 3600;",
     TENDANCE, "trois_heures_pile_donnent_la_difference", "la tendance est sur 2 h au lieu de 3 h"),
    # --- la fenêtre de 2 h 30 à 3 h 30, bornes comprises ---
    (H, "std::int64_t meilleur_ecart = TENDANCE_TOLERANCE_S + 1;", "std::int64_t meilleur_ecart = TENDANCE_TOLERANCE_S;",
     TENDANCE, "la_fenetre_va_de_2h30_a_3h30_bornes_comprises", "une mesure d'exactement 2 h 30 ou 3 h 30 est refusée"),
    (H, "std::int64_t meilleur_ecart = TENDANCE_TOLERANCE_S + 1;", "std::int64_t meilleur_ecart = TENDANCE_TOLERANCE_S + 2;",
     TENDANCE, "la_fenetre_va_de_2h30_a_3h30_bornes_comprises", "une mesure d'une seconde de trop (3 h 30 min 1 s) est acceptée"),
    (H, "inline constexpr std::int64_t TENDANCE_TOLERANCE_S = 30 * 60;", "inline constexpr std::int64_t TENDANCE_TOLERANCE_S = 60 * 60;",
     TENDANCE, "la_fenetre_va_de_2h30_a_3h30_bornes_comprises", "la fenêtre est de 2 h à 4 h"),
    (H, "const std::int64_t ecart = h.instants_s[i] > cible ? h.instants_s[i] - cible : cible - h.instants_s[i];",
     "const std::int64_t ecart = h.instants_s[i] - cible;",
     TENDANCE, "la_fenetre_va_de_2h30_a_3h30_bornes_comprises", "l'écart n'est pas une valeur absolue : toute mesure plus ancienne que la cible convient"),
    # --- laquelle des mesures ---
    (H, "if (ecart < meilleur_ecart) {", "if (ecart <= meilleur_ecart) {",
     TENDANCE, "la_mesure_la_plus_proche_de_3h_est_retenue", "à égalité de distance la plus récente l'emporte au lieu de la plus ancienne"),
    (H, "for (std::size_t i = 0; i < h.n; ++i) {", "for (std::size_t i = 0; i + 1 < h.n; ++i) {",
     TENDANCE, "la_fenetre_va_de_2h30_a_3h30_bornes_comprises", "la dernière mesure de l'historique n'est jamais regardée"),
    # --- noter une mesure ---
    (H, "h.dixiemes_hpa[dernier] = p; // même mesure : la dernière valeur reçue fait foi",
     "(void)p; // même mesure : la première valeur reçue fait foi",
     TENDANCE, "la_meme_mesure_deux_fois_ne_fait_qu_un_point", "la valeur révisée d'une même mesure est perdue"),
    (H, "if (instant_s == h.instants_s[dernier]) {", "if (instant_s == h.instants_s[dernier] + 1) {",
     TENDANCE, "la_meme_mesure_deux_fois_ne_fait_qu_un_point", "la même mesure reçue deux fois fait deux points"),
    (H, "if (instant_s < h.instants_s[dernier])", "if (instant_s > h.instants_s[dernier])",
     TENDANCE, "une_mesure_plus_ancienne_repart_de_zero", "tout nouveau point efface l'historique au lieu d'un point en arrière"),
    (H, "h = HistoPression{}; // en arrière : on repart de zéro", "return; // en arrière : on ignore",
     TENDANCE, "une_mesure_plus_ancienne_repart_de_zero", "une mesure plus ancienne que la dernière est ignorée au lieu de tout recommencer"),
    # --- l'historique borné ---
    (H, "for (std::size_t i = 1; i < TENDANCE_MESURES_MAX; ++i) {", "for (std::size_t i = 2; i < TENDANCE_MESURES_MAX; ++i) {",
     TENDANCE, "l_historique_est_borne_et_garde_les_plus_recentes", "en cas de dépassement deux mesures sortent mais une seule est décomptée"),
    (H, "h.instants_s[i - 1] = h.instants_s[i];", "h.instants_s[i - 1] = h.instants_s[0];",
     TENDANCE, "l_historique_est_borne_et_garde_les_plus_recentes", "tous les instants prennent celui de la plus ancienne quand l'historique est plein"),
    (H, "h.dixiemes_hpa[i - 1] = h.dixiemes_hpa[i];", "h.dixiemes_hpa[i - 1] = h.dixiemes_hpa[0];",
     TENDANCE, "l_historique_est_borne_et_garde_les_plus_recentes", "toutes les pressions prennent celle de la plus ancienne quand l'historique est plein"),
    (H, "inline constexpr std::size_t TENDANCE_MESURES_MAX = 16;", "inline constexpr std::size_t TENDANCE_MESURES_MAX = 12;",
     TENDANCE, "l_historique_est_borne_et_garde_les_plus_recentes", "l'historique ne couvre que 2 h 45 : la borne de 3 h 30 n'est plus atteinte"),
]
