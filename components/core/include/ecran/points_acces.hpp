// Noyau pur : classement du résultat d'une RECHERCHE DE RÉSEAUX Wi-Fi pour la liste de l'écran de réglages.
//
// La radio rend une ligne par point d'accès, pas par réseau : un même SSID apparaît autant de fois qu'il y a de bornes (répéteurs,
// 2,4 et 5 GHz), dans un ordre quelconque, et les réseaux cachés ont un SSID vide. L'écran veut une ligne par SSID, du plus fort au
// plus faible, avec un nombre limité de lignes (la liste tient dans une page). Pas d'allocation : le tableau de sortie est celui de
// l'appelant, dimensionné à l'avance (la RAM interne est comptée, voir docs/mesures/memoire-interne.md).
// C++17 standard, aucun include ESP-IDF, évalué à la compilation.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ecran {

// Un réseau vu à la recherche (même disposition que ce qu'attend main/reseau_wifi.h).
struct PointAcces {
    char ssid[33];        // octets, terminé par un zéro (jamais vide : les réseaux cachés ne sont pas listés)
    std::int8_t rssi_dbm; // par exemple -52 ; l'écran en tire 0 à 4 barres
    bool securise;        // faux : réseau ouvert (pas de mot de passe à demander)
};

namespace wifi {

namespace detail {
// Égalité de deux SSID (octets jusqu'au zéro, au plus 32).
constexpr bool memes_ssid(const char *a, const char *b) noexcept {
    for (std::size_t i = 0; i < 33; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
        if (a[i] == '\0') {
            return true;
        }
    }
    return true;
}

constexpr bool vide(const char *ssid) noexcept { return ssid[0] == '\0'; }

// `a` passe-t-il avant `b` dans la liste ? Plus fort d'abord ; à force égale, l'ordre des octets du SSID (résultat stable d'une
// recherche à l'autre).
constexpr bool avant(const PointAcces &a, const PointAcces &b) noexcept {
    if (a.rssi_dbm != b.rssi_dbm) {
        return a.rssi_dbm > b.rssi_dbm;
    }
    for (std::size_t i = 0; i < 33; ++i) {
        const unsigned char ca = static_cast<unsigned char>(a.ssid[i]);
        const unsigned char cb = static_cast<unsigned char>(b.ssid[i]);
        if (ca != cb) {
            return ca < cb;
        }
        if (ca == 0) {
            break;
        }
    }
    return false;
}

constexpr void copier(PointAcces &vers, const PointAcces &depuis) noexcept {
    for (std::size_t i = 0; i < 33; ++i) {
        vers.ssid[i] = depuis.ssid[i];
    }
    vers.ssid[32] = '\0'; // jamais de lecture au-delà, même si l'appelant a oublié le zéro
    vers.rssi_dbm = depuis.rssi_dbm;
    vers.securise = depuis.securise;
}
} // namespace detail

// Classe `n` points d'accès bruts dans `sortie` (au plus `max` réseaux) : SSID non vides, un seul par SSID (le plus fort, qui
// donne aussi `securise`), du plus fort au plus faible. Rend le nombre de réseaux écrits. `sortie` ne doit pas recouvrir `brut`.
constexpr std::size_t classer_points_acces(const PointAcces *brut, std::size_t n, PointAcces *sortie,
                                           std::size_t max) noexcept {
    std::size_t compte = 0;
    for (std::size_t i = 0; i < n && max > 0; ++i) {
        const PointAcces &p = brut[i];
        if (detail::vide(p.ssid)) {
            continue;
        }
        // Le même SSID est déjà dans la liste ?
        std::size_t existant = compte;
        for (std::size_t j = 0; j < compte; ++j) {
            if (detail::memes_ssid(sortie[j].ssid, p.ssid)) {
                existant = j;
                break;
            }
        }
        if (existant < compte) {
            if (!(p.rssi_dbm > sortie[existant].rssi_dbm)) {
                continue; // un point d'accès plus faible du même réseau : ignoré
            }
            // Plus fort : on retire l'ancienne ligne en décalant, puis on réinsère à sa place.
            for (std::size_t j = existant; j + 1 < compte; ++j) {
                detail::copier(sortie[j], sortie[j + 1]);
            }
            --compte;
        } else if (compte == max && !detail::avant(p, sortie[max - 1])) {
            continue; // liste pleine et ce réseau est plus faible que le dernier
        }
        // Insertion triée (en décalant vers le bas ; le dernier tombe si la liste est pleine).
        std::size_t pos = 0;
        while (pos < compte && !detail::avant(p, sortie[pos])) {
            ++pos;
        }
        std::size_t fin = compte < max ? compte : max - 1;
        for (std::size_t j = fin; j > pos; --j) {
            detail::copier(sortie[j], sortie[j - 1]);
        }
        detail::copier(sortie[pos], p);
        if (compte < max) {
            ++compte;
        }
    }
    return compte;
}

} // namespace wifi
} // namespace ecran
