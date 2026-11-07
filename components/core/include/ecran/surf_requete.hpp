// Noyau pur « surf » : adresses (URL) des deux requêtes d'un spot. C++17 standard, aucun include ESP-IDF, pas de flottant : les
// coordonnées arrivent en centaines de millièmes de degré (« e5 », comme meteo_requete.hpp, dont ce fichier reprend l'écriture
// des degrés).
//
// DEUX requêtes par spot, deux API d'Open-Meteo, sans clé (nom de chaque variable vérifié par des appels réels le 07/11/2026, voir
// docs/mesures/surf.md et test/host/capturer_surf.py ; page lue le même jour : https://open-meteo.com/en/docs/marine-weather-api) :
//   1. la MER, `marine-api.open-meteo.com/v1/marine` : hauteur, période et direction des vagues totales, de la houle et des vagues
//      de vent ; température de la mer ; `sea_level_height_msl` (marée comprise, rapportée au niveau MOYEN GLOBAL de la mer, pas
//      au zéro des cartes marines : Open-Meteo écrit qu'elle « ne remplace pas un annuaire des marées ») ;
//   2. le VENT au spot, `api.open-meteo.com/v1/forecast` (la même API que la météo de l'écran) : vitesse, direction (d'où il vient)
//      et rafales à 10 m.
//
// Choix de paramètres, tous constatés sur des réponses réelles :
//   - `past_hours=6&forecast_hours=48` : 54 heures par variable, de 6 h avant l'heure en cours à 48 h après. Le passé donne la
//     dernière pleine ou basse mer déjà écoulée (la marée est « montante » ou « descendante » par rapport à un extremum) ; 48 h
//     couvrent « demain » en entier même si la dernière mise à jour est ancienne. Réponse mer 4,2 Ko, vent 1,7 Ko.
//   - `timeformat=unixtime` : instants en secondes UNIX, aucun format d'heure à lire. `timezone=auto` : sans lui `utc_offset_seconds`
//     vaut 0 (constaté). Au large `auto` rend `Etc/GMT` (décalage 0, constaté à 44,5° N 6° O) : le décalage de la réponse VENT, qui
//     vient de la terre, est celui à préférer pour dire « 6 h à 21 h locales » (surf_note.hpp).
//   - `cell_selection=sea` : la case de la grille la plus proche EN MER. Les pages d'Open-Meteo se contredisent sur la valeur par
//     défaut (tableau : « sea », texte : « land ») ; `nearest` et `land` rendent des `null` au même endroit (constaté à 43,6° N
//     1,4° O) : on la fixe pour ne pas dépendre d'un défaut.
//   - `length_unit=metric` et `wind_speed_unit=kmh` : fixés aussi, et l'analyseur REFUSE une réponse dont l'unité ne correspond pas
//     (`length_unit=imperial` change l'unité en « ft » : constaté).
//
// Limites de ce qu'Open-Meteo rend, à ne jamais perdre de vue : les modèles de vagues ont une maille de 5 à 25 km et ne voient pas
// le banc de sable ; la case choisie peut être à plusieurs kilomètres du spot (constaté : Pipeline 14 km). Les hauteurs sont celles
// du LARGE : « estimation ». Voir docs/decisions/0013-surf-donnees-note-et-limites.md.
//
// Taille : la ligne de requête de la mer fait moins de 512 octets (le tampon d'envoi par défaut d'`esp_http_client`) ; le test le
// vérifie à la compilation avec `TAILLE_REQUETE_MARINE_MAX`.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/meteo_requete.hpp"

namespace ecran::surf {

// Heures demandées avant et après l'heure en cours (le texte de l'adresse ci-dessous doit porter ces deux nombres : un test le lit).
inline constexpr int PASSE_HEURES = 6;
inline constexpr int FUTUR_HEURES = 48;
inline constexpr int HEURES_ATTENDUES = PASSE_HEURES + FUTUR_HEURES; // 54
// Capacité des tableaux des réponses : une marge au-dessus de 54 ; une réponse plus longue est refusée, pas tronquée en silence.
inline constexpr std::size_t HEURES_MAX = 60;

namespace detail {

inline constexpr std::string_view MARINE_DEBUT = "https://marine-api.open-meteo.com/v1/marine?latitude=";
inline constexpr std::string_view MARINE_FIN =
    "&hourly=wave_height,wave_period,wave_direction,swell_wave_height,swell_wave_period,swell_wave_direction,"
    "wind_wave_height,wind_wave_period,wind_wave_direction,sea_surface_temperature,sea_level_height_msl"
    "&length_unit=metric&cell_selection=sea&past_hours=6&forecast_hours=48&timezone=auto&timeformat=unixtime";

inline constexpr std::string_view VENT_DEBUT = "https://api.open-meteo.com/v1/forecast?latitude=";
inline constexpr std::string_view VENT_FIN =
    "&hourly=wind_speed_10m,wind_direction_10m,wind_gusts_10m&wind_speed_unit=kmh"
    "&past_hours=6&forecast_hours=48&timezone=auto&timeformat=unixtime";

inline constexpr std::string_view MILIEU = "&longitude=";

} // namespace detail

// Taille du plus grand tampon nécessaire, zéro final compris (latitude « -90.00000 » = 9 caractères, longitude « -180.00000 » = 10).
inline constexpr std::size_t TAILLE_REQUETE_MARINE_MAX =
    detail::MARINE_DEBUT.size() + 9 + detail::MILIEU.size() + 10 + detail::MARINE_FIN.size() + 1;
inline constexpr std::size_t TAILLE_REQUETE_VENT_MAX =
    detail::VENT_DEBUT.size() + 9 + detail::MILIEU.size() + 10 + detail::VENT_FIN.size() + 1;

namespace detail {

constexpr std::size_t construire(std::string_view debut, std::string_view fin, std::int32_t latitude_e5,
                                 std::int32_t longitude_e5, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0)
        return 0;
    if (latitude_e5 < -meteo::LATITUDE_E5_MAX || latitude_e5 > meteo::LATITUDE_E5_MAX)
        return 0;
    if (longitude_e5 < -meteo::LONGITUDE_E5_MAX || longitude_e5 > meteo::LONGITUDE_E5_MAX)
        return 0;
    meteo::detail::Sortie s{sortie, capacite};
    s.texte(debut);
    s.degres_e5(latitude_e5);
    s.texte(MILIEU);
    s.degres_e5(longitude_e5);
    s.texte(fin);
    if (s.trop)
        return 0;
    sortie[s.n] = '\0';
    return s.n;
}

} // namespace detail

// Écrit l'adresse de la requête MER dans `sortie` (zéro final compris) et rend sa longueur sans le zéro. Rend 0, sans rien garantir
// sur `sortie`, si une coordonnée sort de -90..90 / -180..180 degrés ou si `capacite` est trop petite.
constexpr std::size_t construire_requete_marine(std::int32_t latitude_e5, std::int32_t longitude_e5, char *sortie,
                                                std::size_t capacite) noexcept {
    return detail::construire(detail::MARINE_DEBUT, detail::MARINE_FIN, latitude_e5, longitude_e5, sortie, capacite);
}

// Idem pour la requête VENT.
constexpr std::size_t construire_requete_vent(std::int32_t latitude_e5, std::int32_t longitude_e5, char *sortie,
                                              std::size_t capacite) noexcept {
    return detail::construire(detail::VENT_DEBUT, detail::VENT_FIN, latitude_e5, longitude_e5, sortie, capacite);
}

} // namespace ecran::surf
