// Noyau pur : adresses (URL) des requêtes MET Norway (Locationforecast) et NWS (api.weather.gov). C++17 standard, aucun include
// ESP-IDF, pas de flottant : les coordonnées arrivent en e5 (45,74906 ° = 4574906), comme pour Open-Meteo (meteo_requete.hpp).
//
// QUATRE DÉCIMALES AU PLUS. MET Norway l'exige (https://api.met.no/doc/TermsOfService : « truncate the results to max 4 decimals to
// facilitate caching ») ; le NWS répond par une redirection (HTTP 301) vers l'adresse tronquée à quatre décimales quand on en
// donne davantage (lu sur api.weather.gov/points le 07/11/2026, docs/mesures/fournisseurs-meteo.md). Les deux sont donc
// demandés à quatre décimales, en TRONQUANT vers zéro (jamais d'arrondi : 11 m au plus d'écart).
//
// DEUX ÉTAPES POUR LE NWS. 1) `/points/{lat},{lon}` donne la maille (bureau, x, y) et le fuseau, ou HTTP 404 hors de la
// couverture ; 2) `/gridpoints/{bureau}/{x},{y}` donne les données. L'adresse de la seconde est RECONSTRUITE ici à partir de trois
// valeurs contrôlées (bureau = 3 lettres majuscules, x et y = entiers) et non copiée dans la réponse de la première : une réponse
// ne peut pas envoyer l'appareil ailleurs que sur api.weather.gov.
//
// L'identification est obligatoire chez les deux (MET : User-Agent propre à l'application avec un contact, sinon HTTP 403 ; NWS :
// en-tête User-Agent exigé). Le contact est l'adresse du dépôt, pas une adresse électronique ni une donnée personnelle.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/meteo_requete.hpp"

namespace ecran::meteo::fournisseurs {

// Texte de l'en-tête `User-Agent` envoyé à MET Norway et au NWS. Le dépôt est privé tant que la publication n'est pas décidée
// (docs/publication.md) : le contact ne mène nulle part avant ce jour-là (docs/decisions/0014).
inline constexpr std::string_view USER_AGENT = "Meridien/ESP32-P4 github.com/Axellum/meridien";

namespace detail {

inline constexpr std::string_view METNO_DEBUT = "https://api.met.no/weatherapi/locationforecast/2.0/complete?lat=";
inline constexpr std::string_view METNO_MILIEU = "&lon=";
inline constexpr std::string_view NWS_POINTS_DEBUT = "https://api.weather.gov/points/";
inline constexpr std::string_view NWS_GRILLE_DEBUT = "https://api.weather.gov/gridpoints/";
// `Accept` demandé : la documentation des deux services donne ces types (GeoJSON pour le NWS).
inline constexpr std::string_view METNO_ACCEPT = "application/json";
inline constexpr std::string_view NWS_ACCEPT = "application/geo+json";

// Degrés en e5 -> « -33.8678 » : quatre décimales, la cinquième est supprimée (troncature vers zéro). Pas de signe devant un zéro.
constexpr void degres_e4(ecran::meteo::detail::Sortie &s, std::int32_t v) noexcept {
    std::int64_t a = v;
    const bool negatif = a < 0;
    if (negatif)
        a = -a;
    const std::int64_t entier = a / 100000;
    const std::int64_t frac = (a % 100000) / 10; // quatre chiffres
    if (negatif && (entier != 0 || frac != 0))
        s.car('-');
    char chiffres[12] = {};
    int nb = 0;
    std::int64_t reste = entier;
    do {
        chiffres[nb++] = static_cast<char>('0' + reste % 10);
        reste /= 10;
    } while (reste > 0);
    while (nb > 0)
        s.car(chiffres[--nb]);
    s.car('.');
    std::int64_t pas = 1000;
    std::int64_t f = frac;
    for (; pas > 0; pas /= 10) {
        s.car(static_cast<char>('0' + f / pas));
        f %= pas;
    }
}

constexpr bool coordonnees_valides(std::int32_t latitude_e5, std::int32_t longitude_e5) noexcept {
    return latitude_e5 >= -LATITUDE_E5_MAX && latitude_e5 <= LATITUDE_E5_MAX && longitude_e5 >= -LONGITUDE_E5_MAX &&
           longitude_e5 <= LONGITUDE_E5_MAX;
}

constexpr void entier(ecran::meteo::detail::Sortie &s, std::int64_t v) noexcept {
    char chiffres[24] = {};
    int nb = 0;
    do {
        chiffres[nb++] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v > 0);
    while (nb > 0)
        s.car(chiffres[--nb]);
}

} // namespace detail

// Plus grand tampon nécessaire, zéro final compris (« -90.0000 » et « -180.0000 »).
inline constexpr std::size_t TAILLE_REQUETE_METNO_MAX =
    detail::METNO_DEBUT.size() + 8 + detail::METNO_MILIEU.size() + 9 + 1;
inline constexpr std::size_t TAILLE_REQUETE_NWS_POINTS_MAX = detail::NWS_POINTS_DEBUT.size() + 8 + 1 + 9 + 1;
// x et y : 4 chiffres au plus, bureau : 3 lettres.
inline constexpr std::size_t TAILLE_REQUETE_NWS_GRILLE_MAX = detail::NWS_GRILLE_DEBUT.size() + 3 + 1 + 4 + 1 + 4 + 1;
// Les mêmes valeurs en texte terminé par un zéro, pour le client réseau (main/http_commun.h).
inline constexpr const char *ACCEPT_METNO = detail::METNO_ACCEPT.data();
inline constexpr const char *ACCEPT_NWS = detail::NWS_ACCEPT.data();

// Plus grande réponse acceptée, par requête (octets sur le fil ET octets décompressés : `client_http::Requete::taille_max`). Les
// plus grandes réponses RÉELLES du 07/11/2026 (test/host/donnees/) : MET Norway 90 783 octets (Tromsø, 91 pas de temps), grille du
// NWS 240 402 octets (San Juan), /points 3 937 octets. Marge d'environ 1,7 fois la plus grande ; une réponse plus grande est refusée
// (jamais lue à moitié) et l'appareil se replie sur Open-Meteo. Deux tampons de cette taille en PSRAM le temps de la requête.
inline constexpr std::size_t TAILLE_REPONSE_METNO_MAX = 160 * 1024;
inline constexpr std::size_t TAILLE_REPONSE_NWS_POINTS_MAX =
    64 * 1024; // 16 fois la réponse réelle ; la borne du client commun
inline constexpr std::size_t TAILLE_REPONSE_NWS_GRILLE_MAX = 384 * 1024;
inline constexpr std::size_t TAILLE_REQUETE_FOURNISSEURS_MAX =
    TAILLE_REQUETE_METNO_MAX > TAILLE_REQUETE_NWS_GRILLE_MAX ? TAILLE_REQUETE_METNO_MAX : TAILLE_REQUETE_NWS_GRILLE_MAX;

// Écrit l'adresse dans `sortie` (zéro final compris) et rend sa longueur sans le zéro. Rend 0 si une coordonnée sort de
// -90..90 / -180..180 degrés ou si `capacite` est trop petite.
constexpr std::size_t construire_requete_metno(std::int32_t latitude_e5, std::int32_t longitude_e5, char *sortie,
                                               std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0 || !detail::coordonnees_valides(latitude_e5, longitude_e5))
        return 0;
    ecran::meteo::detail::Sortie s{sortie, capacite};
    s.texte(detail::METNO_DEBUT);
    detail::degres_e4(s, latitude_e5);
    s.texte(detail::METNO_MILIEU);
    detail::degres_e4(s, longitude_e5);
    if (s.trop)
        return 0;
    sortie[s.n] = '\0';
    return s.n;
}

