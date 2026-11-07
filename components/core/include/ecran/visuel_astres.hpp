// Noyau pur : la lune et le soleil de l'écran principal. Phase de lune (calcul local, sans réseau), moment de la journée,
// position du soleil sur son arc. C++17 standard, aucun include ESP-IDF ni LVGL, entiers seulement (pas de flottant : évaluable
// à la compilation et identique sur le PC et sur la carte).
//
// LUNE : phase MOYENNE. Mois synodique moyen 29,530589 j depuis une nouvelle lune de référence (6 janvier 2000, 18 h 14 UTC,
// la référence classique de l'astronomie). La vraie lune avance de façon irrégulière : l'écart avec la phase réelle atteint
// une quinzaine d'heures, invisible sur une image de huit phases mais à ne pas prendre pour une éphéméride. Le test
// (test_visuel_astres.cpp) compare à des nouvelles et pleines lunes datées, avec une tolérance de 0,75 jour.
//
// SOLEIL : l'écran n'a pas de position astronomique, il a le lever et le coucher du jour (Open-Meteo, par lieu). Le moment de la
// journée en découle : l'aube et le crépuscule durent 40 minutes de part et d'autre (ordre de grandeur du crépuscule civil
// sous nos latitudes ; sert à choisir un dégradé de ciel, pas à promettre une heure).
#pragma once

#include <cstdint>

