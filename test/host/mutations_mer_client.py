"""Mutations du noyau « client de la mer » (components/core/include/ecran/mer_requete.hpp et mer_spots.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_MER : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test qui doit
échouer, nom de la fonction de test qui doit être citée (None : n'importe laquelle, y compris une erreur de compilation voulue), ce que la
mutation simule). Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle écrite dans un des deux en-têtes : une constante de l'adresse demandée à Vigie, les bornes de la plage de
vent, les chiffres écrits, le rythme des requêtes, le plafond de 12 spots, le refus d'un doublon ou d'un identifiant qui n'est pas un slug,
la façon dont le choix suit son spot quand on en retire un, la réparation d'un registre relu en NVS. Ce sont des mutations choisies à la main
sur les règles de ces deux fichiers, pas un balayage automatique : elles ne disent pas que « tout » est testé, seulement que ces règles-là le
sont. Les tâches réseau et la NVS (main/mer_client.cpp, main/reglages_mer.cpp) ne sont pas dans le noyau : elles ne sont pas mutées ici.
"""

TRQ = "test_mer_requete.cpp"
TSP = "test_mer_spots.cpp"

RQ = "mer_requete.hpp"
SP = "mer_spots.hpp"

# ---- tests de la requête et du rythme
DEFAUT = "la_requete_par_defaut_n_a_aucun_parametre_de_vent"
POSITION = "la_position_de_l_appareil_ne_part_pas"
AUTRE_PLAGE = "une_autre_plage_de_vent_part_en_noeuds_entiers"
UNE_BORNE = "une_seule_borne_differente_envoie_quand_meme_les_deux"
DEFAUTS = "les_valeurs_par_defaut_sont_celles_de_vigie"
BORNES = "la_plage_de_vent_suit_les_bornes_de_l_api"
ID_REFUSE = "un_identifiant_de_spot_refuse_donne_zero"
VENT_REFUSE = "une_plage_de_vent_refusee_donne_zero"
ADRESSE = "une_adresse_vide_ou_trop_longue_donne_zero"
TAMPON = "un_tampon_absent_ou_trop_petit_donne_zero"
PIRE_CAS = "le_pire_cas_tient_exactement_dans_le_tampon_maximal"
PERIODE = "la_periode_est_de_dix_minutes_et_respecte_la_politesse_des_alertes"
PREMIER = "le_premier_essai_part_entre_120_et_180_secondes"
APRES_REPONSE = "apres_une_reponse_la_prochaine_requete_est_dans_dix_minutes"
CHANGEMENT = "un_changement_de_spot_attend_un_court_delai_et_une_minute_apres_le_dernier_essai"

# ---- tests du registre
DOUZE = "le_registre_garde_douze_spots_comme_les_spots_de_surf"
PREMIER_CHOISI = "le_premier_spot_ajoute_devient_le_spot_choisi"
TREIZIEME = "le_treizieme_spot_est_refuse"
DOUBLON = "un_doublon_est_refuse_meme_avec_un_autre_nom"
SLUG = "un_identifiant_qui_n_est_pas_un_slug_est_refuse"
NOM = "un_nom_vide_trop_long_ou_mal_forme_est_refuse"
RETIRER_CHOISI = "retirer_le_spot_choisi_passe_au_suivant_ou_au_dernier"
RETIRER_AVANT = "retirer_un_spot_avant_le_choix_garde_le_meme_spot_choisi"
CASE_LIBEREE = "la_case_liberee_est_remise_a_zero"
HORS_LISTE = "retirer_ou_choisir_hors_de_la_liste_est_un_refus"
CHOISIR = "choisir_change_le_spot_demande_et_rien_d_autre"
VENT_REGLE = "la_plage_de_vent_se_regle_dans_les_bornes_de_l_api"
DEMANDE = "la_demande_est_le_spot_choisi_et_le_vent_et_rien_d_autre"
MEME_DEMANDE = "deux_demandes_sont_la_meme_si_spot_et_vent_sont_les_memes"
RELU_INTACT = "un_registre_intact_relu_de_la_nvs_ne_change_pas"
ENTREE_ABIMEE = "une_entree_abimee_est_ecartee_et_les_autres_sont_gardees"
DOUBLON_RELU = "un_doublon_relu_est_ecarte"
VENT_ABIME = "une_plage_de_vent_abimee_retombe_sur_les_valeurs_de_vigie"
A_CORRIGER = "un_registre_a_corriger_n_est_pas_valide"
SANS_ZERO = "un_texte_sans_zero_final_n_est_egal_a_rien"
EGAUX = "deux_registres_ne_sont_egaux_que_s_ils_disent_la_meme_chose"
HORS_CHOIX = "un_choix_hors_de_la_liste_ne_designe_aucun_spot"

