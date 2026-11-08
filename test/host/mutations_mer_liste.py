"""Mutations du noyau « la liste des spots de Vigie lue par l'appareil et la recherche dedans »
(components/core/include/ecran/mer_liste.hpp, ADR-0026), lues par lancer.py --mutations.

Même format que MUTATIONS_MER_API : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement,
test qui doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou
« détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle : le repli des majuscules et des accents, la recherche dans les trois champs (et pas seulement au début),
la requête vide qui donne tout, le plafond de résultats, l'identifiant jamais invalide ni en double, le nom retombant dans l'ordre
`name`, `osm_name`, `place`, puis l'identifiant, la coupe à 32 caractères (nom et lieu) sans couper un caractère, un lieu abîmé qui
n'écarte pas le spot, la liste « tronquée », et ce que fait un toucher (ajouter au registre avec le bon nom). Mutations choisies à la
main, pas un balayage.

Non muté, et pourquoi :
  - les gardes de capacité de `replier` dans la boucle : la garde finale les rattrape, et les retirer écrit hors du tampon, ce que le
    compilateur refuse déjà (évaluation à la compilation) ;
  - le contrôle `Objet` et `lu.ok()` de `analyser` : `champ("spots")` d'une valeur absente ou qui n'est pas un objet n'est pas un
    tableau, donc la réponse est refusée de toute façon ;
  - `element.genre() != Objet` de `lire_spot` : `champ("id")` d'un élément qui n'est pas un objet est absent, donc l'élément est écarté
    de toute façon ; `lire_chaine` : `copier_texte` rend 0 pour une valeur qui n'est pas une chaîne, donc le texte est vide de toute
    façon ;
  - `l.nombre < SPOTS_MAX` dans `chercher` : défense contre une `Liste` mal formée, que `analyser` ne produit jamais ;
  - `break` après `tronquee = true` : sans lui la boucle continue et remet la même valeur (seul le temps change).
"""

L = "mer_liste.hpp"
T = "test_mer_liste.cpp"

REPLI = "le_repli_ramene_les_accents_et_les_majuscules_a_la_lettre_de_base"
ORDINAIRE = "une_reponse_ordinaire_donne_id_nom_et_lieu"
RETOMBE = "le_nom_retombe_sur_osm_name_puis_sur_le_lieu_puis_sur_l_identifiant"
ESPACES = "les_espaces_autour_des_textes_sont_retires"
NOM_LONG = "un_nom_trop_long_est_coupe_a_32_caracteres_sur_une_frontiere_de_caractere"
LIEU_LONG = "un_lieu_trop_long_est_coupe_a_32_caracteres_et_64_octets"
LIEU_ABIME = "un_lieu_abime_est_laisse_vide_sans_ecarter_le_spot"
IDS = "un_identifiant_invalide_ou_double_ecarte_le_spot_et_pas_les_autres"
ILLISIBLE = "une_reponse_qui_n_est_pas_une_liste_de_spots_est_illisible"
VIDE_LISTE = "une_liste_sans_spot_utilisable_est_vide"
SOIXANTE = "soixante_quatre_spots_passent_le_soixante_cinquieme_est_ignore"
CHERCHE = "la_recherche_ne_tient_compte_ni_des_majuscules_ni_des_accents"
VIDE = "une_recherche_vide_donne_tous_les_spots_dans_l_ordre"
ROGNE = "les_espaces_autour_de_la_recherche_sont_ignores"
MAX = "la_recherche_s_arrete_a_max_resultats"
TAMPON = "le_repli_refuse_un_tampon_trop_petit"
AJOUT = "ajouter_un_spot_de_la_liste_le_met_au_registre_avec_son_nom"
REQUETE = "la_requete_est_l_adresse_de_vigie_suivie_de_la_route_des_spots"
REFUS = "la_requete_refuse_une_adresse_vide_trop_longue_ou_un_tampon_trop_petit"
GARDEE = "la_liste_gardee_moins_de_dix_minutes_n_est_pas_retelechargee"
ECHEC = "apres_un_echec_on_attend_quelques_secondes_avant_de_reessayer"

