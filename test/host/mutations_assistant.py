"""Mutations du noyau « assistant vocal » (voir lancer.py --mutations).

Même format que MUTATIONS de lancer.py :
    (fichier du noyau, texte exact à remplacer, remplacement, test qui doit le détecter, nom du test attendu, description)
Le texte à remplacer doit apparaître UNE seule fois dans le fichier (sinon le lanceur le signale « INAPPLICABLE »).
Les ancres suivent le code tel que formaté par clang-format (.clang-format du dépôt) ; chaînes brutes (r'''...''').
"""

E = "test_assistant_etats.cpp"
S = "test_flux_sse.cpp"
R = "test_requetes_openai.cpp"
X = "test_extrait_openai.cpp"
D = "test_decoupe_phrases.cpp"
T = "test_assistant_texte.cpp"

MUTATIONS_ASSISTANT = [
    # --- assistant_etats.hpp ---------------------------------------------------------------------------------
    ('assistant_etats.hpp',
     r'''constexpr const Transition &transition(Etat etat, Evenement evenement) noexcept {
''',
     r'''constexpr const Transition &transition_absente(Etat etat, Evenement evenement) noexcept {
''',
     E, None,
     "la fonction transition n'existe pas"),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Reflexion, E::AppuiEcran, S::Pret, A::ToutArreter);
''',
     r'''    regle(tb, S::Reflexion, E::AppuiEcran, S::Reponse, A::ToutArreter);
''',
     E, 'l_interruption_fonctionne_depuis_reflexion_et_reponse',
     "l'appui en réflexion ne ramène plus à « prêt »"),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Reponse, E::AppuiEcran, S::Pret, A::ToutArreter);
''',
     r'''    regle(tb, S::Reponse, E::AppuiEcran, S::Reponse, A::ToutArreter);
''',
     E, 'l_interruption_fonctionne_depuis_reflexion_et_reponse',
     "l'appui en réponse n'interrompt plus"),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Erreur, E::DelaiDepasse, S::Pret, A::Aucune);
''',
     r'''    regle(tb, S::Erreur, E::DelaiDepasse, S::Erreur, A::Aucune);
''',
     E, 'une_erreur_revient_a_pret_apres_son_delai',
     "l'erreur ne revient plus à « prêt » après son délai"),
    ('assistant_etats.hpp',
     r'''        return 5000;
''',
     r'''        return 0;
''',
     E, 'les_delais_font_sortir_de_tout_etat_actif',
     "l'état d'erreur n'a plus de minuteur"),
    ('assistant_etats.hpp',
     r'''        return 120000;
''',
     r'''        return 1000;
''',
     E, 'delais_de_depart_documentes',
     'délai de réponse changé sans le dire'),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Reflexion, E::DelaiDepasse, S::Erreur, A::ToutArreter, Service::Llm, Cause::NeRepondPas);
''',
     r'''    regle(tb, S::Reflexion, E::DelaiDepasse, S::Reflexion, A::ToutArreter);
''',
     E, 'les_delais_font_sortir_de_tout_etat_actif',
     'la réflexion peut bloquer : son délai ne fait plus rien'),
    ('assistant_etats.hpp',
     r'''        return "Je réfléchis…";
''',
     r'''        return "Je reflechis…";
''',
     E, 'textes_courts_francais_avec_accents_et_sans_code',
     'accents perdus dans le texte de réflexion'),
    ('assistant_etats.hpp',
     r'''        return "Le service refuse la clé. Vérifiez-la.";
''',
     r'''        return "Le service refuse la cle. Verifiez-la.";
''',
     E, 'phrases_d_erreur_exactes_de_l_api_paragraphe_8',
     'phrase « clé refusée » sans accents'),
    ('assistant_etats.hpp',
     r'''        return "Cette adresse ne ressemble pas à un service compatible. Il manque peut-être « /v1 ».";
''',
     r'''        return "Cette adresse ne ressemble pas à un service compatible. Il manque peut-être « /v2 ».";
''',
     E, 'phrases_d_erreur_exactes_de_l_api_paragraphe_8',
     'phrase « il manque /v1 » modifiée'),
    ('assistant_etats.hpp',
     r'''    if (statut == 401 || statut == 403)
''',
     r'''    if (statut == 401)
''',
     E, 'statut_http_vers_cause',
     "le statut 403 n'est plus une clé refusée"),
    ('assistant_etats.hpp',
     r'''    if (statut == 404 && echange_reel)
''',
     r'''    if (statut == 404)
''',
     E, 'statut_http_vers_cause',
     'un 404 sur /models devient une erreur de route'),
    ('assistant_etats.hpp',
     r'''    if (statut >= 500 && statut <= 599)
''',
     r'''    if (statut >= 500 && statut <= 598)
''',
     E, 'statut_http_vers_cause',
     '5xx : borne haute décalée'),
    ('assistant_etats.hpp',
     r'''        if (!t.pris)
            return {avant, avant, Action::Aucune, false}; // ignoré : ni l'état ni la cause d'erreur ne bougent
''',
     r'''''',
     E, 'un_evenement_ignore_en_erreur_garde_la_cause_affichee',
     "un événement ignoré écrase la cause d'erreur"),
    ('assistant_etats.hpp',
     r'''            cause_ = (cause_signalee != Cause::Aucune && cause_signalee != Cause::Nombre) ? cause_signalee : t.cause;
''',
     r'''            cause_ = t.cause;
''',
     E, 'quelle_erreur_dans_quel_etat',
     "la cause signalée par l'appelant est ignorée"),
    ('assistant_etats.hpp',
     r'''            cause_ = Cause::Aucune;
''',
     r'''''',
     E, 'une_erreur_revient_a_pret_apres_son_delai',
     "la cause d'erreur survit au retour à « prêt »"),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Pret, E::MotActivation, S::Ecoute, A::DemarrerCapture);
