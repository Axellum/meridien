"""Mutations du noyau « traduction » (i18n.hpp et la table i18n_donnees.inc), lues par lancer.py --mutations.

Même format que MUTATIONS dans lancer.py : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —,
remplacement, test qui doit échouer, nom de la fonction de test qui doit être citée (None : n'importe laquelle), ce
que la mutation simule). Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Une mutation qui ferait lire hors tableau (indice -1 pour le français) n'est pas retenue : l'erreur de compilation
n'y citerait pas le nom du test, donc on ne saurait pas lequel l'a vue.
Non couvert par ces mutations : la langue courante (`definir_langue`, un std::atomic) ; elle n'est vérifiée qu'à
l'exécution par test_i18n.cpp, et une mutation ne se voit qu'à la compilation.
"""

I18N = "test_i18n.cpp"
HPP = "i18n.hpp"
TABLE = "i18n_donnees.inc"

MUTATIONS_I18N = [
    # --- i18n.hpp : langues et indices ---
    (HPP, "{ Fr = 0, En = 1, De = 2, Nl = 3,", "{ Fr = 0, En = 1, De = 3, Nl = 2,", I18N, "ordre_et_indices_des_langues_figes",
     "néerlandais et allemand échangent leur indice (l'indice enregistré en NVS changerait de langue)"),
    (HPP, '{"fr", "en", "de", "nl", "es", "it", "tr"}', '{"fr", "en", "nl", "de", "es", "it", "tr"}', I18N,
     "ordre_et_indices_des_langues_figes", "les codes de langue sont dans un autre ordre"),
    (HPP, '"Español",  "Italiano", "Türkçe"}', '"Español",  "Italiano", "Turkce"}', I18N,
     "noms_des_langues_dans_leur_propre_langue", "le turc perd ses accents dans son nom"),
    (HPP, "return (i >= 0 && i < static_cast<int>(NOMBRE_LANGUES)) ?", "return (i >= 0 && i <= static_cast<int>(NOMBRE_LANGUES)) ?",
     I18N, "indice_lu_en_nvs_hors_limites_donne_le_defaut", "l'indice 7 (hors tableau) est accepté"),
    (HPP, "return (i >= 0 && i < static_cast<int>(NOMBRE_LANGUES)) ?", "return (i < static_cast<int>(NOMBRE_LANGUES)) ?", I18N,
     "indice_lu_en_nvs_hors_limites_donne_le_defaut", "un indice négatif est accepté"),
    (HPP, "if (CODES[i] == c)\n            return static_cast<Langue>(i);", "if (CODES[i].substr(0, 1) == c.substr(0, 1))\n            return static_cast<Langue>(i);",
     I18N, "codes_de_langue_reconnus_ou_refuses", "« deu » est pris pour « de » (seule la première lettre compte)"),
    # --- i18n.hpp : recherche et repli ---
    (HPP, "if (c < 0) {\n            bas = milieu + 1;", "if (c > 0) {\n            bas = milieu + 1;", I18N,
     "chaque_cle_se_retrouve_par_la_dichotomie", "dichotomie à l'envers"),
    (HPP, "bas = milieu + 1;", "bas = milieu + 2;", I18N, "chaque_cle_se_retrouve_par_la_dichotomie",
     "la dichotomie saute une clé"),
    (HPP, "return detail::chercher(TABLE, NOMBRE_CLES, francais) != nullptr;", "return detail::chercher(TABLE, NOMBRE_CLES, francais) == nullptr;",
     I18N, "cle_inconnue_rendue_telle_quelle", "est_une_cle répond l'inverse"),
    (HPP, "if (langue == Langue::Fr)\n        return texte_francais(francais);\n    const Entree *e = detail::chercher(table",
     "if (langue == Langue::Fr)\n        return {};\n    const Entree *e = detail::chercher(table", I18N,
     "toute_cle_existe_dans_chaque_langue", "le français devient une chaîne vide"),
    (HPP, "if (!e->t[indice(langue) - 1].empty())\n        return e->t[indice(langue) - 1];",
     "if (false)\n        return e->t[indice(langue) - 1];", I18N, "toute_cle_existe_dans_chaque_langue",
     "toutes les langues retombent sur l'anglais"),
    (HPP, "if (!e->t[indice(Langue::En) - 1].empty())\n        return e->t[indice(Langue::En) - 1];",
     "if (false)\n        return e->t[indice(Langue::En) - 1];", I18N, "repli_anglais_puis_francais_puis_la_cle",
     "plus de repli sur l'anglais"),
    (HPP, "    return texte_francais(francais);\n}\n\nconstexpr std::string_view propre(",
     "    return {};\n}\n\nconstexpr std::string_view propre(", I18N,
     "repli_anglais_puis_francais_puis_la_cle", "rien à afficher quand ni la langue ni l'anglais n'ont le texte"),
    # --- i18n_donnees.inc : la table ---
    (TABLE, '{"Annuler", {"Cancel", "Abbrechen",', '{"Annuler", {"Cancel", "",', I18N, "toute_cle_existe_dans_chaque_langue",
     "« Annuler » n'a plus de traduction allemande"),
    (TABLE, '{"Annuler", {', '{"Zzzz", {', I18N, "table_triee_par_octets_sans_doublon",
     "une ligne hors de sa place dans la table triée"),
    (TABLE, '"Placa: %s, %d × %d"', '"Placa: %d × %d, %s"', I18N, "marqueurs_identiques_et_dans_le_meme_ordre",
     "marqueurs dans un autre ordre (un printf afficherait un nombre à la place d'un texte)"),
    (TABLE, '"Scheda: %s, %d × %d"', '"Scheda: %s, %d"', I18N, "marqueurs_identiques_et_dans_le_meme_ordre",
     "un marqueur %d perdu en italien"),
    (TABLE, '"Zu viele Versuche. Versuche es in {n} Sekunden erneut."', '"Zu viele Versuche. Versuche es in {s} Sekunden erneut."',
     I18N, "marqueurs_identiques_et_dans_le_meme_ordre", "un nom {n} renommé en allemand"),
    (TABLE, '{"Brouillard", {"Fog", "Nebel",', '{"Brouillard", {"Fog", "Neb\\377l",', I18N, "cles_et_traductions_en_utf8_valide",
     "octet 0xFF (UTF-8 invalide) dans une traduction"),
    (TABLE, '{"Orage", {"Thunderstorm", "Gewitter",', '{"Orage", {"Thunderstorm", "Gew\\001itter",', I18N,
     "cles_et_traductions_en_utf8_valide", "caractère de contrôle dans une traduction"),
    (TABLE, '{"dimanche", {"Sunday", "Sonntag",', '{"dimanche", {"Monday", "Sonntag",', I18N,
     "vocabulaire_du_tab5_repris_tel_quel", "« dimanche » devient « Monday » en anglais"),
    (TABLE, '{"jour court|mer", {"Wed",', '{"jour court|mer", {"Sea",', I18N, "une_cle_a_contexte_garde_un_seul_sens",
     "mercredi (jour court) se lit « Sea » en anglais"),
    (HPP, "return barre == std::string_view::npos ? cle : cle.substr(barre + 1);", "return cle;", I18N,
     "une_cle_a_contexte_garde_un_seul_sens", "le contexte « jour court| » s'affiche en français"),
    (HPP, "    if (langue == Langue::Fr)\n        return texte_francais(francais);\n    const Entree *e = detail::chercher(table",
     "    if (langue == Langue::Fr)\n        return francais;\n    const Entree *e = detail::chercher(table", I18N,
     "une_cle_a_contexte_garde_un_seul_sens", "en français, tr() rend la clé avec son contexte"),
    (TABLE, '"Yeniden başlat"', '"Yeniden baslat"', I18N, "vocabulaire_du_tab5_repris_tel_quel",
     "« Redémarrer » perd son ş en turc"),
    (TABLE, '{"mercredi", {"Wednesday", "Mittwoch", "woensdag", "miércoles", "mercoledì", "Çarşamba"}},\n', "", I18N,
     "jours_et_mois_de_heure_fr_traduits", "mercredi n'est plus dans la table"),
    (TABLE, '{"Ciel dégagé", {"Clear sky", "Klarer Himmel",',
     '{"Ciel dégagé", {"Clear sky", "Klarer Himmel mit sehr vielen zusätzlichen Wörtern",', I18N,
     "libelles_meteo_traduits_dans_chaque_langue", "un libellé météo allemand trop long pour l'écran"),
    (TABLE, '"Leichter Regen"', '"Gewitter"', I18N, "libelles_meteo_traduits_dans_chaque_langue",
     "deux conditions météo différentes se lisent pareil en allemand"),
    (TABLE, '{"Toucher : aucun", {"Touch: none", "Berührung: keine", "Aanraking: geen", "Toque: ninguno", "Tocco: nessuno", "Dokunuş: yok"}},\n',
     "", I18N, "textes_de_l_ecran_bonjour_traduits", "un texte de l'écran « Bonjour » n'est plus dans la table"),
    (TABLE, '"Weather data: {lien} (CC BY 4.0 license)"', '"Weather data: {lien} (open license)"', I18N,
     "messages_de_l_api_traduits_avec_leurs_marqueurs", "l'attribution anglaise ne cite plus la licence CC BY 4.0"),
]
