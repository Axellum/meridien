// Noyau pur : suivi d'un CHANGEMENT DE WI-FI avec retour arrière (ADR-0007, point 7). Sans cela, une faute de frappe sur le mot de
// passe rend l'écran inatteignable sans refaire l'installation : si la nouvelle connexion n'aboutit pas dans un délai court,
// l'appareil reprend l'ancien réseau et le dit à l'écran.
//
// Ce fichier ne fait que SUIVRE l'essai pour l'afficher (progression, délai, cause, phrase). Reprendre l'ancien réseau est le
// travail de la couche réseau (main/reseau.*, session jalon-1b) ; le délai de 30 s est une valeur de départ « à régler à la
// mesure » (ADR-0007), pas une mesure.
// C++17 standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation.
#pragma once

#include <cstdint>

namespace ecran::wifi {

inline constexpr std::int64_t DELAI_CONNEXION_MS = 30000;

enum class Etat : std::uint8_t { Inactif, Connexion, Reussi, Echec };

enum class Cause : std::uint8_t { Aucune, MotDePasse, Introuvable, Delai, Autre };

// Ce que la couche réseau annonce.
enum class Resultat : std::uint8_t { Connecte, MotDePasseRefuse, ReseauIntrouvable, Autre };

struct Changement {
    Etat etat = Etat::Inactif;
    Cause cause = Cause::Aucune;
    std::int64_t debut_ms = 0;
    bool avait_un_reseau = false; // il y avait un réseau à reprendre : l'échec dit « l'ancien réseau est conservé »
};

constexpr Changement demarrer(std::int64_t maintenant_ms, bool avait_un_reseau) noexcept {
    return {Etat::Connexion, Cause::Aucune, maintenant_ms, avait_un_reseau};
}

// Le résultat n'est pris en compte que pendant l'essai (un résultat tardif après le délai ne change rien).
constexpr Changement sur_resultat(Changement c, Resultat r) noexcept {
    if (c.etat != Etat::Connexion)
        return c;
    switch (r) {
    case Resultat::Connecte:
        c.etat = Etat::Reussi;
        c.cause = Cause::Aucune;
        break;
    case Resultat::MotDePasseRefuse:
        c.etat = Etat::Echec;
        c.cause = Cause::MotDePasse;
        break;
    case Resultat::ReseauIntrouvable:
        c.etat = Etat::Echec;
        c.cause = Cause::Introuvable;
        break;
    case Resultat::Autre:
        c.etat = Etat::Echec;
        c.cause = Cause::Autre;
        break;
    }
    return c;
}

// Appelé à chaque rafraîchissement : passé le délai, l'essai est un échec « sans réponse ». Une horloge qui recule est lue comme
// un délai écoulé (mieux vaut reprendre l'ancien réseau que rester bloqué).
constexpr Changement sur_temps(Changement c, std::int64_t maintenant_ms) noexcept {
    if (c.etat == Etat::Connexion && (maintenant_ms < c.debut_ms || maintenant_ms - c.debut_ms >= DELAI_CONNEXION_MS)) {
        c.etat = Etat::Echec;
        c.cause = Cause::Delai;
    }
    return c;
}

constexpr int secondes_restantes(const Changement &c, std::int64_t maintenant_ms) noexcept {
    if (c.etat != Etat::Connexion)
        return 0;
    const std::int64_t ecoule = maintenant_ms < c.debut_ms ? DELAI_CONNEXION_MS : maintenant_ms - c.debut_ms;
    if (ecoule >= DELAI_CONNEXION_MS)
        return 0;
    return static_cast<int>((DELAI_CONNEXION_MS - ecoule + 999) / 1000); // arrondi par excès : « 1 s » jusqu'à zéro
}

constexpr bool termine(const Changement &c) noexcept { return c.etat == Etat::Reussi || c.etat == Etat::Echec; }

} // namespace ecran::wifi
