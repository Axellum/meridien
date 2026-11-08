"""Mutations du noyau « roue, commande » (components/core/include/ecran/roue_commande.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_ROUE_GEOMETRIE : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement,
test qui doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou
« détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle : l'échelle de l'écran, le seuil de glissement, la demande faite à la géométrie, la repose de la roue
quand le second anneau ne tient pas, ce que fait un toucher. Mutations choisies à la main sur les règles de ce fichier, pas un balayage
automatique : elles disent que ces règles-là sont testées, pas que « tout » l'est.

Non retenu, car indétectable (mutant équivalent) : prendre `m.diametre_1` pour `m.diametre_2` au second anneau. Les deux valent 72
dans roue_geometrie.hpp (DIAMETRE_1 et DIAMETRE_2) : le résultat est identique sur le Tab5 comme sur la 7B. Si un jour ils diffèrent,
la mutation « le second anneau est touché avec le diamètre du premier » devra être ajoutée, avec un test qui les distingue.
"""

CMD = "test_roue_commande.cpp"

H = "roue_commande.hpp"

MUTATIONS_ROUE_COMMANDE = [
    # --- les réglages à trancher sur la carte : le test les fige, les changer est un choix, pas un accident ---
    (H, "inline constexpr Exigence EXIGENCE_A_L_OUVERTURE = Exigence::PremierAnneauSeul;",
     "inline constexpr Exigence EXIGENCE_A_L_OUVERTURE = Exigence::TousLesAnneaux;",
     CMD, "le_reglage_retenu_n_exige_que_le_premier_anneau", "l'ouverture exige le second anneau de chaque famille (le moyeu s'écarte du doigt)"),
    (H, "inline constexpr std::int32_t INACTIVITE_MS = 15000;", "inline constexpr std::int32_t INACTIVITE_MS = 5000;",
     CMD, "le_reglage_retenu_n_exige_que_le_premier_anneau", "la roue se ferme seule après 5 s au lieu de 15"),

    # --- 1. l'écran de la roue ---
    (H, "return Ecran{largeur, hauteur, sur_l < sur_h ? sur_l : sur_h};", "return Ecran{largeur, hauteur, sur_l > sur_h ? sur_l : sur_h};",
     CMD, "l_ecran_de_la_7b_est_a_l_echelle_0_8", "l'échelle prend le plus grand des deux rapports au lieu du plus petit"),
    (H, "const int ref_l = portrait ? TAB5_HAUTEUR : TAB5_LARGEUR;", "const int ref_l = portrait ? TAB5_LARGEUR : TAB5_HAUTEUR;",
     CMD, "le_portrait_prend_le_plus_petit_rapport_lui_aussi", "en portrait, la largeur est comparée à 1280 au lieu de 720"),
    (H, "if (largeur <= 0 || hauteur <= 0) {", "if (largeur <= 0 && hauteur <= 0) {",
     CMD, "un_ecran_sans_taille_garde_les_mesures_du_tab5", "un écran dont une seule dimension est nulle ou négative donne une échelle absurde"),
    (H, "constexpr int glissement(const Ecran &e) noexcept { return mettre_a_l_echelle(GLISSEMENT_PX, e.echelle_pm); }",
     "constexpr int glissement(const Ecran &e) noexcept { return GLISSEMENT_PX; }",
     CMD, "le_seuil_de_glissement_suit_l_echelle", "le seuil de glissement ne suit pas l'échelle de la 7B"),
    (H, "inline constexpr int GLISSEMENT_PX = 24;", "inline constexpr int GLISSEMENT_PX = 25;",
     CMD, "le_seuil_de_glissement_suit_l_echelle", "le seuil de glissement du Tab5 passe de 24 à 25 px"),
    (H, "return dx * dx + dy * dy > seuil * seuil;", "return dx * dx + dy * dy >= seuil * seuil;",
     CMD, "le_glissement_se_mesure_a_vol_d_oiseau", "un doigt à exactement le seuil est déjà un glissement"),
    (H, "return dx * dx + dy * dy > seuil * seuil;", "return dx * dx > seuil * seuil;",
     CMD, "le_glissement_se_mesure_a_vol_d_oiseau", "seul le déplacement horizontal compte pour un glissement"),

    # --- 2. la demande faite à la géométrie ---
    (H, "d.n = detail::limiter(n, 0, BOUTONS_MAX);", "d.n = detail::limiter(n, 0, CHOIX_MAX);",
     CMD, "la_demande_borne_le_nombre_de_boutons_et_accepte_rien", "la demande accepte 8 boutons au lieu de 6 au premier anneau"),
    (H, "d.mot_choix = mot_choix;", "d.mot_choix = 0;",
     CMD, "la_demande_exigeant_tous_les_anneaux", "la largeur des mots de choix n'est pas transmise à la géométrie"),
    (H, "d.choix[i] = exigence == Exigence::TousLesAnneaux ? detail::limiter(formes[i].choix, 0, CHOIX_MAX) : 0;",
     "d.choix[i] = exigence == Exigence::PremierAnneauSeul ? detail::limiter(formes[i].choix, 0, CHOIX_MAX) : 0;",
     CMD, "la_demande_sans_exiger_les_seconds_anneaux", "l'exigence est inversée : le premier anneau seul exige tous les seconds"),
    (H, "d.choix[i] = exigence == Exigence::TousLesAnneaux ? detail::limiter(formes[i].choix, 0, CHOIX_MAX) : 0;",
     "d.choix[i] = exigence == Exigence::TousLesAnneaux ? formes[i].choix : 0;",
     CMD, "la_demande_exigeant_tous_les_anneaux", "le nombre de choix d'une famille n'est plus borné aux 8 du second anneau"),
    (H, "d.mot[i] = formes[i].mot;", "d.mot[i] = 0;",
     CMD, "la_demande_sans_exiger_les_seconds_anneaux", "la largeur du mot de chaque bouton n'est pas transmise"),

    # --- le dépliement ---
    (H, "r.tient = anneau_complet(m, r.second, m.diametre_2);\n    if (r.tient || famille < 0",
     "r.tient = true;\n    if (r.tient || famille < 0",
     CMD, "dans_un_coin_la_roue_se_repose_pour_que_le_second_anneau_tienne", "le second anneau est tenu pour entier sans regarder : la roue n'est jamais reposée"),
    (H, "exigee.choix[famille] = detail::limiter(n, 0, CHOIX_MAX);", "exigee.choix[famille] = 0;",
     CMD, "dans_un_coin_la_roue_se_repose_pour_que_le_second_anneau_tienne", "la roue reposée n'exige pas le second anneau de la famille dépliée"),
    (H, "const Roue neuve = ouvrir(m, roue.appui, exigee);", "const Roue neuve = ouvrir(m, roue.moyeu, exigee);",
     CMD, "dans_un_coin_la_roue_se_repose_pour_que_le_second_anneau_tienne", "la roue reposée oublie le point d'appui d'origine et prend le moyeu pour doigt"),
    (H, "    r.reposee = true;\n    return r;", "    r.reposee = false;\n    return r;",
     CMD, "dans_un_coin_la_roue_se_repose_pour_que_le_second_anneau_tienne", "la roue reposée ne le dit pas"),
    (H, "if (neuve.moyeu == roue.moyeu) {\n        return r; // le moyeu ne bouge pas : rien ne tient mieux ailleurs\n    }",
     "",
     CMD, "si_rien_ne_tient_la_roue_ne_bouge_pas_et_le_dit", "une roue qui ne peut pas bouger se déclare quand même reposée"),

    # --- 3. ce que fait un toucher ---
    (H, "if (annule || a_glisse(appui, relache, seuil)) {", "if (a_glisse(appui, relache, seuil)) {",
     CMD, "un_appui_long_sur_la_roue_ne_fait_rien", "un appui long sur la roue est pris pour un toucher"),
    (H, "if (annule || a_glisse(appui, relache, seuil)) {", "if (annule) {",
     CMD, "un_glissement_ne_fait_rien", "un glissement est pris pour un toucher"),
    (H, "const bool second_ouvert = famille >= 0 && famille < roue.premier.n;", "const bool second_ouvert = famille >= 0;",
     CMD, "une_famille_hors_du_premier_anneau_n_ouvre_pas_de_second_anneau", "une famille hors du premier anneau ouvre un second anneau"),
    (H, "        if (a >= 0 || b >= 0) {\n            if (a == b) {\n                d.geste = Geste::ChoisirChoix;",
     "        if (a >= 0 && b >= 0) {\n            if (a == b) {\n                d.geste = Geste::ChoisirChoix;",
     CMD, "un_doigt_pose_sur_le_bord_d_un_bouton_et_leve_juste_dehors_ne_fait_rien", "un doigt posé sur un choix et levé juste dehors replie la roue"),
    (H, "            if (a == b) {\n                d.geste = Geste::ChoisirChoix;", "            if (a >= 0) {\n                d.geste = Geste::ChoisirChoix;",
     CMD, "un_doigt_pose_sur_un_bouton_et_leve_sur_un_autre_ne_fait_rien", "un doigt posé sur un choix et levé sur un autre choisit le premier"),
    (H, "                d.rang = a;\n                d.famille = famille;", "                d.rang = a;\n                d.famille = -1;",
     CMD, "toucher_un_choix_le_choisit", "le choix ne dit pas de quelle famille il vient"),
    (H, "        if (a != b) {\n            return d;\n        }\n        d.rang = a;",
     "        d.rang = a >= 0 ? a : b;\n        if (a < 0) {\n            return d;\n        }",
     CMD, "un_doigt_pose_sur_le_bord_d_un_bouton_et_leve_juste_dehors_ne_fait_rien", "un doigt posé sur un bouton et levé juste dehors le choisit"),
    (H, "        if (a != b) {\n            return d;\n        }\n        d.rang = a;",
     "        if (a < 0) {\n            return d;\n        }\n        d.rang = a;",
     CMD, "un_doigt_pose_sur_un_bouton_et_leve_sur_un_autre_ne_fait_rien", "un doigt posé sur un bouton et levé sur un autre choisit le premier"),
    (H, "if (est_famille != nullptr && est_famille[a]) {", "if (est_famille != nullptr && !est_famille[a]) {",
     CMD, "toucher_une_famille_la_deplie", "une famille se choisit comme une commande, une commande se déplie"),
    (H, "if (est_famille != nullptr && est_famille[a]) {", "if (est_famille[a]) {",
     CMD, "la_roue_sans_table_de_familles_n_a_que_des_commandes", "une roue sans table de familles lit une table qui n'existe pas"),
    (H, "d.geste = second_ouvert && famille == a ? Geste::Replier : Geste::Deplier;",
     "d.geste = second_ouvert && famille != a ? Geste::Replier : Geste::Deplier;",
     CMD, "toucher_la_famille_depliee_la_replie", "toucher la famille dépliée la déplie encore ; toucher une autre la replie"),
    (H, "d.geste = second_ouvert ? Geste::Replier : Geste::Fermer;", "d.geste = second_ouvert ? Geste::Fermer : Geste::Replier;",
     CMD, "toucher_hors_des_boutons_ferme_ou_replie", "un toucher hors des boutons ferme quand il devrait replier, et inversement"),
    (H, "const int a = bouton_sous(m, second, m.diametre_2, appui);", "const int a = bouton_sous(m, second, 0, appui);",
     CMD, "la_zone_de_toucher_du_second_anneau_est_celle_de_la_geometrie", "la zone du second anneau oublie le diamètre du bouton (il ne reste que la marge)"),
]
