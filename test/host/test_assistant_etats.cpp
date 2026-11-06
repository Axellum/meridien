// Tests du noyau pur : machine à états de l'assistant vocal (table état × événement).
#include "ecran/assistant_etats.hpp"
#include "mini.hpp"

using namespace ecran::assistant;

namespace {

constexpr std::size_t NE = NB_ETATS;
constexpr std::size_t NV = NB_EVENEMENTS;

constexpr Etat etat_n(std::size_t i) { return static_cast<Etat>(i); }
constexpr Evenement ev_n(std::size_t i) { return static_cast<Evenement>(i); }

constexpr bool egal(std::string_view a, std::string_view b) { return a == b; }

constexpr bool est_actif(Etat e) { return e != Etat::Pret && e != Etat::Erreur; }

// Nombre de caractères (et non d'octets) d'un texte UTF-8.
constexpr std::size_t caracteres(std::string_view s) {
    std::size_t n = 0;
    for (char c : s) {
        if ((static_cast<unsigned char>(c) & 0xC0U) != 0x80U) ++n;
    }
    return n;
}

constexpr bool a_un_non_ascii(std::string_view s) {
    for (char c : s) {
        if (static_cast<unsigned char>(c) >= 0x80U) return true;
    }
    return false;
}

// Applique un événement à un état isolé (machine neuve placée dans cet état par un chemin légal).
constexpr Machine machine_dans(Etat cible) {
    Machine m;
    switch (cible) {
        case Etat::Pret: break;
        case Etat::Ecoute: m.traiter(Evenement::MotActivation); break;
        case Etat::Transcription:
            m.traiter(Evenement::MotActivation);
            m.traiter(Evenement::FinDeParole);
            break;
        case Etat::Reflexion:
            m.traiter(Evenement::MotActivation);
            m.traiter(Evenement::FinDeParole);
            m.traiter(Evenement::TexteReconnu);
            break;
        case Etat::Reponse:
            m.traiter(Evenement::MotActivation);
            m.traiter(Evenement::FinDeParole);
            m.traiter(Evenement::TexteReconnu);
            m.traiter(Evenement::MorceauReponse);
            break;
        case Etat::Erreur:
            m.traiter(Evenement::MotActivation);
            m.traiter(Evenement::FinDeParole);
            m.traiter(Evenement::ErreurStt);
            break;
        case Etat::Nombre: break;
    }
    return m;
}

constexpr int le_jeu_d_etats_et_d_evenements_est_celui_attendu() {
    VERIFIER(NE == 6);
    VERIFIER(NV == 11);
    VERIFIER(NB_CAUSES == 10);
    for (std::size_t i = 0; i < NE; ++i) VERIFIER(machine_dans(etat_n(i)).etat() == etat_n(i));
    return 0;
}

constexpr int aucune_transition_vers_un_etat_inconnu() {
    // Toutes les paires état × événement : état suivant dans le jeu, action dans le jeu, cohérence service/cause.
    for (std::size_t s = 0; s < NE; ++s) {
        for (std::size_t e = 0; e < NV; ++e) {
            const Transition& t = transition(etat_n(s), ev_n(e));
            VERIFIER(static_cast<std::size_t>(t.suivant) < NE);
            VERIFIER(static_cast<std::size_t>(t.action) < static_cast<std::size_t>(Action::Nombre));
            VERIFIER(static_cast<std::size_t>(t.service) < static_cast<std::size_t>(Service::Nombre));
            VERIFIER(static_cast<std::size_t>(t.cause) < NB_CAUSES);
            if (t.pris && t.suivant == Etat::Erreur) {
                VERIFIER(t.cause != Cause::Aucune);  // une erreur a toujours une phrase à afficher
            } else {
                VERIFIER(t.service == Service::Aucun && t.cause == Cause::Aucune);
            }
            if (!t.pris) {  // ignoré : ne bouge pas et ne déclenche rien
                VERIFIER(t.suivant == etat_n(s));
                VERIFIER(t.action == Action::Aucune);
            }
        }
    }
    return 0;
}

constexpr int la_machine_suit_la_table_pour_toutes_les_paires() {
    for (std::size_t s = 0; s < NE; ++s) {
        for (std::size_t e = 0; e < NV; ++e) {
            Machine m = machine_dans(etat_n(s));
            const Resultat r = m.traiter(ev_n(e));
            const Transition& t = transition(etat_n(s), ev_n(e));
            VERIFIER(r.avant == etat_n(s));
            VERIFIER(r.apres == t.suivant && m.etat() == t.suivant);
            VERIFIER(r.action == t.action);
            VERIFIER(r.pris_en_compte == t.pris);
        }
    }
    return 0;
}

constexpr int cheminement_nominal_de_l_echange() {
    Machine m;
    VERIFIER(m.etat() == Etat::Pret);
    Resultat r = m.traiter(Evenement::MotActivation);
    VERIFIER(r.apres == Etat::Ecoute && r.action == Action::DemarrerCapture);
    r = m.traiter(Evenement::FinDeParole);
    VERIFIER(r.apres == Etat::Transcription && r.action == Action::EnvoyerAudio);
    r = m.traiter(Evenement::TexteReconnu);
    VERIFIER(r.apres == Etat::Reflexion && r.action == Action::EnvoyerAuLlm);
    r = m.traiter(Evenement::MorceauReponse);
    VERIFIER(r.apres == Etat::Reponse && r.pris_en_compte);
    r = m.traiter(Evenement::MorceauReponse);  // les morceaux suivants gardent l'état ET réarment le minuteur
    VERIFIER(r.apres == Etat::Reponse && r.pris_en_compte && r.action == Action::Aucune);
    r = m.traiter(Evenement::FinDeLecture);
    VERIFIER(r.apres == Etat::Pret);
    return 0;
}

constexpr int l_interruption_ramene_a_pret_depuis_tout_etat_actif() {
    for (std::size_t s = 0; s < NE; ++s) {
        if (!est_actif(etat_n(s))) continue;
        Machine m = machine_dans(etat_n(s));
        const Resultat r = m.traiter(Evenement::AppuiEcran);
        VERIFIER(r.apres == Etat::Pret);
        VERIFIER(r.action == Action::ToutArreter);  // coupe requêtes, capture et lecture
        VERIFIER(r.pris_en_compte);
    }
    return 0;
}

constexpr int l_interruption_fonctionne_depuis_reflexion_et_reponse() {
    for (Etat cible : {Etat::Reflexion, Etat::Reponse}) {
        Machine m = machine_dans(cible);
        VERIFIER(m.etat() == cible);
        const Resultat r = m.traiter(Evenement::AppuiEcran);
        VERIFIER(r.apres == Etat::Pret && r.action == Action::ToutArreter);
        VERIFIER(m.cause() == Cause::Aucune && m.service_en_erreur() == Service::Aucun);
        // et la machine est de nouveau utilisable
        VERIFIER(m.traiter(Evenement::MotActivation).apres == Etat::Ecoute);
    }
    return 0;
}

constexpr int appui_en_pret_ne_fait_rien_et_appui_en_erreur_ferme_le_message() {
    Machine m;
    const Resultat r = m.traiter(Evenement::AppuiEcran);
    VERIFIER(r.apres == Etat::Pret && !r.pris_en_compte && r.action == Action::Aucune);
    Machine e = machine_dans(Etat::Erreur);
    VERIFIER(e.etat() == Etat::Erreur);
    VERIFIER(e.traiter(Evenement::AppuiEcran).apres == Etat::Pret);
    return 0;
}

constexpr int une_erreur_revient_a_pret_apres_son_delai() {
    VERIFIER(delai_ms(Etat::Erreur) > 0);
    // Quelles que soient la cause et la façon d'y arriver, DelaiDepasse ramène à « prêt ».
    for (std::size_t c = 1; c < NB_CAUSES; ++c) {
        for (Evenement ev : {Evenement::ErreurStt, Evenement::ErreurLlm, Evenement::ErreurTts}) {
            for (std::size_t s = 0; s < NE; ++s) {
                Machine m = machine_dans(etat_n(s));
                const Resultat r = m.traiter(ev, static_cast<Cause>(c));
                if (!r.pris_en_compte) continue;
                VERIFIER(m.cause() == static_cast<Cause>(c));
                const Resultat fin = m.traiter(Evenement::DelaiDepasse);
                VERIFIER(fin.apres == Etat::Pret && fin.action == Action::Aucune);
                VERIFIER(m.cause() == Cause::Aucune && m.service_en_erreur() == Service::Aucun);
            }
        }
    }
    return 0;
}

constexpr int quelle_erreur_dans_quel_etat() {
    // ErreurStt : seulement en transcription ; ErreurLlm : réflexion et réponse ; ErreurTts : réponse.
    // Ailleurs c'est une réponse tardive d'une requête déjà annulée : ignorée (pas de fausse erreur à l'écran).
    for (std::size_t s = 0; s < NE; ++s) {
        const Etat e = etat_n(s);
        VERIFIER(transition(e, Evenement::ErreurStt).pris == (e == Etat::Transcription));
        VERIFIER(transition(e, Evenement::ErreurLlm).pris == (e == Etat::Reflexion || e == Etat::Reponse));
        VERIFIER(transition(e, Evenement::ErreurTts).pris == (e == Etat::Reponse));
    }
    Machine m = machine_dans(Etat::Transcription);
    VERIFIER(m.traiter(Evenement::ErreurStt, Cause::CleRefusee).apres == Etat::Erreur);
    VERIFIER(m.service_en_erreur() == Service::Stt && m.cause() == Cause::CleRefusee);
    m = machine_dans(Etat::Reflexion);
    m.traiter(Evenement::ErreurLlm, Cause::ModeleInconnu);
    VERIFIER(m.service_en_erreur() == Service::Llm && m.cause() == Cause::ModeleInconnu);
    m = machine_dans(Etat::Reponse);
    m.traiter(Evenement::ErreurTts, Cause::Limite);
    VERIFIER(m.service_en_erreur() == Service::Tts && m.cause() == Cause::Limite);
    return 0;
}

constexpr int un_evenement_ignore_en_erreur_garde_la_cause_affichee() {
    Machine m = machine_dans(Etat::Transcription);
    m.traiter(Evenement::ErreurStt, Cause::CleRefusee);
    for (Evenement ev : {Evenement::FinDeParole, Evenement::TexteReconnu, Evenement::RienReconnu, Evenement::MorceauReponse,
                         Evenement::FinDeLecture, Evenement::ErreurStt, Evenement::ErreurLlm, Evenement::ErreurTts}) {
        const Resultat r = m.traiter(ev, Cause::Limite);  // réponse tardive d'une requête annulée
        VERIFIER(!r.pris_en_compte && r.apres == Etat::Erreur && r.action == Action::Aucune);
        VERIFIER(m.cause() == Cause::CleRefusee && m.service_en_erreur() == Service::Stt);
    }
    return 0;
}

constexpr int cause_par_defaut_quand_l_appelant_n_en_donne_pas() {
    Machine m = machine_dans(Etat::Transcription);
    m.traiter(Evenement::ErreurStt);
    VERIFIER(m.cause() == Cause::NeRepondPas);
    m = machine_dans(Etat::Reflexion);
    m.traiter(Evenement::FinDeLecture);  // flux terminé sans un seul morceau
    VERIFIER(m.etat() == Etat::Erreur && m.service_en_erreur() == Service::Llm);
    VERIFIER(m.cause() == Cause::ReponseInattendue);
    m = machine_dans(Etat::Reponse);
    m.traiter(Evenement::DelaiDepasse);
    VERIFIER(m.etat() == Etat::Erreur && m.service_en_erreur() == Service::Aucun && m.cause() == Cause::NeRepondPas);
    return 0;
}

constexpr int les_delais_font_sortir_de_tout_etat_actif() {
    VERIFIER(delai_ms(Etat::Pret) == 0);  // « prêt » attend le mot d'activation sans fin : c'est voulu
    for (std::size_t s = 0; s < NE; ++s) {
        const Etat e = etat_n(s);
        if (e == Etat::Pret) continue;
        VERIFIER(delai_ms(e) >= 1000 && delai_ms(e) <= 300000);  // un état sans minuteur pourrait bloquer
        const Transition& t = transition(e, Evenement::DelaiDepasse);
        VERIFIER(t.pris);                         // le délai est traité partout
        VERIFIER(t.suivant != e);                 // et fait bouger
        VERIFIER(t.suivant == Etat::Pret || t.suivant == Etat::Erreur);
    }
    return 0;
}

constexpr int pas_de_blocage_possible_pret_atteignable_depuis_tout_etat() {
    // Par DelaiDepasse seul : au plus 2 pas (actif -> erreur -> prêt). Par AppuiEcran : 1 pas depuis actif.
    for (std::size_t s = 0; s < NE; ++s) {
        Etat e = etat_n(s);
        int pas = 0;
        while (e != Etat::Pret && pas < 10) {
            e = transition(e, Evenement::DelaiDepasse).suivant;
            ++pas;
        }
        VERIFIER(e == Etat::Pret && pas <= 2);
    }
    return 0;
}

constexpr int tous_les_etats_sont_atteignables_depuis_pret() {
    bool vu[NE] = {};
    vu[0] = true;
    for (std::size_t tour = 0; tour < NE; ++tour) {
        for (std::size_t s = 0; s < NE; ++s) {
            if (!vu[s]) continue;
            for (std::size_t e = 0; e < NV; ++e) vu[static_cast<std::size_t>(transition(etat_n(s), ev_n(e)).suivant)] = true;
        }
    }
    for (std::size_t s = 0; s < NE; ++s) VERIFIER(vu[s]);
    return 0;
}

constexpr int toute_suite_d_evenements_de_longueur_4_retombe_a_pret() {
    // Exhaustif : 11^4 = 14641 suites depuis « prêt » ; quel que soit l'état atteint, des délais ramènent à « prêt »
    // en 2 pas au plus, et l'appui sur l'écran le fait en 1 pas depuis un état actif.
    constexpr std::size_t TOTAL = NV * NV * NV * NV;
    for (std::size_t n = 0; n < TOTAL; ++n) {
        Machine m;
        std::size_t reste = n;
        for (int k = 0; k < 4; ++k) {
            m.traiter(ev_n(reste % NV), (reste % 3 == 0) ? Cause::CleRefusee : Cause::Aucune);
            reste /= NV;
            VERIFIER(static_cast<std::size_t>(m.etat()) < NE);
        }
        if (est_actif(m.etat())) {
            Machine copie = m;
            VERIFIER(copie.traiter(Evenement::AppuiEcran).apres == Etat::Pret);
        }
        int pas = 0;
        while (m.etat() != Etat::Pret && pas < 5) {
            m.traiter(Evenement::DelaiDepasse);
            ++pas;
        }
        VERIFIER(m.etat() == Etat::Pret && pas <= 2);
    }
    return 0;
}

constexpr int pret_n_est_reveille_que_par_le_mot_d_activation() {
    for (std::size_t e = 0; e < NV; ++e) {
        const Transition& t = transition(Etat::Pret, ev_n(e));
        if (ev_n(e) == Evenement::MotActivation) {
            VERIFIER(t.pris && t.suivant == Etat::Ecoute && t.action == Action::DemarrerCapture);
        } else {
            VERIFIER(!t.pris && t.suivant == Etat::Pret);
        }
    }
    return 0;
}

constexpr int delai_en_ecoute_est_un_retour_silencieux_pas_une_panne() {
    Machine m = machine_dans(Etat::Ecoute);
    const Resultat r = m.traiter(Evenement::DelaiDepasse);
    VERIFIER(r.apres == Etat::Pret && r.action == Action::ToutArreter);
    return 0;
}

constexpr int texte_vide_reconnu_ramene_a_pret_sans_erreur() {
    Machine m = machine_dans(Etat::Transcription);
    const Resultat r = m.traiter(Evenement::RienReconnu);
    VERIFIER(r.apres == Etat::Pret && r.action == Action::Aucune && m.cause() == Cause::Aucune);
    return 0;
}

constexpr int mot_d_activation_en_erreur_relance_l_ecoute() {
    Machine m = machine_dans(Etat::Erreur);
    const Resultat r = m.traiter(Evenement::MotActivation);
    VERIFIER(r.apres == Etat::Ecoute && r.action == Action::DemarrerCapture);
    VERIFIER(m.cause() == Cause::Aucune);
    return 0;
}

constexpr int textes_courts_francais_avec_accents_et_sans_code() {
    constexpr std::string_view ATTENDUS[] = {"Prêt", "Je vous écoute…", "Je transcris…", "Je réfléchis…", "Je réponds…",
                                             "Un problème est survenu."};
    for (std::size_t s = 0; s < NE; ++s) {
        const std::string_view t = texte(etat_n(s));
        VERIFIER(egal(t, ATTENDUS[s]));
        VERIFIER(caracteres(t) <= 30);                    // une ligne d'écran
        VERIFIER(t.front() >= 'A' && t.front() <= 'Z');   // majuscule
        for (char c : t) VERIFIER(!(c >= '0' && c <= '9'));
        for (std::size_t j = s + 1; j < NE; ++j) VERIFIER(!egal(t, texte(etat_n(j))));
    }
    VERIFIER(a_un_non_ascii(texte(Etat::Pret)) && a_un_non_ascii(texte(Etat::Ecoute)));
    VERIFIER(a_un_non_ascii(texte(Etat::Reflexion)) && a_un_non_ascii(texte(Etat::Reponse)));
    VERIFIER(egal(texte(Etat::Nombre), ""));
    return 0;
}

constexpr int phrases_d_erreur_exactes_de_l_api_paragraphe_8() {
    struct Attendu {
        Cause cause;
        std::string_view phrase;
    };
    constexpr Attendu ATTENDUS[] = {
        {Cause::NeRepondPas, "Le service ne répond pas. Est-il lancé et sur le même réseau ?"},
        {Cause::AdresseIntrouvable, "Cette adresse est introuvable."},
        {Cause::CertificatRefuse, "Le certificat de ce service n'est pas reconnu."},
        {Cause::CleRefusee, "Le service refuse la clé. Vérifiez-la."},
        {Cause::MauvaiseAdresse,
         "Cette adresse ne ressemble pas à un service compatible. Il manque peut-être « /v1 »."},
        {Cause::ModeleInconnu, "Ce modèle n'existe pas sur ce service."},
        {Cause::Limite, "Le service limite les appels. Réessayez dans un instant."},
        {Cause::ProblemeDuService, "Le service a un problème de son côté."},
        {Cause::ReponseInattendue, "Le service a répondu autre chose que prévu."},
    };
    VERIFIER(sizeof(ATTENDUS) / sizeof(ATTENDUS[0]) == NB_CAUSES - 1);
    for (std::size_t i = 0; i < NB_CAUSES - 1; ++i) {
        VERIFIER(egal(texte_erreur(ATTENDUS[i].cause), ATTENDUS[i].phrase));
        VERIFIER(ATTENDUS[i].cause == static_cast<Cause>(i + 1));
        for (std::size_t j = i + 1; j < NB_CAUSES - 1; ++j) VERIFIER(!egal(ATTENDUS[i].phrase, ATTENDUS[j].phrase));
        // jamais un code HTTP brut : pas trois chiffres de suite
        int suite = 0;
        for (char c : ATTENDUS[i].phrase) {
            suite = (c >= '0' && c <= '9') ? suite + 1 : 0;
            VERIFIER(suite < 3);
        }
    }
    VERIFIER(egal(texte_erreur(Cause::Aucune), "Un problème est survenu."));
    return 0;
}

constexpr int l_ecran_affiche_la_phrase_de_la_cause_en_erreur() {
    Machine m = machine_dans(Etat::Pret);
    VERIFIER(egal(m.texte_affiche(), "Prêt"));
    m = machine_dans(Etat::Reflexion);
    VERIFIER(egal(m.texte_affiche(), "Je réfléchis…"));
    m.traiter(Evenement::ErreurLlm, Cause::CleRefusee);
    VERIFIER(egal(m.texte_affiche(), "Le service refuse la clé. Vérifiez-la."));
    m.traiter(Evenement::DelaiDepasse);
    VERIFIER(egal(m.texte_affiche(), "Prêt"));
    return 0;
}

constexpr int titres_des_services_comme_dans_la_page_de_reglages() {
    VERIFIER(egal(titre_service(Service::Stt), "Comprendre ma voix"));
    VERIFIER(egal(titre_service(Service::Llm), "Réfléchir"));
    VERIFIER(egal(titre_service(Service::Tts), "Parler"));
    VERIFIER(egal(titre_service(Service::Aucun), ""));
    return 0;
}

constexpr int statut_http_vers_cause() {
    VERIFIER(cause_depuis_http(401) == Cause::CleRefusee);
    VERIFIER(cause_depuis_http(403) == Cause::CleRefusee);
    VERIFIER(cause_depuis_http(404) == Cause::MauvaiseAdresse);
    VERIFIER(cause_depuis_http(404, false) == Cause::ReponseInattendue);  // /models : pas une erreur de route
    VERIFIER(cause_depuis_http(404, true, true) == Cause::ModeleInconnu);
    VERIFIER(cause_depuis_http(400, true, true) == Cause::ModeleInconnu);
    VERIFIER(cause_depuis_http(400) == Cause::ReponseInattendue);
    VERIFIER(cause_depuis_http(429) == Cause::Limite);
    VERIFIER(cause_depuis_http(500) == Cause::ProblemeDuService);
    VERIFIER(cause_depuis_http(503) == Cause::ProblemeDuService);
    VERIFIER(cause_depuis_http(599) == Cause::ProblemeDuService);
    VERIFIER(cause_depuis_http(200) == Cause::ReponseInattendue);
    VERIFIER(cause_depuis_http(0) == Cause::ReponseInattendue);
    VERIFIER(cause_depuis_http(600) == Cause::ReponseInattendue);
    VERIFIER(cause_depuis_http(-1) == Cause::ReponseInattendue);
    return 0;
}

constexpr int delais_de_depart_documentes() {
    VERIFIER(delai_ms(Etat::Ecoute) == 15000);
    VERIFIER(delai_ms(Etat::Transcription) == 20000);
    VERIFIER(delai_ms(Etat::Reflexion) == 30000);
    VERIFIER(delai_ms(Etat::Reponse) == 120000);
    VERIFIER(delai_ms(Etat::Erreur) == 5000);
    VERIFIER(delai_ms(Etat::Nombre) == 0);
    return 0;
}

}  // namespace

