"""Mutations du noyau « ordonnanceur de la tâche réseau des données » (ordonnanceur_reseau.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS dans lancer.py : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —,
remplacement, test qui doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule).

Chaque mutation simule une erreur d'ordonnancement : une source qui passe sans avoir été réveillée, un anti-rebond perdu ou
repoussé à chaque réveil, un réveil qui retarde un pas proche, une source qui en affame une autre, la liste des spots qui fait la
queue derrière la météo, une source qui ne s'endort plus, une attente sans plafond ou négative.
Non couvert : la tâche FreeRTOS, les notifications et les pas des clients (main/tache_donnees.cpp, main/*_client.cpp), vérifiés
seulement sur la carte.
"""

ORD = "ordonnanceur_reseau.hpp"
T_ORD = "test_ordonnanceur_reseau.cpp"

MUTATIONS_ORDONNANCEUR = [
    # --- réveil ---
    (ORD, "    std::int64_t echeance_ms[kNombreSources] = {kJamais, kJamais, kJamais, kJamais};",
     "    std::int64_t echeance_ms[kNombreSources] = {0, 0, 0, 0};", T_ORD, "rien_ne_passe_sans_reveil",
     "toutes les sources passent au démarrage sans avoir été réveillées (la mer d'un appareil sans spot interroge Vigie)"),
    (ORD, "    {1500, false}, // Meteo", "    {0, false}, // Meteo", T_ORD, "reveil_meteo_attend_l_anti_rebond",
     "la météo repart à chaque changement de zone, sans laisser passer une rafale de changements"),
    (ORD, "        if (au_plus_tard < echeance_ms[i]) {\n            echeance_ms[i] = au_plus_tard;\n        }",
     "        echeance_ms[i] = au_plus_tard;", T_ORD, "rafale_de_reveils_ne_repousse_pas",
     "chaque réveil repousse l'échéance : une rafale de réglages retarde la requête sans fin"),
    (ORD, "        if (au_plus_tard < echeance_ms[i]) {\n            echeance_ms[i] = au_plus_tard;\n        }",
     "        if (au_plus_tard > echeance_ms[i] || echeance_ms[i] == kJamais) {\n            echeance_ms[i] = au_plus_tard;\n        }",
     T_ORD, "reveil_avance_un_pas_lointain", "un réveil n'avance pas une requête prévue dans 15 min"),
    (ORD, "        reveillee[i] = true;\n    }", "    }", T_ORD, "reveil_signale_une_seule_fois",
     "le pas ne sait pas qu'il a été réveillé : la météo attend la fin de son délai malgré un changement de zone"),
    (ORD, "        reveillee[i] = false;\n        return r;", "        return r;", T_ORD, "reveil_signale_une_seule_fois",
     "un réveil est signalé à chaque pas : la météo redemande en boucle"),
    # --- choix ---
    (ORD, "            if (echeance_ms[i] == kJamais || echeance_ms[i] > maintenant_ms) {",
     "            if (echeance_ms[i] == kJamais) {", T_ORD, "seules_les_sources_dues_passent",
     "une source passe avant son échéance : les requêtes ne respectent plus leur rythme"),
    (ORD, "            if (echeance_ms[i] == kJamais || echeance_ms[i] > maintenant_ms) {",
     "            if (echeance_ms[i] == kJamais || echeance_ms[i] >= maintenant_ms) {", T_ORD, "seules_les_sources_dues_passent",
     "une source ne passe qu'une milliseconde après son échéance"),
    (ORD, "            if (echeance_ms[i] < echeance_ms[choisie]) {", "            if (echeance_ms[i] > echeance_ms[choisie]) {",
     T_ORD, "la_plus_en_retard_passe_d_abord", "la source la moins en retard passe d'abord"),
    (ORD, "            if (echeance_ms[i] < echeance_ms[choisie]) {", "            if (echeance_ms[i] <= echeance_ms[choisie]) {",
     T_ORD, "egalite_dans_l_ordre_de_la_table", "à égalité, la dernière de la table passe d'abord"),
    (ORD, "            if (echeance_ms[i] < echeance_ms[choisie]) {", "            if (false) {", T_ORD,
     "une_source_qui_rend_zero_n_affame_pas_les_autres", "la première de la table passe toujours : elle affame les autres"),
    (ORD, "                if (p) {\n                    choisie = i;\n                }\n                continue;",
     "                continue;", T_ORD, "liste_des_spots_passe_avant_les_autres",
     "la liste des spots fait la queue derrière la météo et les alertes pendant qu'une personne attend"),
    (ORD, "    {0, true},     // ListeSpots", "    {0, false},    // ListeSpots", T_ORD, "liste_des_spots_passe_avant_les_autres",
     "la liste des spots n'est plus prioritaire"),
    (ORD, "    {0, false},    // Vigie", "    {0, true},     // Vigie", T_ORD, "liste_des_spots_passe_avant_les_autres",
     "les alertes passent devant la liste des spots demandée à l'écran"),
    # --- après un pas ---
    (ORD, "        if (delai_ms == kPlusRien) {\n            echeance_ms[i] = kJamais;\n            return;\n        }", "", T_ORD,
     "plus_rien_endort_la_source", "une liste lue est redemandée sans fin (« plus rien » lu comme « tout de suite »)"),
    (ORD, "(delai_ms < 0 ? 0 : delai_ms)", "delai_ms", T_ORD, "delai_negatif_vaut_zero",
     "un délai négatif met l'échéance dans le passé"),
    (ORD, "        echeance_ms[i] = maintenant_ms + (delai_ms", "        echeance_ms[i] = (delai_ms", T_ORD,
     "delai_compte_depuis_la_fin_du_pas", "le délai est compté depuis le démarrage de l'appareil"),
    # --- attente ---
    (ORD, "            if (reste < attente) {", "            if (reste > attente) {", T_ORD, "attente_jusqu_a_la_prochaine_echeance",
     "la tâche dort jusqu'au plafond et rate les échéances proches"),
    (ORD, "        return attente < 0 ? 0 : attente;", "        return attente;", T_ORD, "attente_jusqu_a_la_prochaine_echeance",
     "une échéance dépassée donne une attente négative"),
    (ORD, "        std::int64_t attente = plafond_ms;", "        std::int64_t attente = kJamais;", T_ORD,
     "rien_ne_passe_sans_reveil", "sans rien de prévu, la tâche dort sans fin (aucune trace au journal)"),
]
