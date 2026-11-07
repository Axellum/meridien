// Noyau pur : lever et coucher du soleil, calculés sur l'appareil pour une position et un jour local. C++17 standard, aucun
// include ESP-IDF, évaluable à la compilation (pas de <cmath> : sinus, cosinus et leurs inverses sont écrits ici).
//
// POURQUOI : ni MET Norway Locationforecast ni le NWS (api.weather.gov, grille) ne donnent le lever ni le coucher du soleil
// (docs/mesures/fournisseurs-meteo.md). MET Norway a un autre produit pour ça (« Sunrise 3.0 »), mais un second appel par
// position pour deux heures serait une requête de plus à son serveur ; le NWS n'en a pas dans la grille. Le calcul se fait donc ici.
//
// MÉTHODE : l'algorithme du NOAA Global Monitoring Laboratory (« Solar Calculator », formules d'après J. Meeus,
// « Astronomical Algorithms ») : longitude moyenne et anomalie moyenne du Soleil, équation du centre, longitude apparente,
// obliquité, déclinaison, équation du temps, puis l'angle horaire du lever pour une hauteur de -0,833 ° (rayon du disque solaire
// 0,267 ° + réfraction atmosphérique 0,566 °). Précision attendue : une à deux minutes aux latitudes tempérées, moins bonne près
// des cercles polaires, où la durée du jour change de plusieurs minutes d'un jour à l'autre (comparaison avec les valeurs du
// produit Sunrise 3.0 de MET Norway et avec un second calcul indépendant : test/host/controle_python_fournisseurs.py).
// Les formules sont en `double` (l'ESP32-P4 les émule en logiciel : une dizaine d'appels par rafraîchissement, quelques
// millisecondes) ; les entrées et sorties sont des entiers (coordonnées en e5, instants en secondes).
//
// Nuit polaire / jour polaire : l'angle horaire n'existe pas. `Situation::NuitPolaire` : le soleil ne se lève pas ce jour-là ;
// `Situation::JourPolaire` : il ne se couche pas. Le lecteur de météo le traduit comme Open-Meteo le fait (meteo_openmeteo.hpp,
// `Jour::sans_soleil()` : lever == coucher == minuit local pour la nuit polaire).
#pragma once

#include <cstdint>