MUTATIONS_MER_CLIENT = [
    # ---- la requête ----------------------------------------------------------------------------------------------------------------
    (
        RQ,
        'REQUETE_DEBUT = "/v1/embedded/frame?device_type=generic&spot=";',
        'REQUETE_DEBUT = "/v1/embedded/frame?device_type=generic&spots=";',
        TRQ,
        DEFAUT,
        "le paramètre du spot s'appelle autrement : Vigie répondrait 422 ou la mer d'un autre spot",
    ),
    (
        RQ,
        'REQUETE_VENT_MIN = "&wind_min_kn=";',
        'REQUETE_VENT_MIN = "&wind_min=";',
        TRQ,
        AUTRE_PLAGE,
        "la borne basse du vent part sous un autre nom : Vigie l'ignore et garde 12 nœuds",
    ),
    (
        RQ,
        'REQUETE_VENT_MAX = "&wind_max_kn=";',
        'REQUETE_VENT_MAX = "&wind_max=";',
        TRQ,
        AUTRE_PLAGE,
        "la borne haute du vent part sous un autre nom : Vigie l'ignore et garde 25 nœuds",
    ),
    (
        RQ,
        "VENT_MIN_DEFAUT_NOEUDS = 12;",
        "VENT_MIN_DEFAUT_NOEUDS = 13;",
        TRQ,
        DEFAUTS,
        "le défaut de la borne basse n'est plus celui de Vigie : la requête par défaut porterait des paramètres, hors du cache commun",
    ),
    (
        RQ,
        "VENT_MAX_DEFAUT_NOEUDS = 25;",
        "VENT_MAX_DEFAUT_NOEUDS = 26;",
        TRQ,
        DEFAUTS,
        "le défaut de la borne haute n'est plus celui de Vigie",
    ),
    (
        RQ,
        "VENT_PLAFOND_NOEUDS = 60;",
        "VENT_PLAFOND_NOEUDS = 61;",
        TRQ,
        BORNES,
        "un vent de 61 nœuds partirait : l'API répond 422",
    ),
    (
        RQ,
        "min_noeuds > 0 && min_noeuds < max_noeuds",
        "min_noeuds >= 0 && min_noeuds < max_noeuds",
        TRQ,
        BORNES,
        "une borne basse de 0 nœud serait acceptée : l'API répond 422",
    ),
    (
        RQ,
        "min_noeuds < max_noeuds && max_noeuds <= VENT_PLAFOND_NOEUDS",
        "min_noeuds <= max_noeuds && max_noeuds <= VENT_PLAFOND_NOEUDS",
        TRQ,
        BORNES,
        "min égal à max serait accepté : l'API répond 422",
    ),
    (
        RQ,
        "max_noeuds <= VENT_PLAFOND_NOEUDS",
        "max_noeuds < VENT_PLAFOND_NOEUDS",
        TRQ,
        BORNES,
        "le plafond de 60 nœuds serait refusé alors que l'API l'accepte",
    ),
    (
        RQ,
        "if (v >= 10)",
        "if (v > 10)",
        TRQ,
        AUTRE_PLAGE,
        "10 nœuds s'écrirait « 0 » : une autre plage que celle réglée partirait",
    ),
    (
        RQ,
        "static_cast<char>('0' + v % 10)",
        "static_cast<char>('0' + v % 9)",
        TRQ,
        AUTRE_PLAGE,
        "le dernier chiffre de la plage de vent est faux",
    ),
    (
        RQ,
        "detail::REQUETE_DEBUT.size() + SPOT_ID_MAX +",
        "detail::REQUETE_DEBUT.size() + SPOT_ID_MAX - 1 +",
        TRQ,
        PIRE_CAS,
        "le tampon maximal est trop petit d'un octet : le pire cas ne tient plus",
    ),
    (
        RQ,
        "SPOT_ID_MAX = 64;",
        "SPOT_ID_MAX = 63;",
        TRQ,
        PIRE_CAS,
        "un identifiant de 64 octets (le plus long d'un slug) ne tiendrait plus",
    ),
    (
        RQ,
        "adresse.size() > vigie::ADRESSE_MAX",
        "adresse.size() >= vigie::ADRESSE_MAX",
        TRQ,
        ADRESSE,
        "l'adresse de Vigie de la longueur maximale serait refusée",
    ),
    (
        RQ,
        "if (!spot_valide(spot) || !plage_de_vent_valide(vent_min_noeuds, vent_max_noeuds))",
        "if (!plage_de_vent_valide(vent_min_noeuds, vent_max_noeuds))",
        TRQ,
        ID_REFUSE,
        "un identifiant qui n'est pas un slug partirait vers Vigie (injection de paramètres dans l'adresse)",
    ),
    (
        RQ,
        "if (!spot_valide(spot) || !plage_de_vent_valide(vent_min_noeuds, vent_max_noeuds))",
        "if (!spot_valide(spot))",
        TRQ,
        VENT_REFUSE,
        "une plage de vent refusée partirait quand même : l'API répondrait 422 toutes les dix minutes",
    ),
    (
        RQ,
        "vent_min_noeuds != VENT_MIN_DEFAUT_NOEUDS || vent_max_noeuds != VENT_MAX_DEFAUT_NOEUDS",
        "vent_min_noeuds != VENT_MIN_DEFAUT_NOEUDS && vent_max_noeuds != VENT_MAX_DEFAUT_NOEUDS",
        TRQ,
        UNE_BORNE,
        "si une seule borne change, la plage n'est pas envoyée : Vigie garde ses défauts",
    ),
    (
        RQ,
        "if (s.trop)\n        return 0;\n    sortie[s.n] = '\\0';",
        "sortie[s.n] = '\\0';",
        TRQ,
        TAMPON,
        "un tampon trop petit n'est plus signalé : une requête coupée partirait",
    ),
    # ---- le rythme -----------------------------------------------------------------------------------------------------------------
    (
        RQ,
        "PERIODE_S = 10 * 60;",
        "PERIODE_S = 11 * 60;",
        TRQ,
        PERIODE,
        "la période n'est plus de dix minutes : la mer paraîtrait vieillie plus longtemps",
    ),
    (
        RQ,
        "PREMIER_ESSAI_S = 120;",
        "PREMIER_ESSAI_S = 90;",
        TRQ,
        PREMIER,
        "le premier essai part avant les 120 s : il marche sur les alertes et la météo au démarrage",
    ),
    (
        RQ,
        "60 * static_cast<std::int64_t>(alea) / 255",
        "60 * static_cast<std::int64_t>(alea) / 256",
        TRQ,
        PREMIER,
        "l'aléa n'atteint plus 180 s : l'étalement du premier essai est plus étroit que prévu",
    ),
    (
        RQ,
        "PERIODE_S, alea)",
        "PERIODE_S * 2, alea)",
        TRQ,
        APRES_REPONSE,
        "la mer n'est redemandée qu'après vingt minutes",
    ),
    (RQ, "ANTI_REBOND_S = 3;", "ANTI_REBOND_S = 4;", TRQ, CHANGEMENT, "le délai après un changement n'est plus de 3 s"),
    (
        RQ,
        "dernier_essai_s + vigie::ESPACEMENT_MIN_S",
        "dernier_essai_s",
        TRQ,
        CHANGEMENT,
        "un changement de spot ne respecte plus les 60 s entre deux requêtes",
    ),
    (
        RQ,
        "au_plus_tot > apres_rebond ? au_plus_tot : apres_rebond",
        "au_plus_tot < apres_rebond ? au_plus_tot : apres_rebond",
        TRQ,
        CHANGEMENT,
        "le plus petit des deux délais gagne au lieu du plus grand : une requête part trop tôt",
    ),
    # ---- le registre : plafond, ajout, doublon -------------------------------------------------------------------------------------
    (
        SP,
        "REGISTRE_MAX = 12;",
        "REGISTRE_MAX = 13;",
        TSP,
        DOUZE,
        "le registre garde 13 spots : plus que les spots de surf saisis à la main",
    ),
    (
        SP,
        "if (r.nombre >= REGISTRE_MAX)\n        return ErreurRegistre::Plein;",
        "if (r.nombre > REGISTRE_MAX)\n        return ErreurRegistre::Plein;",
        TSP,
        TREIZIEME,
        "un treizième spot est écrit hors du tableau",
    ),
    (
        SP,
        "    if (r.nombre == 0)\n        r.choisi = 0;\n    r.spots[r.nombre] = neuf;",
        "    r.spots[r.nombre] = neuf;",
        TSP,
        PREMIER_CHOISI,
        "le premier spot ajouté à un registre abîmé ne devient pas le spot choisi",
    ),
    (
        SP,
        "if (detail::meme_id(r.spots[i], neuf))",
        "if (false)",
        TSP,
        DOUBLON,
        "le même spot peut être ajouté deux fois",
    ),
    (
        SP,
        "if (!spot_valide(id) || !detail::copier(id, neuf.id, sizeof neuf.id))",
        "if (!detail::copier(id, neuf.id, sizeof neuf.id))",
        TSP,
        SLUG,
        "un identifiant qui n'est pas un slug entre dans le registre",
    ),
    (
        SP,
        "if (!detail::copier(nom, neuf.nom, sizeof neuf.nom) || !surf::nom_de_spot_valide(neuf.nom, sizeof neuf.nom))",
        "if (!detail::copier(nom, neuf.nom, sizeof neuf.nom))",
        TSP,
        NOM,
        "un nom vide ou en UTF-8 abîmé entre dans le registre",
    ),
    (
        SP,
        "if (c == '\\0')\n            return false;",
        "if (false)\n            return false;",
        TSP,
        NOM,
        "un nom avec un octet nul au milieu est accepté et coupé en silence",
    ),
    # ---- le registre : retirer, choisir, vent ----------------------------------------------------------------------------------------
    (
        SP,
        "if (r.nombre > REGISTRE_MAX || indice >= r.nombre)\n        return ErreurRegistre::Indice;\n    for (std::size_t i = indice;",
        "if (r.nombre > REGISTRE_MAX || indice > r.nombre)\n        return ErreurRegistre::Indice;\n    for (std::size_t i = indice;",
        TSP,
        HORS_LISTE,
        "retirer la case juste après le dernier spot est accepté",
    ),
    (
        SP,
        "if (r.nombre > REGISTRE_MAX || indice >= r.nombre)\n        return ErreurRegistre::Indice;\n    r.choisi = static_cast",
        "if (r.nombre > REGISTRE_MAX || indice > r.nombre)\n        return ErreurRegistre::Indice;\n    r.choisi = static_cast",
        TSP,
        HORS_LISTE,
        "choisir la case juste après le dernier spot est accepté : plus aucun spot choisi",
    ),
    (
        SP,
        "for (std::size_t i = indice; i + 1 < r.nombre; ++i)",
        "for (std::size_t i = indice; i + 2 < r.nombre; ++i)",
        TSP,
        None,
        "le dernier spot n'est pas décalé quand on en retire un avant lui",
    ),
    (
        SP,
        "r.spots[r.nombre - 1] = SpotMer{};",
        "r.spots[r.nombre - 1] = r.spots[r.nombre - 1];",
        TSP,
        CASE_LIBEREE,
        "la case libérée garde l'ancien spot : deux registres égaux ne le sont plus octet pour octet",
    ),
    (
        SP,
        "} else if (indice < r.choisi) {",
        "} else if (indice <= r.choisi) {",
        TSP,
        RETIRER_CHOISI,
        "retirer le spot choisi décale le choix au lieu de passer au suivant",
    ),
    (
        SP,
        "} else if (indice < r.choisi) {",
        "} else if (indice < r.choisi && false) {",
        TSP,
        RETIRER_AVANT,
        "retirer un spot avant le choix change le spot demandé",
    ),
    (
        SP,
        "} else if (r.choisi >= r.nombre) {",
        "} else if (r.choisi > r.nombre) {",
        TSP,
        RETIRER_CHOISI,
        "retirer le dernier spot quand il est choisi laisse le choix hors de la liste",
    ),
    (
        SP,
        "r.choisi = static_cast<std::uint8_t>(indice);",
        "r.choisi = static_cast<std::uint8_t>(indice + 1);",
        TSP,
        CHOISIR,
        "choisir un spot choisit son voisin",
    ),
    (
        SP,
        "if (!plage_de_vent_valide(min_noeuds, max_noeuds))\n        return ErreurRegistre::Vent;",
        "if (false)\n        return ErreurRegistre::Vent;",
        TSP,
        VENT_REGLE,
        "une plage de vent refusée par l'API est enregistrée",
    ),
    (
        SP,
        "r.vent_min_kn = static_cast<std::int16_t>(min_noeuds);",
        "r.vent_min_kn = static_cast<std::int16_t>(max_noeuds);",
        TSP,
        VENT_REGLE,
        "la borne basse prend la valeur de la borne haute",
    ),
    (
        SP,
        "r.vent_max_kn = static_cast<std::int16_t>(max_noeuds);",
        "r.vent_max_kn = static_cast<std::int16_t>(min_noeuds);",
        TSP,
        VENT_REGLE,
        "la borne haute prend la valeur de la borne basse",
    ),
    # ---- le spot choisi, l'égalité, la demande -------------------------------------------------------------------------------------
    (
        SP,
        "r.nombre <= REGISTRE_MAX && r.choisi < r.nombre",
        "r.nombre <= REGISTRE_MAX && r.choisi <= r.nombre",
        TSP,
        HORS_CHOIX,
        "un choix juste après le dernier spot lit une case hors de la liste",
    ),
    (
        SP,
        "r.nombre <= REGISTRE_MAX && r.choisi < r.nombre",
        "r.choisi < r.nombre",
        TSP,
        HORS_CHOIX,
        "un registre qui annonce plus de 12 spots désigne un spot hors du tableau",
    ),
    (SP, "a.nombre != b.nombre || ", "", TSP, EGAUX, "deux registres de longueurs différentes seraient égaux"),
    (SP, "a.choisi != b.choisi || ", "", TSP, EGAUX, "deux registres au choix différent seraient égaux"),
    (
        SP,
        "a.vent_min_kn != b.vent_min_kn ||",
        "false ||",
        TSP,
        EGAUX,
        "deux registres à la borne basse différente seraient égaux",
    ),
    (
        SP,
        "a.vent_max_kn != b.vent_max_kn)",
        "false)",
        TSP,
        EGAUX,
        "deux registres à la borne haute différente seraient égaux",
    ),
    (
        SP,
        "!detail::memes_textes(a.spots[i].nom, sizeof a.spots[i].nom, b.spots[i].nom, sizeof b.spots[i].nom))",
        "false)",
        TSP,
        EGAUX,
        "deux registres aux noms différents seraient égaux",
    ),
    (
        SP,
        "if (!detail::meme_id(a.spots[i], b.spots[i]) ||",
        "if (false ||",
        TSP,
        EGAUX,
        "deux registres aux identifiants différents seraient égaux",
    ),
    (
        SP,
        "if (la == cap_a || lb == cap_b || la != lb)",
        "if (la != lb)",
        TSP,
        SANS_ZERO,
        "deux textes sans zéro final et de même longueur sont jugés égaux",
    ),
    (
        SP,
        "d.vent_min_kn = r.vent_min_kn;",
        "d.vent_min_kn = r.vent_max_kn;",
        TSP,
        DEMANDE,
        "la demande porte la borne haute à la place de la borne basse",
    ),
    (
        SP,
        "if (!a.presente)\n        return true;",
        "if (!a.presente)\n        return false;",
        TSP,
        MEME_DEMANDE,
        "deux demandes vides ne sont plus jugées pareilles : la mer en mémoire serait oubliée sans raison",
    ),
    (
        SP,
        "if (a.presente != b.presente)\n        return false;",
        "if (false)\n        return false;",
        TSP,
        MEME_DEMANDE,
        "une demande vide et une demande réelle sont jugées pareilles",
    ),
    (
        SP,
        "a.vent_min_kn == b.vent_min_kn &&",
        "true &&",
        TSP,
        MEME_DEMANDE,
        "un changement de la borne basse ne fait pas oublier la mer en mémoire",
    ),
    (
        SP,
        "a.vent_max_kn == b.vent_max_kn;",
        "true;",
        TSP,
        MEME_DEMANDE,
        "un changement de la borne haute ne fait pas oublier la mer en mémoire",
    ),
    # ---- la NVS : réparer sans tout perdre -----------------------------------------------------------------------------------------
    (
        SP,
        "brut.nombre < REGISTRE_MAX ? brut.nombre : REGISTRE_MAX",
        "brut.nombre",
        TSP,
        None,
        "plus de douze entrées annoncées : la lecture sort du tableau",
    ),
    (
        SP,
        "if (!detail::spot_utilisable(s))\n            continue;",
        "if (false)\n            continue;",
        TSP,
        ENTREE_ABIMEE,
        "une entrée abîmée relue de la NVS reste dans le registre",
    ),
    (
        SP,
        "if (doublon)\n            continue;",
        "if (false)\n            continue;",
        TSP,
        DOUBLON_RELU,
        "un doublon relu de la NVS reste dans le registre",
    ),
    (
        SP,
        "if (i == brut.choisi)",
        "if (i == 0)",
        TSP,
        None,
        "après une réparation le choix retombe toujours sur le premier spot, même quand son spot a survécu",
    ),
    (
        SP,
        "if (plage_de_vent_valide(brut.vent_min_kn, brut.vent_max_kn)) {",
        "if (true) {",
        TSP,
        VENT_ABIME,
        "une plage de vent abîmée relue de la NVS est gardée : l'API répondrait 422",
    ),
    (
        SP,
        "surf::nom_de_spot_valide(s.nom, sizeof s.nom);",
        "true;",
        TSP,
        ENTREE_ABIMEE,
        "un nom abîmé relu de la NVS est jugé utilisable",
    ),
    (
        SP,
        "if (r.nombre > REGISTRE_MAX || r.choisi >= (r.nombre > 0 ? r.nombre : 1))",
        "if (r.choisi >= (r.nombre > 0 ? r.nombre : 1))",
        TSP,
        None,
        "un registre de plus de 12 entrées est jugé valide",
    ),
    (
        SP,
        "r.choisi >= (r.nombre > 0 ? r.nombre : 1)",
        "r.choisi > (r.nombre > 0 ? r.nombre : 1)",
        TSP,
        A_CORRIGER,
        "un choix juste après le dernier spot est jugé valide",
    ),
    (
        SP,
        "if (!plage_de_vent_valide(r.vent_min_kn, r.vent_max_kn))\n        return false;",
        "if (false)\n        return false;",
        TSP,
        A_CORRIGER,
        "un registre à plage de vent refusée est jugé valide et serait enregistré",
    ),
    (
        SP,
        "if (!detail::spot_utilisable(r.spots[i]))\n            return false;",
        "if (false)\n            return false;",
        TSP,
        A_CORRIGER,
        "un registre à entrée abîmée est jugé valide",
    ),
    (
        SP,
        "if (detail::meme_id(r.spots[j], r.spots[i]))\n                return false;",
        "if (false)\n                return false;",
        TSP,
        A_CORRIGER,
        "un registre à doublon est jugé valide",
    ),
]
