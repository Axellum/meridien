// Noyau pur : machine à états de l'assistant vocal (prêt, écoute, transcription, réflexion, réponse, erreur).
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation (les tests de test/host/ s'en servent).
//
// Pilotée par une TABLE état × événement (`TABLE`), construite à la compilation à partir d'une liste de règles ;
// toute paire absente de la liste est « ignorée » : l'état ne change pas et rien n'est à faire. Garanties
// (vérifiées sur toutes les paires par test/host/test_assistant_etats.cpp) : aucune transition vers un état
// inconnu ; l'appui sur l'écran ramène à « prêt » depuis tout état actif ; chaque état actif a un délai non nul
// qui le fait sortir ; une erreur revient à « prêt » après son délai ; donc aucun blocage possible.
//
// Cascade STT -> LLM -> TTS (ADR-0002) : la machine ne fait ni réseau ni audio. Elle dit seulement dans quel
// état on est, quoi faire (`Action`), quel texte afficher et pendant combien de temps attendre au plus.
//
// Délais : valeurs de DÉPART, non mesurées sur l'appareil (à régler après les essais sur carte). Chaque délai est
// un délai d'INACTIVITÉ : l'appelant le réarme après tout événement pris en compte (`Resultat::pris_en_compte`).
//
// Les textes sont en UTF-8 : sous MSVC, compiler avec /utf-8.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ecran::assistant {

enum class Etat : std::uint8_t {
    Pret = 0,       // attend le mot d'activation
    Ecoute,         // capture la voix de l'utilisateur
    Transcription,  // STT : l'audio est envoyé, on attend le texte
    Reflexion,      // LLM : le texte est envoyé, on attend le premier morceau de réponse
    Reponse,        // la réponse arrive et est lue (TTS)
    Erreur,         // un des trois services a échoué : message affiché jusqu'au délai
    Nombre          // sentinelle, pas un état
};

enum class Evenement : std::uint8_t {
    MotActivation = 0,
    FinDeParole,     // l'utilisateur a fini de parler (VAD) ou la durée maximale est atteinte
    TexteReconnu,    // le STT a rendu un texte non vide
    RienReconnu,     // le STT a rendu un texte vide (silence, bruit) : ajout par rapport à la liste de la mission
    MorceauReponse,  // un morceau de réponse du LLM est arrivé
    FinDeLecture,    // le flux du LLM est terminé ET la lecture est finie
    AppuiEcran,      // interruption par l'utilisateur
    DelaiDepasse,    // le minuteur de l'état courant a expiré
    ErreurStt,
    ErreurLlm,
    ErreurTts,
    Nombre
};

// Quel service a échoué (titre affiché au-dessus de la phrase). `Aucun` : cause inconnue ou commune.
enum class Service : std::uint8_t { Aucun = 0, Stt, Llm, Tts, Nombre };

// Cause d'une erreur d'un service : une phrase de docs/api-reglages.md §8 par cause, jamais un code.
enum class Cause : std::uint8_t {
    Aucune = 0,          // non précisée : la machine prend la cause par défaut de la transition
    NeRepondPas,         // connexion refusée, délai dépassé
    AdresseIntrouvable,  // nom d'hôte inconnu
    CertificatRefuse,    // TLS
    CleRefusee,          // 401, 403
    MauvaiseAdresse,     // 404 sur la route d'un échange réel : il manque peut-être « /v1 »
    ModeleInconnu,       // 404 ou 400 qui parle du modèle
    Limite,              // 429
    ProblemeDuService,   // 5xx
    ReponseInattendue,   // pas du JSON, pas d'audio, réponse vide, autre statut
    Nombre
};

// Ce que l'appelant doit faire en réaction (l'unique effet de bord de la machine, c'est lui qui l'exécute).
enum class Action : std::uint8_t {
    Aucune = 0,
    DemarrerCapture,  // ouvrir le micro et démarrer le VAD
    EnvoyerAudio,     // arrêter la capture et envoyer l'audio au STT
    EnvoyerAuLlm,     // envoyer le texte reconnu au LLM (flux)
    ToutArreter,      // couper capture, requêtes réseau en cours et lecture
    Nombre
};

constexpr std::size_t NB_ETATS = static_cast<std::size_t>(Etat::Nombre);
constexpr std::size_t NB_EVENEMENTS = static_cast<std::size_t>(Evenement::Nombre);
constexpr std::size_t NB_CAUSES = static_cast<std::size_t>(Cause::Nombre);

struct Transition {
    Etat suivant;
    Action action;
    Service service;  // renseigné seulement si `suivant == Erreur`
    Cause cause;      // idem : cause par défaut, remplacée par celle que l'appelant signale
    bool pris;        // faux : événement sans objet dans cet état (état inchangé, rien à faire)
};

namespace detail {

struct TableTransitions {
    Transition t[NB_ETATS][NB_EVENEMENTS];
};

constexpr void regle(TableTransitions& tb, Etat de, Evenement sur, Etat vers, Action action,
                     Service service = Service::Aucun, Cause cause = Cause::Aucune) noexcept {
    tb.t[static_cast<std::size_t>(de)][static_cast<std::size_t>(sur)] = {vers, action, service, cause, true};
}

constexpr TableTransitions fabriquer_table() noexcept {
    TableTransitions tb{};
    for (std::size_t s = 0; s < NB_ETATS; ++s) {
        for (std::size_t e = 0; e < NB_EVENEMENTS; ++e) {
            tb.t[s][e] = {static_cast<Etat>(s), Action::Aucune, Service::Aucun, Cause::Aucune, false};  // ignorer
        }
    }
    using E = Evenement;
    using S = Etat;
    using A = Action;
    // Prêt : seul le mot d'activation réveille. Tout le reste (dont une erreur tardive d'une requête annulée) est ignoré.
    regle(tb, S::Pret, E::MotActivation, S::Ecoute, A::DemarrerCapture);
    // Écoute. Un délai ici = personne n'a parlé : retour silencieux à « prêt », ce n'est pas une panne.
    regle(tb, S::Ecoute, E::FinDeParole, S::Transcription, A::EnvoyerAudio);
    regle(tb, S::Ecoute, E::AppuiEcran, S::Pret, A::ToutArreter);
    regle(tb, S::Ecoute, E::DelaiDepasse, S::Pret, A::ToutArreter);
    // Transcription (STT).
    regle(tb, S::Transcription, E::TexteReconnu, S::Reflexion, A::EnvoyerAuLlm);
    regle(tb, S::Transcription, E::RienReconnu, S::Pret, A::Aucune);
    regle(tb, S::Transcription, E::AppuiEcran, S::Pret, A::ToutArreter);
    regle(tb, S::Transcription, E::DelaiDepasse, S::Erreur, A::ToutArreter, Service::Stt, Cause::NeRepondPas);
    regle(tb, S::Transcription, E::ErreurStt, S::Erreur, A::ToutArreter, Service::Stt, Cause::NeRepondPas);
    // Réflexion (LLM, avant le premier morceau). Un flux terminé sans aucun morceau = réponse vide = erreur.
    regle(tb, S::Reflexion, E::MorceauReponse, S::Reponse, A::Aucune);
    regle(tb, S::Reflexion, E::FinDeLecture, S::Erreur, A::ToutArreter, Service::Llm, Cause::ReponseInattendue);
    regle(tb, S::Reflexion, E::AppuiEcran, S::Pret, A::ToutArreter);
    regle(tb, S::Reflexion, E::DelaiDepasse, S::Erreur, A::ToutArreter, Service::Llm, Cause::NeRepondPas);
    regle(tb, S::Reflexion, E::ErreurLlm, S::Erreur, A::ToutArreter, Service::Llm, Cause::NeRepondPas);
    // Réponse (le LLM écrit, le TTS lit). Un délai ici ne désigne pas de coupable : LLM ou TTS.
    regle(tb, S::Reponse, E::MorceauReponse, S::Reponse, A::Aucune);
    regle(tb, S::Reponse, E::FinDeLecture, S::Pret, A::Aucune);
    regle(tb, S::Reponse, E::AppuiEcran, S::Pret, A::ToutArreter);
    regle(tb, S::Reponse, E::DelaiDepasse, S::Erreur, A::ToutArreter, Service::Aucun, Cause::NeRepondPas);
    regle(tb, S::Reponse, E::ErreurLlm, S::Erreur, A::ToutArreter, Service::Llm, Cause::NeRepondPas);
    regle(tb, S::Reponse, E::ErreurTts, S::Erreur, A::ToutArreter, Service::Tts, Cause::NeRepondPas);
    // Erreur : le mot d'activation relance tout de suite ; un appui ou le délai ramènent à « prêt ».
    regle(tb, S::Erreur, E::MotActivation, S::Ecoute, A::DemarrerCapture);
    regle(tb, S::Erreur, E::AppuiEcran, S::Pret, A::Aucune);
    regle(tb, S::Erreur, E::DelaiDepasse, S::Pret, A::Aucune);
    return tb;
}

}  // namespace detail

inline constexpr detail::TableTransitions TABLE = detail::fabriquer_table();

constexpr const Transition& transition(Etat etat, Evenement evenement) noexcept {
    return TABLE.t[static_cast<std::size_t>(etat)][static_cast<std::size_t>(evenement)];
}

// Délai d'inactivité maximal de l'état, en millisecondes ; 0 = pas de minuteur (seul « prêt »).
// Valeurs de départ, NON mesurées sur l'appareil.
constexpr std::uint32_t delai_ms(Etat etat) noexcept {
    switch (etat) {
        case Etat::Pret: return 0;
        case Etat::Ecoute: return 15000;
        case Etat::Transcription: return 20000;
        case Etat::Reflexion: return 30000;
        case Etat::Reponse: return 120000;
        case Etat::Erreur: return 5000;
        case Etat::Nombre: break;
    }
    return 0;
}

// Texte court de l'état, en français avec accents. Pour « erreur », voir `texte_erreur` (dépend de la cause).
constexpr std::string_view texte(Etat etat) noexcept {
    switch (etat) {
        case Etat::Pret: return "Prêt";
        case Etat::Ecoute: return "Je vous écoute…";
        case Etat::Transcription: return "Je transcris…";
        case Etat::Reflexion: return "Je réfléchis…";
        case Etat::Reponse: return "Je réponds…";
        case Etat::Erreur: return "Un problème est survenu.";
        case Etat::Nombre: break;
    }
    return "";
}

// Phrases de docs/api-reglages.md §8 (cause -> message). Jamais un code, jamais le texte renvoyé par le service.
constexpr std::string_view texte_erreur(Cause cause) noexcept {
    switch (cause) {
        case Cause::NeRepondPas: return "Le service ne répond pas. Est-il lancé et sur le même réseau ?";
        case Cause::AdresseIntrouvable: return "Cette adresse est introuvable.";
        case Cause::CertificatRefuse: return "Le certificat de ce service n'est pas reconnu.";
        case Cause::CleRefusee: return "Le service refuse la clé. Vérifiez-la.";
        case Cause::MauvaiseAdresse:
            return "Cette adresse ne ressemble pas à un service compatible. Il manque peut-être « /v1 ».";
        case Cause::ModeleInconnu: return "Ce modèle n'existe pas sur ce service.";
        case Cause::Limite: return "Le service limite les appels. Réessayez dans un instant.";
        case Cause::ProblemeDuService: return "Le service a un problème de son côté.";
        case Cause::ReponseInattendue: return "Le service a répondu autre chose que prévu.";
        case Cause::Aucune:
        case Cause::Nombre: break;
    }
    return "Un problème est survenu.";
}

// Titre au-dessus de la phrase d'erreur : les noms des trois cartes de la page de réglages.
constexpr std::string_view titre_service(Service service) noexcept {
    switch (service) {
        case Service::Stt: return "Comprendre ma voix";
        case Service::Llm: return "Réfléchir";
        case Service::Tts: return "Parler";
        case Service::Aucun:
        case Service::Nombre: break;
    }
    return "";
}

// Statut HTTP d'un service -> cause (docs/api-reglages.md §8). `echange_reel` : la route appelée est celle d'un
// vrai échange (chat/completions, audio/speech, audio/transcriptions) et non `/models`, dont un 404/405 n'est pas
// une erreur. `mentionne_le_modele` : le message d'erreur du service parle du modèle (voir extrait_openai.hpp).
constexpr Cause cause_depuis_http(int statut, bool echange_reel = true, bool mentionne_le_modele = false) noexcept {
    if (statut == 401 || statut == 403) return Cause::CleRefusee;
    if ((statut == 404 || statut == 400) && mentionne_le_modele) return Cause::ModeleInconnu;
    if (statut == 404 && echange_reel) return Cause::MauvaiseAdresse;
    if (statut == 429) return Cause::Limite;
    if (statut >= 500 && statut <= 599) return Cause::ProblemeDuService;
    return Cause::ReponseInattendue;
}

struct Resultat {
    Etat avant;
    Etat apres;
    Action action;
    bool pris_en_compte;  // faux : événement ignoré dans cet état ; vrai : l'appelant réarme le minuteur de `apres`
};

class Machine {
public:
    constexpr Etat etat() const noexcept { return etat_; }
    constexpr Service service_en_erreur() const noexcept { return service_; }
    constexpr Cause cause() const noexcept { return cause_; }

    // Texte à l'écran pour l'état courant (la phrase de la cause en état d'erreur).
    constexpr std::string_view texte_affiche() const noexcept {
        return etat_ == Etat::Erreur ? texte_erreur(cause_) : texte(etat_);
    }

    // `cause_signalee` : pour ErreurStt / ErreurLlm / ErreurTts, la cause constatée par l'appelant (voir
    // `cause_depuis_http`) ; `Aucune` garde la cause par défaut de la transition.
    constexpr Resultat traiter(Evenement evenement, Cause cause_signalee = Cause::Aucune) noexcept {
        const Etat avant = etat_;
        const Transition& t = transition(avant, evenement);
        if (!t.pris) return {avant, avant, Action::Aucune, false};  // ignoré : ni l'état ni la cause d'erreur ne bougent
        etat_ = t.suivant;
        if (t.suivant == Etat::Erreur) {
            service_ = t.service;
            cause_ = (cause_signalee != Cause::Aucune && cause_signalee != Cause::Nombre) ? cause_signalee : t.cause;
        } else {
            service_ = Service::Aucun;
            cause_ = Cause::Aucune;
        }
        return {avant, t.suivant, t.action, t.pris};
    }

private:
    Etat etat_ = Etat::Pret;
    Service service_ = Service::Aucun;
    Cause cause_ = Cause::Aucune;
};

}  // namespace ecran::assistant
