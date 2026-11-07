// Noyau pur : adresse (URL) de la requête des alertes officielles à Vigie. C++17 standard, aucun include ESP-IDF, pas de flottant :
// les coordonnées arrivent en CENTAINES DE MILLIÈMES de degré (« e5 », comme celles de la météo) et partent arrondies à TROIS
// décimales (≈ 110 m : les zones d'alerte font des kilomètres, et l'instance publique n'a pas à connaître la position au mètre).
//
// Route : GET <adresse>/v1/vigilances?lat=…&lon=…&lang=<langue de l'écran>,en (ADR-0009). `lang` est un paramètre de Vigie
// (« lang=fr,en garde seulement ces langues », lu dans vigie_api/app/main.py le 07/11/2026) : sans lui un avertissement
// d'un service allemand ou espagnol arrive en 8 langues et pèse jusqu'à 26 Ko ; avec lui, en deux. Les mots de l'émetteur ne sont
// pas touchés : Vigie les filtre par langue, sans les réécrire.
//
// L'adresse de base est un réglage (https:// obligatoire : un avertissement lu en clair pourrait être falsifié, et
// `esp_crt_bundle_attach` ne vérifie que le TLS). `normaliser_adresse` la nettoie et la refuse si elle ne sert à rien.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ecran::vigie {

// Instance publique de Vigie (la même que dans docs/recherche et dans le dépôt vigie_web). Limitée par adresse IP et « pas faite pour
// bâtir un service dessus » : une requête par période, voir vigie_planning.hpp.
inline constexpr std::string_view ADRESSE_PUBLIQUE = "https://vigie.auxfilsdesidees.fr";

inline constexpr std::size_t ADRESSE_MAX = 80; // octets de l'adresse de base, sans le zéro final
inline constexpr std::int32_t LATITUDE_E5_MAX = 9000000;
inline constexpr std::int32_t LONGITUDE_E5_MAX = 18000000;

namespace detail {

inline constexpr std::string_view REQUETE_DEBUT = "/v1/vigilances?lat=";
inline constexpr std::string_view REQUETE_MILIEU = "&lon=";
inline constexpr std::string_view REQUETE_LANGUE = "&lang=";
inline constexpr std::string_view LANGUE_DE_SECOURS = "en";

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
    // Millièmes de degré -> « -33.868 » (signe, partie entière, point, trois chiffres).
    constexpr void degres_e3(std::int32_t v) noexcept {
        std::int64_t a = v;
        if (a < 0) {
            car('-');
            a = -a;
        }
        std::int64_t entier = a / 1000;
        char chiffres[12] = {};
        int nb = 0;
        do {
            chiffres[nb++] = static_cast<char>('0' + entier % 10);
            entier /= 10;
        } while (entier > 0);
        while (nb > 0)
            car(chiffres[--nb]);
        car('.');
        const std::int64_t frac = a % 1000;
        car(static_cast<char>('0' + frac / 100));
        car(static_cast<char>('0' + (frac / 10) % 10));
        car(static_cast<char>('0' + frac % 10));
    }
};

// e5 -> e3, arrondi à la valeur la plus proche, la moitié loin de zéro (comme `en_fixe` de json_mini.hpp).
constexpr std::int32_t e5_vers_e3(std::int32_t v) noexcept {
    const std::int64_t a = v < 0 ? -static_cast<std::int64_t>(v) : v;
    const std::int64_t r = (a + 50) / 100;
    return static_cast<std::int32_t>(v < 0 ? -r : r);
}

