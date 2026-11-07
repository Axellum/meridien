// Noyau pur : machine à états de l'ESSAI D'UN NOUVEAU WI-FI avec RETOUR ARRIÈRE automatique (ADR-0007, point 7).
//
// Pourquoi : une faute de frappe sur le mot de passe ne doit jamais couper l'appareil du réseau qui marchait. L'essai est
// borné (DELAI_ESSAI_MS, 30 s) ; s'il échoue, la couche réseau remet l'ANCIEN réseau et seulement alors dit « échec » à l'écran.
// Ce fichier ne fait que DÉCIDER (que faire à chaque événement) ; l'exécution (esp_wifi_set_config, esp_wifi_connect, minuteurs,
// rappel) est dans main/reseau.cpp, qui n'est testé que sur la carte. C++17 standard, aucun include ESP-IDF, évalué à la compilation.
//
// Règles :
//  - Tous les événements arrivent dans le MÊME contexte (la boucle d'événements du système) : cette machine n'a aucun verrou.
//  - Une cause « sûre » (mot de passe refusé, réseau introuvable) arrête l'essai plus tôt que le délai, mais seulement quand la
//    MÊME cause a été vue SEUIL_PREUVES fois de suite : un seul échec isolé (brouillage, balise manquée) ne condamne pas un bon mot
//    de passe. Une cause « autre » n'arrête jamais l'essai : seul le délai le fait.
//  - La déconnexion que NOUS provoquons (quitter l'ancien réseau pour essayer le nouveau, code 8) n'est pas un échec de l'essai.
//  - Après un abandon (la personne a quitté l'écran), le retour arrière a lieu quand même mais le rappel n'est plus appelé.
//  - Rien ici ne voit le SSID ni le mot de passe.
#pragma once

#include <cstdint>

#include "ecran/changement_wifi.hpp"

