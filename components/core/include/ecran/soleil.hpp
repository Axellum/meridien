// Noyau pur : lever et coucher du soleil d'un lieu et d'un jour, par le calcul (ADR-0012). Sert au ciel de l'écran principal quand
// la météo n'est pas là (premier démarrage sans réseau, cache absent) : à la place d'un « jour de 8 h à 18 h » écrit en dur, faux
// partout et toute l'année.
//
// Méthode : l'algorithme de la NOAA (Global Monitoring Laboratory, « Solar Calculation Details » : équation du temps, déclinaison,
// angle horaire à 90,833° du zénith = rayon du soleil + réfraction), calculé deux fois (une première estimation à midi solaire, puis
// à l'instant estimé du lever ou du coucher), comme la feuille de calcul de la NOAA. Précision attendue : de l'ordre de la minute
// sous 60° de latitude, quelques minutes près des cercles polaires ; ce n'est pas une éphéméride (voir test_soleil.cpp : comparé à
// une implémentation indépendante en Python, et aux levers et couchers réels d'Open-Meteo de six lieux).
// Cas polaires : jour polaire (le soleil ne se couche pas) et nuit polaire (il ne se lève pas) sont rendus tels quels, sans heure.
//
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation. Pas de <cmath> (ses fonctions ne sont pas constexpr) : sinus,
// arc tangente et racine sont écrits ici, en double précision (séries de Taylor et méthode de Newton), à ~1e-13 près, bien en deçà de
// la seconde sur un lever de soleil. En double : coût de quelques centaines d'opérations par appel, à ne faire que par jour et par lieu.
#pragma once

#include <cstdint>

#include "ecran/heure_fr.hpp"