''',
     r'''    regle(tb, S::Pret, E::MotActivation, S::Transcription, A::DemarrerCapture);
''',
     E, 'pret_n_est_reveille_que_par_le_mot_d_activation',
     "le mot d'activation saute l'écoute"),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Reflexion, E::FinDeLecture, S::Erreur, A::ToutArreter, Service::Llm, Cause::ReponseInattendue);
''',
     r'''    regle(tb, S::Reflexion, E::FinDeLecture, S::Pret, A::Aucune);
''',
     E, 'cause_par_defaut_quand_l_appelant_n_en_donne_pas',
     'une réponse vide du LLM passe sous silence'),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Ecoute, E::DelaiDepasse, S::Pret, A::ToutArreter);
''',
     r'''    regle(tb, S::Ecoute, E::DelaiDepasse, S::Erreur, A::ToutArreter, Service::Stt, Cause::NeRepondPas);
''',
     E, 'delai_en_ecoute_est_un_retour_silencieux_pas_une_panne',
     'le silence devient une erreur'),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Reponse, E::ErreurTts, S::Erreur, A::ToutArreter, Service::Tts, Cause::NeRepondPas);
''',
     r'''    regle(tb, S::Transcription, E::ErreurTts, S::Erreur, A::ToutArreter, Service::Tts, Cause::NeRepondPas);
''',
     E, 'quelle_erreur_dans_quel_etat',
     'une erreur TTS est prise en transcription et plus en réponse'),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Erreur, E::MotActivation, S::Ecoute, A::DemarrerCapture);
''',
     r'''    regle(tb, S::Erreur, E::MotActivation, S::Pret, A::Aucune);
''',
     E, 'mot_d_activation_en_erreur_relance_l_ecoute',
     "le mot d'activation ne relance plus depuis l'erreur"),
    ('assistant_etats.hpp',
     r'''        return "Comprendre ma voix";
''',
     r'''        return "Comprendre la voix";
''',
     E, 'titres_des_services_comme_dans_la_page_de_reglages',
     'titre de carte différent de la page'),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Transcription, E::RienReconnu, S::Pret, A::Aucune);
''',
     r'''    regle(tb, S::Transcription, E::RienReconnu, S::Erreur, A::Aucune, Service::Stt, Cause::ReponseInattendue);
''',
     E, 'texte_vide_reconnu_ramene_a_pret_sans_erreur',
     'un texte vide devient une erreur'),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Reponse, E::MorceauReponse, S::Reponse, A::Aucune);
''',
     r'''
''',
     E, 'cheminement_nominal_de_l_echange',
     "les morceaux en réponse sont ignorés (le minuteur n'est plus réarmé)"),
    ('assistant_etats.hpp',
     r'''    regle(tb, S::Transcription, E::TexteReconnu, S::Reflexion, A::EnvoyerAuLlm);
''',
     r'''    regle(tb, S::Transcription, E::TexteReconnu, S::Reflexion, A::Aucune);
''',
     E, 'cheminement_nominal_de_l_echange',
     "le texte reconnu n'est plus envoyé au LLM"),
    ('assistant_etats.hpp',
     r'''            tb.t[s][e] = {static_cast<Etat>(s), Action::Aucune, Service::Aucun, Cause::Aucune, false}; // ignorer
''',
     r'''            tb.t[s][e] = {Etat::Pret, Action::Aucune, Service::Aucun, Cause::Aucune, false}; // ignorer
''',
     E, 'aucune_transition_vers_un_etat_inconnu',
     'un événement ignoré ramène à « prêt »'),
    ('assistant_etats.hpp',
     r'''    constexpr std::string_view texte_affiche() const noexcept {
        return etat_ == Etat::Erreur ? texte_erreur(cause_) : texte(etat_);
    }
''',
     r'''    constexpr std::string_view texte_affiche() const noexcept { return texte(etat_); }
''',
     E, 'l_ecran_affiche_la_phrase_de_la_cause_en_erreur',
     "l'écran n'affiche plus la cause"),
    ('assistant_etats.hpp',
     r'''        return "Prêt";
''',
     r'''        return "Pret";
