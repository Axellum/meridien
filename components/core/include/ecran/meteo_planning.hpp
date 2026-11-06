// Noyau pur : quand relancer la requête météo. C++17 standard, aucun include ESP-IDF, aucune horloge ni aléa dans le noyau
// (l'appelant injecte un octet aléatoire).
//
// Rythme : un essai toutes les 15 min quand tout va bien (recherche 03 : 15 à 30 min ; l'API gratuite tolère 600 appels par
// minute, on en fait 4 par heure). Après un échec : 15 s, 30 s, 1 min, 2 min… doublés à chaque échec de suite, plafonnés à
// 15 min. Après un HTTP 429 (« trop de requêtes ») : jamais moins de 10 min. Un dixième de décalage au plus est ajouté, pour
// que plusieurs appareils ne frappent pas le serveur à la même seconde.
#pragma once

#include <cstdint>

namespace ecran::meteo {

inline constexpr std::int64_t PERIODE_S = 15 * 60;
inline constexpr std::int64_t PREMIER_REESSAI_S = 15;
inline constexpr std::int64_t PAUSE_TROP_DE_REQUETES_S = 10 * 60;

// `echecs_consecutifs` : 0 après une réponse réussie. `alea` : octet aléatoire fourni par l'appelant.
constexpr std::int64_t delai_avant_essai_s(int echecs_consecutifs, bool trop_de_requetes, std::uint8_t alea) noexcept {
    std::int64_t base = PERIODE_S;
    if (echecs_consecutifs > 0) {
        base = PREMIER_REESSAI_S;
        for (int i = 1; i < echecs_consecutifs && base < PERIODE_S; ++i)
            base *= 2; // s'arrête dès le plafond : pas de dépassement pour un très grand nombre d'échecs
        if (base > PERIODE_S)
            base = PERIODE_S;
    }
    if (trop_de_requetes && base < PAUSE_TROP_DE_REQUETES_S)
        base = PAUSE_TROP_DE_REQUETES_S;
    return base + (base / 10) * alea / 255;
}

} // namespace ecran::meteo
