// Noyau pur : parcours d'un tableau JSON élément par élément, en un seul passage. C++17 standard, aucun include ESP-IDF.
//
// `json::Valeur::element(i)` repart du début du tableau à chaque appel : lire les n éléments coûte n²/2 sauts. Les réponses de
// MET Norway (≈ 90 éléments de 1 Ko) et du NWS (≈ 140 intervalles par couche) sont assez longues pour que ça compte sur la
// carte ; ce parcours lit chaque élément une fois. Il s'appuie sur les mêmes fonctions de json_mini.hpp que `element()` et rend
// les mêmes valeurs.
#pragma once

#include <cstddef>
#include <string_view>

#include "ecran/json_mini.hpp"

namespace ecran::json {

class Parcours {
  public:
    // `tableau` : une valeur déjà validée par `json::lire`. Autre chose qu'un tableau : le parcours est vide.
    constexpr explicit Parcours(const Valeur &tableau) noexcept
        : brut_(tableau.brut()), p_(tableau.genre() == Genre::Tableau ? 1 : tableau.brut().size()) {}

    // Place l'élément suivant dans `v` et rend vrai ; rend faux à la fin du tableau.
    constexpr bool suivant(Valeur &v) noexcept {
        p_ = detail::sauter_blancs(brut_, p_);
        if (p_ >= brut_.size() || brut_[p_] == ']') {
            p_ = brut_.size();
            return false;
        }
        const std::size_t fin = detail::fin_valeur(brut_, p_);
        v = Valeur(brut_.substr(p_, fin - p_), detail::genre_de(brut_[p_]));
        p_ = detail::sauter_blancs(brut_, fin);
        if (p_ < brut_.size() && brut_[p_] == ',') {
            ++p_;
        } else {
            p_ = brut_.size(); // dernier élément
        }
        return true;
    }

  private:
    std::string_view brut_;
    std::size_t p_;
};

} // namespace ecran::json
