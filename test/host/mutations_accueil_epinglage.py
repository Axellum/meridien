"""Mutations du réglage « Profil » de l'accueil (ADR-0023) : le choix Auto / profil épinglé, de la page web et de l'écran jusqu'au
pilote du mode automatique. Lues par lancer.py --mutations.

Même format que MUTATIONS_ACCUEIL_CARTES : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test
qui doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou « détectée
par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle écrite dans accueil_epinglage.hpp et l'ADR-0023 : « Auto » n'épingle rien, un choix hors table (une NVS
abîmée) n'épingle rien non plus, le réglage n'offre que ce que l'appareil dessine, une valeur refusée ne change rien, et l'hôte, la page
web et l'écran voient la même valeur.
Choisies à la main : elles ne disent pas que « tout » est testé, seulement que ces règles-là le sont.

NE FIGURENT PAS (le code change, le comportement non, ou l'état n'est pas encore atteignable) :
  - `profil_disponible(e.profil)` dans `profil_offert` : le seul profil épinglable aujourd'hui (Météo) est dessiné, donc le test ne peut
    pas le voir refusé avant qu'un second profil existe sans être dessiné ; la règle est écrite et la table PROFILS_DISPONIBLES est
    testée par test_accueil_pilote.cpp ;
  - `&& !tronque` et `pr.genre() == json::Genre::Chaine` dans la validation : double garde, la copie du texte d'un nombre ou d'un texte
    trop long donne déjà un code inconnu.
"""

T = "test_accueil_epinglage.cpp"
HOTE = "test_hote_appareil.cpp"
API = "test_api_reglages.cpp"
E = "accueil_epinglage.hpp"
V = "reglages_valeurs.hpp"

MUTATIONS_ACCUEIL_EPINGLAGE = [
    # --- du réglage au pilote : accueil_epinglage.hpp ---
    (E, "return {true, Profil::Meteo};", "return {};", T, "meteo_epingle_le_profil_meteo",
     "« Météo » n'épingle rien : le pilote reste en Auto malgré le choix"),
    (E, "    return {};", "    return {true, Profil::Meteo};", T, "auto_n_epingle_rien",
     "« Auto » (ou un choix hors table) épingle Météo : les règles de l'ADR-0015 ne décident plus jamais"),
    (E, "a.epingle == b.epingle && (!a.epingle || a.profil == b.profil)", "a.epingle == b.epingle && a.profil == b.profil", T,
     "deux_epinglages_sont_egaux_si_aucun_n_epingle", "deux « pas d'épinglage » sont vus différents à cause d'un profil qui ne compte pas"),
    (E, "if (choix == reglages::ProfilAccueil::Auto)", "if (false)", T, "le_reglage_n_offre_que_ce_que_l_appareil_dessine",
     "« Auto » n'est plus proposé : la personne ne peut plus dépingler"),
    # --- les valeurs : reglages_valeurs.hpp ---
    (V, "return (i >= 0 && i < static_cast<int>(NOMBRE_PROFILS_ACCUEIL)) ? static_cast<ProfilAccueil>(i) : defaut;",
     "return i >= 0 ? static_cast<ProfilAccueil>(i) : defaut;", T, "une_nvs_abimee_ne_donne_jamais_un_profil_epingle",
     "un octet de NVS hors table devient un profil inventé au lieu d'« Auto »"),
    (V, "if (PROFILS_ACCUEIL[i] == c)", "if (PROFILS_ACCUEIL[i].substr(0, c.size()) == c)", T, "un_code_inconnu_n_est_pas_un_profil",
     "un code vide ou tronqué est pris pour « auto »"),
    (V, "if (pr.existe()) {", "if (true) {", T, "la_validation_accepte_les_deux_codes_et_refuse_le_reste",
     "un envoi sans « profil » (la luminosité seule) est refusé : la page ne peut plus régler un champ à la fois"),
    (V, "n.profil = *v;", "n.profil = ProfilAccueil::Auto;", T, "la_validation_accepte_les_deux_codes_et_refuse_le_reste",
     "le profil choisi est accepté puis oublié (toujours Auto)"),
    # --- l'ordre offert : registre_reglages.hpp ---
    ("registre_reglages.hpp", '{{"auto", "Auto", true}, {"meteo", "Météo", true}}', '{{"meteo", "Météo", true}, {"auto", "Auto", true}}',
     T, "le_registre_et_l_api_offrent_les_memes_profils_dans_le_meme_ordre",
     "l'écran et la page web n'offrent pas les profils dans le même ordre (l'indice envoyé n'est plus le bon)"),
    # --- l'hôte de l'appareil : hote_appareil.hpp ---
    ("hote_appareil.hpp", "return static_cast<std::int32_t>(p_.affichage().profil);", "return 0;", HOTE,
     "le_profil_de_l_accueil_passe_par_l_hote", "l'écran de réglages affiche toujours « Auto », même quand la page web a épinglé Météo"),
    ("hote_appareil.hpp", "a.profil = reglages::profil_accueil_depuis_indice(valeur);", "a.profil = reglages::ProfilAccueil::Meteo;", HOTE,
     "le_profil_de_l_accueil_passe_par_l_hote", "choisir « Auto » sur l'écran épingle Météo : impossible de dépingler"),
    # --- l'API : api_reglages.hpp ---
    ("api_reglages.hpp", 'e.champ("profil", reglages::code(a.profil));', 'e.champ("profil", "auto");', API,
     "le_profil_de_l_accueil_se_regle_et_se_relit", "la page web relit toujours « auto » : elle croit que rien n'est épinglé"),
    ("api_reglages.hpp", '"profil_inconnu"', '"profil"', API, "un_profil_inconnu_est_refuse_sans_rien_changer",
     "le code d'erreur d'un profil inconnu n'est plus celui que la page attend"),
]
