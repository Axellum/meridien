// Noyau pur : ce que font les boutons de la page « La mer » de l'écran de réglages (ADR-0026) hors de la liste des spots (elle est dans
// mer_spots.hpp : ajouter, retirer, choisir). C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
//
// Les boutons « − » et « + » de la plage de vent du kite : chacun décale UNE borne d'un nœud. Le décalage passe par `regler_vent`, donc
// par les mêmes règles que la page web et que l'API (0 < min < max <= 60, nœuds entiers) : un bouton dont le décalage serait refusé est
// grisé (`vent_decalable`), il n'envoie jamais un réglage que l'appareil refuserait.
#pragma once

#include <cstdint>

#include "ecran/mer_spots.hpp"

namespace ecran::mer::reglage {

enum class Borne : std::uint8_t {
    Min, // le vent le plus faible qui convient
    Max, // le vent le plus fort qui convient
};

inline constexpr int PAS_VENT_NOEUDS = 1;

namespace detail {

// Les deux bornes après le décalage de `borne` de `pas` nœuds (sans rien vérifier).
constexpr void decale(const Registre &r, Borne borne, int pas, std::int64_t &mini, std::int64_t &maxi) noexcept {
    mini = r.vent_min_kn;
    maxi = r.vent_max_kn;
    if (borne == Borne::Min)
        mini += pas;
    else
        maxi += pas;
}

} // namespace detail

// Le décalage donne-t-il une plage de vent valide ?
constexpr bool vent_decalable(const Registre &r, Borne borne, int pas) noexcept {
    std::int64_t mini = 0;
    std::int64_t maxi = 0;
    detail::decale(r, borne, pas, mini, maxi);
    return plage_de_vent_valide(mini, maxi);
}

// Décale la borne ; `ErreurRegistre::Vent` et rien de changé si la plage ne serait pas valide.
constexpr ErreurRegistre decaler_vent(Registre &r, Borne borne, int pas) noexcept {
    std::int64_t mini = 0;
    std::int64_t maxi = 0;
    detail::decale(r, borne, pas, mini, maxi);
    return regler_vent(r, mini, maxi);
}

} // namespace ecran::mer::reglage