MUTATIONS_MER_LISTE = [
    # --- replier : majuscules, accents, table ---
    (L, "if (c >= 'A' && c <= 'Z') {", "if (false) {",
     T, REPLI, "les majuscules ne sont plus ramenées en minuscules : « Évian » ne trouve plus « evian »"),
    (L, "} else if (c == 0xC3 && i + 1 < texte.size()) {", "} else if (false) {",
     T, REPLI, "les lettres accentuées du latin 1 (é, ç, ô…) ne sont plus repliées"),
    (L, "} else if ((c == 0xC4 || c == 0xC5) && i + 1 < texte.size()) {", "} else if (false) {",
     T, REPLI, "les lettres du latin étendu (ş, ğ, ł, ž, œ…) ne sont plus repliées"),
    (L, "detail::REPLI[(c == 0xC4 ? 0x40 : 0x80) + (d - 0x80)]", "detail::REPLI[(c == 0xC4 ? 0x80 : 0x40) + (d - 0x80)]",
     T, REPLI, "les deux demi-pages du latin étendu sont échangées (ş devient autre chose)"),
    (L, '"th", "ss", "a"', '"th", "s", "a"',
     T, REPLI, "« ß » est replié en « s » au lieu de « ss »"),
    (L, '"oe", "oe", "r"', '"o", "o", "r"',
     T, REPLI, "« œ » est replié en « o » au lieu de « oe »"),
    (L, "    if (n >= capacite)\n        return false;\n    sortie[n] = '\\0';",
     "    if (n > capacite)\n        return false;\n    sortie[n] = '\\0';",
     T, TAMPON, "un texte vide est « replié » dans un tampon de taille zéro, sans place pour le zéro final"),
    # --- contient_replie, chercher ---
    (L, "if (aiguille.empty())\n        return true;", "if (aiguille.empty())\n        return false;",
     T, VIDE, "une recherche vide ne donne plus aucun spot"),
    (L, ".find(aiguille) != std::string_view::npos;", ".find(aiguille) == 0;",
     T, CHERCHE, "la recherche ne trouve que le début du texte"),
    (L, "contient_replie(nom_du_spot(s), cherche) ||", "false ||",
     T, CHERCHE, "la recherche ne regarde plus le nom"),
    (L, "contient_replie(lieu_du_spot(s), cherche) ||", "false ||",
     T, CHERCHE, "la recherche ne regarde plus le lieu"),
    (L, "||\n            contient_replie(id_du_spot(s), cherche))", "||\n            false)",
     T, CHERCHE, "la recherche ne regarde plus l'identifiant"),
    (L, "requete = detail::rogner(requete);", "(void)0;",
     T, ROGNE, "les espaces autour de ce qui est tapé comptent dans la recherche"),
    (L, "i < n && trouves < max; ++i)", "i < n && trouves <= max; ++i)",
     T, MAX, "la recherche rend un résultat de plus que le plafond demandé"),
    # --- analyser : la réponse ---
    (L, "if (spots.genre() != json::Genre::Tableau)\n        return Issue::Illisible;",
     "if (false)\n        return Issue::Illisible;",
     T, ILLISIBLE, "une réponse sans liste « spots » est prise pour une liste vide au lieu d'être illisible"),
    (L, "sortie.tronquee = true;", "sortie.tronquee = false;",
     T, SOIXANTE, "une liste de plus de 64 spots ne dit pas qu'elle est tronquée"),
    (L, "return sortie.nombre == 0 ? Issue::Vide : Issue::Ok;", "return Issue::Ok;",
     T, VIDE_LISTE, "une réponse sans aucun spot utilisable est donnée pour réussie"),
    # --- lire_spot : l'identifiant ---
    (L, "!spot_valide(texte_id) || ", "",
     T, IDS, "un identifiant qui n'est pas un « slug » de Vigie (majuscule, espace) est gardé"),
    (L, " || deja_vu(deja, texte_id)", "",
     T, IDS, "deux spots de même identifiant sont gardés tous les deux"),
    # --- lire_spot : le nom ---
    (L, '{"name", "osm_name", "place"}', '{"osm_name", "name", "place"}',
     T, ORDINAIRE, "le nom OpenStreetMap passe avant le nom de Vigie"),
    (L, '{"name", "osm_name", "place"}', '{"name", "place", "place"}',
     T, RETOMBE, "le nom OpenStreetMap n'est plus essayé quand « name » manque"),
    (L, '{"name", "osm_name", "place"}', '{"name", "osm_name", "osm_name"}',
     T, RETOMBE, "la commune n'est plus essayée quand les deux noms manquent"),
    (L, "if (nom.empty())\n        nom = couper(texte_id,", "if (false)\n        nom = couper(texte_id,",
     T, RETOMBE, "un spot sans aucun nom est écarté au lieu de porter son identifiant"),
    (L, "candidat = couper(candidat, surf::SPOT_NOM_MAX_CARACTERES, NOM_OCTETS - 1);",
     "candidat = couper(candidat, 1000, NOM_OCTETS - 1);",
     T, NOM_LONG, "un nom de plus de 32 caractères n'est plus coupé (le registre le refuserait)"),
    # --- lire_spot : le lieu ---
    (L, "lieu = couper(lieu, surf::SPOT_NOM_MAX_CARACTERES, LIEU_OCTETS - 1);",
     "lieu = couper(lieu, 1000, LIEU_OCTETS - 1);",
     T, LIEU_LONG, "un lieu de plus de 32 caractères n'est plus coupé (il serait vidé par la règle des noms)"),
    (L, "lieu = couper(lieu, surf::SPOT_NOM_MAX_CARACTERES, LIEU_OCTETS - 1);",
     "lieu = couper(lieu, surf::SPOT_NOM_MAX_CARACTERES, 1000);",
     T, LIEU_LONG, "un lieu en caractères de 3 octets dépasse son tampon de 64 octets (il est vidé)"),
    (L, "        s.lieu[0] = '\\0';", "        (void)0;",
     T, LIEU_ABIME, "un lieu avec un caractère de contrôle est gardé tel quel"),
    # --- couper, rogner, taille_caractere ---
    (L, "if (octets + n > t.size() || octets + n > max_octets)\n            break;",
     "if (octets + n > t.size())\n            break;",
     T, LIEU_LONG, "la coupe ne tient plus compte du nombre d'octets, seulement des caractères"),
    (L, "while (!t.empty() && est_espace(t.front()))", "while (false && !t.empty() && est_espace(t.front()))",
     T, ESPACES, "les espaces au début d'un texte sont gardés"),
    (L, "while (!t.empty() && est_espace(t.back()))", "while (false && !t.empty() && est_espace(t.back()))",
     T, ESPACES, "les espaces à la fin d'un texte sont gardés"),
    (L, "if (c >= 0xF0 && c <= 0xF4)\n        return 4;", "if (c >= 0xF0 && c <= 0xF4)\n        return 3;",
     T, NOM_LONG, "un caractère sur 4 octets (émoji) est compté sur 3 : la coupe le déchire"),
    (L, "return c <= 0xEF ? 3 : 1;", "return c <= 0xEF ? 2 : 1;",
     T, LIEU_LONG, "un caractère sur 3 octets est compté sur 2 : la coupe le déchire"),
    (L, "if (c >= 0xC2)\n        return 2;", "if (c >= 0xC2)\n        return 3;",
     T, NOM_LONG, "un caractère sur 2 octets est compté sur 3 : la coupe le déchire"),
    # --- la requête ---
    (L, 'REQUETE_LISTE = "/v1/surf/spots";', 'REQUETE_LISTE = "/v1/surf/spot";',
     T, REQUETE, "la route demandée à Vigie n'existe pas"),
    (L, "adresse.empty() || adresse.size() > vigie::ADRESSE_MAX)\n        return 0;\n    vigie::detail::Sortie s{sortie, capacite};\n    s.texte(adresse);\n    s.texte(REQUETE_LISTE);",
     "adresse.empty() || adresse.size() >= vigie::ADRESSE_MAX)\n        return 0;\n    vigie::detail::Sortie s{sortie, capacite};\n    s.texte(adresse);\n    s.texte(REQUETE_LISTE);",
     T, REFUS, "l'adresse de 80 octets, la plus longue permise, est refusée"),
    (L, "    s.texte(REQUETE_LISTE);\n    if (s.trop)\n        return 0;", "    s.texte(REQUETE_LISTE);\n    if (false)\n        return 0;",
     T, REFUS, "une adresse coupée faute de place est envoyée à Vigie"),
    # --- le rythme ---
    (L, "age_liste_s >= 0 && age_liste_s < LISTE_VALABLE_S", "age_liste_s < LISTE_VALABLE_S",
     T, GARDEE, "une horloge qui recule fait garder une liste douteuse"),
    (L, "age_liste_s >= 0 && age_liste_s < LISTE_VALABLE_S", "age_liste_s >= 0 && age_liste_s <= LISTE_VALABLE_S",
     T, GARDEE, "une liste de dix minutes pile est encore gardée"),
    (L, "if (liste_gardee && age_liste_s", "if (age_liste_s",
     T, GARDEE, "sans liste gardée, l'âge d'une liste qui n'existe pas empêche de télécharger"),
    (L, "age_essai_s >= 0 && age_essai_s < PAUSE_APRES_ECHEC_S", "age_essai_s >= 0 && age_essai_s <= PAUSE_APRES_ECHEC_S",
     T, ECHEC, "la pause après un échec dure une seconde de trop"),
    (L, "age_essai_s >= 0 && age_essai_s < PAUSE_APRES_ECHEC_S", "age_essai_s < PAUSE_APRES_ECHEC_S",
     T, ECHEC, "une horloge qui recule empêche de réessayer après un échec"),
    (L, "if (dernier_echec && age_essai_s", "if (age_essai_s",
     T, ECHEC, "même après un succès, on attend avant de relire"),
    # --- ce que fait un toucher ---
    (L, "return indice_du_spot(r, id_du_spot(s)) < r.nombre;", "return indice_du_spot(r, id_du_spot(s)) <= r.nombre;",
     T, AJOUT, "un spot absent du registre est dit « déjà dans le registre »"),
    (L, "return ajouter(r, id_du_spot(s), nom_du_spot(s));", "return ajouter(r, nom_du_spot(s), id_du_spot(s));",
     T, AJOUT, "le spot est ajouté avec son nom pour identifiant"),
]
