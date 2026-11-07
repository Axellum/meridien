"""Mutations de la table des sources météo (fournisseurs_meteo.hpp) et de son lien avec le lecteur du firmware, lues par
lancer.py --mutations.

Même format que MUTATIONS_METEO_APPAREIL : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —,
remplacement, test qui doit échouer, nom de la fonction de test qui doit être citée (None : n'importe quelle erreur de
compilation, par exemple le static_assert de la table), ce que la mutation simule). Une mutation « non détectée » ou « détectée
par autre chose » fait échouer le lanceur.

Écartées volontairement (mutations équivalentes : aucun comportement observable ne change) : `if (i >= NOMBRE) return false;`
de propose_pour (l'indice NOMBRE n'est jamais dans la liste), et les conditions de table_coherente() qu'aucune table réelle ne
viole (elles sont éprouvées en cassant la TABLE elle-même, plus bas).
"""

FOURNISSEURS = "test_fournisseurs_meteo.cpp"
F = "fournisseurs_meteo.hpp"
LECTEUR_ET_TABLE = "le_lecteur_et_la_table_sont_d_accord_sur_chaque_reponse_reelle"
LIMITES_DE_LA_TABLE = "chaque_reponse_reelle_a_une_entree_et_les_limites_de_la_table"
LISTE_PAR_PAYS = "liste_proposee_pour_chaque_pays"
CODE_PAYS = "code_pays_reconnu_a_la_lettre_pres"
TOLERANCE = "tolerer_une_limite_fait_apparaitre_les_sources_qui_n_ont_que_celle_la"
SOURCE_VALABLE = "source_valable_pour_garde_la_source_ou_remet_automatique"

