// Noyau pur : règles de validité d'une zone météo (nom affiché, coordonnées, fuseau) avant de l'enregistrer en NVS. C++17
// standard, aucun include ESP-IDF. L'appareil les applique à tout ce qui vient de la page de réglages ou de la NVS : rien
// d'invraisemblable ne part vers Open-Meteo.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/meteo_requete.hpp"

namespace ecran::meteo {

// Degrés décimaux -> centièmes de millième de degré, arrondi à la valeur la plus proche (la moitié s'éloigne de zéro).
// Refuse (rend faux) un nombre non fini ou hors de ±`max_deg`. NaN échoue à la comparaison : `!(v >= -max && v <= max)`.
constexpr bool degres_vers_e5(double v, double max_deg, std::int32_t &e5) noexcept {
    if (!(v >= -max_deg && v <= max_deg))
        return false;
    const double mis_a_l_echelle = v * 100000.0;
    e5 = static_cast<std::int32_t>(mis_a_l_echelle >= 0.0 ? mis_a_l_echelle + 0.5 : mis_a_l_echelle - 0.5);
    return true;
}

constexpr bool latitude_vers_e5(double v, std::int32_t &e5) noexcept { return degres_vers_e5(v, 90.0, e5); }
constexpr bool longitude_vers_e5(double v, std::int32_t &e5) noexcept { return degres_vers_e5(v, 180.0, e5); }

namespace detail {

// UTF-8 bien formé : pas d'octet de continuation isolé, pas de forme trop longue (C0, C1, E0 80.., F0 80..), pas de
// substitut UTF-16 (ED A0..BF), rien au-delà de U+10FFFF (F4 90.. et F5..FF). Rend la longueur ou -1.
constexpr int longueur_utf8(const char *t, std::size_t restant) noexcept {
    const unsigned c = static_cast<unsigned char>(t[0]);
    const auto suite = [&](std::size_t i, unsigned bas, unsigned haut) {
        if (i >= restant)
            return false;
        const unsigned o = static_cast<unsigned char>(t[i]);
        return o >= bas && o <= haut;
    };
    if (c < 0x80)
        return 1;
    if (c >= 0xC2 && c <= 0xDF)
        return suite(1, 0x80, 0xBF) ? 2 : -1;
    if (c == 0xE0)
        return suite(1, 0xA0, 0xBF) && suite(2, 0x80, 0xBF) ? 3 : -1;
    if (c == 0xED)
        return suite(1, 0x80, 0x9F) && suite(2, 0x80, 0xBF) ? 3 : -1;
    if (c >= 0xE1 && c <= 0xEF)
        return suite(1, 0x80, 0xBF) && suite(2, 0x80, 0xBF) ? 3 : -1;
    if (c == 0xF0)
        return suite(1, 0x90, 0xBF) && suite(2, 0x80, 0xBF) && suite(3, 0x80, 0xBF) ? 4 : -1;
    if (c >= 0xF1 && c <= 0xF3)
        return suite(1, 0x80, 0xBF) && suite(2, 0x80, 0xBF) && suite(3, 0x80, 0xBF) ? 4 : -1;
    if (c == 0xF4)
        return suite(1, 0x80, 0x8F) && suite(2, 0x80, 0xBF) && suite(3, 0x80, 0xBF) ? 4 : -1;
    return -1;
}

} // namespace detail

// Nom à afficher : terminé par un zéro dans `capacite` octets, UTF-8 bien formé, ni caractère de contrôle ni espace seule
// (un nom fait d'espaces ne dirait rien à l'écran), au moins un caractère.
constexpr bool nom_de_zone_valide(const char *nom, std::size_t capacite) noexcept {
    if (nom == nullptr || capacite < 2)
        return false;
    bool visible = false;
    std::size_t i = 0;
    while (i < capacite && nom[i] != '\0') {
        const unsigned c = static_cast<unsigned char>(nom[i]);
        if (c < 0x20 || c == 0x7F)
            return false;
        const int n = detail::longueur_utf8(nom + i, capacite - i);
        if (n < 0)
            return false;
        if (c != ' ')
            visible = true;
        i += static_cast<std::size_t>(n);
    }
    return i < capacite && nom[i] == '\0' && visible;
}

// Nom IANA (« Europe/Paris », « America/Argentina/Buenos_Aires », « Etc/GMT+5 ») : mêmes caractères que `verifier()` de
// meteo_openmeteo.hpp, au moins un, terminé par un zéro dans `capacite` octets.
constexpr bool fuseau_de_zone_valide(const char *fuseau, std::size_t capacite) noexcept {
    if (fuseau == nullptr || capacite < 2)
        return false;
    std::size_t i = 0;
    while (i < capacite && fuseau[i] != '\0') {
        const char c = fuseau[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '_' ||
              c == '-' || c == '+')) {
            return false;
        }
        ++i;
    }
    return i >= 1 && i < capacite && fuseau[i] == '\0';
}

} // namespace ecran::meteo
