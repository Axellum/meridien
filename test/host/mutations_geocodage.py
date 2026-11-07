"""Mutations du noyau « recherche de lieu » (geocodage.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_REGLAGES : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement,
test qui doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée »
ou « détectée par autre chose » fait échouer le lanceur.

Ce qui est visé : les règles qui décident quel lieu est proposé à l'utilisateur et lequel devient la ville de l'appareil
(nom, fuseau, coordonnées, altitude « inconnue » du service, 8 lieux au plus, mêmes règles que la page) et la machine d'état
d'une recherche (la dernière demande gagne, une réponse obsolète est jetée, une demande se termine une seule fois).
Non couvert : tout ce qui se passe hors du noyau (téléchargement, tâche, verrou TLS : main/geocodage.cpp).
"""

GEO = "geocodage.hpp"
T_GEO = "test_geocodage.cpp"

MUTATIONS_GEOCODAGE = [
    # --- lecture de la réponse du service ---
    (GEO, "metres >= ALTITUDE_MIN && metres <= ALTITUDE_MAX", "metres > ALTITUDE_MIN && metres <= ALTITUDE_MAX", T_GEO, "altitude_bornes",
     "-500 m (mer Morte) n'est plus une altitude plausible"),
    (GEO, "metres >= ALTITUDE_MIN && metres <= ALTITUDE_MAX", "metres >= ALTITUDE_MIN && metres < ALTITUDE_MAX", T_GEO, "altitude_bornes",
     "9000 m n'est plus une altitude plausible"),
    (GEO, "ALTITUDE_MIN = -500.0;", "ALTITUDE_MIN = -10000.0;", T_GEO, "altitude_bornes", "-9999 (« inconnue » pour Open-Meteo) devient une altitude"),
    (GEO, 'l.altitude_connue = v.champ("elevation").en_double(l.altitude) && altitude_plausible(l.altitude);',
     'l.altitude_connue = v.champ("elevation").en_double(l.altitude);', T_GEO, "altitude_inconnue_du_service_n_est_pas_transmise",
     "l'altitude -9999 d'Istanbul Old Town part vers la page"),
    (GEO, "if (l.nom[0] == '\\0' || l.fuseau[0] == '\\0' ||", "if (l.nom[0] == '\\0' ||", T_GEO, "lieux_invalides_ecartes",
     "un lieu sans fuseau est proposé (l'appareil ne saurait pas régler l'heure)"),
    (GEO, "!reglages::utf8_valide(l.nom) || !reglages::sans_controle(l.nom) ||", "!reglages::utf8_valide(l.nom) ||", T_GEO,
     "lieux_invalides_ecartes", "un nom avec un octet de contrôle est proposé"),
    (GEO, "!(lat >= -90.0 && lat <= 90.0)", "!(lat >= -90.0 && lat < 90.0)", T_GEO, "lieux_invalides_ecartes", "le pôle Nord est écarté"),
    (GEO, "|| !(lat >= -90.0 && lat <= 90.0) ||", "||", T_GEO, "lieux_invalides_ecartes", "un lieu de latitude 95 est proposé"),
    (GEO, "!(lon >= -180.0 && lon <= 180.0)", "!(lon > -180.0 && lon <= 180.0)", T_GEO, "lieux_invalides_ecartes",
     "la longitude -180 est écartée"),
    (GEO, "!(lon >= -180.0 && lon <= 180.0)) {\n        return false;", ") {\n        return false;", T_GEO, "lieux_invalides_ecartes",
     "un lieu de longitude 181 est proposé"),
    (GEO, "if (!reglages::utf8_valide(l.region) || !reglages::sans_controle(l.region))", "if (!reglages::utf8_valide(l.region))", T_GEO,
     "region_et_pays_invalides_laisses_vides", "une région avec un octet de contrôle est affichée"),
    (GEO, "if (!reglages::utf8_valide(l.pays) || !reglages::sans_controle(l.pays))", "if (!reglages::utf8_valide(l.pays))", T_GEO,
     "region_et_pays_invalides_laisses_vides", "un pays avec un octet de contrôle est affiché"),
    (GEO, "habitants >= 0", "habitants >= -9", T_GEO, "champs_facultatifs_absents", "une population négative est transmise"),
    (GEO, "i < total && n < max && n < LIEUX_MAX", "i < total && n < LIEUX_MAX", T_GEO, "au_plus_huit_lieux",
     "le tampon de l'appelant déborde (plus de lieux que sa taille)"),
    (GEO, "i < total && n < max && n < LIEUX_MAX", "i < total && n < max", T_GEO, "au_plus_huit_lieux", "plus de 8 lieux renvoyés"),
    # --- JSON de la page ---
    (GEO, 'e.champ("id", l.id_connu ? api::entier_en_texte(l.id, texte_id) : std::string_view{});',
     'e.champ("id", api::entier_en_texte(l.id, texte_id));', T_GEO, "champs_facultatifs_absents", "un lieu sans identifiant reçoit l'identifiant « 0 »"),
    (GEO, 'if (l.altitude_connue)\n            e.champ_decimal("altitude", l.altitude, 0);', 'e.champ_decimal("altitude", l.altitude, 0);', T_GEO,
     "altitude_inconnue_du_service_n_est_pas_transmise", "l'altitude est écrite même quand elle est inconnue"),
    (GEO, 'if (l.population_connue)\n            e.champ_entier("population", l.population);', 'e.champ_entier("population", l.population);', T_GEO,
     "champs_facultatifs_absents", "une population inconnue est écrite « 0 »"),
    (GEO, 'e.champ_decimal("latitude", l.latitude, 5);', 'e.champ_decimal("latitude", l.latitude, 2);', T_GEO,
     "json_de_la_page_relu_champ_par_champ", "la latitude perd ses décimales (1 km d'erreur)"),
    (GEO, 'e.brut(", ");', 'e.brut(",");', T_GEO, "quelques_valeurs_connues_en_clair", "le libellé perd l'espace après la virgule"),
    (GEO, "if (p.empty())\n            continue;", "if (false)\n            continue;", T_GEO, "region_et_pays_invalides_laisses_vides",
     "un libellé avec une région vide s'écrit « A, , Pays »"),
    # --- la ville choisie ---
    (GEO, "if (fuseaux::trouver(l.fuseau) == nullptr) {", "if (false) {", T_GEO, "vers_zone_refuse_dans_l_ordre_nom_coordonnees_fuseau",
     "un fuseau inconnu de l'appareil devient la ville"),
    (GEO, "if (!(l.latitude >= -90.0 && l.latitude <= 90.0) || !(l.longitude >= -180.0 && l.longitude <= 180.0)) {",
     "if (!(l.latitude >= -90.0 && l.latitude <= 90.0)) {", T_GEO, "vers_zone_refuse_dans_l_ordre_nom_coordonnees_fuseau",
     "une longitude hors plage devient la ville"),
    (GEO, "reglages::nombre_de_caracteres(nom) > reglages::NOM_MAX_CARACTERES", "reglages::nombre_de_caracteres(nom) >= reglages::NOM_MAX_CARACTERES",
     T_GEO, "vers_zone_nom_long_et_champs_facultatifs", "un nom de 80 caractères est refusé"),
    (GEO, "!reglages::utf8_valide(nom) || !reglages::sans_controle(nom) ||", "!reglages::utf8_valide(nom) ||", T_GEO,
     "vers_zone_refuse_dans_l_ordre_nom_coordonnees_fuseau", "un nom avec un octet de contrôle devient la ville"),
    (GEO, "const std::string_view nom = reglages::rogner(l.nom);", "const std::string_view nom = l.nom;", T_GEO,
     "vers_zone_refuse_dans_l_ordre_nom_coordonnees_fuseau", "un nom fait d'espaces devient la ville"),
    (GEO, "lettres = lettres && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));", "lettres = lettres && (c >= 'A' && c <= 'Z');", T_GEO,
     "vers_zone_nom_long_et_champs_facultatifs", "un code pays en minuscules est perdu"),
    (GEO, "    if (lettres)\n        reglages::copier_texte(cp,", "    if (true)\n        reglages::copier_texte(cp,", T_GEO,
     "vers_zone_nom_long_et_champs_facultatifs", "un code pays avec un chiffre est gardé"),
    # --- l'état d'une recherche ---
    (GEO, "        ++generation_;\n        nombre_ = 0;", "        nombre_ = 0;", T_GEO, "la_derniere_demande_gagne",
     "la réponse d'une ancienne recherche est montrée pour la nouvelle"),
    (GEO, "        nombre_ = 0;\n        a_traiter_ = false;\n        if (v.erreur", "        nombre_ = 0;\n        if (v.erreur", T_GEO,
     "texte_refuse_abandonne_la_recherche", "une demande refusée laisse l'ancienne demande en attente"),
    (GEO, "        nombre_ = 0;\n        a_traiter_ = false;", "        a_traiter_ = false;", T_GEO, "nouvelle_demande_efface_les_resultats",
     "les résultats de la recherche précédente restent affichés pendant la nouvelle"),
    (GEO, "etat_ = Etat::Terminee;\n            return v.erreur;", "etat_ = Etat::Inactive;\n            return v.erreur;", T_GEO,
     "texte_refuse_abandonne_la_recherche", "un texte refusé laisse l'écran sans réponse"),
    (GEO, "reglages::copier_texte(v.q, texte_, sizeof texte_);", "reglages::copier_texte(texte, texte_, sizeof texte_);", T_GEO,
     "recherche_cycle_normal", "le texte cherché garde ses espaces de début et de fin"),
    (GEO, "if (!a_traiter_)\n            return false;", "if (false)\n            return false;", T_GEO, "recherche_cycle_normal",
     "la tâche croit toujours avoir une demande à faire"),
    (GEO, "numero = generation_;\n        a_traiter_ = false;", "numero = generation_;", T_GEO, "recherche_cycle_normal",
     "une demande est prise deux fois"),
    (GEO, "if (numero != generation_ || etat_ != Etat::EnCours)", "if (etat_ != Etat::EnCours)", T_GEO, "la_derniere_demande_gagne",
     "la réponse d'une demande remplacée est acceptée"),
    (GEO, "if (numero != generation_ || etat_ != Etat::EnCours)", "if (numero != generation_)", T_GEO, "une_demande_ne_se_termine_qu_une_fois",
     "une demande finie peut être terminée une seconde fois"),
    (GEO, "etat_ = Etat::SansConnexion;\n            return true;", "etat_ = Etat::Terminee;\n            return true;", T_GEO,
     "recherche_sans_connexion", "une panne réseau est présentée comme « aucun résultat »"),
    (GEO, "const std::size_t garde = n < LIEUX_MAX ? n : LIEUX_MAX;", "const std::size_t garde = n;", T_GEO, "plus_de_huit_resultats_bornes",
     "plus de 8 résultats écrasent la mémoire de l'état"),
    (GEO, "const std::size_t n = nombre_ < max ? nombre_ : max;", "const std::size_t n = nombre_;", T_GEO, "recherche_cycle_normal",
     "la copie des résultats déborde le tampon de l'appelant"),
]
