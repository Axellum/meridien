"""Mutations du noyau « écriture du registre des spots par l'API » (components/core/include/ecran/mer_api.hpp et les deux routes
/api/sea de api_reglages.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_ROUE_ENTREES : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement,
test qui doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou
« détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle de ce que la page peut envoyer : la liste entière remplace l'ancienne, un refus n'applique rien, chaque
refus porte sa propre erreur, le même spot reste choisi quand l'envoi n'en nomme pas d'autre, la plage de vent est en nœuds entiers ; et,
côté route, le champ fautif nommé, rien écrit quand rien ne change, un défaut interne (500) quand l'appareil ne peut pas écrire ou lire
son registre. Mutations choisies à la main, pas un balayage. Non muté : la garde « plus de 12 objets » de `ErreurRegistre::Plein` dans la
boucle (inatteignable : la taille est vérifiée avant), les tampons de lecture (un texte trop long y est refusé de toute façon par `ajouter`).
"""

API = "mer_api.hpp"
ROUTE = "api_reglages.hpp"
T_API = "test_mer_api.cpp"
T_ROUTE = "test_api_reglages.cpp"

MUTATIONS_MER_API = [
    # --- mer_api.hpp : lire la liste ---
    (API, "if (spots.genre() != json::Genre::Tableau)\n            return ErreurApi::Spots;",
     "if (false)\n            return ErreurApi::Spots;",
     T_API, "spots_qui_n_est_pas_une_liste_d_objets_est_refuse", "« spots »: \"x\" vide la liste au lieu d'être refusé"),
    (API, "if (element.genre() != json::Genre::Objet)\n                return ErreurApi::Spots;",
     "if (false)\n                return ErreurApi::Spots;",
     T_API, "spots_qui_n_est_pas_une_liste_d_objets_est_refuse", "un élément de la liste qui n'est pas un objet est pris pour un spot sans identifiant"),
    (API, "if (n > REGISTRE_MAX)\n            return ErreurApi::Plein;",
     "if (n > REGISTRE_MAX)\n            return ErreurApi::Spots;",
     T_API, "douze_spots_passent_treize_sont_trop", "treize spots donnent « liste invalide » au lieu de « liste pleine »"),
    (API, "neuf.nombre = 0;\n        neuf.choisi = 0;\n        for (std::size_t i = 0; i < n; ++i) {",
     "neuf.choisi = 0;\n        for (std::size_t i = 0; i < n; ++i) {",
     T_API, "la_nouvelle_liste_remplace_l_ancienne", "la nouvelle liste s'ajoute à l'ancienne au lieu de la remplacer"),
    # --- mer_api.hpp : quelle erreur pour quel refus ---
    (API, "case ErreurRegistre::Doublon:\n                return ErreurApi::Doublon;",
     "case ErreurRegistre::Doublon:\n                return ErreurApi::Identifiant;",
     T_API, "deux_spots_de_meme_identifiant_sont_refuses", "un doublon est annoncé comme un identifiant invalide"),
    (API, "case ErreurRegistre::Nom:\n                return ErreurApi::Nom;",
     "case ErreurRegistre::Nom:\n                return ErreurApi::Identifiant;",
     T_API, "un_nom_absent_vide_trop_long_ou_abime_est_refuse", "un nom refusé par le registre est annoncé comme un identifiant invalide"),
    (API, "if (!lire_texte(element.champ(\"nom\"), nom, sizeof nom, texte_nom))\n                return ErreurApi::Nom;",
     "if (!lire_texte(element.champ(\"nom\"), nom, sizeof nom, texte_nom))\n                return ErreurApi::Identifiant;",
     T_API, "un_nom_absent_vide_trop_long_ou_abime_est_refuse", "un nom absent est annoncé comme un identifiant invalide"),
    # --- mer_api.hpp : le spot choisi ---
    (API, "if (p < neuf.nombre)\n                neuf.choisi = static_cast<std::uint8_t>(p);\n        }\n    }",
     "if (false)\n                neuf.choisi = static_cast<std::uint8_t>(p);\n        }\n    }",
     T_API, "sans_choisi_le_meme_spot_reste_choisi_s_il_est_toujours_la", "changer l'ordre de la liste change aussi le spot dont l'écran montre la mer"),
    (API, "if (!texte.empty())\n                return ErreurApi::Choix;",
     "if (false)\n                return ErreurApi::Choix;",
     T_API, "un_choix_hors_de_la_liste_ou_mal_forme_est_refuse", "un spot choisi est accepté sur une liste vide"),
    (API, "if (p >= neuf.nombre)\n                return ErreurApi::Choix;\n            neuf.choisi",
     "if (false)\n                return ErreurApi::Choix;\n            neuf.choisi",
     T_API, "un_choix_hors_de_la_liste_ou_mal_forme_est_refuse", "un spot choisi hors de la liste est accepté (le choix pointe hors de la liste)"),
    (API, "if (id_du_spot(r.spots[i]) == id)\n            return i;",
     "if (id_du_spot(r.spots[i]) != id)\n            return i;",
     T_API, "choisi_nomme_un_spot_de_la_liste_telle_qu_elle_sera", "le spot trouvé par son identifiant est le premier qui ne lui ressemble pas"),
    # --- mer_api.hpp : la plage de vent ---
    (API, "std::int64_t mini = neuf.vent_min_kn;", "std::int64_t mini = 0;",
     T_API, "un_seul_vent_envoye_est_juge_avec_celui_du_registre", "un maximum envoyé seul est jugé avec un minimum de zéro, pas avec celui du registre"),
    (API, "if (vent_min.existe() && !vent_min.en_entier(mini))\n            return ErreurApi::Vent;",
     "if (false)\n            return ErreurApi::Vent;",
     T_API, "la_plage_de_vent_est_en_noeuds_entiers_et_ordonnee", "un minimum de 12,5 nœuds est ignoré au lieu d'être refusé"),
    # --- mer_api.hpp : tout ou rien ---
    (API, "if (e != ErreurApi::Aucune)\n        neuf = actuel;",
     "if (false)\n        neuf = actuel;",
     T_API, "un_refus_n_applique_rien_meme_pour_les_champs_valides", "un refus laisse dans le registre à enregistrer ce qui avait déjà été appliqué"),
    (API, "neuf = actuel;\n    const ErreurApi e = detail::appliquer(corps, actuel, neuf);",
     "const ErreurApi e = detail::appliquer(corps, actuel, neuf);",
     T_API, "un_objet_vide_ne_change_rien", "un champ absent efface ce qui était enregistré au lieu de rester inchangé"),
    # --- api_reglages.hpp : les routes /api/sea ---
    (ROUTE, "if (est(rq, Methode::Put, \"/api/sea\"))", "if (est(rq, Methode::Put, \"/api/sea2\"))",
     T_ROUTE, "mer_ecrite_la_liste_entiere_puis_relue_telle_que_gardee", "la page ne peut plus écrire le registre (PUT introuvable)"),
    (ROUTE, "\"Le spot choisi n’est pas dans la liste.\", \"choisi\");",
     "\"Le spot choisi n’est pas dans la liste.\", \"spots\");",
     T_ROUTE, "mer_refusee_tout_ou_rien_avec_le_champ_fautif", "un choix refusé désigne le champ « spots » au lieu de « choisi »"),
    (ROUTE, "objet.champ(\"vent_min_kn\").existe() ? \"vent_min_kn\" : \"vent_max_kn\"",
     "objet.champ(\"vent_min_kn\").existe() ? \"vent_max_kn\" : \"vent_min_kn\"",
     T_ROUTE, "mer_refusee_tout_ou_rien_avec_le_champ_fautif", "une plage de vent refusée désigne l'autre champ"),
    (ROUTE, "return erreur(t, 422, \"trop_de_spots\"", "return erreur(t, 422, \"trop_de_spot\"",
     T_ROUTE, "mer_treize_spots_trop", "l'identifiant stable de « liste pleine » change"),
    (ROUTE, "if (!mer::egaux(*t.mer_neuf, *t.mer_actuel) && !hote_.mer_enregistrer(*t.mer_neuf))\n            return panne(t);",
     "if (!hote_.mer_enregistrer(*t.mer_neuf))\n            return panne(t);",
     T_ROUTE, "mer_inchangee_n_ecrit_rien", "un envoi qui ne change rien écrit quand même en mémoire permanente et réveille le client réseau"),
    (ROUTE, "if (!mer::egaux(*t.mer_neuf, *t.mer_actuel) && !hote_.mer_enregistrer(*t.mer_neuf))\n            return panne(t);",
     "if (!mer::egaux(*t.mer_neuf, *t.mer_actuel) && !hote_.mer_enregistrer(*t.mer_neuf))\n            return mer_lire(t);",
     T_ROUTE, "echec_d_ecriture_du_registre_mer_donne_500", "une écriture ratée répond 200 avec l'ancien registre, la page croit avoir enregistré"),
    (ROUTE, "if (t.mer_actuel == nullptr || !hote_.mer_lire(*t.mer_actuel))\n            return panne(t);",
     "if (t.mer_actuel == nullptr)\n            return panne(t);",
     T_ROUTE, "registre_mer_absent_donne_500_sans_rien_ecraser", "un registre que l'appareil n'a pas pu lire est montré comme vide"),
    (ROUTE, "if (!hote_.mer_lire(*t.mer_actuel))\n            return panne(t);\n        switch (mer::valider_mer(",
     "if (false)\n            return panne(t);\n        switch (mer::valider_mer(",
     T_ROUTE, "registre_mer_absent_donne_500_sans_rien_ecraser", "un envoi est jugé et enregistré contre un registre vide quand l'appareil n'a pas pu lire le sien"),
    (ROUTE, "e.champ_entier(\"spots_max\", static_cast<std::int64_t>(mer::REGISTRE_MAX));",
     "e.champ_entier(\"spots_max\", static_cast<std::int64_t>(mer::REGISTRE_MAX) - 1);",
     T_ROUTE, "mer_lue_vide_avec_ses_bornes", "la page affiche une limite de spots qui n'est pas celle de l'appareil"),
    (ROUTE, "e.champ_entier(\"vent_plafond_kn\", mer::VENT_PLAFOND_NOEUDS);",
     "e.champ_entier(\"vent_plafond_kn\", mer::VENT_PLAFOND_NOEUDS + 1);",
     T_ROUTE, "mer_lue_vide_avec_ses_bornes", "la page affiche un plafond de vent qui n'est pas celui de l'appareil"),
    (ROUTE, "e.champ(\"choisi\", choisi != nullptr ? mer::id_du_spot(*choisi) : std::string_view{});",
     "e.champ(\"choisi\", std::string_view{});",
     T_ROUTE, "mer_ecrite_la_liste_entiere_puis_relue_telle_que_gardee", "la réponse ne dit jamais quel spot est choisi"),
]
