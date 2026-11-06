// Noyau pur : quelle icône pour quel temps. Code météo (WMO) + jour/nuit -> composition de pièces teintées, et choix de la
// taille d'image. C++17 standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation (test/host/test_visuel_icones.cpp).
//
// Les dessins sont dans main/assets/icones_meteo.bin, écrit par tools/icones/generer_icones.py, qui écrit AUSSI la table
// visuel_icones_table.inc incluse ici : les identifiants des pièces et la composition de chaque icône n'existent donc qu'à un
// endroit (le script), jamais recopiés à la main dans le code. Une icône = 1 à 3 pièces empilées ; chaque pièce a un RÔLE de
// teinte (soleil, nuage, pluie...) que le thème (main/palette_principal.h) traduit en couleur : changer de thème ne touche
// pas aux images.
//
// Contrat avec meteo_wmo.hpp : `Icone` ne distingue le jour de la nuit que pour les codes 0 à 2. Ici, les averses (80 à 82)
// et les averses de neige (85, 86) montrent AUSSI un soleil ou une lune derrière le nuage, d'où le paramètre `jour`.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/meteo_wmo.hpp"

namespace ecran::visuel {

#include "ecran/visuel_icones_table.inc"

// Rôle de teinte d'une pièce. L'ordre ne compte pas pour le binaire (seul le thème l'utilise).
enum class Role : std::uint8_t { AUCUN, soleil, lune, nuage, nuage_sombre, pluie, neige, eclair, brume, NOMBRE };

enum class Famille : std::uint8_t { METEO, INFO, LUNE };

// Identifiant d'une pièce : l'ordre est celui du binaire (jamais réordonné par le script sans changer la version).
enum class Piece : std::uint16_t {
#define X(nom, famille) nom,
    ECRAN_VISUEL_PIECES(X)
#undef X
        AUCUNE,
    NOMBRE = AUCUNE
};

inline constexpr Famille FAMILLES_PIECES[] = {
#define X(nom, famille) Famille::famille,
    ECRAN_VISUEL_PIECES(X)
#undef X
};

constexpr Famille famille(Piece piece) noexcept { return FAMILLES_PIECES[static_cast<std::size_t>(piece)]; }

// Les 8 phases de lune sont des pièces consécutives (phase_00 = nouvelle lune ... phase_04 = pleine lune).
constexpr Piece piece_phase_lune(int indice) noexcept {
    const int i = ((indice % NB_PHASES_LUNE) + NB_PHASES_LUNE) % NB_PHASES_LUNE;
    return static_cast<Piece>(static_cast<int>(Piece::phase_00) + i);
}

enum class IdIcone : std::uint8_t {
#define X(nom, nb, p1, r1, p2, r2, p3, r3) nom,
    ECRAN_VISUEL_ICONES(X)
#undef X
        NOMBRE
};

struct Couche {
    Piece piece;
    Role role;
};

struct Composition {
    std::uint8_t nb; // 1 à 3 couches utiles
    Couche couches[3];
};

inline constexpr Composition COMPOSITIONS[] = {
#define X(nom, nb, p1, r1, p2, r2, p3, r3) {nb, {{Piece::p1, Role::r1}, {Piece::p2, Role::r2}, {Piece::p3, Role::r3}}},
    ECRAN_VISUEL_ICONES(X)
#undef X
};

constexpr const Composition &composition(IdIcone id) noexcept { return COMPOSITIONS[static_cast<std::size_t>(id)]; }

// Icône d'un temps. `jour` : champ is_day d'Open-Meteo (la nuit, les averses montrent une lune).
constexpr IdIcone icone_pour(meteo::Icone icone, bool jour) noexcept {
    switch (icone) {
    case meteo::Icone::Inconnue:
        return IdIcone::inconnue;
    case meteo::Icone::Soleil:
        return IdIcone::soleil_jour;
    case meteo::Icone::Lune:
        return IdIcone::lune_nuit;
    case meteo::Icone::PeuNuageuxJour:
        return IdIcone::peu_nuageux_jour;
    case meteo::Icone::PeuNuageuxNuit:
        return IdIcone::peu_nuageux_nuit;
    case meteo::Icone::NuageuxJour:
        return IdIcone::nuageux_jour;
    case meteo::Icone::NuageuxNuit:
        return IdIcone::nuageux_nuit;
    case meteo::Icone::Couvert:
        return IdIcone::couvert;
    case meteo::Icone::Brouillard:
        return IdIcone::brouillard;
    case meteo::Icone::Bruine:
        return IdIcone::bruine;
    case meteo::Icone::PluieVerglacante:
        return IdIcone::pluie_verglacante;
    case meteo::Icone::Pluie:
        return IdIcone::pluie;
    case meteo::Icone::PluieForte:
        return IdIcone::pluie_forte;
    case meteo::Icone::Neige:
        return IdIcone::neige;
    case meteo::Icone::NeigeForte:
        return IdIcone::neige_forte;
    case meteo::Icone::GrainsDeNeige:
        return IdIcone::grains_de_neige;
    case meteo::Icone::Averses:
        return jour ? IdIcone::averses_jour : IdIcone::averses_nuit;
    case meteo::Icone::AversesViolentes:
        return IdIcone::averses_violentes;
    case meteo::Icone::AversesDeNeige:
        return jour ? IdIcone::averses_de_neige_jour : IdIcone::averses_de_neige_nuit;
    case meteo::Icone::Orage:
        return IdIcone::orage;
    case meteo::Icone::OrageGrele:
        return IdIcone::orage_grele;
    case meteo::Icone::Nombre:
        break;
    }
    return IdIcone::inconnue;
}

// Icône d'un code WMO d'Open-Meteo (un code inconnu donne le nuage seul).
constexpr IdIcone icone_pour_code(int code_wmo, bool jour) noexcept {
    return icone_pour(meteo::decrire(code_wmo, jour).icone, jour);
}

// Plus grande taille de `tailles` (croissantes) qui ne dépasse pas `cible` ; la plus petite si aucune ne tient ; 0 si la liste
// est vide. L'écran ne met jamais une image plus grande que la place prévue : une marge de plus vaut mieux qu'un débordement.
constexpr int choisir_taille(const int *tailles, std::size_t n, int cible) noexcept {
    if (tailles == nullptr || n == 0)
        return 0;
    int choix = tailles[0];
    for (std::size_t i = 0; i < n; ++i) {
        if (tailles[i] <= cible)
            choix = tailles[i];
    }
    return choix;
}

constexpr int taille_meteo(int cible) noexcept {
    return choisir_taille(TAILLES_METEO, sizeof(TAILLES_METEO) / sizeof(TAILLES_METEO[0]), cible);
}
constexpr int taille_info(int cible) noexcept {
    return choisir_taille(TAILLES_INFO, sizeof(TAILLES_INFO) / sizeof(TAILLES_INFO[0]), cible);
}
constexpr int taille_lune(int cible) noexcept {
    return choisir_taille(TAILLES_LUNE, sizeof(TAILLES_LUNE) / sizeof(TAILLES_LUNE[0]), cible);
}

} // namespace ecran::visuel
