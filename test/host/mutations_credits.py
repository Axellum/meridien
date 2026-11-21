"""Mutations du noyau « page Sources » (credits.hpp, ADR-0029), lues par lancer.py --mutations.

Même format que les autres fichiers de mutations : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement,
test qui doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Chaque mutation casse UNE règle : rien
n'est cité sans être montré, la météo est créditée au service qui l'a donnée, les licences sont dites.
"""

CRE = "test_credits.cpp"
C = "credits.hpp"

MUTATIONS_CREDITS = [
    (C, "    if (p.meteo) {\n        ajouter(Rubrique::Meteo", "    if (true) {\n        ajouter(Rubrique::Meteo",
     CRE, "pas_de_meteo_pas_de_credit_meteo", "la météo est créditée même quand aucune météo n'est montrée"),
    (C, "        return METEO_MET_NO;", "        return METEO_OPEN_METEO;",
     CRE, "la_meteo_est_creditee_au_service_qui_l_a_donnee", "la météo de MET Norway est créditée à Open-Meteo"),
    (C, "        return METEO_NWS;", "        return METEO_MET_NO;",
     CRE, "la_meteo_est_creditee_au_service_qui_l_a_donnee", "la météo du NWS porte la licence de MET Norway"),
    (C, "if (p.origine == meteo::fournisseurs::Famille::Nws)\n            ajouter(Rubrique::MeteoNws", "if (false)\n            ajouter(Rubrique::MeteoNws",
     CRE, "la_meteo_est_creditee_au_service_qui_l_a_donnee", "l'avertissement « ni approuvé ni parrainé » du NWS disparaît"),
    (C, "    if (p.alertes)\n        ajouter(Rubrique::Alertes", "    if (true)\n        ajouter(Rubrique::Alertes",
     CRE, "vigie_n_est_citee_que_si_les_alertes_sont_actives", "Vigie est citée alors que les alertes sont désactivées"),
    (C, "    if (p.normales) {", "    if (true) {",
     CRE, "les_normales_nomment_meteo_france_la_licence_et_la_date_d_edition", "les normales sont créditées sans qu'aucune ne soit montrée"),
    (C, "avec_date ? NORMALES : NORMALES_SANS_DATE, NORMALES_NOM,\n                avec_date ? p.edition : nullptr", "NORMALES, NORMALES_NOM,\n                p.edition",
     CRE, "les_normales_nomment_meteo_france_la_licence_et_la_date_d_edition", "la phrase demande une date d'édition qu'on n'a pas"),
    (C, "const bool avec_date = p.edition != nullptr && p.edition[0] != '\\0';", "const bool avec_date = p.edition != nullptr;",
     CRE, "les_normales_nomment_meteo_france_la_licence_et_la_date_d_edition", "une date vide est montrée comme une date"),
    (C, "inline constexpr const char *NORMALES_NOM = \"Météo-France\";", "inline constexpr const char *NORMALES_NOM = \"Vigie\";",
     CRE, "les_normales_nomment_meteo_france_la_licence_et_la_date_d_edition", "les normales ne nomment plus Météo-France"),
    (C, "fiches climatologiques éditées le {date} (Licence Ouverte 2.0), via Vigie\"", "fiches climatologiques éditées le {date}, via Vigie\"",
     CRE, "chaque_phrase_est_une_cle_de_la_table", "la phrase ne nomme plus la Licence Ouverte 2.0 (et n'est plus une clé de la table)"),
    (C, "        ajouter(Rubrique::Mer, nullptr, nullptr, nullptr);\n        ajouter(Rubrique::Spots, SPOTS, nullptr, nullptr);\n    }",
     "        ajouter(Rubrique::Mer, nullptr, nullptr, nullptr);\n    }",
     CRE, "la_mer_et_les_spots_vont_ensemble", "le crédit OpenStreetMap des spots disparaît"),
    (C, "    ajouter(Rubrique::Lieux, LIEUX, LIEUX_NOM, nullptr);", "    if (p.meteo)\n        ajouter(Rubrique::Lieux, LIEUX, LIEUX_NOM, nullptr);",
     CRE, "toujours_les_lieux_et_les_polices", "le crédit GeoNames disparaît quand aucune météo n'est là"),
    (C, "    ajouter(Rubrique::Polices, POLICES, nullptr, nullptr);\n    return l;", "    return l;",
     CRE, "toujours_les_lieux_et_les_polices", "les polices et icônes ne sont plus créditées"),
    (C, "inline constexpr std::size_t LIGNES_MAX = 8;", "inline constexpr std::size_t LIGNES_MAX = 7;",
     CRE, "l_ordre_est_celui_de_l_ecran", "la liste est trop courte pour toutes les mentions"),
    (C, "    if (p.alertes)\n        ajouter(Rubrique::Alertes, ALERTES, ALERTES_NOM, nullptr);\n    if (p.normales) {",
     "    if (p.normales) {",
     CRE, "vigie_n_est_citee_que_si_les_alertes_sont_actives", "Vigie n'est jamais citée"),
]