// Première étape du NWS : la maille de cette position.
constexpr std::size_t construire_requete_nws_points(std::int32_t latitude_e5, std::int32_t longitude_e5, char *sortie,
                                                    std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0 || !detail::coordonnees_valides(latitude_e5, longitude_e5))
        return 0;
    ecran::meteo::detail::Sortie s{sortie, capacite};
    s.texte(detail::NWS_POINTS_DEBUT);
    detail::degres_e4(s, latitude_e5);
    s.car(',');
    detail::degres_e4(s, longitude_e5);
    if (s.trop)
        return 0;
    sortie[s.n] = '\0';
    return s.n;
}

// Le bureau du NWS est un code de TROIS lettres majuscules (« OKX », « HFO »). Rien d'autre n'entre dans l'adresse.
constexpr bool bureau_nws_valide(std::string_view bureau) noexcept {
    if (bureau.size() != 3)
        return false;
    for (const char c : bureau) {
        if (c < 'A' || c > 'Z')
            return false;
    }
    return true;
}

inline constexpr int MAILLE_NWS_MAX = 9999; // x et y : quelques centaines dans la réalité ; quatre chiffres au plus ici

// Seconde étape du NWS : les données de la maille (bureau, x, y) rendue par la première. Rend 0 si le bureau n'est pas trois
// lettres majuscules ou si x ou y sort de 0..MAILLE_NWS_MAX.
constexpr std::size_t construire_requete_nws_grille(std::string_view bureau, int x, int y, char *sortie,
                                                    std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0 || !bureau_nws_valide(bureau) || x < 0 || x > MAILLE_NWS_MAX || y < 0 ||
        y > MAILLE_NWS_MAX)
        return 0;
    ecran::meteo::detail::Sortie s{sortie, capacite};
    s.texte(detail::NWS_GRILLE_DEBUT);
    s.texte(bureau);
    s.car('/');
    detail::entier(s, x);
    s.car(',');
    detail::entier(s, y);
    if (s.trop)
        return 0;
    sortie[s.n] = '\0';
    return s.n;
}

} // namespace ecran::meteo::fournisseurs
