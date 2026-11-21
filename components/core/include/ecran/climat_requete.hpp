// Noyau pur : adresse (URL) de la requête des normales climatiques à Vigie. C++17 standard, aucun include ESP-IDF, pas de flottant :
// les coordonnées arrivent en centaines de millièmes de degré (« e5 », comme celles de la météo et des alertes) et partent arrondies
// à TROIS décimales (≈ 110 m : une station Météo-France est choisie dans un rayon de 50 km, la position au mètre n'a pas à partir).
//
// Route : GET <adresse>/v1/climate/normals?lat=…&lon=… (ADR-0029). Aucun autre paramètre : `tmax`, `tmin` et `rain_mm` ne sont PAS
// envoyés. La réponse ne dépend alors que de la station et du mois, donc elle est la même toute la journée (en-tête
// `Cache-Control: max-age=86400`), et c'est l'écran qui compare son propre maximum prévu à la normale : la valeur du jour ne quitte pas
// l'appareil.
//
// L'adresse de base est celle des alertes (ADR-0009) : `vigie::normaliser_adresse` la nettoie et refuse tout ce qui n'est pas https://.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/vigie_requete.hpp"

namespace ecran::climat {

inline constexpr std::string_view REQUETE_DEBUT = "/v1/climate/normals?lat=";
inline constexpr std::string_view REQUETE_MILIEU = "&lon=";

// Plus grand tampon nécessaire, zéro final compris : adresse de base + chemin + « -90.000 » + « &lon= » + « -180.000 ».
inline constexpr std::size_t TAILLE_REQUETE_MAX =
    vigie::ADRESSE_MAX + REQUETE_DEBUT.size() + 7 + REQUETE_MILIEU.size() + 8 + 1;

// Écrit l'adresse complète dans `sortie` (zéro final compris) et rend sa longueur sans le zéro. `adresse` doit être celle que
// `vigie::normaliser_adresse` a rendue. Rend 0, sans rien garantir sur `sortie`, si une coordonnée sort de -90..90 / -180..180 degrés,
// si l'adresse est vide ou trop longue, ou si `capacite` est trop petite.
constexpr std::size_t construire_requete(std::string_view adresse, std::int32_t latitude_e5, std::int32_t longitude_e5,
                                         char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0 || adresse.empty() || adresse.size() > vigie::ADRESSE_MAX)
        return 0;
    if (latitude_e5 < -vigie::LATITUDE_E5_MAX || latitude_e5 > vigie::LATITUDE_E5_MAX)
        return 0;
    if (longitude_e5 < -vigie::LONGITUDE_E5_MAX || longitude_e5 > vigie::LONGITUDE_E5_MAX)
        return 0;
    vigie::detail::Sortie s{sortie, capacite};
    s.texte(adresse);
    s.texte(REQUETE_DEBUT);
    s.degres_e3(vigie::detail::e5_vers_e3(latitude_e5));
    s.texte(REQUETE_MILIEU);
    s.degres_e3(vigie::detail::e5_vers_e3(longitude_e5));
    if (s.trop)
        return 0;
    sortie[s.n] = '\0';
    return s.n;
}

} // namespace ecran::climat
