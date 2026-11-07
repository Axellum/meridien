// Noyau pur : adresse (URL) de la requête Open-Meteo pour une position. C++17 standard, aucun include ESP-IDF, pas de flottant :
// les coordonnées arrivent en CENTAINES DE MILLIÈMES de degré (« e5 » : 45,74906 ° = 4574906), ce qui donne 5 décimales,
// soit environ un mètre.
//
// Paramètres : ceux dont `analyser_riche()` (meteo_riche.hpp) lit la réponse — `current` (courant complet), `hourly` (24 heures,
// +1 étiquette : voir ci-dessous), `daily` sur 7 jours, `minutely_15` (pluie par quarts d'heure), fuseau automatique, instants
// en secondes UNIX. Sans clé d'API. Chaque nom de variable a été vérifié par un appel réel le 07/11/2026 (docs/mesures/meteo.md,
// test/host/capturer_meteo_riche.py).
//
// Pourquoi `forecast_hours=25` et `forecast_minutely_15=9` : Open-Meteo donne un cumul (précipitations, probabilité, UV moyen)
// pour l'intervalle qui PRÉCÈDE son étiquette ; pour garder 24 heures complètes (et 8 quarts d'heure à venir plus le dernier
// quart écoulé) il faut une étiquette de plus : le noyau décale les cumuls d'une étiquette (voir meteo_riche.hpp).
//
// Attention côté appareil : l'adresse fait 863 octets au plus (TAILLE_REQUETE_MAX - 1). `esp_http_client` écrit la LIGNE de
// requête (« GET /chemin?… HTTP/1.1 », 851 octets au plus) en une fois dans son tampon d'envoi, qui fait 512 octets par défaut :
// on demande `buffer_size_tx` = 1024. Les en-têtes (Host, User-Agent…) partent ensuite par tranches du même tampon (lu dans
// esp_http_client.c d'ESP-IDF 5.5.5, `esp_http_client_request_send`) : seule la première ligne doit y tenir (voir le test de ce
// fichier). Réponse mesurée : voir docs/mesures/meteo.md (4 427 à 4 575 octets, non compressée : on n'envoie pas `Accept-Encoding`).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ecran::meteo {

inline constexpr std::int32_t LATITUDE_E5_MAX = 9000000;   // 90 °
inline constexpr std::int32_t LONGITUDE_E5_MAX = 18000000; // 180 °

namespace detail {

inline constexpr std::string_view REQUETE_DEBUT = "https://api.open-meteo.com/v1/forecast?latitude=";
inline constexpr std::string_view REQUETE_MILIEU = "&longitude=";
inline constexpr std::string_view REQUETE_FIN =
    "&current=temperature_2m,apparent_temperature,is_day,precipitation,weather_code,cloud_cover,wind_speed_10m,"
    "wind_direction_10m,wind_gusts_10m,pressure_msl,relative_humidity_2m"
    "&hourly=temperature_2m,apparent_temperature,relative_humidity_2m,precipitation_probability,precipitation,"
    "weather_code,cloud_cover,wind_speed_10m,wind_direction_10m,wind_gusts_10m,uv_index,is_day&forecast_hours=25"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,apparent_temperature_max,apparent_temperature_min,"
    "sunrise,sunset,sunshine_duration,uv_index_max,precipitation_sum,snowfall_sum,precipitation_hours,"
    "precipitation_probability_max,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant"
    "&minutely_15=precipitation&forecast_minutely_15=9"
    "&forecast_days=7&timezone=auto&timeformat=unixtime";

// Écrit dans un tampon borné, sans jamais déborder ; `trop` devient vrai à la première place manquante.
struct Sortie {
    char *p;
    std::size_t capacite;
    std::size_t n = 0;
    bool trop = false;

    constexpr void car(char c) noexcept {
        if (n + 1 >= capacite) { // garder la place du zéro final
            trop = true;
            return;
        }
        p[n++] = c;
    }
    constexpr void texte(std::string_view t) noexcept {
        for (const char c : t)
            car(c);
    }
    // Degrés en e5 -> « -33.86785 » (signe, partie entière, point, cinq chiffres).
    constexpr void degres_e5(std::int32_t v) noexcept {
        std::int64_t a = v;
        if (a < 0) {
            car('-');
            a = -a;
        }
        std::int64_t entier = a / 100000;
        char chiffres[12] = {};
        int nb = 0;
        do {
            chiffres[nb++] = static_cast<char>('0' + entier % 10);
            entier /= 10;
        } while (entier > 0);
        while (nb > 0)
            car(chiffres[--nb]);
        car('.');
        std::int64_t frac = a % 100000;
        for (std::int64_t pas = 10000; pas > 0; pas /= 10) {
            car(static_cast<char>('0' + frac / pas));
            frac %= pas;
        }
    }
};

} // namespace detail

// Taille du plus grand tampon nécessaire, zéro final compris (latitude et longitude au plus long : « -90.00000 », « -180.00000 »).
inline constexpr std::size_t TAILLE_REQUETE_MAX =
    detail::REQUETE_DEBUT.size() + 9 + detail::REQUETE_MILIEU.size() + 10 + detail::REQUETE_FIN.size() + 1;

// Écrit l'adresse complète dans `sortie` (zéro final compris) et rend sa longueur sans le zéro. Rend 0, sans rien garantir
// sur `sortie`, si une coordonnée sort de -90..90 / -180..180 degrés ou si `capacite` est trop petite.
constexpr std::size_t construire_requete(std::int32_t latitude_e5, std::int32_t longitude_e5, char *sortie,
                                         std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0)
        return 0;
    if (latitude_e5 < -LATITUDE_E5_MAX || latitude_e5 > LATITUDE_E5_MAX)
        return 0;
    if (longitude_e5 < -LONGITUDE_E5_MAX || longitude_e5 > LONGITUDE_E5_MAX)
        return 0;
    detail::Sortie s{sortie, capacite};
    s.texte(detail::REQUETE_DEBUT);
    s.degres_e5(latitude_e5);
    s.texte(detail::REQUETE_MILIEU);
    s.degres_e5(longitude_e5);
    s.texte(detail::REQUETE_FIN);
    if (s.trop)
        return 0;
    sortie[s.n] = '\0';
    return s.n;
}

} // namespace ecran::meteo