namespace ecran::visuel {

// ---- lune ----

inline constexpr std::int64_t LUNE_NOUVELLE_REFERENCE_S = 947182440; // 2000-01-06 18:14:00 UTC
inline constexpr std::int64_t LUNE_MOIS_SYNODIQUE_S = 2551443;       // 29,530589 jours
inline constexpr int LUNE_NB_PHASES = 8;

struct PhaseLune {
    int indice;          // 0 nouvelle, 1 premier croissant, 2 premier quartier, 3 gibbeuse croissante, 4 pleine,
                         // 5 gibbeuse décroissante, 6 dernier quartier, 7 dernier croissant
    int age_dixiemes_j;  // âge depuis la nouvelle lune, en dixièmes de jour (0 à 295)
    int eclairement_pct; // part du disque éclairée, 0 à 100
};

namespace detail {

// cos(2*pi*x) pour x en [0, 1) donné en millièmes de tour, rendu en millièmes (-1000..1000). Série de Taylor sur [-pi, pi] en
// entiers à virgule fixe (précision ~ 0,2 %, bien au-delà de ce que dessine une image de huit phases).
constexpr int cos_tour_milliemes(int milliemes_de_tour) noexcept {
    int m = milliemes_de_tour % 1000;
    if (m < 0)
        m += 1000;
    if (m > 500)
        m -= 1000; // -500..500 : angle de -pi à pi
    // angle en milliradians = m * 2*pi (6283,185 / 1000)
    const long long a = static_cast<long long>(m) * 6283 / 1000; // milliradians, |a| <= 3141
    // cos x = 1 - x^2/2 + x^4/24 - x^6/720 + x^8/40320 - x^10/3628800, calculé en millionièmes pour limiter l'arrondi
    const long long x2 = a * a;             // milliradians^2 : 1e-6 rad^2
    const long long t1 = x2 / 2;            // x^2/2 en 1e-6
    const long long x4 = x2 * x2 / 1000000; // x^4 en 1e-6 (x2 est en 1e-6 : le produit est en 1e-12)
    const long long t2 = x4 / 24;
    const long long x6 = x4 * x2 / 1000000;
    const long long t3 = x6 / 720;
    const long long x8 = x6 * x2 / 1000000;
    const long long t4 = x8 / 40320;
    const long long x10 = x8 * x2 / 1000000;
    const long long t5 = x10 / 3628800;
    const long long c = 1000000 - t1 + t2 - t3 + t4 - t5; // en millionièmes
    return static_cast<int>(c / 1000);
}

constexpr std::int64_t modulo_positif(std::int64_t a, std::int64_t b) noexcept {
    const std::int64_t r = a % b;
    return r < 0 ? r + b : r;
}

} // namespace detail

constexpr PhaseLune phase_lune(std::int64_t epoch_s) noexcept {
    const std::int64_t age_s = detail::modulo_positif(epoch_s - LUNE_NOUVELLE_REFERENCE_S, LUNE_MOIS_SYNODIQUE_S);
    PhaseLune p{};
    p.age_dixiemes_j = static_cast<int>(age_s * 10 / 86400);
    // huitièmes de cycle, arrondis : la nouvelle lune est centrée sur l'indice 0 (plage de -1,85 j à +1,85 j)
    p.indice = static_cast<int>((age_s * LUNE_NB_PHASES * 2 + LUNE_MOIS_SYNODIQUE_S) / (2 * LUNE_MOIS_SYNODIQUE_S)) %
               LUNE_NB_PHASES;
    const int milliemes_de_tour = static_cast<int>(age_s * 1000 / LUNE_MOIS_SYNODIQUE_S);
    // éclairement = (1 - cos(2*pi*age/T)) / 2
    p.eclairement_pct = (1000 - detail::cos_tour_milliemes(milliemes_de_tour) + 10) / 20;
    if (p.eclairement_pct < 0)
        p.eclairement_pct = 0;
    if (p.eclairement_pct > 100)
        p.eclairement_pct = 100;
    return p;
}

// Croissante (éclairée à droite dans l'hémisphère nord) ou décroissante ; faux aux phases 0 et 4.
constexpr bool lune_croissante(const PhaseLune &p) noexcept { return p.indice >= 1 && p.indice <= 3; }

// ---- moment de la journée ----

enum class MomentJour : std::uint8_t { NUIT, AUBE, JOUR, CREPUSCULE };

inline constexpr std::int64_t DUREE_AUBE_S = 40 * 60;

struct ArcSoleil {
    MomentJour moment;
    int position_pm; // position sur l'arc du lever au coucher, en millièmes (0 à 1000) ; -1 si le soleil n'est pas levé
    std::int64_t duree_jour_s;  // coucher - lever ; 0 si inconnue
    std::int64_t avant_lever_s; // secondes avant le lever du jour (>0 seulement avant le lever), 0 sinon
    bool soleil_connu;          // lever et coucher exploitables (faux : nuit polaire, valeurs absentes, incohérentes)
};

// `maintenant_s`, `lever_s`, `coucher_s` : instants UNIX du MÊME jour local. `est_jour` : is_day d'Open-Meteo, qui sert quand le
// lever et le coucher sont inutilisables (absents = 0, ou égaux comme en nuit polaire, ou dans le mauvais ordre).
constexpr ArcSoleil arc_soleil(std::int64_t maintenant_s, std::int64_t lever_s, std::int64_t coucher_s,
                               bool est_jour) noexcept {
    ArcSoleil a{};
    a.position_pm = -1;
    if (lever_s <= 0 || coucher_s <= lever_s) {
        a.moment = est_jour ? MomentJour::JOUR : MomentJour::NUIT;
        return a;
    }
    a.soleil_connu = true;
    a.duree_jour_s = coucher_s - lever_s;
    if (maintenant_s < lever_s - DUREE_AUBE_S) {
        a.moment = MomentJour::NUIT;
        a.avant_lever_s = lever_s - maintenant_s;
    } else if (maintenant_s < lever_s + DUREE_AUBE_S) {
        a.moment = MomentJour::AUBE;
        a.avant_lever_s = maintenant_s < lever_s ? lever_s - maintenant_s : 0;
    } else if (maintenant_s < coucher_s - DUREE_AUBE_S) {
        a.moment = MomentJour::JOUR;
    } else if (maintenant_s < coucher_s + DUREE_AUBE_S) {
        a.moment = MomentJour::CREPUSCULE;
    } else {
        a.moment = MomentJour::NUIT;
    }
    if (maintenant_s >= lever_s && maintenant_s <= coucher_s) {
        a.position_pm = static_cast<int>((maintenant_s - lever_s) * 1000 / a.duree_jour_s);
    }
    return a;
}

} // namespace ecran::visuel
