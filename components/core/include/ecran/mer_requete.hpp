// Noyau pur : la requête de la mer d'un spot, et le rythme des requêtes. C++17 standard, aucun include ESP-IDF, pas de flottant, aucune
// horloge ni aléa dans le noyau (l'appelant injecte l'instant et un octet aléatoire).
//
// Route : GET <adresse>/v1/embedded/frame?device_type=generic&spot=<id>[&wind_min_kn=<n>&wind_max_kn=<n>]. C'est la trame que lit
// mer_trame.hpp. Contrat : vigie_api/docs/embedded-frame.md (relu sur origin/main de vigie-api le 08/11/2026). Les paramètres :
//   - `spot`        : identifiant d'un spot de `GET /v1/surf/spots` (ADR-0018 : un « slug », mer::spot_valide). Un spot inconnu de Vigie
//                     répond 404 ; c'est un échec comme un autre ici, le registre de spots ne l'efface pas tout seul ;
//   - `device_type` : `generic`. Il n'est repris que dans le champ 5 de la ligne HEAD (profil servi) ; ni la liste des lignes ni le
//                     poids de la trame n'en dépendent (embedded_frame.py de vigie-api, relu le 08/11/2026) ;
//   - `wind_min_kn`, `wind_max_kn` : la plage de vent du kite, en nœuds (0 < min < max <= 60 ; autre chose : HTTP 422). Elle n'est
//                     envoyée que si elle n'est pas celle par défaut de Vigie (12 à 25 nœuds) : l'adresse est alors la même que
//                     celle de tous les appareils aux réglages par défaut, que le cache de Vigie sert une seule fois.
//   - ni `lat` ni `lon` : la mer d'un spot ne dépend pas de la position de l'appareil. La position de la maison ne part donc PAS vers
//                     Vigie dans cette requête.
// La trame porte aussi les lignes ZONE / RAIN5M / DAY / HAZ des zones surveillées par l'instance (≈ 5 Ko, 4 992 à 5 022 octets sur les
// six trames de test/host/donnees/) : on ne les lit pas ici.
//
// Rythme (politesse envers l'instance publique : limitée par adresse IP, « pas faite pour bâtir un service dessus ») : les règles des
// alertes (vigie_planning.hpp : jamais de rafale au démarrage, attente doublée après chaque échec, 30 minutes après un 429 ou un 503,
// 60 s au moins entre deux requêtes), avec une période de 10 minutes, la plus courte que ces règles acceptent. Pourquoi 10 minutes :
// la mer est « vieillie » au-delà de 600 s depuis sa génération (mer_vue.hpp, seuil repris de vigie-display et non calibré) ; une
// période plus longue la montrerait vieillie une bonne part du temps. Elle ne coûte rien tant qu'aucun spot n'est choisi
// (mer_spots.hpp : registre vide = aucune requête).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/mer_trame.hpp"
#include "ecran/vigie_planning.hpp"
#include "ecran/vigie_requete.hpp"