constexpr bool est_minuscule(char c) noexcept { return c >= 'a' && c <= 'z'; }
constexpr char en_minuscule(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
constexpr bool est_alnum_ascii(char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

} // namespace detail

// Plus grand tampon nécessaire, zéro final compris : adresse de base + chemin + « -90.000 » + « &lon= » + « -180.000 » + « &lang=xx,en ».
inline constexpr std::size_t TAILLE_REQUETE_MAX = ADRESSE_MAX + detail::REQUETE_DEBUT.size() + 7 +
                                                  detail::REQUETE_MILIEU.size() + 8 + detail::REQUETE_LANGUE.size() +
                                                  2 + 1 + detail::LANGUE_DE_SECOURS.size() + 1;

// Nettoie l'adresse de base saisie dans les réglages et l'écrit dans `sortie` (zéro final compris). Rend sa longueur (sans le zéro),
// ou 0 sans rien garantir sur `sortie` si l'adresse est refusée :
//   - espaces au début et à la fin retirés ; « / » de fin retirés ; le schéma est écrit en minuscules ;
//   - doit commencer par « https:// » (« http:// » est refusé) et nommer un hôte qui commence par une lettre ou un chiffre ;
//   - ni espace ni caractère de contrôle ni octet >= 0x80, ni « ? », « # », « @ » ni « \ » (pas de paramètres, pas d'identifiants) ;
//   - au plus ADRESSE_MAX octets. Un port (« :8443 ») et un chemin (« /vigie ») sont acceptés.
constexpr std::size_t normaliser_adresse(std::string_view saisie, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0)
        return 0;
    while (!saisie.empty() && (saisie.front() == ' ' || saisie.front() == '\t'))
        saisie.remove_prefix(1);
    while (!saisie.empty() && (saisie.back() == ' ' || saisie.back() == '\t' || saisie.back() == '/'))
        saisie.remove_suffix(1);
    constexpr std::string_view SCHEMA = "https://";
    if (saisie.size() <= SCHEMA.size() || saisie.size() > ADRESSE_MAX || saisie.size() + 1 > capacite)
        return 0;
    for (std::size_t i = 0; i < SCHEMA.size(); ++i) {
        if (detail::en_minuscule(saisie[i]) != SCHEMA[i])
            return 0;
    }
    if (!detail::est_alnum_ascii(saisie[SCHEMA.size()]))
        return 0;
    for (std::size_t i = SCHEMA.size(); i < saisie.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(saisie[i]);
        if (c <= 0x20 || c >= 0x7F || c == '?' || c == '#' || c == '@' || c == '\\')
            return 0;
    }
    for (std::size_t i = 0; i < saisie.size(); ++i)
        sortie[i] = i < SCHEMA.size() ? SCHEMA[i] : saisie[i];
    sortie[saisie.size()] = '\0';
    return saisie.size();
}

// Code de langue accepté dans `lang=` : deux lettres minuscules (« fr »), comme ecran::i18n::CODES.
constexpr bool code_langue_valide(std::string_view code) noexcept {
    return code.size() == 2 && detail::est_minuscule(code[0]) && detail::est_minuscule(code[1]);
}

// Écrit l'adresse complète dans `sortie` (zéro final compris) et rend sa longueur sans le zéro. `adresse` doit être celle que
// `normaliser_adresse` a rendue. Rend 0, sans rien garantir sur `sortie`, si une coordonnée sort de -90..90 / -180..180 degrés, si la
// langue n'est pas un code à deux lettres minuscules, si l'adresse est vide ou trop longue, ou si `capacite` est trop petite.
constexpr std::size_t construire_requete(std::string_view adresse, std::int32_t latitude_e5, std::int32_t longitude_e5,
                                         std::string_view langue, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0 || adresse.empty() || adresse.size() > ADRESSE_MAX)
        return 0;
    if (latitude_e5 < -LATITUDE_E5_MAX || latitude_e5 > LATITUDE_E5_MAX)
        return 0;
    if (longitude_e5 < -LONGITUDE_E5_MAX || longitude_e5 > LONGITUDE_E5_MAX)
        return 0;
    if (!code_langue_valide(langue))
        return 0;
    detail::Sortie s{sortie, capacite};
    s.texte(adresse);
    s.texte(detail::REQUETE_DEBUT);
    s.degres_e3(detail::e5_vers_e3(latitude_e5));
    s.texte(detail::REQUETE_MILIEU);
    s.degres_e3(detail::e5_vers_e3(longitude_e5));
    s.texte(detail::REQUETE_LANGUE);
    s.texte(langue);
    if (langue != detail::LANGUE_DE_SECOURS) { // « fr,en » ; pour l'anglais, « en » tout court
        s.car(',');
        s.texte(detail::LANGUE_DE_SECOURS);
    }
    if (s.trop)
        return 0;
    sortie[s.n] = '\0';
    return s.n;
}

} // namespace ecran::vigie