namespace ecran::soleil {

enum class Situation : std::uint8_t {
    Normal,      // le soleil se lève et se couche : `lever_s` et `coucher_s` sont valables
    JourPolaire, // il ne se couche pas ce jour-là : pas d'heure
    NuitPolaire  // il ne se lève pas ce jour-là : pas d'heure
};

struct LeverCoucher {
    Situation situation = Situation::Normal;
    std::int64_t lever_s = 0;   // instant UNIX (UTC), 0 hors du cas Normal
    std::int64_t coucher_s = 0; // instant UNIX (UTC), 0 hors du cas Normal
};

namespace detail {

inline constexpr double PI = 3.14159265358979323846;
inline constexpr double RAD = PI / 180.0;
inline constexpr double ZENITH_LEVER_DEG = 90.833; // 90° + rayon apparent du soleil (16') + réfraction moyenne (34')

constexpr double plancher(double x) noexcept {
    const long long n = static_cast<long long>(x);
    return static_cast<double>(x < static_cast<double>(n) ? n - 1 : n);
}

constexpr double racine(double x) noexcept {
    if (!(x > 0.0)) {
        return 0.0;
    }
    double g = x > 1.0 ? x * 0.5 : 1.0;
    for (int i = 0; i < 80; ++i) { // Newton : la convergence est quadratique, 80 tours couvrent largement 1e-12 … 1e12
        g = 0.5 * (g + x / g);
    }
    return g;
}

// Angle ramené dans [-pi, pi].
constexpr double ramener(double x) noexcept {
    const double tours = x / (2.0 * PI);
    const long long n = static_cast<long long>(tours >= 0.0 ? tours + 0.5 : tours - 0.5);
    return x - static_cast<double>(n) * 2.0 * PI;
}

constexpr double sinus(double x) noexcept {
    x = ramener(x);
    const double x2 = x * x;
    double terme = x;
    double somme = x;
    for (int k = 1; k <= 14; ++k) { // x^29 / 29! < 1e-16 pour |x| <= pi
        terme *= -x2 / ((2.0 * k) * (2.0 * k + 1.0));
        somme += terme;
    }
    return somme;
}

constexpr double cosinus(double x) noexcept { return sinus(x + PI / 2.0); }

constexpr double tangente(double x) noexcept { return sinus(x) / cosinus(x); }

// Arc tangente : |x| > 1 ramené à 1/x, puis trois demi-angles (atan x = 2 atan(x / (1 + sqrt(1 + x^2)))) avant la série.
constexpr double arc_tangente(double x) noexcept {
    const bool negatif = x < 0.0;
    if (negatif) {
        x = -x;
    }
    const bool inverse = x > 1.0;
    if (inverse) {
        x = 1.0 / x;
    }
    for (int i = 0; i < 3; ++i) {
        x = x / (1.0 + racine(1.0 + x * x));
    }
    const double x2 = x * x;
    double terme = x;
    double somme = x;
    for (int k = 1; k <= 12; ++k) { // x <= tan(pi/32) = 0,098 : le terme 12 vaut moins de 1e-26
        terme *= -x2;
        somme += terme / (2.0 * k + 1.0);
    }
    double angle = 8.0 * somme;
    if (inverse) {
        angle = PI / 2.0 - angle;
    }
    return negatif ? -angle : angle;
}

constexpr double arc_sinus(double x) noexcept {
    if (x >= 1.0) {
        return PI / 2.0;
    }
    if (x <= -1.0) {
        return -PI / 2.0;
    }
    return arc_tangente(x / racine(1.0 - x * x));
}

constexpr double arc_cosinus(double x) noexcept { return PI / 2.0 - arc_sinus(x); }

// Équation du temps (minutes) et déclinaison du soleil (radians) au jour julien `jd` (formules de la NOAA).
struct Astre {
    double equation_min = 0.0;
    double declinaison = 0.0;
};

constexpr Astre position_du_soleil(double jd) noexcept {
    const double t = (jd - 2451545.0) / 36525.0; // siècles juliens depuis J2000
    const double l0 = (280.46646 + t * (36000.76983 + t * 0.0003032)) * RAD;
    const double m = (357.52911 + t * (35999.05029 - 0.0001537 * t)) * RAD;
    const double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    const double equation_du_centre = sinus(m) * (1.914602 - t * (0.004817 + 0.000014 * t)) +
                                      sinus(2.0 * m) * (0.019993 - 0.000101 * t) + sinus(3.0 * m) * 0.000289;
    const double longitude_vraie = 280.46646 + t * (36000.76983 + t * 0.0003032) + equation_du_centre;
    const double omega = (125.04 - 1934.136 * t) * RAD;
    const double longitude_apparente = (longitude_vraie - 0.00569 - 0.00478 * sinus(omega)) * RAD;
    const double obliquite_moyenne =
        23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
    const double obliquite = (obliquite_moyenne + 0.00256 * cosinus(omega)) * RAD;
    Astre a;
    a.declinaison = arc_sinus(sinus(obliquite) * sinus(longitude_apparente));
    const double u = tangente(obliquite / 2.0);
    const double y = u * u;
    const double eq = y * sinus(2.0 * l0) - 2.0 * e * sinus(m) + 4.0 * e * y * sinus(m) * cosinus(2.0 * l0) -
                      0.5 * y * y * sinus(4.0 * l0) - 1.25 * e * e * sinus(2.0 * m);
    a.equation_min = 4.0 * eq / RAD;
    return a;
}

// Un événement (lever si `lever`, sinon coucher) évalué avec la position du soleil à `jd`. `minutes_utc` : minutes depuis minuit UTC
// du jour de base ; peut sortir de [0, 1440[ (l'angle horaire peut repousser l'événement sur le jour voisin).
struct Evenement {
    Situation situation = Situation::Normal;
    double minutes_utc = 0.0;
};

constexpr Evenement evenement(double latitude_deg, double longitude_deg, double jd, bool lever) noexcept {
    const Astre a = position_du_soleil(jd);
    const double lat = latitude_deg * RAD;
    const double numerateur = cosinus(ZENITH_LEVER_DEG * RAD) - sinus(lat) * sinus(a.declinaison);
    const double denominateur = cosinus(lat) * cosinus(a.declinaison);
    Evenement ev;
    if (denominateur < 1e-9) { // sur un pôle : le soleil ne se couche ou ne se lève que d'une saison à l'autre
        ev.situation = numerateur > 0.0 ? Situation::NuitPolaire : Situation::JourPolaire;
        return ev;
    }
    const double cos_h = numerateur / denominateur;
    if (cos_h > 1.0) {
        ev.situation = Situation::NuitPolaire;
        return ev;
    }
    if (cos_h < -1.0) {
        ev.situation = Situation::JourPolaire;
        return ev;
    }
    const double angle_horaire_deg = arc_cosinus(cos_h) / RAD;
    const double delta = longitude_deg + (lever ? angle_horaire_deg : -angle_horaire_deg);
    ev.minutes_utc = 720.0 - 4.0 * delta - a.equation_min;
    return ev;
}

constexpr std::int64_t arrondi_s(double minutes) noexcept {
    return static_cast<std::int64_t>(plancher(minutes * 60.0 + 0.5));
}

} // namespace detail

// Lever et coucher du soleil du jour civil (an, mois, jour) d'un lieu. `latitude_deg` : nord positif ; `longitude_deg` : est positif.
// `decalage_s` : décalage à l'UTC du fuseau ce jour-là (en secondes, positif à l'est) ; il ne sert qu'à choisir de quel jour solaire
// il s'agit (celui dont le midi est le plus proche du midi local), pas à décaler le résultat : `lever_s` et `coucher_s` sont des
// instants UNIX (UTC), que l'appelant convertit en heure locale avec le fuseau.
constexpr LeverCoucher lever_coucher(double latitude_deg, double longitude_deg, int an, int mois, int jour,
                                     std::int32_t decalage_s) noexcept {
    LeverCoucher r;
    if (!(latitude_deg >= -90.0 && latitude_deg <= 90.0) || !(longitude_deg >= -180.0 && longitude_deg <= 180.0)) {
        r.situation = Situation::NuitPolaire; // lieu invraisemblable : aucune heure, l'appelant retombe sur son repli
        return r;
    }
    const std::int64_t midi_local_s =
        ecran::heure::detail::jours_depuis_epoch(an, mois, jour) * 86400 + 43200 - decalage_s;
    const double midi_moyen_min = 720.0 - 4.0 * longitude_deg;
    // Jour UTC de base : celui dont le midi solaire moyen tombe le plus près du midi local.
    const double n_reel = (static_cast<double>(midi_local_s) - midi_moyen_min * 60.0) / 86400.0 + 0.5;
    const std::int64_t n = static_cast<std::int64_t>(detail::plancher(n_reel));
    const double jd_minuit = static_cast<double>(n) + 2440587.5; // le 1970-01-01 00:00 UTC est le jour julien 2440587,5

    detail::Evenement lever1 =
        detail::evenement(latitude_deg, longitude_deg, jd_minuit + midi_moyen_min / 1440.0, true);
    detail::Evenement coucher1 =
        detail::evenement(latitude_deg, longitude_deg, jd_minuit + midi_moyen_min / 1440.0, false);
    if (lever1.situation != Situation::Normal || coucher1.situation != Situation::Normal) {
        r.situation = lever1.situation != Situation::Normal ? lever1.situation : coucher1.situation;
        return r;
    }
    // Seconde passe, à l'instant estimé : la déclinaison et l'équation du temps ont bougé depuis midi.
    const detail::Evenement lever2 =
        detail::evenement(latitude_deg, longitude_deg, jd_minuit + lever1.minutes_utc / 1440.0, true);
    const detail::Evenement coucher2 =
        detail::evenement(latitude_deg, longitude_deg, jd_minuit + coucher1.minutes_utc / 1440.0, false);
    const double lever_min = lever2.situation == Situation::Normal ? lever2.minutes_utc : lever1.minutes_utc;
    const double coucher_min = coucher2.situation == Situation::Normal ? coucher2.minutes_utc : coucher1.minutes_utc;
    r.lever_s = n * 86400 + detail::arrondi_s(lever_min);
    r.coucher_s = n * 86400 + detail::arrondi_s(coucher_min);
    if (r.coucher_s <= r.lever_s) { // incohérent (cas limites près des pôles) : mieux vaut aucune heure qu'une fausse
        r.situation = Situation::NuitPolaire;
        r.lever_s = 0;
        r.coucher_s = 0;
    }
    return r;
}

} // namespace ecran::soleil
