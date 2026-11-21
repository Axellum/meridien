// Noyau pur : quand interroger Vigie pour les normales climatiques. C++17 standard, aucun include ESP-IDF, aucune horloge ni aléa dans
// le noyau (l'appelant injecte un octet aléatoire).
//
// Les normales sont des moyennes de 1991 à 2020 par MOIS et les fiches de Météo-France sont refaites chaque mois : une requête par
// JOUR suffit largement (la réponse porte `Cache-Control: max-age=86400`). L'instance publique est limitée par adresse IP :
//   - Premier essai : 3 à 5 minutes après que le réseau et l'heure sont prêts, après la météo (immédiate) et les alertes (60 à 120 s) ;
//   - Après une réponse lue : 24 h, plus un décalage de 30 minutes au plus ;
//   - Après un échec : 5, 10, 20, 40 minutes, puis 1 h au plus ; après un HTTP 429 ou 503 : 1 h au moins ;
//   - Quand la réponse en mémoire n'est plus du mois local (le jour de la bascule de mois) : nouvel essai dans 1 h au plus (jamais plus
//     d'une requête par heure pour cette raison : Vigie date en UTC, le lieu peut déjà être au mois suivant) ;
//   - Écart minimal entre deux requêtes quoi qu'il arrive (changement de lieu ou d'adresse compris) : 5 minutes.
#pragma once

#include <cstdint>

namespace ecran::climat {

inline constexpr std::int64_t PREMIER_ESSAI_S = 180; // plus 0 à 120 s d'aléa
inline constexpr std::int64_t PERIODE_S = 24 * 3600;
inline constexpr std::int64_t PREMIER_REESSAI_S = 300;
inline constexpr std::int64_t PLAFOND_REESSAI_S = 3600;
inline constexpr std::int64_t PAUSE_TROP_DE_REQUETES_S = 3600;
inline constexpr std::int64_t ESPACEMENT_MIN_S = 300;
inline constexpr std::int64_t ATTENTE_MOIS_CHANGE_S = 3600;

// Attente avant la TOUTE PREMIÈRE requête après le démarrage.
constexpr std::int64_t delai_premier_essai_s(std::uint8_t alea) noexcept {
    return PREMIER_ESSAI_S + 120 * static_cast<std::int64_t>(alea) / 255;
}

// Attente avant la requête suivante. `echecs_consecutifs` : 0 après une réponse lue.
constexpr std::int64_t delai_avant_essai_s(int echecs_consecutifs, bool trop_de_requetes, std::uint8_t alea) noexcept {
    std::int64_t base = PERIODE_S;
    if (echecs_consecutifs > 0) {
        base = PREMIER_REESSAI_S;
        for (int i = 1; i < echecs_consecutifs && base < PLAFOND_REESSAI_S; ++i)
            base *= 2; // s'arrête dès le plafond : pas de dépassement pour un très grand nombre d'échecs
        if (base > PLAFOND_REESSAI_S)
            base = PLAFOND_REESSAI_S;
    }
    if (trop_de_requetes && base < PAUSE_TROP_DE_REQUETES_S)
        base = PAUSE_TROP_DE_REQUETES_S;
    const std::int64_t decalage =
        (echecs_consecutifs > 0 ? base / 10 : 30 * 60) * static_cast<std::int64_t>(alea) / 255;
    return base + decalage;
}

// Le prochain essai quand la réponse en mémoire n'est plus du mois local : au plus une heure après le dernier essai, et jamais avant
// l'écart minimal. `prochain_prevu_s` et `dernier_essai_s` sont des instants (secondes depuis le démarrage), `maintenant_s` aussi.
constexpr std::int64_t prochain_apres_changement_de_mois_s(std::int64_t maintenant_s, std::int64_t dernier_essai_s,
                                                           std::int64_t prochain_prevu_s) noexcept {
    std::int64_t t = dernier_essai_s + ATTENTE_MOIS_CHANGE_S;
    if (t < maintenant_s)
        t = maintenant_s;
    if (t < dernier_essai_s + ESPACEMENT_MIN_S)
        t = dernier_essai_s + ESPACEMENT_MIN_S;
    return t < prochain_prevu_s ? t : prochain_prevu_s;
}

} // namespace ecran::climat
