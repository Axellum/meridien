// Noyau pur : adresse (URL) de la requête Open-Meteo pour une position. C++17 standard, aucun include ESP-IDF, pas de flottant :
// les coordonnées arrivent en CENTAINES DE MILLIÈMES de degré (« e5 » : 45,74906 ° = 4574906), ce qui donne 5 décimales,
// soit environ un mètre.
//
// Paramètres : ceux de docs/recherche/03-meteo-heure-reglages-calendrier.md §1 et de l'en-tête de meteo_openmeteo.hpp, ceux
// dont `analyser()` lit la réponse (current, hourly pour la probabilité de précipitations, daily sur 7 jours, fuseau
// automatique, instants en secondes UNIX). Sans clé d'API. Réponse mesurée : 2 826 à 2 960 octets, non compressée (on n'envoie
// pas `Accept-Encoding`).
//
// Attention côté appareil : l'adresse fait 479 octets au plus (TAILLE_REQUETE_MAX - 1) et `esp_http_client` écrit toute la
// requête (ligne, en-têtes) dans son tampon d'envoi, qui fait 512 octets par défaut : on demande `buffer_size_tx` = 1024
// (voir le test de ce fichier).
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
    "&current=temperature_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,wind_direction_10m,wind_gusts_10m,"
    "pressure_msl,relative_humidity_2m"
    "&hourly=temperature_2m,weather_code,precipitation_probability,uv_index&forecast_hours=48"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,precipitation_probability_"
    "max"
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