namespace ecran::mer {

// ---- la requête ----------------------------------------------------------------------------------------------------------------

inline constexpr std::size_t SPOT_ID_MAX = 64; // octets d'un identifiant de spot, sans le zéro final (mer::spot_valide)

// La plage de vent du kite, en nœuds entiers. Défauts et plafond : ceux de vigie-api (DEFAULT_WIND_MIN_KN, DEFAULT_WIND_MAX_KN,
// MAX_WIND_KN dans app/services/activities.py, relus le 08/11/2026).
inline constexpr int VENT_MIN_DEFAUT_NOEUDS = 12;
inline constexpr int VENT_MAX_DEFAUT_NOEUDS = 25;
inline constexpr int VENT_PLAFOND_NOEUDS = 60;

// 0 < min < max <= 60, comme l'API (hors de là elle répond 422).
constexpr bool plage_de_vent_valide(std::int64_t min_noeuds, std::int64_t max_noeuds) noexcept {
    return min_noeuds > 0 && min_noeuds < max_noeuds && max_noeuds <= VENT_PLAFOND_NOEUDS;
}

namespace detail {

inline constexpr std::string_view REQUETE_DEBUT = "/v1/embedded/frame?device_type=generic&spot=";
inline constexpr std::string_view REQUETE_VENT_MIN = "&wind_min_kn=";
inline constexpr std::string_view REQUETE_VENT_MAX = "&wind_max_kn=";

// Un entier de 1 à 99 en décimal (la plage de vent tient en deux chiffres).
constexpr void deux_chiffres(vigie::detail::Sortie &s, int v) noexcept {
    if (v >= 10)
        s.car(static_cast<char>('0' + v / 10));
    s.car(static_cast<char>('0' + v % 10));
}

} // namespace detail

// Plus grand tampon nécessaire, zéro final compris : adresse de base + chemin + identifiant + les deux paramètres de vent.
inline constexpr std::size_t TAILLE_REQUETE_MAX = vigie::ADRESSE_MAX + detail::REQUETE_DEBUT.size() + SPOT_ID_MAX +
                                                  detail::REQUETE_VENT_MIN.size() + 2 +
                                                  detail::REQUETE_VENT_MAX.size() + 2 + 1;

// Écrit l'adresse complète dans `sortie` (zéro final compris) et rend sa longueur sans le zéro. `adresse` est celle que
// vigie::normaliser_adresse a rendue (ou l'instance publique). Rend 0, sans rien garantir sur `sortie`, si l'adresse est vide ou trop
// longue, si `spot` n'est pas un identifiant valide (mer::spot_valide), si la plage de vent est refusée par plage_de_vent_valide, ou
// si `capacite` est trop petite.
constexpr std::size_t construire_requete(std::string_view adresse, std::string_view spot, int vent_min_noeuds,
                                         int vent_max_noeuds, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0 || adresse.empty() || adresse.size() > vigie::ADRESSE_MAX)
        return 0;
    if (!spot_valide(spot) || !plage_de_vent_valide(vent_min_noeuds, vent_max_noeuds))
        return 0;
    vigie::detail::Sortie s{sortie, capacite};
    s.texte(adresse);
    s.texte(detail::REQUETE_DEBUT);
    s.texte(spot);
    if (vent_min_noeuds != VENT_MIN_DEFAUT_NOEUDS || vent_max_noeuds != VENT_MAX_DEFAUT_NOEUDS) {
        s.texte(detail::REQUETE_VENT_MIN);
        detail::deux_chiffres(s, vent_min_noeuds);
        s.texte(detail::REQUETE_VENT_MAX);
        detail::deux_chiffres(s, vent_max_noeuds);
    }
    if (s.trop)
        return 0;
    sortie[s.n] = '\0';
    return s.n;
}

// ---- le rythme -----------------------------------------------------------------------------------------------------------------

inline constexpr std::int64_t PERIODE_S = 10 * 60; // entre deux requêtes réussies
static_assert(PERIODE_S >= vigie::periode_s(vigie::PERIODE_MIN_MIN),
              "pas plus vite que ce que la politesse des alertes permet");

// Attente avant la TOUTE PREMIÈRE requête après que le réseau et l'heure sont prêts : 120 à 180 s. Les alertes partent à 60 à 120 s et
// la météo avant elles : les trois ne se marchent pas dessus (et le verrou TLS commun les met en file de toute façon).
inline constexpr std::int64_t PREMIER_ESSAI_S = 120;

constexpr std::int64_t delai_premier_essai_s(std::uint8_t alea) noexcept {
    return PREMIER_ESSAI_S + 60 * static_cast<std::int64_t>(alea) / 255;
}

// Attente avant la requête suivante : celle des alertes (vigie::delai_avant_essai_s) avec la période de la mer.
// `echecs_consecutifs` : 0 après une réponse réussie ; `trop_de_requetes` : le dernier échec était un 429 ou un 503.
constexpr std::int64_t delai_avant_essai_s(int echecs_consecutifs, bool trop_de_requetes, std::uint8_t alea) noexcept {
    return vigie::delai_avant_essai_s(echecs_consecutifs, trop_de_requetes, PERIODE_S, alea);
}

// Changement de spot ou de plage de vent (ou d'adresse) : l'ancienne mer ne vaut plus pour le nouveau spot. Une seule requête, après
// un court délai (plusieurs changements de suite = une seule requête) et 60 s au moins après la précédente. Rend l'instant, en
// secondes, du prochain essai.
inline constexpr std::int64_t ANTI_REBOND_S = 3;

constexpr std::int64_t prochain_essai_apres_changement_s(std::int64_t dernier_essai_s,
                                                         std::int64_t maintenant_s) noexcept {
    const std::int64_t au_plus_tot = dernier_essai_s + vigie::ESPACEMENT_MIN_S;
    const std::int64_t apres_rebond = maintenant_s + ANTI_REBOND_S;
    return au_plus_tot > apres_rebond ? au_plus_tot : apres_rebond;
}

} // namespace ecran::mer
