"""Mutations du noyau « météo riche » (meteo_riche.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_METEO_APPAREIL : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —,
remplacement, test qui doit échouer, nom de la fonction de test qui doit être citée (None : n'importe laquelle), ce que la
mutation simule). Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Le décalage d'une étiquette des cumuls (Open-Meteo donne le cumul de l'intervalle qui PRÉCÈDE l'étiquette) est la mutation qui
compte le plus : sans elle, la pluie de l'heure et le « dans N minutes » seraient décalés d'une heure ou d'un quart d'heure
sans qu'aucun test sur des chiffres isolés ne s'en aperçoive ; ce sont les valeurs de Python sur de VRAIES réponses qui la voient.
"""

RICHE = "test_meteo_riche.cpp"
H = "meteo_riche.hpp"

MUTATIONS_METEO_RICHE = [
    # --- décalage d'une étiquette (cumuls horaires et quarts d'heure) ---
    (H, 'h.precip_dixiemes_mm = l.fixe16(pluie.element(i + 1), "hourly.precipitation", 1);',
     'h.precip_dixiemes_mm = l.fixe16(pluie.element(i), "hourly.precipitation", 1);',
     RICHE, "les_cumuls_sont_decales_d_une_etiquette_sur_seattle", "la pluie de l'heure est celle de l'étiquette (l'heure d'avant)"),
    (H, 'h.uv_dixiemes = l.fixe16(uv.element(i + 1), "hourly.uv_index", 1);',
     'h.uv_dixiemes = l.fixe16(uv.element(i), "hourly.uv_index", 1);',
     RICHE, "les_cumuls_sont_decales_d_une_etiquette_sur_seattle", "l'UV moyen de l'heure est celui de l'heure d'avant"),
    (H, "proba_presente ? proba.element(i + 1) : json::Valeur{};", "proba_presente ? proba.element(i) : json::Valeur{};",
     RICHE, "les_cumuls_sont_decales_d_une_etiquette_sur_seattle", "la probabilité de l'heure est celle de l'heure d'avant"),
    (H, "essai.quarts_debut_s = t - PAS_QUART_S;", "essai.quarts_debut_s = t;",
     RICHE, "les_quarts_d_heure_sont_recales_sur_l_intervalle_qui_precede", "les quarts d'heure commencent à l'étiquette (un quart trop tard)"),
    (H, "h.temperature_dixiemes = l.fixe16(temp.element(i), ", "h.temperature_dixiemes = l.fixe16(temp.element(i + 1), ",
     RICHE, None, "la température de l'heure est celle de l'heure suivante"),
    (H, "h.jour = (j == 1);", "h.jour = (j == 0);",
     RICHE, None, "le jour et la nuit sont inversés pour une heure"),
    # --- combien d'heures et de quarts ---
    (H, "if (c.n < 2) {", "if (c.n < 1) {",
     RICHE, "une_seule_etiquette_horaire_ne_fait_pas_une_heure", "une seule étiquette horaire fait une heure (sans étiquette suivante)"),
    (H, "const std::size_t nb =\n        (c.n - 1 < static_cast<std::size_t>(HEURES_MAX)) ? c.n - 1 : static_cast<std::size_t>(HEURES_MAX);",
     "const std::size_t nb =\n        (c.n < static_cast<std::size_t>(HEURES_MAX)) ? c.n : static_cast<std::size_t>(HEURES_MAX);",
     RICHE, "une_petite_reponse_donne_moins_d_heures_et_de_quarts", "la dernière étiquette compte comme une heure (lecture hors du tableau)"),
    (H, "inline constexpr int HEURES_MAX = 24;", "inline constexpr int HEURES_MAX = 25;",
     RICHE, "plus_de_vingt_cinq_etiquettes_sont_tronquees_a_vingt_quatre_heures", "25 heures gardées au lieu de 24"),
    (H, "inline constexpr int QUARTS_MAX = 9;", "inline constexpr int QUARTS_MAX = 8;",
     RICHE, None, "8 quarts d'heure gardés au lieu de 9"),
    # --- arrondis ---
    (H, "return static_cast<std::int16_t>((centiemes + 3000) / 6000);", "return static_cast<std::int16_t>(centiemes / 6000);",
     RICHE, None, "l'ensoleillement est tronqué à la minute au lieu d'être arrondi"),
    # --- sections cassées : vidées, jamais gardées à moitié ---
    (H, "    diag = verifier_heures_riche(essai, false); // Open-Meteo ne produit jamais d'inconnu\n    if (diag.ok()) {",
     "    diag = verifier_heures_riche(essai, false);\n    if (true) {",
     RICHE, "une_temperature_horaire_invraisemblable_vide_les_heures", "des heures invraisemblables sont gardées"),
    (H, "    diag = verifier_quarts_riche(essai);\n    if (diag.ok()) {", "    diag = verifier_quarts_riche(essai);\n    if (true) {",
     RICHE, "une_pluie_negative_est_refusee", "des quarts d'heure invraisemblables sont gardés"),
    (H, "    diag = verifier_jours_riche(base, essai, false); // idem\n    if (diag.ok()) {",
     "    diag = verifier_jours_riche(base, essai, false);\n    if (true) {",
     RICHE, "une_neige_negative_vide_les_jours_riches_sans_toucher_aux_jours_de_base", "des compléments de jours invraisemblables sont gardés"),
    (H, "} else if (t - precedent != PAS_QUART_S) {", "} else if (false) {",
     RICHE, "des_quarts_pas_a_quinze_minutes_sont_refuses", "des quarts d'heure qui ne se suivent pas de 15 min sont acceptés"),
    (H, "if (i > 0 && h.debut_s - r.heures[i - 1].debut_s != PAS_HEURE_S)", "if (false)",
     RICHE, "verifier_meteo_refuse_les_incoherences", "des heures qui ne se suivent pas de 3600 s sont acceptées"),
    (H, "if (!dans(h.temperature_dixiemes, -900, 600))", "if (!dans(h.temperature_dixiemes, -900, 6000))",
     RICHE, "une_temperature_horaire_invraisemblable_vide_les_heures", "99 °C est une température horaire plausible"),
    (H, "if (!dans(r.quarts_dixiemes[i], 0, 1000))", "if (!dans(r.quarts_dixiemes[i], -1000, 1000))",
     RICHE, "une_pluie_negative_est_refusee", "une pluie négative par quart d'heure est acceptée"),
    (H, "if (!p.entier(r.nuages_pct, 0, 100))", "if (!p.entier(r.nuages_pct, 0, 101))",
     RICHE, "une_couverture_nuageuse_de_101_pour_cent_est_refusee", "101 % de nuages est accepté"),
    (H, "if (r.nb_jours != 0 && r.nb_jours != base.nb_jours)", "if (false)",
     RICHE, "cache_v2_refuse_une_place_insuffisante_et_une_entree_invraisemblable", "des jours riches qui ne suivent pas les jours de base sont acceptés"),
    # --- indices de l'heure en cours ---
    (H, "if (maintenant_s >= r.heures[i].debut_s && maintenant_s < r.heures[i].debut_s + PAS_HEURE_S)",
     "if (maintenant_s >= r.heures[i].debut_s && maintenant_s <= r.heures[i].debut_s + PAS_HEURE_S)",
     RICHE, "heure_courante_aux_bornes_de_lyon", "l'instant qui finit une heure appartient encore à cette heure"),
    (H, "if (maintenant_s >= debut && maintenant_s < debut + PAS_QUART_S)", "if (maintenant_s >= debut && maintenant_s <= debut + PAS_QUART_S)",
     RICHE, "quart_courant_aux_bornes", "l'instant qui finit un quart d'heure appartient encore à ce quart"),
    # --- pluie des prochaines minutes ---
    (H, "inline constexpr std::int16_t SEUIL_PLUIE_DIXIEMES = 1;", "inline constexpr std::int16_t SEUIL_PLUIE_DIXIEMES = 2;",
     RICHE, "pluie_proche_le_seuil_est_un_dixieme_de_millimetre", "0,1 mm par quart d'heure n'est plus de la pluie"),
    (H, "p.en_cours = r.quarts_dixiemes[q] >= SEUIL_PLUIE_DIXIEMES;", "p.en_cours = r.quarts_dixiemes[q] > SEUIL_PLUIE_DIXIEMES;",
     RICHE, "pluie_proche_le_seuil_est_un_dixieme_de_millimetre", "exactement 0,1 mm dans le quart en cours n'est pas de la pluie en cours"),
    (H, "inline constexpr std::int64_t DELAI_QUARTS_PERIMES_S = 30 * 60;", "inline constexpr std::int64_t DELAI_QUARTS_PERIMES_S = 60 * 60;",
     RICHE, "pluie_proche_perimee_apres_trente_minutes", "les quarts d'heure ne sont périmés qu'après une heure"),
    (H, "p.perimee = (maintenant_s - recu_a_s) > DELAI_QUARTS_PERIMES_S;", "p.perimee = (maintenant_s - recu_a_s) >= DELAI_QUARTS_PERIMES_S;",
     RICHE, "pluie_proche_perimee_apres_trente_minutes", "trente minutes pile suffisent à périmer"),
    (H, "return secondes <= 0 ? 0 : static_cast<int>((secondes + 59) / 60);", "return secondes <= 0 ? 0 : static_cast<int>(secondes / 60);",
     RICHE, "pluie_proche_qui_commence_dans_le_quart_suivant", "les minutes avant la pluie sont arrondies vers le bas (« dans 0 min » alors qu'il reste 59 s)"),
    (H, "    if (fin < n) // la pluie s'arrête dans la fenêtre : au début du premier quart sec",
     "    if (fin <= n) // la pluie s'arrête dans la fenêtre : au début du premier quart sec",
     RICHE, "pluie_proche_sans_fin_dans_la_fenetre", "une pluie qui touche la fin de la fenêtre est donnée pour finie"),
    (H, "for (int i = q; i < n; ++i) {", "for (int i = 0; i < n; ++i) {",
     RICHE, "pluie_proche_reelle_de_seattle", "le cumul compte aussi les quarts d'heure déjà passés"),
    (H, "if (r.nb_quarts == 0 || maintenant_s + TOLERANCE_HORLOGE_S < recu_a_s)", "if (r.nb_quarts == 0 || maintenant_s < recu_a_s)",
     RICHE, "pluie_proche_horloge_non_reglee_ou_en_retard_n_est_pas_connue", "la tolérance de 5 min sur l'horloge disparaît"),
    (H, "if (r.nb_quarts == 0 || maintenant_s + TOLERANCE_HORLOGE_S < recu_a_s)", "if (r.nb_quarts == 0)",
     RICHE, "pluie_proche_horloge_non_reglee_ou_en_retard_n_est_pas_connue", "une horloge en retard sur la réception est crue"),
    # --- cache binaire v2 ---
    (H, "if (donnees[2] != VERSION_CACHE_RICHE) {", "if (false) {",
     RICHE, "cache_v1_d_avant_est_ignore_proprement", "un cache d'une autre version est lu comme un v2"),
    (H, "    if (detail::crc32(donnees, taille - 4) != attendu) {", "    if (false) {",
     RICHE, "cache_v2_refuse_tout_octet_altere", "la somme de contrôle du cache v2 n'est plus vérifiée"),
    (H, "    if (lat < -LATITUDE_E5_MAX || lat > LATITUDE_E5_MAX || lon < -LONGITUDE_E5_MAX || lon > LONGITUDE_E5_MAX) {",
     "    if (false) {",
     RICHE, "cache_v2_scelle_mais_invraisemblable_est_refuse", "une position invraisemblable est relue du cache v2"),
    (H, "if (rd.trop || rd.n != taille - 4) { // trop court, ou octets en trop avant la somme", "if (rd.trop) {",
     RICHE, "cache_v2_scelle_mais_invraisemblable_est_refuse", "des octets en trop avant la somme sont acceptés"),
    (H, "if (nb_h > static_cast<std::uint32_t>(HEURES_MAX)) {", "if (nb_h > 255U) {",
     RICHE, "cache_v2_scelle_mais_invraisemblable_est_refuse", "25 heures sont relues d'un cache"),
    (H, "    out.diag = verifier_meteo(out.meteo);\n    if (out.diag.ok()) {", "    out.diag = Diagnostic{};\n    if (out.diag.ok()) {",
     RICHE, "cache_v2_scelle_mais_invraisemblable_est_refuse", "les valeurs relues du cache ne sont plus contrôlées"),
    (H, "if (sortie == nullptr || !verifier_meteo(m).ok())", "if (sortie == nullptr)",
     RICHE, "cache_v2_refuse_une_place_insuffisante_et_une_entree_invraisemblable", "une météo invraisemblable est écrite en cache"),
    (H, "h.debut_s = premiere + PAS_HEURE_S * static_cast<std::int64_t>(i);", "h.debut_s = premiere;",
     RICHE, "cache_v2_aller_retour_de_chaque_reponse_reelle", "toutes les heures du cache portent la première étiquette"),
    (H, "h.proba_pct = static_cast<std::int16_t>(rd.i8());", "h.proba_pct = static_cast<std::int16_t>(rd.u8());",
     RICHE, "cache_v2_garde_les_probabilites_inconnues", "une probabilité inconnue (-1) revient du cache en 255 %"),
]
