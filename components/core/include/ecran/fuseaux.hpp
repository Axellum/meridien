// Noyau pur : fuseau IANA (« Europe/Paris », donné par le géocodage d'Open-Meteo) -> chaîne POSIX TZ
// (« CET-1CEST,M3.5.0,M10.5.0/3 ») pour setenv("TZ", ...) + tzset(). ESP-IDF n'embarque pas la base IANA.
//
// Données : posix_tz_db (MIT) + 5 compléments tirés de la base tz, voir components/core/ATTRIBUTION.md.
// Table générée par outils/generer_fuseaux.py dans fuseaux_donnees.inc (triée par octets sur le nom).
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation (les tests de test/host/ s'en servent).
//
// Limite connue : posix_tz_db ne sait écrire que des règles POSIX ; une zone dont les règles ne s'expriment
// pas ainsi (ex. Africa/Casablanca, dont le décalage change à l'heure du Ramadan) y est donnée sans changement
// d'heure. Comportement réel de tzset() de newlib sur les formes « <-03>3 » : non vérifié sur l'appareil.
#pragma once

#include <cstddef>
#include <string_view>

namespace ecran::fuseaux {

struct Entree {
    std::string_view iana;   // « Europe/Paris »
    std::string_view posix;  // « CET-1CEST,M3.5.0,M10.5.0/3 »
};

inline constexpr Entree TABLE[] = {
#include "fuseaux_donnees.inc"
};
inline constexpr std::size_t NOMBRE = sizeof(TABLE) / sizeof(TABLE[0]);

// Recherche exacte par nom IANA (la casse compte : « europe/paris » n'existe pas). nullptr si inconnu.
constexpr const Entree* trouver(std::string_view iana) noexcept {
    std::size_t bas = 0;
    std::size_t haut = NOMBRE;
    while (bas < haut) {
        const std::size_t milieu = bas + (haut - bas) / 2;
        const int c = TABLE[milieu].iana.compare(iana);
        if (c == 0) return &TABLE[milieu];
        if (c < 0) {
            bas = milieu + 1;
        } else {
            haut = milieu;
        }
    }
    return nullptr;
}

// Chaîne POSIX d'un nom IANA connu ; vide si inconnu (l'appelant refuse alors la zone).
constexpr std::string_view posix_pour(std::string_view iana) noexcept {
    const Entree* e = trouver(iana);
    return e != nullptr ? e->posix : std::string_view{};
}

namespace detail {

// Minuscule ASCII, et l'espace vaut « _ » : « new york » trouve « America/New_York ».
constexpr char normaliser(char c) noexcept {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
    return c == ' ' ? '_' : c;
}

// `saisie` apparaît-elle dans `iana` à partir de la position `pos` ?
constexpr bool apparait_a(std::string_view iana, std::size_t pos, std::string_view saisie) noexcept {
    if (pos + saisie.size() > iana.size()) return false;
    for (std::size_t i = 0; i < saisie.size(); ++i) {
        if (normaliser(iana[pos + i]) != normaliser(saisie[i])) return false;
    }
    return true;
}

// Début de la dernière partie du nom (la ville) : « Europe/Paris » -> 7.
constexpr std::size_t debut_ville(std::string_view iana) noexcept {
    const std::size_t p = iana.rfind('/');
    return p == std::string_view::npos ? 0 : p + 1;
}

constexpr bool contient(std::string_view iana, std::string_view saisie) noexcept {
    for (std::size_t pos = 0; pos + saisie.size() <= iana.size(); ++pos) {
        if (apparait_a(iana, pos, saisie)) return true;
    }
    return false;
}

constexpr bool commence_par(std::string_view iana, std::string_view saisie) noexcept {
    return apparait_a(iana, 0, saisie) || apparait_a(iana, debut_ville(iana), saisie);
}

}  // namespace detail

// Recherche libre (ASCII, sans tenir compte de la casse ni de « _ » / espace). Écrit dans `resultats` au plus
// `max` pointeurs vers la table et rend leur nombre. Ordre : d'abord les noms dont le début, ou la ville,
// commence par la saisie (« paris » -> Europe/Paris), puis ceux qui la contiennent ; ordre de la table à
// l'intérieur de chaque groupe. Saisie vide : aucun résultat. Pas de repli sur les accents (« Montréal »).
constexpr std::size_t chercher(std::string_view saisie, const Entree** resultats, std::size_t max) noexcept {
    if (saisie.empty() || resultats == nullptr) return 0;
    std::size_t n = 0;
    for (std::size_t i = 0; i < NOMBRE && n < max; ++i) {
        if (detail::commence_par(TABLE[i].iana, saisie)) resultats[n++] = &TABLE[i];
    }
    for (std::size_t i = 0; i < NOMBRE && n < max; ++i) {
        if (!detail::commence_par(TABLE[i].iana, saisie) && detail::contient(TABLE[i].iana, saisie)) {
            resultats[n++] = &TABLE[i];
        }
    }
    return n;
}

}  // namespace ecran::fuseaux
