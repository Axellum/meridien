// Noyau pur : règles de l'INSTALLATION GUIDÉE (ADR-0007, points 5 et 6). Sept étapes dans l'ordre (langue, Wi-Fi, lieu, affichage,
// assistant, téléphone, récapitulatif) ; l'étape en cours se garde en mémoire permanente et un redémarrage au milieu reprend à la
// même étape ; seules la langue et le Wi-Fi (ou son abandon explicite) bloquent la suite. « Refaire l'installation » relance la
// séquence AVEC les valeurs actuelles ; « Réinitialiser l'appareil » efface tout, après une confirmation en deux gestes : ce sont
// deux commandes distinctes, l'une n'est jamais l'autre.
// C++17 standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/i18n.hpp"
#include "ecran/registre_reglages.hpp"

namespace ecran::installation {

using registre::Etape;
using registre::NOMBRE_ETAPES;

// Première : carte neuve ou réinitialisée (pas de sortie possible avant la fin). Refaire : lancée depuis les réglages, avec les
// valeurs actuelles préremplies et une sortie à tout moment.
enum class Mode : std::uint8_t { Premiere, Refaire };

struct Progression {
    Etape etape = Etape::Langue;
    bool terminee = false; // le drapeau « installation terminée » décide de l'écran affiché au démarrage
    Mode mode = Mode::Premiere;
};

constexpr bool operator==(const Progression &a, const Progression &b) noexcept {
    return a.etape == b.etape && a.terminee == b.terminee && a.mode == b.mode;
}

// ---- mémoire permanente : UN octet ---------------------------------------------------------------------------------------
//   bits 0 à 3 : étape en cours (1 à 7), ou 8 = installation terminée ; bit 4 : mode « refaire ».
// 0, une valeur hors liste ou une valeur écrite par une version plus récente donnent « première installation, étape 1 » : un
// stockage abîmé ne doit jamais faire passer pour terminée une installation qui ne l'est pas, ni bloquer l'appareil.
inline constexpr std::uint8_t CODE_TERMINEE = 8;
inline constexpr std::uint8_t BIT_REFAIRE = 0x10;

constexpr std::uint8_t encoder(const Progression &p) noexcept {
    const std::uint8_t base = p.terminee ? CODE_TERMINEE : static_cast<std::uint8_t>(p.etape);
    return static_cast<std::uint8_t>(base | (p.mode == Mode::Refaire ? BIT_REFAIRE : 0));
}

constexpr Progression decoder(long long v) noexcept {
    if (v < 0 || v > 0x1F)
        return {};
    const Mode mode = (v & BIT_REFAIRE) != 0 ? Mode::Refaire : Mode::Premiere;
    const int bas = static_cast<int>(v & 0x0F);
    if (bas == CODE_TERMINEE)
        return {Etape::Langue, true, mode};
    if (bas >= 1 && bas <= NOMBRE_ETAPES)
        return {static_cast<Etape>(bas), false, mode};
    return {};
}

// Quel écran au démarrage : l'installation, ou l'écran principal.
constexpr bool installation_a_montrer(const Progression &p) noexcept { return !p.terminee; }

// ---- déplacement entre les étapes -------------------------------------------------------------------------------------

constexpr int numero(Etape e) noexcept { return static_cast<int>(e); }

constexpr Etape suivante(Etape e) noexcept {
    return e == Etape::Recapitulatif || e == Etape::Aucune ? e : static_cast<Etape>(numero(e) + 1);
}

constexpr Etape precedente(Etape e) noexcept {
    return e == Etape::Langue || e == Etape::Aucune ? e : static_cast<Etape>(numero(e) - 1);
}

// Un retour arrière est possible partout sauf à la première étape (rien avant).
constexpr bool retour_possible(Etape e) noexcept { return e != Etape::Langue && e != Etape::Aucune; }

// Étapes qu'on peut laisser pour plus tard (« Plus tard »).
constexpr bool facultative(Etape e) noexcept { return e == Etape::Assistant || e == Etape::Telephone; }

// Ce que l'écran sait au moment de dire « Continuer ».
struct Contexte {
    bool langue_choisie = false; // la personne a touché une langue (même celle déjà en place)
    bool wifi_connecte = false;
    bool wifi_abandonne = false; // « Continuer sans Wi-Fi », choisi explicitement
};

// Seules la langue et le Wi-Fi (ou son abandon explicite) bloquent la suite.
constexpr bool peut_continuer(Etape e, const Contexte &c, Mode mode) noexcept {
    switch (e) {
    case Etape::Langue:
        return c.langue_choisie || mode == Mode::Refaire; // refaire : une langue est déjà en place
    case Etape::Wifi:
        return c.wifi_connecte || c.wifi_abandonne;
    default:
        return true;
    }
}

// Changer la langue de l'écran pendant la PREMIÈRE installation entraîne celle de l'assistant, si elle était la même qu'avant le
// changement (ADR-0006 : les deux réglages restent distincts, ce n'est qu'un point de départ). En « refaire » ou dans les
// réglages, on ne touche pas à une langue d'assistant que la personne a choisie.
constexpr i18n::Langue langue_assistant_apres_choix(Mode mode, i18n::Langue ancienne_ecran, i18n::Langue assistant,
                                                    i18n::Langue nouvelle_ecran) noexcept {
    return mode == Mode::Premiere && assistant == ancienne_ecran ? nouvelle_ecran : assistant;
}

// ---- transitions de la progression -------------------------------------------------------------------------------------

// « Refaire l'installation guidée » : la séquence repart de la première étape, les valeurs ne sont pas touchées (elles ne sont pas
// dans la progression).
constexpr Progression refaire(const Progression &) noexcept { return {Etape::Langue, false, Mode::Refaire}; }

// « Réinitialiser l'appareil » : l'appareil retrouve ses valeurs par défaut (l'hôte parcourt le registre) et la séquence repart.
constexpr Progression apres_reinitialisation() noexcept { return {Etape::Langue, false, Mode::Premiere}; }

constexpr Progression terminer(const Progression &p) noexcept { return {Etape::Langue, true, p.mode}; }

// L'étape quittée devient la suivante : c'est ce qui est écrit en mémoire permanente à chaque « Continuer ».
constexpr Progression avancer(const Progression &p) noexcept {
    if (p.etape == Etape::Recapitulatif)
        return terminer(p);
    return {suivante(p.etape), false, p.mode};
}

constexpr Progression reculer(const Progression &p) noexcept { return {precedente(p.etape), false, p.mode}; }

// « Quitter » l'installation relancée depuis les réglages : la progression redevient « terminée ». Refusé en première installation.
constexpr bool sortie_libre(Mode mode) noexcept { return mode == Mode::Refaire; }

// ---- réinitialisation : confirmation en deux gestes ---------------------------------------------------------------------

// Premier toucher : le bouton change de texte et attend. Second toucher dans le délai : exécute. Sans second toucher, l'attente
// retombe seule. « Annuler » la retombe tout de suite. Sans cette sécurité un seul geste efface tout.
inline constexpr std::int64_t DELAI_CONFIRMATION_MS = 5000;

enum class Confirmation : std::uint8_t { Repos, Armee };
enum class Geste : std::uint8_t { Toucher, Annuler, Temps };

struct EtatConfirmation {
    Confirmation etat = Confirmation::Repos;
    std::int64_t depuis_ms = 0;
};

struct ResultatConfirmation {
    EtatConfirmation etat;
    bool executer; // vrai une seule fois, au second toucher
};

constexpr ResultatConfirmation confirmer(const EtatConfirmation &e, Geste geste, std::int64_t maintenant_ms) noexcept {
    const bool expire = e.etat == Confirmation::Armee &&
                        (maintenant_ms < e.depuis_ms || maintenant_ms - e.depuis_ms >= DELAI_CONFIRMATION_MS);
    switch (geste) {
    case Geste::Annuler:
        return {{Confirmation::Repos, 0}, false};
    case Geste::Temps:
        return expire ? ResultatConfirmation{{Confirmation::Repos, 0}, false} : ResultatConfirmation{e, false};
    case Geste::Toucher:
        if (e.etat == Confirmation::Armee && !expire)
            return {{Confirmation::Repos, 0}, true};
        return {{Confirmation::Armee, maintenant_ms}, false};
    }
    return {{Confirmation::Repos, 0}, false};
}

} // namespace ecran::installation