MUTATIONS_FOURNISSEURS = [
    # --- ce que la table propose face au lecteur ---
    (F, "return (f.limites & static_cast<std::uint8_t>(~tolere)) == 0;", "return (f.limites & static_cast<std::uint8_t>(~tolere)) != 0;",
     FOURNISSEURS, LECTEUR_ET_TABLE, "une source est proposée exactement quand le lecteur la refuse"),
    (F, "inline constexpr std::uint8_t LECTEUR_TOLERE = limite::SANS_ENSOLEILLEMENT;", "inline constexpr std::uint8_t LECTEUR_TOLERE = 0;",
     FOURNISSEURS, TOLERANCE, "la table croit que le lecteur ne tolère plus l'ensoleillement manquant"),
    (F, "inline constexpr std::uint8_t LECTEUR_TOLERE = limite::SANS_ENSOLEILLEMENT;",
     "inline constexpr std::uint8_t LECTEUR_TOLERE = limite::SANS_ENSOLEILLEMENT | limite::SANS_UV;",
     FOURNISSEURS, LECTEUR_ET_TABLE, "la table propose les sources sans UV alors que le lecteur les refuse"),
    ("meteo_openmeteo.hpp", 'j.uv_max_dixiemes = l.fixe16(cols[6].tableau.element(i), "daily.uv_index_max", 1);',
     'j.uv_max_dixiemes = cols[6].tableau.element(i).est_nul() ? std::int16_t{0} : l.fixe16(cols[6].tableau.element(i), "daily.uv_index_max", 1);',
     FOURNISSEURS, LECTEUR_ET_TABLE, "le lecteur accepte un indice UV vide sans que la table le sache"),
    # --- la liste proposée pour un pays ---
    (F, "f.pays.empty() != (passe == 1)", "f.pays.empty() == (passe == 1)",
     FOURNISSEURS, LISTE_PAR_PAYS, "les sources mondiales passent avant les nationales"),
    (F, "|| !proposable(f, tolere) ||", "|| !proposable(f) ||",
     FOURNISSEURS, TOLERANCE, "la tolérance passée à pour_pays est ignorée"),
    (F, " || !concerne_le_pays(f, code_pays))", ")",
     FOURNISSEURS, LISTE_PAR_PAYS, "toutes les sources nationales sont proposées dans tous les pays"),
    (F, "l.indices[l.n++] = INDICE_AUTO;", "",
     FOURNISSEURS, LISTE_PAR_PAYS, "« Automatique » n'est plus en tête de la liste"),
    (F, "return f.pays.empty() || detail::code_dans(f.pays, code_pays);", "return detail::code_dans(f.pays, code_pays);",
     FOURNISSEURS, LISTE_PAR_PAYS, "une source mondiale n'est plus d'aucun pays"),
    (F, "constexpr Liste pour_pays(std::string_view code_pays, std::uint8_t tolere = LECTEUR_TOLERE) noexcept {",
     "constexpr Liste pour_pays(std::string_view code_pays, std::uint8_t tolere = 0xFF) noexcept {",
     FOURNISSEURS, LISTE_PAR_PAYS, "par défaut, la liste suppose que le lecteur tolère tout"),
    (F, "if (l.indices[k] == i)", "if (l.indices[k] != i)",
     FOURNISSEURS, "propose_pour_dit_si_la_source_convient_a_la_ville", "propose_pour dit l'inverse"),
    # --- la source gardée quand la ville change ---
    (F, "return propose_pour(id, code_pays, tolere) ? TABLE[indice_de(id)].id : TABLE[INDICE_AUTO].id;",
     "return propose_pour(id, code_pays) ? TABLE[indice_de(id)].id : TABLE[INDICE_AUTO].id;",
     FOURNISSEURS, SOURCE_VALABLE, "la tolérance passée à source_valable_pour est ignorée"),
    (F, "return propose_pour(id, code_pays, tolere) ? TABLE[indice_de(id)].id : TABLE[INDICE_AUTO].id;",
     "return propose_pour(id, code_pays, tolere) ? TABLE[indice_de(id)].id : TABLE[1].id;",
     FOURNISSEURS, SOURCE_VALABLE, "une source qui ne convient plus est remplacée par Météo-France au lieu de « Automatique »"),
    # --- le code de pays ---
    (F, "    if (code.size() != 2)\n        return false;", "    if (code.size() < 2)\n        return false;",
     FOURNISSEURS, CODE_PAYS, "un code de trois lettres est pris pour son début"),
    (F, "if (debut && fin && liste[i] == a && liste[i + 1] == b)", "if (fin && liste[i] == a && liste[i + 1] == b)",
     FOURNISSEURS, CODE_PAYS, "un code est reconnu au milieu d'un mot de la liste"),
    (F, "if (debut && fin && liste[i] == a && liste[i + 1] == b)", "if (debut && liste[i] == a && liste[i + 1] == b)",
     FOURNISSEURS, CODE_PAYS, "un code est reconnu au début d'un mot plus long"),
    (F, "constexpr char majuscule(char c) noexcept { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }",
     "constexpr char majuscule(char c) noexcept { return c; }",
     FOURNISSEURS, CODE_PAYS, "la casse du code de pays compte"),
    (F, "if (a < 'A' || a > 'Z' || b < 'A' || b > 'Z')", "if (b < 'A' || b > 'Z')",
     FOURNISSEURS, CODE_PAYS, "un chiffre est accepté en première lettre d'un code"),
    (F, "if (a < 'A' || a > 'Z' || b < 'A' || b > 'Z')", "if (a < 'A' || a > 'Z')",
     FOURNISSEURS, CODE_PAYS, "un chiffre est accepté en seconde lettre d'un code"),
    (F, "if (pays.size() % 3 != 2)", "if (pays.size() % 3 != 1)",
     FOURNISSEURS, CODE_PAYS, "une liste de pays bien écrite est refusée"),
    (F, "if (separateur ? c != ' ' : (c < 'A' || c > 'Z'))", "if (separateur ? false : (c < 'A' || c > 'Z'))",
     FOURNISSEURS, CODE_PAYS, "n'importe quel séparateur est accepté entre deux codes"),
    # --- la recherche ---
    (F, "return f != nullptr ? f->modeles : std::string_view{};", "return f->modeles;",
     FOURNISSEURS, "automatique_ne_demande_aucun_modele", "un identifiant inconnu fait déréférencer un pointeur nul"),
    (F, "return &TABLE[i];", "return &TABLE[0];",
     FOURNISSEURS, LIMITES_DE_LA_TABLE, "trouver rend toujours « Automatique »"),
    (F, "            return i;\n", "            return 0;\n",
     FOURNISSEURS, "ordre_et_identifiants_figes", "indice_de rend toujours la première position"),
    # --- les données de la table ---
    (F, '"NO SE FI DK IS EE",    0}', '"NO SE FI DK IS EE",    limite::SANS_UV}',
     FOURNISSEURS, LIMITES_DE_LA_TABLE, "la table déclare une limite que la réponse réelle n'a pas"),
    (F, '"DE",                   limite::SANS_UV},', '"DE",                   0},',
     FOURNISSEURS, LIMITES_DE_LA_TABLE, "la table oublie la limite d'UV de DWD"),
    (F, '{"dwd",', '{"dwdx",',
     FOURNISSEURS, "ordre_et_identifiants_figes", "un identifiant de la table change"),
    (F, '"gfs_seamless",', '"ncep_gfs013",',
     FOURNISSEURS, "seuls_les_modeles_verifies_sont_dans_la_table", "un modèle jamais capturé entre dans la table"),
    (F, '"icon_seamless",', '"icon_global",',
     FOURNISSEURS, "les_modeles_ecartes_ne_sont_pas_dans_la_table", "un modèle proscrit (icon_global) entre dans la table"),
    (F, '"NO SE FI DK IS EE",    0}', '"NO SE FI DK IS EE FR", 0}',
     FOURNISSEURS, "chaque_pays_annonce_par_la_table_a_sa_reponse_reelle", "la table annonce un pays jamais capturé (MET Norway en France)"),
    (F, '"NL BE LU",             0}', '"NL BE LU DE",          0}',
     FOURNISSEURS, "chaque_pays_annonce_par_la_table_a_sa_reponse_reelle", "la table annonce un pays jamais capturé (KNMI en Allemagne)"),
    (F, '"NO SE FI DK IS EE",    0}', '"",                     0}',
     FOURNISSEURS, LISTE_PAR_PAYS, "MET Norway devient une source mondiale"),
    (F, '{"auto",        "Automatique",              true,', '{"auto",        "Automatique",              false,',
     FOURNISSEURS, "automatique_ne_demande_aucun_modele", "« Automatique » n'est plus traduit"),
    # --- la table casse, la compilation doit refuser (static_assert de fournisseurs_meteo.hpp) ---
    (F, '{"dwd",', '{"meteofrance",',
     FOURNISSEURS, None, "deux entrées ont le même identifiant"),
    (F, '"icon_seamless",', '"icon_seamless_beaucoup_trop_long",',
     FOURNISSEURS, None, "un modèle dépasse la borne de l'adresse"),
    (F, '"NO SE FI DK IS EE",', '"NO,SE FI DK IS EE",',
     FOURNISSEURS, None, "une liste de pays est mal écrite"),
    (F, '"gfs_seamless",', '"GFS_seamless",',
     FOURNISSEURS, None, "un modèle contient des majuscules"),
    (F, 'TABLE[0].id != "auto"', 'TABLE[0].id != "automatique"',
     FOURNISSEURS, None, "la première entrée n'est plus « auto »"),
]