''',
     E, 'textes_courts_francais_avec_accents_et_sans_code',
     '« Prêt » sans accent'),
    # --- flux_sse.hpp ----------------------------------------------------------------------------------------
    ('flux_sse.hpp',
     r'''    template <class F> constexpr std::size_t alimenter(std::string_view morceau, F &&sur_evenement) {
''',
     r'''    template <class F> constexpr std::size_t alimenter_absente(std::string_view morceau, F &&sur_evenement) {
''',
     S, None,
     "la méthode alimenter n'existe pas"),
    ('flux_sse.hpp',
     r'''                if (c == '\n')
                    continue; // le LF d'un CRLF dont le CR était dans le morceau précédent
''',
     r'''                if (false)
                    continue;
''',
     S, 'cr_et_lf_dans_deux_morceaux_ne_font_qu_une_fin_de_ligne',
     "le LF d'un CRLF coupé compte pour une ligne vide"),
    ('flux_sse.hpp',
     r'''                if (fin_de_ligne()) {
''',
     r'''                if (c == '\n' && fin_de_ligne()) {
''',
     S, 'chaque_evenement_est_livre_des_son_dernier_octet',
     "une ligne vide terminée par un CR seul ne livre plus l'événement avant le LF suivant"),
    ('flux_sse.hpp',
     r'''                saute_lf_ = (c == '\r');
''',
     r'''                saute_lf_ = false;
''',
     S, 'les_trois_fins_de_ligne_sont_equivalentes',
     'un CRLF compte pour deux fins de ligne'),
    ('flux_sse.hpp',
     r'''                etat_ = Etat::Commentaire;
''',
     r'''                etat_ = Etat::Valeur;
''',
     S, 'commentaires_et_champs_inconnus_sont_ignores',
     'les commentaires sont lus comme des valeurs'),
    ('flux_sse.hpp',
     r'''            if (c != ' ')
                valeur(c); // un seul espace est retiré après les deux-points
''',
     r'''            valeur(c);
''',
     S, 'un_seul_espace_apres_les_deux_points_est_retire',
     "l'espace après « data: » est conservé"),
    ('flux_sse.hpp',
     r'''    constexpr void debut_ligne_data() noexcept {
        if (a_donnees_)
            ecrire_donnee('\n');
        a_donnees_ = true;
    }
''',
     r'''    constexpr void debut_ligne_data() noexcept { a_donnees_ = true; }
''',
     S, 'data_sur_plusieurs_lignes_est_joint_par_un_retour_a_la_ligne',
     'les lignes data ne sont plus séparées'),
    ('flux_sse.hpp',
     r'''        const EvenementSse ev{std::string_view(type_, type_n_), donnees, !trop_long_ && donnees == "[DONE]",
''',
     r'''        const EvenementSse ev{std::string_view(type_, type_n_), donnees, !trop_long_ && donnees == "[DONE]x",
''',
     S, 'flux_d_openai_avec_done',
     "[DONE] n'est plus reconnu"),
    ('flux_sse.hpp',
     r'''            if (termine_)
''',
     r'''            if (false)
''',
     S, 'rien_n_est_lu_apres_done',
     'on continue de lire après [DONE]'),
    ('flux_sse.hpp',
     r'''        if (data_n_ >= CAPACITE_DONNEES) {
''',
     r'''        if (data_n_ >= CAPACITE_DONNEES - 1) {
''',
     S, 'evenement_trop_gros_est_signale_pas_tronque',
     "capacité utile réduite d'un octet"),
    ('flux_sse.hpp',
     r'''            return a_donnees_;
''',
     r'''            return true;
''',
     S, 'data_vide_est_un_evenement_a_donnees_vides',
     'une ligne vide sans data expédie un événement'),
    ('flux_sse.hpp',
     r'''    constexpr bool evenement_incomplet() const noexcept {
        return !(etat_ == Etat::DebutLigne && !a_donnees_ && type_n_ == 0);
    }
''',
     r'''    constexpr bool evenement_incomplet() const noexcept { return false; }
''',
     S, 'evenement_incomplet_a_l_arret_du_flux',
     "un événement incomplet n'est jamais signalé"),
    ('flux_sse.hpp',
     r'''        if (nom == "data")
''',
     r'''        if (nom == "datax")
''',
     S, 'flux_d_openai_avec_done',
     "le champ data n'est plus reconnu"),
    ('flux_sse.hpp',
     r'''            return Champ::Event;
''',
     r'''            return Champ::Data;
''',
     S, 'type_d_evenement_puis_oubli_sans_donnees',
     'le champ event est pris pour des données'),
    ('flux_sse.hpp',
     r'''        } else if (champ_ == Champ::Event && type_n_ < CAPACITE_TYPE) {
''',
     r'''        } else if (champ_ == Champ::Event && type_n_ < CAPACITE_TYPE - 1) {
''',
     S, 'type_trop_long_est_tronque_sans_deborder',
     "type d'événement tronqué d'un octet de trop"),
    ('flux_sse.hpp',
     r'''            if (!a_donnees_)
                type_n_ = 0;
''',
     r'''
''',
     S, 'type_d_evenement_puis_oubli_sans_donnees',
     "le type d'un événement sans données survit"),
    ('flux_sse.hpp',
     r'''        termine_ = false;
''',
     r'''''',
     S, 'rien_n_est_lu_apres_done',
     'remettre_a_zero ne rouvre pas le flux'),
    ('flux_sse.hpp',
     r'''        if (nom_n_ < sizeof(nom_)) {
''',
     r'''        if (true) {
''',
     S, 'commentaires_et_champs_inconnus_sont_ignores',
     'un nom de champ très long déborde'),
    # --- requetes_openai.hpp ---------------------------------------------------------------------------------
    ('requetes_openai.hpp',
     r'''constexpr bool assembler_url(Tampon &sortie, std::string_view base, std::string_view route) noexcept {
''',
     r'''constexpr bool assembler_url_absente(Tampon &sortie, std::string_view base, std::string_view route) noexcept {
''',
     R, None,
     "la fonction assembler_url n'existe pas"),
    ('requetes_openai.hpp',
     r'''constexpr bool json_chaine(Tampon &t, std::string_view s) noexcept {
''',
     r'''constexpr bool json_chaine_absente(Tampon &t, std::string_view s) noexcept {
''',
     R, None,
     "la fonction json_chaine n'existe pas"),
    ('requetes_openai.hpp',
     r'''    if (s.size() < 2 || s[0] != 'v' || !est_chiffre(s[1]))
''',
     r'''    if (s.size() < 2 || s[0] != 'u' || !est_chiffre(s[1]))
''',
     R, 'la_base_a_t_elle_une_version',
     "« v1 » n'est plus un segment de version"),
    ('requetes_openai.hpp',
     r'''    if (dernier_segment == "v1" && (route == "v1" || detail::commence_par(route, "v1/"))) {
''',
     r'''    if (dernier_segment == "v2" && (route == "v1" || detail::commence_par(route, "v1/"))) {
''',
     R, 'assemblage_adresse_et_route',
     "le /v1 doublé n'est plus retiré"),
    ('requetes_openai.hpp',
     r'''    while (!base.empty() && base.back() == '/')
        base.remove_suffix(1);
''',
     r'''
''',
     R, 'assemblage_adresse_et_route',
     'les « / » finaux de la base restent'),
    ('requetes_openai.hpp',
     r'''    while (!route.empty() && route.front() == '/')
        route.remove_prefix(1);
''',
     r'''
''',
     R, 'assemblage_adresse_et_route',
     'les « / » initiaux de la route restent'),
    ('requetes_openai.hpp',
     r'''    if (!detail::commence_par(base, "http://") && !detail::commence_par(base, "https://"))
        return false;
''',
     r'''
''',
     R, 'adresses_refusees',
     'une adresse sans http(s) est acceptée'),
    ('requetes_openai.hpp',
     r'''        if (detail::caractere_de_controle(c) || c == ' ' || c == '?' || c == '#')
''',
     r'''        if (detail::caractere_de_controle(c) || c == ' ' || c == '?')
''',
     R, 'adresses_refusees',
     "un « # » dans l'adresse est accepté"),
    ('requetes_openai.hpp',
     r'''            t.ecrire("\\\"");
''',
     r'''            t.ecrire("\"");
''',
     R, 'json_texte_simple_et_caracteres_speciaux',
     'guillemet non échappé'),
    ('requetes_openai.hpp',
     r'''            t.ecrire("\\\\");
''',
     r'''            t.ecrire("\\");
''',
     R, 'json_texte_simple_et_caracteres_speciaux',
     'barre inverse non doublée'),
    ('requetes_openai.hpp',
     r'''            t.ecrire("\\n");
''',
     r'''            t.ecrire("\\r");
''',
     R, 'json_texte_simple_et_caracteres_speciaux',
     'retour à la ligne écrit comme un retour chariot'),
    ('requetes_openai.hpp',
     r'''                t.ecrire("\\u00");
''',
     r'''                t.ecrire("\\u01");
''',
     R, 'json_caracteres_de_controle_en_u00xx',
     'caractère de contrôle mal codé'),
    ('requetes_openai.hpp',
     r'''                t.ecrire("\xEF\xBF\xBD"); // U+FFFD, un par octet invalide
''',
     r'''                t.ecrire("?"); // U+FFFD, un par octet invalide
''',
     R, 'json_octets_invalides_remplaces_par_u_fffd',
     'octet invalide remplacé par « ? »'),
    ('requetes_openai.hpp',
     r'''        if (premier < n && taille > budget_octets - (total < budget_octets ? total : budget_octets))
''',
     r'''        if (premier < n && taille >= budget_octets - (total < budget_octets ? total : budget_octets))
''',
     R, 'fenetre_d_historique_bornee_en_octets',
     "budget d'historique : borne exclusive"),
    ('requetes_openai.hpp',
     r'''        if (premier < n && taille > budget_octets - (total < budget_octets ? total : budget_octets))
''',
     r'''        if (taille > budget_octets - (total < budget_octets ? total : budget_octets))
''',
     R, 'fenetre_d_historique_bornee_en_octets',
     'la dernière question peut sortir de la fenêtre'),
    ('requetes_openai.hpp',
     r'''    while (premier + 1 < n && messages[premier].role != Role::Utilisateur)
        ++premier;
''',
     r'''
''',
     R, 'fenetre_d_historique_bornee_en_octets',
     'la fenêtre peut commencer par une réponse orpheline'),
    ('requetes_openai.hpp',
     r'''    if (!o.prompt_systeme.empty()) {
''',
     r'''    if (false) {
''',
     R, 'corps_chat_exact_avec_modele_et_prompt',
     "le prompt système n'est plus envoyé"),
    ('requetes_openai.hpp',
     r'''    t.ecrire("\"stream\":true,\"messages\":[");
''',
     r'''    t.ecrire("\"stream\":false,\"messages\":[");
''',
     R, 'corps_chat_exact_avec_modele_et_prompt',
     "le flux n'est plus demandé"),
    ('requetes_openai.hpp',
     r'''        return "user";
''',
     r'''        return "usr";
''',
     R, 'noms_des_roles',
     'rôle utilisateur mal nommé'),
    ('requetes_openai.hpp',
     r'''    if (o.nombre == 0 || o.historique == nullptr)
        return false;
''',
     r'''
''',
     R, 'corps_chat_refuse_l_absence_de_message_et_le_debordement',
     'un corps de chat sans message est accepté'),
    ('requetes_openai.hpp',
     r'''    if (o.texte.empty())
        return false;
''',
     r'''
''',
     R, 'corps_tts_refuse_texte_vide_et_debordement',
     'un texte vide est envoyé au TTS'),
    ('requetes_openai.hpp',
     r'''    detail::champ_texte(t, premier, "response_format", nom_format_audio(o.format));
''',
     r'''    detail::champ_texte(t, premier, "format", nom_format_audio(o.format));
''',
     R, 'corps_tts_exact',
     'champ response_format mal nommé'),
    ('requetes_openai.hpp',
     r'''    for (int i = 15; i >= 0; --i)
''',
     r'''    for (int i = 14; i >= 0; --i)
''',
     R, 'frontiere_depuis_une_graine',
     'frontière : un chiffre hexadécimal de moins'),
    ('requetes_openai.hpp',
     r'''    if (f.empty() || f.size() > 70)
''',
     r'''    if (f.empty() || f.size() > 71)
''',
     R, 'frontieres_admissibles_et_refusees',
     'frontière de 71 caractères acceptée'),
    ('requetes_openai.hpp',
     r'''    detail::champ_multipart(t, frontiere, "response_format", "json");
''',
     r'''    detail::champ_multipart(t, frontiere, "response_format", "text");
''',
     R, 'cadre_multipart_exact_avant_et_apres_l_audio',
     'format de réponse STT changé'),
    ('requetes_openai.hpp',
     r'''        if (caractere_de_controle(c) || c == '"')
''',
     r'''        if (c == '"')
''',
     R, 'multipart_refuse_les_valeurs_dangereuses',
     'caractères de contrôle acceptés dans un champ multipart'),
    ('requetes_openai.hpp',
     r'''    return v.find(frontiere) == std::string_view::npos; // la valeur ne doit pas pouvoir imiter la frontière
''',
     r'''    return true; // la valeur ne doit pas pouvoir imiter la frontière
''',
     R, 'multipart_refuse_les_valeurs_dangereuses',
     'une valeur peut contenir la frontière'),
    ('requetes_openai.hpp',
     r'''    if (v.empty() || v.size() > 80)
''',
     r'''    if (v.size() > 80)
''',
     R, 'multipart_refuse_nom_de_fichier_type_et_frontiere_invalides',
     'nom de fichier vide accepté'),
    ('requetes_openai.hpp',
     r'''        const bool ok =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || est_chiffre(c) || c == '.' || c == '_' || c == '-';
''',
     r'''        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || est_chiffre(c) || c == '.' || c == '_' ||
                        c == '-' || c == '/';
''',
     R, 'multipart_refuse_nom_de_fichier_type_et_frontiere_invalides',
     '« / » accepté dans un nom de fichier'),
    ('requetes_openai.hpp',
     r'''    t.ecrire("--\r\n");
''',
     r'''    t.ecrire("--");
''',
     R, 'cadre_multipart_exact_avant_et_apres_l_audio',
     'fermeture multipart sans fin de ligne'),
    ('requetes_openai.hpp',
     r'''    t.ecrire("multipart/form-data; boundary=");
''',
     r'''    t.ecrire("multipart/form; boundary=");
''',
     R, 'en_tete_content_type',
     'type de contenu multipart faux'),
    ('requetes_openai.hpp',
     r'''    if (!o.modele.empty())
        detail::champ_multipart(t, frontiere, "model", o.modele);
''',
     r'''    detail::champ_multipart(t, frontiere, "model", o.modele);
''',
     R, 'cadre_multipart_omet_modele_et_langue_vides',
     'modèle vide envoyé'),
    ('requetes_openai.hpp',
     r'''inline constexpr std::string_view ROUTE_STT = "audio/transcriptions";
''',
     r'''inline constexpr std::string_view ROUTE_STT = "audio/transcription";
''',
     R, 'routes_documentees',
     'route STT mal écrite'),
    # --- extrait_openai.hpp ----------------------------------------------------------------------------------
    ('extrait_openai.hpp',
     r'''constexpr Extrait contenu_du_delta(std::string_view json, char *sortie, std::size_t capacite) noexcept {
''',
     r'''constexpr Extrait contenu_du_delta_absente(std::string_view json, char *sortie, std::size_t capacite) noexcept {
''',
     X, None,
     "la fonction contenu_du_delta n'existe pas"),
    ('extrait_openai.hpp',
     r'''constexpr bool mentionne_modele(std::string_view message) noexcept {
''',
     r'''constexpr bool mentionne_modele_absente(std::string_view message) noexcept {
''',
     X, None,
     "la fonction mentionne_modele n'existe pas"),
    ('extrait_openai.hpp',
     r'''    constexpr detail::Etape CHEMIN[] = {{"choices"}, {""}, {"delta"}, {"content"}};
''',
     r'''    constexpr detail::Etape CHEMIN[] = {{"choices"}, {""}, {"delta"}, {"contents"}};
''',
     X, 'morceau_de_reponse_ordinaire',
     'mauvaise clé de contenu'),
    ('extrait_openai.hpp',
     r'''            if (nom == cle)
                return Lecture::Texte; // « Texte » = trouvé (position sur la valeur)
''',
     r'''            if (nom.size() >= cle.size() && nom.substr(nom.size() - cle.size()) == cle)
                return Lecture::Texte;
''',
     X, 'le_raisonnement_d_un_modele_local_n_est_jamais_lu',
     'reasoning_content lu comme content'),
    ('extrait_openai.hpp',
     r'''                    if (!sauter_chaine())
                        return false;
''',
     r'''                    {
                        ++i_;
                        continue;
                    }
''',
     X, 'la_cle_content_ailleurs_n_est_pas_prise',
     'les accolades dans une chaîne comptent comme structure'),
    ('extrait_openai.hpp',
     r'''                    sortie.ecrire('\n');
''',
     r'''                    sortie.ecrire('\r');
''',
     X, 'echappements_json_decodes_en_utf8',
     '\\n décodé en retour chariot'),
    ('extrait_openai.hpp',
     r'''                    sortie.ecrire('/');
''',
     r'''                    sortie.ecrire('\\');
''',
     X, 'echappements_json_decodes_en_utf8',
     '\\/ décodé en barre inverse'),
    ('extrait_openai.hpp',
     r'''                                cp = 0x10000U + ((cp - 0xD800U) << 10) + (bas - 0xDC00U);
''',
     r'''                                cp = 0x10000U + ((cp - 0xD800U) << 9) + (bas - 0xDC00U);
''',
     X, 'echappements_json_decodes_en_utf8',
     'paire de surrogates mal combinée'),
    ('extrait_openai.hpp',
     r'''                        cp = 0xFFFDU; // seconde moitié sans la première
''',
     r'''                        cp = '?'; // seconde moitié sans la première
''',
     X, 'surrogates_isoles_deviennent_u_fffd',
     'surrogate bas isolé accepté'),
    ('extrait_openai.hpp',
     r'''                    if (cp != 0) { // \u0000 : ignoré
''',
     r'''                    if (true) {
''',
     X, 'echappements_json_decodes_en_utf8',
     '\\u0000 écrit dans le texte'),
    ('extrait_openai.hpp',
     r'''                    sortie.ecrire("\xEF\xBF\xBD");
''',
     r'''                    sortie.ecrire("?");
''',
     X, 'utf8_brut_conserve_et_octets_invalides_remplaces',
     'octet UTF-8 invalide remplacé par « ? »'),
    ('extrait_openai.hpp',
     r'''            } else if (c >= 'A' && c <= 'F') {
''',
     r'''            } else if (c >= 'A' && c <= 'E') {
''',
     X, 'chiffres_hexadecimaux_en_majuscules_et_minuscules',
     'chiffre hexadécimal F majuscule refusé'),
    ('extrait_openai.hpp',
     r'''                return sortie.ok() ? Lecture::Texte : Lecture::TropLong;
''',
     r'''                return Lecture::Texte;
''',
     X, 'texte_trop_long_pour_le_tableau_n_est_jamais_tronque',
     'texte tronqué sans le dire'),
    ('extrait_openai.hpp',
     r'''    if (l.consommer_mot("null"))
        return {Lecture::Absent, std::string_view()};
''',
     r'''
''',
     X, 'premier_et_dernier_morceaux',
     'content:null devient une erreur'),
    ('extrait_openai.hpp',
     r'''    return {Lecture::Texte, detail::rogner(e.texte)};
''',
     r'''    return {Lecture::Texte, e.texte};
''',
     X, 'transcription_ordinaire_et_rognee',
     'transcription non rognée'),
    ('extrait_openai.hpp',
     r'''    constexpr detail::Etape CHEMIN[] = {{"text"}};
''',
     r'''    constexpr detail::Etape CHEMIN[] = {{"texte"}};
''',
     X, 'transcription_ordinaire_et_rognee',
     'mauvaise clé de transcription'),
    ('extrait_openai.hpp',
     r'''    constexpr detail::Etape MESSAGE[] = {{"message"}};
''',
     r'''    constexpr detail::Etape MESSAGE[] = {{"msg"}};
''',
     X, 'message_d_erreur_openai',
     "mauvaise clé de message d'erreur"),
    ('extrait_openai.hpp',
     r'''    if (l.courant() != '"') {
''',
     r'''    if (true) {
''',
     X, 'autres_formes_de_corps_d_erreur',
     "error en chaîne n'est plus lu"),
    ('extrait_openai.hpp',
     r'''    if (tolere_tableau_racine && l.consommer('[')) {
''',
     r'''    if (false && tolere_tableau_racine && l.consommer('[')) {
''',
     X, 'autres_formes_de_corps_d_erreur',
     'erreur en tableau racine non lue'),
    ('extrait_openai.hpp',
     r'''    return detail::contient_sans_casse(message, "model") || detail::contient_sans_casse(message, "modèle");
''',
     r'''    return detail::contient_sans_casse(message, "model");
''',
     X, 'le_message_d_erreur_parle_t_il_du_modele',
     "« modèle » (accent) n'est plus reconnu"),
    ('extrait_openai.hpp',
     r'''            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a'); // casse ASCII seulement
''',
     r'''
''',
     X, 'le_message_d_erreur_parle_t_il_du_modele',
     'la casse compte dans la recherche de « model »'),
    # --- decoupe_phrases.hpp ---------------------------------------------------------------------------------
    ('decoupe_phrases.hpp',
     r'''            if (c == '\n') {
''',
     r'''            if (c == '\r') {
''',
     D, 'retours_a_la_ligne_coupent',
     'le retour à la ligne ne coupe plus'),
    ('decoupe_phrases.hpp',
     r'''            if (k >= n_)
                return false; // on ne sait pas encore ce qui suit : attendre le prochain morceau
''',
     r'''            if (k >= n_) {
                fin = k;
                suite = k;
                return true;
            }
''',
     D, 'decimal_coupe_entre_deux_morceaux',
     'coupe sans attendre le caractère suivant'),
    ('decoupe_phrases.hpp',
     r'''            if (detail::est_blanc_phrase(buf_[k])) {
''',
     r'''            if (true) {
''',
     D, 'nombres_decimaux_et_adresses_ne_coupent_pas',
     'coupe après un point même sans blanc'),
    ('decoupe_phrases.hpp',
     r'''                if (!(point_seul && point_d_abreviation_avant(i))) {
''',
     r'''                if (true) {
''',
     D, 'abreviations_courantes_ne_coupent_pas',
     'les abréviations coupent'),
    ('decoupe_phrases.hpp',
     r'''        if (chiffres)
''',
     r'''        if (false)
''',
     D, 'listes_numerotees_ne_sont_pas_coupees',
     '« 1. » coupe la liste'),
    ('decoupe_phrases.hpp',
     r'''    if (mot.size() == 1 && mot[0] >= 'A' && mot[0] <= 'Z')
''',
     r'''    if (mot.size() == 2 && mot[0] >= 'A' && mot[0] <= 'Z')
''',
     D, 'initiales_et_sigles_ne_coupent_pas',
     '« M. » coupe'),
    ('decoupe_phrases.hpp',
     r'''    "st",   "ste", "sts",    "stes", "cie",  "cf",    "etc", "ex",  "env",  "av",   "apr",   "ap",
''',
     r'''    "st",   "ste", "sts",    "stes", "cie",  "cf",    "etx", "ex",  "env",  "av",   "apr",   "ap",
''',
     D, 'abreviations_courantes_ne_coupent_pas',
     '« etc. » coupe'),
    ('decoupe_phrases.hpp',
     r'''    "m",    "mm",  "mme",    "mmes", "mlle", "mlles", "me",  "dr",  "drs",  "pr",   "prs",   "mgr",
''',
     r'''    "m",    "mm",  "mmx",    "mmes", "mlle", "mlles", "me",  "dr",  "drs",  "pr",   "prs",   "mgr",
''',
     D, 'abreviations_courantes_ne_coupent_pas',
     '« Mme. » coupe'),
    ('decoupe_phrases.hpp',
     r'''    "vs",   "fig", "vol",    "chap", "art",  "tél",   "tel", "bd",  "boul", "pp",   "p",     "n",
''',
     r'''    "vs",   "fig", "vol",    "chap", "art",  "tél2",  "tel", "bd",  "boul", "pp",   "p",     "n",
''',
     D, 'abreviations_courantes_ne_coupent_pas',
     '« tél. » coupe'),
    ('decoupe_phrases.hpp',
     r'''        if (minuscule_ascii(mot[i]) != modele_minuscule[i])
''',
     r'''        if (mot[i] != modele_minuscule[i])
''',
     D, 'abreviations_sans_tenir_compte_de_la_casse',
     'la casse compte pour les abréviations'),
    ('decoupe_phrases.hpp',
     r'''                if (lettres == 0 || lettres > 2)
''',
     r'''                if (lettres == 0 || lettres > 9)
''',
     D, 'initiales_et_sigles_ne_coupent_pas',
     'un nom de domaine passe pour un sigle'),
    ('decoupe_phrases.hpp',
     r'''        if (!phrase.empty() && detail::a_du_contenu(phrase))
''',
     r'''        if (!phrase.empty())
''',
     D, 'phrases_sans_contenu_ne_sont_pas_emises',
     'des phrases de ponctuation sont envoyées au TTS'),
    ('decoupe_phrases.hpp',
     r'''        const bool ponctuation =
            cp < 0xC0U || (cp >= 0x2000U && cp <= 0x206FU); // Latin-1 symboles, ponctuation générale
''',
     r'''        const bool ponctuation = cp < 0xC0U; // Latin-1 symboles, ponctuation générale
''',
     D, 'a_du_contenu_ponctuation_et_lettres',
     '« … » compte comme du contenu'),
    ('decoupe_phrases.hpp',
     r'''    while (!s.empty() && est_blanc_phrase(s.front()))
        s.remove_prefix(1);
''',
     r'''
''',
     D, 'blancs_de_bord_retires',
     'les blancs de début restent'),
    ('decoupe_phrases.hpp',
     r'''                return p + 1;
''',
     r'''                return p;
''',
     D, 'coupe_forcee_prefere_la_virgule',
     'coupe forcée avant la virgule'),
    ('decoupe_phrases.hpp',
     r'''        for (std::size_t p = n_; p-- > moitie;) {
''',
     r'''        for (std::size_t p = n_; p-- > 0;) {
''',
     D, 'coupe_forcee_ignore_les_separateurs_de_la_premiere_moitie',
     'la coupe forcée cherche une espace dans la première moitié'),
    ('decoupe_phrases.hpp',
     r'''        for (std::size_t p = n_ - 1; p-- > moitie;) { // p = n_-2 … moitie
''',
     r'''        for (std::size_t p = n_ - 1; p-- > 0;) { // p = n_-2 … moitie
''',
     D, 'coupe_forcee_ignore_les_separateurs_de_la_premiere_moitie',
     'la coupe forcée cherche une virgule dans la première moitié'),
    ('decoupe_phrases.hpp',
     r'''        const std::size_t entier = utf8::coupe_sans_casser(std::string_view(buf_, n_), n_);
''',
     r'''        const std::size_t entier = n_;
''',
     D, 'coupe_forcee_sans_espace_au_dernier_caractere_entier',
     'la coupe forcée casse un caractère UTF-8'),
    ('decoupe_phrases.hpp',
     r'''        if (c == '"' || c == '\'' || c == ')' || c == ']')
''',
     r'''        if (c == '"' || c == '\'' || c == ']')
''',
     D, 'guillemets_et_parentheses_fermants_restent_avec_la_phrase',
     'la parenthèse fermante passe à la phrase suivante'),
    ('decoupe_phrases.hpp',
     r'''               static_cast<unsigned char>(buf_[i + 1]) == 0x80U && static_cast<unsigned char>(buf_[i + 2]) == 0xA6U;
''',
     r'''               static_cast<unsigned char>(buf_[i + 1]) == 0x80U && static_cast<unsigned char>(buf_[i + 2]) == 0xA7U;
''',
     D, 'points_de_suspension_et_ponctuation_multiple',
     "« … » n'est plus un terminateur"),
    ('decoupe_phrases.hpp',
     r'''        emettre(sur_phrase, n_);
''',
     r'''
''',
     D, 'phrases_simples',
     "la fin de réponse n'est plus émise"),
    ('decoupe_phrases.hpp',
     r'''            if (c != '.' && c != '!' && c != '?' && !ellipse_en(i)) {
''',
     r'''            if (c != '.' && c != '!' && !ellipse_en(i)) {
''',
     D, 'phrases_simples',
     '« ? » ne termine plus une phrase'),
    # --- assistant_texte.hpp ---------------------------------------------------------------------------------
    ('assistant_texte.hpp',
     r'''constexpr bool valide(std::string_view s) noexcept {
''',
     r'''constexpr bool valide_absente(std::string_view s) noexcept {
''',
     T, None,
     "la fonction utf8::valide n'existe pas"),
    ('assistant_texte.hpp',
     r'''constexpr std::size_t encoder(std::uint32_t cp, char *sortie) noexcept {
''',
     r'''constexpr std::size_t encoder_absente(std::uint32_t cp, char *sortie) noexcept {
''',
     T, None,
     "la fonction utf8::encoder n'existe pas"),
    ('assistant_texte.hpp',
     r'''    if (b >= 0xC2U && b <= 0xDFU)
''',
     r'''    if (b >= 0xC0U && b <= 0xDFU)
''',
     T, 'longueur_attendue_d_apres_le_premier_octet',
     '0xC0 accepté comme octet de tête'),
    ('assistant_texte.hpp',
     r'''        if (b0 == 0xE0U && b1 < 0xA0U)
            return 0; // sur-longue
''',
     r'''
''',
     T, 'sequences_valides_et_invalides',
     'sur-longue sur 3 octets acceptée'),
    ('assistant_texte.hpp',
     r'''        if (b0 == 0xEDU && b1 >= 0xA0U)
            return 0; // surrogate U+D800 à U+DFFF
''',
     r'''
''',
     T, 'sequences_valides_et_invalides',
     'surrogate encodé accepté'),
    ('assistant_texte.hpp',
     r'''        if (b0 == 0xF0U && b1 < 0x90U)
            return 0; // sur-longue
''',
     r'''
''',
     T, 'sequences_valides_et_invalides',
     'sur-longue sur 4 octets acceptée'),
    ('assistant_texte.hpp',
     r'''        if (b0 == 0xF4U && b1 >= 0x90U)
            return 0; // au-delà de U+10FFFF
''',
     r'''''',
     T, 'sequences_valides_et_invalides',
     'U+110000 accepté'),
    ('assistant_texte.hpp',
     r'''    if (cp >= 0xD800U && cp <= 0xDFFFU)
        return 0;
''',
     r'''
''',
     T, 'encodage_aller_retour_sur_les_bornes',
     "encodage d'un surrogate accepté"),
    ('assistant_texte.hpp',
     r'''    if (cp > 0x10FFFFU)
        return 0;
''',
     r'''
''',
     T, 'encodage_aller_retour_sur_les_bornes',
     'encodage au-delà de U+10FFFF accepté'),
    ('assistant_texte.hpp',
     r'''    if (cp < 0x800U) {
''',
     r'''    if (cp < 0x7FFU) {
''',
     T, 'encodage_aller_retour_sur_les_bornes',
     'U+07FF encodé sur 3 octets'),
    ('assistant_texte.hpp',
     r'''    return tete + attendu <= n ? n : tete;
''',
     r'''    return n;
''',
     T, 'coupe_sans_casser_un_caractere',
     "coupe au milieu d'un caractère"),
    ('assistant_texte.hpp',
     r'''        if (deborde_ || s.size() > cap_ - n_) {
''',
     r'''        if (deborde_ || s.size() + 1 > cap_ - n_) {
''',
     T, 'tampon_ecrit_sans_deborder',
     'le tampon perd un octet de capacité'),
    ('assistant_texte.hpp',
     r'''        } while (v != 0);
''',
     r'''        } while (v != 0 && k < 19);
''',
     T, 'tampon_ecrit_des_nombres',
     'nombres de 20 chiffres tronqués'),
]
