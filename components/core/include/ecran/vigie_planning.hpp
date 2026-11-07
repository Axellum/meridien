// Noyau pur : quand interroger Vigie. C++17 standard, aucun include ESP-IDF, aucune horloge ni aléa dans le noyau (l'appelant injecte
// un octet aléatoire).
//
// L'instance publique de Vigie est limitée par adresse IP et « n'est pas faite pour bâtir un service dessus » (README de vigie_api) :
// une seule requête par période, jamais de rafale au démarrage, et on s'éloigne quand ça ne va pas.
//   - Période : réglage en minutes, 10 à 180, 15 par défaut. La réponse de Vigie est gardée 60 s par son cache (« Cache-Control:
//     max-age=60, s-maxage=120 ») : interroger plus souvent qu'une fois par minute ne servirait à rien. Mesure de ce que coûte une
//     requête : docs/mesures/alertes.md.
//   - Premier essai : 60 à 120 s après que le réseau et l'heure sont prêts (la météo part la première ; les deux ne se marchent pas dessus).
//   - Après un échec : 1 min, 2 min, 4 min… doublés à chaque échec de suite, plafonnés à la période. Après un HTTP 429 (« trop de
//     requêtes ») ou un 503 : jamais moins de 30 min. Un dixième de décalage au plus est ajouté, pour que plusieurs appareils derrière
//     la même adresse ne frappent pas à la même seconde.
//   - Écart minimal entre deux requêtes quoi qu'il arrive (changement de langue ou de réglage compris) : 60 s.
#pragma once

#include <cstdint>

namespace ecran::vigie {

inline constexpr int PERIODE_MIN_MIN = 10;
inline constexpr int PERIODE_MAX_MIN = 180;
inline constexpr int PERIODE_DEFAUT_MIN = 15;

inline constexpr std::int64_t PREMIER_ESSAI_S = 60; // plus 0 à 60 s d'aléa
inline constexpr std::int64_t PREMIER_REESSAI_S = 60;
inline constexpr std::int64_t PAUSE_TROP_DE_REQUETES_S = 30 * 60;
inline constexpr std::int64_t ESPACEMENT_MIN_S = 60;

// Période demandée (minutes, lue en NVS) -> secondes, bornée à 10..180 minutes.
constexpr std::int64_t periode_s(int minutes) noexcept {
    if (minutes < PERIODE_MIN_MIN)
        minutes = PERIODE_MIN_MIN;
    if (minutes > PERIODE_MAX_MIN)
        minutes = PERIODE_MAX_MIN;
    return static_cast<std::int64_t>(minutes) * 60;
}

// Attente avant la TOUTE PREMIÈRE requête après le démarrage (ou après la mise en route du service).
constexpr std::int64_t delai_premier_essai_s(std::uint8_t alea) noexcept {
    return PREMIER_ESSAI_S + 60 * static_cast<std::int64_t>(alea) / 255;
}

// Attente avant la requête suivante. `echecs_consecutifs` : 0 après une réponse réussie. `periode` : en secondes (periode_s).
constexpr std::int64_t delai_avant_essai_s(int echecs_consecutifs, bool trop_de_requetes, std::int64_t periode,
                                           std::uint8_t alea) noexcept {
    std::int64_t base = periode;
    if (echecs_consecutifs > 0) {
        base = PREMIER_REESSAI_S;
        for (int i = 1; i < echecs_consecutifs && base < periode; ++i)
            base *= 2; // s'arrête dès le plafond : pas de dépassement pour un très grand nombre d'échecs
        if (base > periode)
            base = periode;
    }
    if (trop_de_requetes && base < PAUSE_TROP_DE_REQUETES_S)
        base = PAUSE_TROP_DE_REQUETES_S;
    if (base < ESPACEMENT_MIN_S)
        base = ESPACEMENT_MIN_S;
    return base + (base / 10) * alea / 255;
}

} // namespace ecran::vigie