namespace ecran::wifi {

// Combien de temps on laisse à l'ancien réseau pour revenir (adresse IP) avant de rendre quand même la main à l'écran.
// Valeur de départ, à régler par la mesure sur la carte (docs/mesures/wifi-retour-arriere.md).
inline constexpr std::int64_t DELAI_RETOUR_MS = 20000;
// Délai de l'essai : celui que l'écran affiche (changement_wifi.hpp), pour que les deux comptes à rebours coïncident.
inline constexpr std::int64_t DELAI_ESSAI_MS = DELAI_CONNEXION_MS;
// Une même cause sûre vue ce nombre de fois de suite arrête l'essai sans attendre le délai.
inline constexpr std::uint8_t SEUIL_PREUVES = 2;

// Codes de déconnexion de l'événement WIFI_EVENT_STA_DISCONNECTED (wifi_err_reason_t, esp_wifi_types_generic.h, ESP-IDF 5.5.5,
// lus dans les sources). Seuls ceux dont on tire une décision sont nommés.
namespace raison {
inline constexpr int FIN_D_ASSOCIATION = 8; // WIFI_REASON_ASSOC_LEAVE : nous avons quitté le réseau nous-mêmes
inline constexpr int MIC_ECHEC = 14;        // WIFI_REASON_MIC_FAILURE
inline constexpr int POIGNEE_DE_MAIN_DELAI =
    15; // WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT : cas classique d'un mot de passe faux
inline constexpr int AUCUN_POINT_D_ACCES = 201;   // WIFI_REASON_NO_AP_FOUND
inline constexpr int AUTHENTIFICATION = 202;      // WIFI_REASON_AUTH_FAIL
inline constexpr int POIGNEE_TEMPS = 204;         // WIFI_REASON_HANDSHAKE_TIMEOUT
inline constexpr int SECURITE_INCOMPATIBLE = 210; // WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY
inline constexpr int MODE_AUTH_SEUIL = 211;       // WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD
inline constexpr int RSSI_SEUIL = 212;            // WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD
} // namespace raison

// Ce que la cause d'une déconnexion dit à la personne. « Autre » : on ne sait pas (ou pas assez pour condamner l'essai).
constexpr Resultat resultat_pour_raison(int code) noexcept {
    switch (code) {
    case raison::MIC_ECHEC:
    case raison::POIGNEE_DE_MAIN_DELAI:
    case raison::AUTHENTIFICATION:
    case raison::POIGNEE_TEMPS:
        return Resultat::MotDePasseRefuse;
    case raison::AUCUN_POINT_D_ACCES:
    case raison::SECURITE_INCOMPATIBLE:
    case raison::MODE_AUTH_SEUIL:
    case raison::RSSI_SEUIL:
        return Resultat::ReseauIntrouvable;
    default:
        return Resultat::Autre;
    }
}

enum class Phase : std::uint8_t {
    Repos, // aucun essai
    Essai, // le nouveau réseau est en cours d'essai ; l'ancien est quitté
    Retour // l'essai a échoué ou a été abandonné : l'ancien réseau est remis, on attend son adresse IP
};

struct Essai {
    Phase phase = Phase::Repos;
    bool avait_ancien = false; // il y avait un réseau à reprendre
    bool muet = false;         // abandon : plus de rappel
    bool cause_connue = false; // au moins une déconnexion expliquée vue pendant l'essai
    Resultat derniere_cause = Resultat::Autre;
    Resultat attendu = Resultat::Autre; // ce que dira le rappel quand l'ancien réseau sera revenu
    std::uint8_t preuves = 0;           // nombre de fois de suite que derniere_cause a été vue
};

// Ce que la couche réseau doit faire après un événement. Plusieurs drapeaux peuvent être vrais en même temps (par exemple
// « adopter » puis « rappeler »).
struct Sortie {
    Essai etat;
    bool appliquer_nouveau =
        false; // quitter l'ancien réseau, configurer le nouveau, se connecter, armer le minuteur de l'essai
    bool appliquer_ancien = false; // reconfigurer l'ancien réseau, se connecter, armer le minuteur du retour
    bool adopter_nouveau =
        false;             // le nouveau réseau devient le réseau courant (c'est lui qu'on reprendra au prochain essai)
    bool rappeler = false; // appeler le rappel de l'écran avec `resultat`
    Resultat resultat = Resultat::Autre;
};

constexpr bool occupe(const Essai &e) noexcept { return e.phase != Phase::Repos; }

namespace detail {
// L'essai est perdu pour la cause `r` : on reprend l'ancien réseau s'il y en avait un, sinon on le dit tout de suite.
constexpr Sortie echec(Essai e, Resultat r) noexcept {
    Sortie s;
    if (e.avait_ancien) {
        e.phase = Phase::Retour;
        e.attendu = r;
        s.appliquer_ancien = true;
    } else {
        e.phase = Phase::Repos;
        s.rappeler = !e.muet;
        s.resultat = r;
    }
    s.etat = e;
    return s;
}
} // namespace detail

// La personne demande un essai. Refusé (état inchangé, aucune action) s'il y en a déjà un.
constexpr Sortie demarrer(Essai e, bool avait_ancien) noexcept {
    Sortie s;
    if (e.phase != Phase::Repos) {
        s.etat = e;
        return s;
    }
    e = Essai{};
    e.phase = Phase::Essai;
    e.avait_ancien = avait_ancien;
    s.etat = e;
    s.appliquer_nouveau = true;
    return s;
}

// Une adresse IP est obtenue (réseau en cours d'essai, ou ancien réseau revenu).
constexpr Sortie sur_adresse_ip(Essai e) noexcept {
    Sortie s;
    if (e.phase == Phase::Essai) {
        e.phase = Phase::Repos;
        s.adopter_nouveau = true;
        s.rappeler = !e.muet;
        s.resultat = Resultat::Connecte;
    } else if (e.phase == Phase::Retour) {
        e.phase = Phase::Repos;
        s.rappeler = !e.muet;
        s.resultat = e.attendu;
    }
    s.etat = e;
    return s;
}

// Une déconnexion du Wi-Fi, avec son code. Seul l'essai en cours en tire quelque chose.
constexpr Sortie sur_deconnexion(Essai e, int code) noexcept {
    Sortie s;
    s.etat = e;
    if (e.phase != Phase::Essai || code == raison::FIN_D_ASSOCIATION) {
        return s;
    }
    const Resultat cause = resultat_pour_raison(code);
    if (e.cause_connue && e.derniere_cause == cause) {
        if (e.preuves < 255) {
            ++e.preuves;
        }
    } else {
        e.cause_connue = true;
        e.derniere_cause = cause;
        e.preuves = 1;
    }
    if (cause != Resultat::Autre && e.preuves >= SEUIL_PREUVES) {
        return detail::echec(e, cause);
    }
    s.etat = e;
    return s;
}

// Le délai de l'essai est écoulé sans adresse IP.
constexpr Sortie sur_delai_essai(Essai e) noexcept {
    if (e.phase != Phase::Essai) {
        Sortie s;
        s.etat = e;
        return s;
    }
    return detail::echec(e, e.cause_connue ? e.derniere_cause : Resultat::Autre);
}

// L'ancien réseau n'est pas revenu dans le délai du retour : on rend quand même la main (le Wi-Fi continue de le chercher).
constexpr Sortie sur_delai_retour(Essai e) noexcept {
    Sortie s;
    if (e.phase == Phase::Retour) {
        e.phase = Phase::Repos;
        s.rappeler = !e.muet;
        s.resultat = e.attendu;
    }
    s.etat = e;
    return s;
}

// La personne quitte l'écran (« Retour », « Annuler », ou son propre délai) : on reprend l'ancien réseau, sans rappel.
constexpr Sortie abandonner(Essai e) noexcept {
    Sortie s;
    if (e.phase == Phase::Essai) {
        e.muet = true;
        if (e.avait_ancien) {
            e.phase = Phase::Retour;
            s.appliquer_ancien = true;
        } else {
            e.phase = Phase::Repos;
        }
    } else if (e.phase == Phase::Retour) {
        e.muet = true;
    }
    s.etat = e;
    return s;
}

} // namespace ecran::wifi