namespace ecran::astro {

inline constexpr double PI = 3.14159265358979323846;
inline constexpr double HAUTEUR_LEVER_DEG = -0.833;

namespace detail {

constexpr double vers_rad(double deg) noexcept { return deg * (PI / 180.0); }
constexpr double vers_deg(double rad) noexcept { return rad * (180.0 / PI); }

// Ramène un angle (radians) dans [-π, π].
constexpr double reduire(double x) noexcept {
    const double tour = 2.0 * PI;
    const double n = x / tour;
    // arrondi à l'entier le plus proche sans <cmath>
    const long long k = static_cast<long long>(n >= 0.0 ? n + 0.5 : n - 0.5);
    x -= static_cast<double>(k) * tour;
    return x;
}

// Séries de Taylor sur [-π, π] : 12 termes donnent mieux que 1e-15 (l'erreur de la dernière série est x^25/25! < 1e-12 à π).
constexpr double sinus(double x) noexcept {
    x = reduire(x);
    double terme = x;
    double somme = x;
    for (int n = 1; n <= 12; ++n) {
        terme *= -x * x / (static_cast<double>(2 * n) * static_cast<double>(2 * n + 1));
        somme += terme;
    }
    return somme;
}

constexpr double cosinus(double x) noexcept {
    x = reduire(x);
    double terme = 1.0;
    double somme = 1.0;
    for (int n = 1; n <= 12; ++n) {
        terme *= -x * x / (static_cast<double>(2 * n - 1) * static_cast<double>(2 * n));
        somme += terme;
    }
    return somme;
}

constexpr double tangente(double x) noexcept { return sinus(x) / cosinus(x); }

// Arc sinus par dichotomie sur [-π/2, π/2] (sinus y est croissant) : 60 coupes, erreur < 1e-18. x est ramené dans [-1, 1].
constexpr double arc_sinus(double x) noexcept {
    if (x >= 1.0)
        return PI / 2.0;
    if (x <= -1.0)
        return -PI / 2.0;
    double bas = -PI / 2.0;
    double haut = PI / 2.0;
    for (int i = 0; i < 60; ++i) {
        const double milieu = 0.5 * (bas + haut);
        if (sinus(milieu) < x) {
            bas = milieu;
        } else {
            haut = milieu;
        }
    }
    return 0.5 * (bas + haut);
}

// Arc cosinus par dichotomie sur [0, π] (cosinus y est décroissant).
constexpr double arc_cosinus(double x) noexcept {
    if (x >= 1.0)
        return 0.0;
    if (x <= -1.0)
        return PI;
    double bas = 0.0;
    double haut = PI;
    for (int i = 0; i < 60; ++i) {
        const double milieu = 0.5 * (bas + haut);
        if (cosinus(milieu) > x) {
            bas = milieu;
        } else {
            haut = milieu;
        }
    }
    return 0.5 * (bas + haut);
}

struct PositionSoleil {
    double declinaison_rad = 0.0;
    double equation_du_temps_min = 0.0; // minutes : temps solaire vrai - temps solaire moyen
};

// `t` : siècles juliens depuis J2000 (JD - 2451545) / 36525.
constexpr PositionSoleil position_soleil(double t) noexcept {
    const double l0 = vers_rad(280.46646 + t * (36000.76983 + t * 0.0003032));
    const double m = vers_rad(357.52911 + t * (35999.05029 - 0.0001537 * t));
    const double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    const double c = sinus(m) * (1.914602 - t * (0.004817 + 0.000014 * t)) +
                     sinus(2.0 * m) * (0.019993 - 0.000101 * t) + sinus(3.0 * m) * 0.000289;
    const double vraie = vers_rad(vers_deg(l0) + c);
    const double omega = vers_rad(125.04 - 1934.136 * t);
    const double apparente = vraie - vers_rad(0.00569) - vers_rad(0.00478) * sinus(omega);
    const double obliquite_moyenne =
        23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
    const double obliquite = vers_rad(obliquite_moyenne + 0.00256 * cosinus(omega));
    PositionSoleil p;
    p.declinaison_rad = arc_sinus(sinus(obliquite) * sinus(apparente));
    const double y = tangente(obliquite / 2.0) * tangente(obliquite / 2.0);
    const double eq = y * sinus(2.0 * l0) - 2.0 * e * sinus(m) + 4.0 * e * y * sinus(m) * cosinus(2.0 * l0) -
                      0.5 * y * y * sinus(4.0 * l0) - 1.25 * e * e * sinus(2.0 * m);
    p.equation_du_temps_min = 4.0 * vers_deg(eq);
    return p;
}

constexpr double siecles_juliens(std::int64_t instant_s) noexcept {
    const double jd = static_cast<double>(instant_s) / 86400.0 + 2440587.5;
    return (jd - 2451545.0) / 36525.0;
}

} // namespace detail

// Hauteur du centre du Soleil au-dessus de l'horizon (degrés, sans réfraction) à l'instant `instant_s`, pour une position en e5.
constexpr double hauteur_soleil_deg(std::int64_t instant_s, std::int32_t latitude_e5,
                                    std::int32_t longitude_e5) noexcept {
    const double lat = detail::vers_rad(static_cast<double>(latitude_e5) / 100000.0);
    const double lon = static_cast<double>(longitude_e5) / 100000.0;
    const detail::PositionSoleil p = detail::position_soleil(detail::siecles_juliens(instant_s));
    const std::int64_t jours = instant_s >= 0 ? instant_s / 86400 : -((-instant_s + 86399) / 86400);
    const double minutes_utc = static_cast<double>(instant_s - jours * 86400) / 60.0;
    const double temps_solaire_vrai_min = minutes_utc + p.equation_du_temps_min + 4.0 * lon;
    const double angle_horaire = detail::vers_rad(temps_solaire_vrai_min / 4.0 - 180.0);
    const double s = detail::sinus(lat) * detail::sinus(p.declinaison_rad) +
                     detail::cosinus(lat) * detail::cosinus(p.declinaison_rad) * detail::cosinus(angle_horaire);
    return detail::vers_deg(detail::arc_sinus(s));
}

// Le Soleil est-il levé (centre à plus de -0,833 °) ?
constexpr bool soleil_leve(std::int64_t instant_s, std::int32_t latitude_e5, std::int32_t longitude_e5) noexcept {
    return hauteur_soleil_deg(instant_s, latitude_e5, longitude_e5) > HAUTEUR_LEVER_DEG;
}

enum class Situation : std::uint8_t {
    Normal,      // un lever et un coucher dans le jour
    NuitPolaire, // le soleil ne se lève pas
    JourPolaire  // le soleil ne se couche pas
};

struct EvenementsSoleil {
    Situation situation = Situation::Normal;
    std::int64_t lever_s = 0;   // instant UNIX ; valable si situation == Normal
    std::int64_t coucher_s = 0; // idem
};

// Lever et coucher du jour local dont le minuit de début est `debut_jour_s` (instant UNIX). Le jour dure 23, 24 ou 25 heures
// (changement d'heure) : le calcul prend le midi solaire le plus proche du milieu de ce jour.
constexpr EvenementsSoleil evenements_du_jour(std::int64_t debut_jour_s, std::int32_t latitude_e5,
                                              std::int32_t longitude_e5) noexcept {
    const double lat = detail::vers_rad(static_cast<double>(latitude_e5) / 100000.0);
    const double lon = static_cast<double>(longitude_e5) / 100000.0;
    const std::int64_t milieu = debut_jour_s + 43200;

    // Midi solaire (instant UTC) du jour solaire le plus proche du milieu du jour local, affiné deux fois (l'équation du temps
    // et la déclinaison varient de quelques secondes d'un instant à l'autre de la journée).
    double midi = static_cast<double>(milieu);
    for (int passe = 0; passe < 3; ++passe) {
        const detail::PositionSoleil p =
            detail::position_soleil(detail::siecles_juliens(static_cast<std::int64_t>(midi)));
        // jour solaire d'indice d : midi(d) = d * 86400 + 43200 - lon * 240 - eqt * 60
        const double depuis = (midi + lon * 240.0 + p.equation_du_temps_min * 60.0 - 43200.0) / 86400.0;
        const long long d = static_cast<long long>(depuis >= 0.0 ? depuis + 0.5 : depuis - 0.5);
        midi = static_cast<double>(d) * 86400.0 + 43200.0 - lon * 240.0 - p.equation_du_temps_min * 60.0;
    }
    const detail::PositionSoleil p = detail::position_soleil(detail::siecles_juliens(static_cast<std::int64_t>(midi)));

    EvenementsSoleil r;
    const double cos_lat_dec = detail::cosinus(lat) * detail::cosinus(p.declinaison_rad);
    const double sin_lat_dec = detail::sinus(lat) * detail::sinus(p.declinaison_rad);
    const double numerateur = detail::cosinus(detail::vers_rad(90.0 - HAUTEUR_LEVER_DEG)) - sin_lat_dec;
    // cos(H) = numérateur / cos(lat) cos(déc) : au-delà de ±1, pas de lever (aux pôles le dénominateur s'annule)
    const double ratio = cos_lat_dec > 1e-9 ? numerateur / cos_lat_dec : (numerateur > 0.0 ? 2.0 : -2.0);
    if (ratio > 1.0) {
        r.situation = Situation::NuitPolaire;
        return r;
    }
    if (ratio < -1.0) {
        r.situation = Situation::JourPolaire;
        return r;
    }
    const double demi_jour_s = detail::vers_deg(detail::arc_cosinus(ratio)) * 240.0; // 4 minutes par degré
    const double lever = midi - demi_jour_s;
    const double coucher = midi + demi_jour_s;
    r.lever_s = static_cast<std::int64_t>(lever + 0.5);
    r.coucher_s = static_cast<std::int64_t>(coucher + 0.5);
    return r;
}

} // namespace ecran::astro