#define LISTE(X)                                                        \
    X(le_jeu_d_etats_et_d_evenements_est_celui_attendu)                 \
    X(aucune_transition_vers_un_etat_inconnu)                           \
    X(la_machine_suit_la_table_pour_toutes_les_paires)                  \
    X(cheminement_nominal_de_l_echange)                                 \
    X(l_interruption_ramene_a_pret_depuis_tout_etat_actif)              \
    X(l_interruption_fonctionne_depuis_reflexion_et_reponse)            \
    X(appui_en_pret_ne_fait_rien_et_appui_en_erreur_ferme_le_message)   \
    X(une_erreur_revient_a_pret_apres_son_delai)                        \
    X(quelle_erreur_dans_quel_etat)                                     \
    X(un_evenement_ignore_en_erreur_garde_la_cause_affichee)            \
    X(cause_par_defaut_quand_l_appelant_n_en_donne_pas)                 \
    X(les_delais_font_sortir_de_tout_etat_actif)                        \
    X(pas_de_blocage_possible_pret_atteignable_depuis_tout_etat)        \
    X(tous_les_etats_sont_atteignables_depuis_pret)                     \
    X(toute_suite_d_evenements_de_longueur_4_retombe_a_pret)            \
    X(pret_n_est_reveille_que_par_le_mot_d_activation)                  \
    X(delai_en_ecoute_est_un_retour_silencieux_pas_une_panne)           \
    X(texte_vide_reconnu_ramene_a_pret_sans_erreur)                     \
    X(mot_d_activation_en_erreur_relance_l_ecoute)                      \
    X(textes_courts_francais_avec_accents_et_sans_code)                 \
    X(phrases_d_erreur_exactes_de_l_api_paragraphe_8)                   \
    X(l_ecran_affiche_la_phrase_de_la_cause_en_erreur)                  \
    X(titres_des_services_comme_dans_la_page_de_reglages)               \
    X(statut_http_vers_cause)                                           \
    X(delais_de_depart_documentes)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
