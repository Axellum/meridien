// Tendance de la pression sur 3 heures, MESURÉE par l'appareil.
//
// Pourquoi pas Open-Meteo : sa pression « passée » demande `past_hours=3`, qui ajoute trois heures AVANT l'heure en cours dans
// toutes les séries horaires et décale donc tous les indices de la réponse ; et sa pression à venir n'est pas une tendance
// observée. L'appareil, lui, reçoit déjà `current.pressure_msl` toutes les 15 minutes : il suffit de s'en souvenir. Aucun octet de
// plus à la requête, aucune de plus à la réponse.
//
// Ce que ça donne, et ne donne pas :
//  - tendance = pression de la mesure la plus récente moins celle de la mesure faite environ 3 h plus tôt, en dixièmes d'hPa
//    (positive : la pression monte). « Environ » : la mesure retenue est la plus proche de 3 h, pourvu qu'elle en soit à
//    30 minutes ou moins (donc d'âge 2 h 30 à 3 h 30, bornes comprises) ; à égalité, la plus ancienne. Aucune mise à l'échelle :
//    un relevé toutes les 15 minutes tombe presque toujours à moins de 8 minutes de la cible ;
//  - les instants sont ceux de la MESURE (`current.time`, donnés par le serveur), jamais l'horloge de l'appareil : la tendance
//    ne dépend pas de la mise à l'heure ;
//  - la mémoire est en RAM, pas en cache : après un démarrage ou un changement de lieu la tendance est inconnue pendant ~3 h,
//    le temps de refaire 3 h de relevés. Inconnue vaut mieux qu'une tendance fausse d'un autre lieu ;
//  - la même mesure reçue deux fois (deux réponses de suite portent le même `current.time`) ne fait qu'un point, avec la
//    dernière valeur reçue ; une mesure PLUS ANCIENNE que la dernière veut dire que quelque chose a changé (autre lieu, serveur
//    qui revient en arrière) : on repart de zéro plutôt que de mélanger.
//
// C++17 standard, `constexpr`, aucun include ESP-IDF : testé sur le PC (test_meteo_tendance.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

namespace ecran::meteo {

inline constexpr std::size_t TENDANCE_MESURES_MAX = 16; // 4 h à une mesure par quart d'heure
inline constexpr std::int64_t TENDANCE_DUREE_S = 3 * 3600;
inline constexpr std::int64_t TENDANCE_TOLERANCE_S = 30 * 60;

// Les dernières pressions mesurées, de la plus ancienne à la plus récente (instants strictement croissants).
struct HistoPression {
    std::int64_t instants_s[TENDANCE_MESURES_MAX] = {};
    std::int16_t dixiemes_hpa[TENDANCE_MESURES_MAX] = {}; // pression ramenée au niveau de la mer, en dixièmes d'hPa
    std::uint8_t n = 0;
};

struct Tendance {
    bool connue = false;
    int dixiemes_hpa = 0; // pression actuelle moins pression d'il y a ~3 h
};

// Note la pression mesurée à `instant_s` (UNIX, UTC). À appeler à chaque nouvelle réponse valide.
constexpr void tendance_noter(HistoPression &h, std::int64_t instant_s, int pression_dixiemes_hpa) noexcept {
    const std::int16_t p = static_cast<std::int16_t>(pression_dixiemes_hpa);
    if (h.n > 0) {
        const std::size_t dernier = static_cast<std::size_t>(h.n) - 1;
        if (instant_s == h.instants_s[dernier]) {
            h.dixiemes_hpa[dernier] = p; // même mesure : la dernière valeur reçue fait foi
            return;
        }
        if (instant_s < h.instants_s[dernier])
            h = HistoPression{}; // en arrière : on repart de zéro
    }
    // Plein : la plus ancienne sort ; 16 mesures d'un quart d'heure couvrent 3 h 45, plus que la fenêtre de 3 h 30.
    if (h.n == TENDANCE_MESURES_MAX) {
        for (std::size_t i = 1; i < TENDANCE_MESURES_MAX; ++i) {
            h.instants_s[i - 1] = h.instants_s[i];
            h.dixiemes_hpa[i - 1] = h.dixiemes_hpa[i];
        }
        --h.n;
    }
    h.instants_s[h.n] = instant_s;
    h.dixiemes_hpa[h.n] = p;
    ++h.n;
}

// Tendance de la mesure (`instant_s`, `pression_dixiemes_hpa`) par rapport à la mesure d'il y a ~3 h. Inconnue si l'historique
// n'a aucune mesure de 2 h 30 à 3 h 30 avant `instant_s`.
constexpr Tendance tendance_3h(const HistoPression &h, std::int64_t instant_s, int pression_dixiemes_hpa) noexcept {
    Tendance t;
    const std::int64_t cible = instant_s - TENDANCE_DUREE_S;
    std::int64_t meilleur_ecart = TENDANCE_TOLERANCE_S + 1;
    for (std::size_t i = 0; i < h.n; ++i) {
        const std::int64_t ecart = h.instants_s[i] > cible ? h.instants_s[i] - cible : cible - h.instants_s[i];
        if (ecart < meilleur_ecart) { // strict : à égalité, la première trouvée (la plus ancienne) reste
            meilleur_ecart = ecart;
            t.connue = true;
            t.dixiemes_hpa = pression_dixiemes_hpa - h.dixiemes_hpa[i];
        }
    }
    return t;
}

} // namespace ecran::meteo
