// Noyau pur : quand relancer la requête d'un service lu directement (MET Norway, NWS). C++17 standard, aucun include ESP-IDF, aucune
// horloge ni aléa dans le noyau (l'appelant donne l'heure et un octet aléatoire). Décision et mesures : ADR-0014,
// docs/mesures/fournisseurs-meteo.md.
//
// Ce que les deux services demandent, et ce que les réponses réelles du 07/11/2026 ont montré :
//   - MET Norway : ne pas redemander avant `Expires` (dans les huit captures, `Expires` = `Last-Modified` + 30 min environ, mais la
//     réponse peut avoir déjà 25 minutes dans le cache de MET Norway : `Expires` tombe alors 310 à 1 828 s après la requête, sous
//     le plancher ci-dessous dans trois captures sur huit ; le plancher, plus strict, s'applique), ne pas frapper à l'heure ronde (les
//     appareils qui interrogent à :00 font un pic), étaler le trafic.
//   - NWS : la grille se renouvelle à l'heure (Cache-Control max-age de 108 s à 3 600 s, `Expires` jamais au-delà d'une heure) ;
//     l'adresse de la grille (/points) change rarement et le NWS l'annonce valable de 2 h à 24 h (`Expires` de 7 380 s à 86 400 s dans
//     les six captures) : l'appareil la garde 24 heures, jamais plus.
// Le délai après un succès est donc : au moins le plancher du service, au moins jusqu'à `Expires` s'il est lu et dans le futur, au plus
// 90 minutes (une météo de plus de 2 h est « périmée », meteo_synthese.hpp : un `Expires` lointain ou faux ne doit pas la laisser
// vieillir), plus un décalage d'un dixième au plus, puis écarté de l'heure ronde. Après un échec, le rythme d'avant s'applique
// (meteo_planning.hpp : 15 s, 30 s… plafonnés à 15 min ; 10 min au moins après un HTTP 429).
#pragma once

#include <cstdint>

#include "ecran/fournisseurs_meteo.hpp"
#include "ecran/meteo_planning.hpp"

namespace ecran::meteo::fournisseurs {

inline constexpr std::int64_t PLANCHER_METNO_S = 30 * 60;
inline constexpr std::int64_t PLANCHER_NWS_S = 60 * 60;
inline constexpr std::int64_t PLAFOND_S = 90 * 60;
// L'adresse de la grille du NWS (réponse de /points) est gardée en mémoire vive 24 h, puis redemandée.
inline constexpr std::int64_t DUREE_MAILLE_NWS_S = 24 * 60 * 60;
// Autour de l'heure ronde : de 1 minute avant à 2 minutes après, on ne frappe pas (MET Norway le demande).
inline constexpr std::int64_t AVANT_HEURE_RONDE_S = 60;
inline constexpr std::int64_t APRES_HEURE_RONDE_S = 120;

// Le plus court délai entre deux requêtes réussies à ce service. Open-Meteo garde son rythme d'avant (meteo_planning.hpp).
constexpr std::int64_t plancher_s(Famille f) noexcept {
    switch (f) {
    case Famille::MetNo:
        return PLANCHER_METNO_S;
    case Famille::Nws:
        return PLANCHER_NWS_S;
    case Famille::OpenMeteo:
        break;
    }
    return PERIODE_S;
}

namespace detail {

// L'heure ronde la plus proche tombe-t-elle dans la zone interdite autour de `instant_s` ?
constexpr bool pres_de_l_heure_ronde(std::int64_t instant_s) noexcept {
    const std::int64_t reste = ((instant_s % 3600) + 3600) % 3600;
    return reste >= 3600 - AVANT_HEURE_RONDE_S || reste < APRES_HEURE_RONDE_S;
}

} // namespace detail

// Secondes à attendre avant la prochaine requête à `f` après une réponse réussie reçue à `maintenant_s` (UNIX). `expires_s` : l'en-tête
// `Expires` de cette réponse (UNIX), 0 s'il manquait ou n'a pas pu être lu. `alea` : octet aléatoire fourni par l'appelant.
// Le résultat reste sous 2 heures, pour tout `expires_s`.
constexpr std::int64_t delai_apres_succes_s(Famille f, std::int64_t maintenant_s, std::int64_t expires_s,
                                            std::uint8_t alea) noexcept {
    std::int64_t base = plancher_s(f);
    const std::int64_t jusqu_a_expires = expires_s - maintenant_s;
    if (expires_s > 0 && jusqu_a_expires > base)
        base = jusqu_a_expires;
    if (base > PLAFOND_S)
        base = PLAFOND_S;
    std::int64_t delai = base + (base / 10) * alea / 255;
    if (detail::pres_de_l_heure_ronde(maintenant_s + delai)) {
        // On arrive juste après la zone interdite, plus un étalement de 0 à 63 s pour que deux appareils ne retombent pas ensemble.
        const std::int64_t reste = ((maintenant_s + delai) % 3600 + 3600) % 3600;
        const std::int64_t jusqu_a_la_sortie =
            reste >= 3600 - AVANT_HEURE_RONDE_S ? 3600 - reste + APRES_HEURE_RONDE_S : APRES_HEURE_RONDE_S - reste;
        delai += jusqu_a_la_sortie + (alea & 0x3FU);
    }
    return delai;
}

// L'adresse de grille obtenue à `obtenue_s` peut-elle encore servir à `maintenant_s` ? Faux si elle a 24 h, ou si l'horloge est
// revenue en arrière (heure incertaine : on redemande plutôt que de se fier à un âge négatif).
constexpr bool maille_nws_valable(std::int64_t obtenue_s, std::int64_t maintenant_s) noexcept {
    const std::int64_t age = maintenant_s - obtenue_s;
    return obtenue_s > 0 && age >= 0 && age < DUREE_MAILLE_NWS_S;
}

} // namespace ecran::meteo::fournisseurs
