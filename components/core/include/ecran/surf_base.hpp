// Noyau pur « surf » : briques communes (valeur absente, arrondis entiers, angles, cosinus entier, rampes linéaires). C++17
// standard, aucun include ESP-IDF, aucun flottant à l'exécution : la seule table qui en a besoin (le cosinus) est calculée à la
// compilation et vérifiée contre `math.cos` de Python (test/host/controle_python_surf.py).
//
// Rien ici n'est propre à une source de données : ces fonctions servent à la fois la lecture des réponses (surf_donnees.hpp), le
// vent (surf_vent.hpp), la marée (surf_maree.hpp) et la note (surf_note.hpp).
#pragma once

#include <cstddef>
#include <cstdint>

namespace ecran::surf {

// « Pas de donnée » : la valeur était `null` (ou absente) dans la réponse. JAMAIS 0 : une houle de 0 cm est une mesure (mer plate),
// « pas de donnée » n'en est pas une, et le noyau ne note ni ne classe sur une valeur qu'il n'a pas. Toutes les grandeurs de ce
// noyau tiennent dans un `int16_t`, et cette valeur-sentinelle est hors de toutes leurs plages.
inline constexpr std::int16_t ABSENT = INT16_MIN;

constexpr bool present(std::int16_t v) noexcept { return v != ABSENT; }

// ---- arrondis entiers ---------------------------------------------------------------------------------------------------

// n / d arrondi à l'entier le plus proche, la moitié s'éloignant de zéro (comme `en_fixe` de json_mini.hpp). d > 0.
constexpr std::int64_t division_arrondie(std::int64_t n, std::int64_t d) noexcept {
    return n >= 0 ? (n + d / 2) / d : -((-n + d / 2) / d);
}

// n / d arrondi vers le bas (vers -infini), pour que « minuit local » reste juste même avec un numérateur négatif. d > 0.
constexpr std::int64_t division_plancher(std::int64_t n, std::int64_t d) noexcept {
    const std::int64_t q = n / d;
    return (n % d != 0 && n < 0) ? q - 1 : q;
}

constexpr std::int64_t limiter(std::int64_t v, std::int64_t bas, std::int64_t haut) noexcept {
    return v < bas ? bas : (v > haut ? haut : v);
}

// Rampe linéaire entre (va -> pa) et (vb -> pb), plate au-delà des deux bouts. Sert à toutes les courbes du barème (hauteur,
// période, force du vent) : une courbe = deux points, lisibles dans le tableau de l'ADR. Les deux valeurs peuvent être rangées dans
// un sens ou dans l'autre ; si va == vb la courbe est une marche et rend pa. La VALEUR TOTALE (pas l'écart) est arrondie à l'entier
// le plus proche, la moitié vers le haut : 32,5 points font 33. Les points n'étant jamais négatifs, c'est l'arrondi de
// `division_arrondie`. Calcul exact en entiers : numérateur pa·d + (pb − pa)·(w − va) sur d = vb − va.
constexpr std::int64_t rampe(std::int64_t v, std::int64_t va, std::int64_t pa, std::int64_t vb,
                             std::int64_t pb) noexcept {
    if (va == vb)
        return pa;
    if (va > vb) { // on se ramène à va < vb
        const std::int64_t tv = va;
        const std::int64_t tp = pa;
        va = vb;
        pa = pb;
        vb = tv;
        pb = tp;
    }
    const std::int64_t w = limiter(v, va, vb);
    const std::int64_t d = vb - va;
    return division_arrondie(pa * d + (pb - pa) * (w - va), d);
}

// ---- angles -------------------------------------------------------------------------------------------------------------

// Degrés quelconques -> 0..359 (-1 -> 359, 360 -> 0, 725 -> 5).
constexpr int normaliser_degres(std::int64_t d) noexcept {
    const std::int64_t r = d % 360;
    return static_cast<int>(r < 0 ? r + 360 : r);
}

// Écart entre deux directions, 0..180 (359° et 0° : 1°). Une direction est un angle sur un cercle : jamais `a - b` tel quel.
constexpr int ecart_angulaire(std::int64_t a, std::int64_t b) noexcept {
    const int e = normaliser_degres(a - b);
    return e > 180 ? 360 - e : e;
}

namespace detail {

// cos(x) pour x en radians, 0 <= x <= pi/2, par la série de Taylor (14 termes : erreur sous 1e-15 sur cet intervalle). Évaluée à
// la compilation seulement : l'appareil ne fait aucun calcul flottant pour un cosinus.
constexpr double cosinus_serie(double x) noexcept {
    double terme = 1.0;
    double somme = 1.0;
    for (int k = 1; k < 14; ++k) {
        terme *= -x * x / (static_cast<double>(2 * k - 1) * static_cast<double>(2 * k));
        somme += terme;
    }
    return somme;
}

// cos(0°) .. cos(90°) en millièmes, arrondis à l'entier le plus proche : 1000, 1000, 999, 999, 998 ... 17, 0.
struct TableCosinus {
    std::int16_t pm[91];
    constexpr TableCosinus() noexcept : pm{} {
        constexpr double PI = 3.14159265358979323846;
        for (int deg = 0; deg <= 90; ++deg) {
            const double c = cosinus_serie(static_cast<double>(deg) * PI / 180.0);
            pm[deg] = static_cast<std::int16_t>(c * 1000.0 + 0.5); // c >= 0 sur 0..90°
        }
    }
};

inline constexpr TableCosinus TABLE_COSINUS{};

} // namespace detail

// cos(d) en millièmes (-1000..1000) pour des degrés entiers quelconques. Exact au millième : 60° -> 500, 90° -> 0, 120° -> -500.
constexpr int cosinus_pm(std::int64_t deg) noexcept {
    int a = normaliser_degres(deg);
    if (a > 180)
        a = 360 - a; // cos(360° - a) = cos(a)
    return a <= 90 ? detail::TABLE_COSINUS.pm[a] : -detail::TABLE_COSINUS.pm[180 - a];
}

} // namespace ecran::surf
