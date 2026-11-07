// Noyau pur : dates, heures et durées ISO 8601 des réponses de MET Norway et du NWS. C++17 standard, aucun include ESP-IDF,
// évaluable à la compilation.
//
// Formes lues, et elles seules (tout le reste est refusé, jamais deviné) — celles que les deux services emploient, relevées sur
// leurs réponses réelles du 07/11/2026 (docs/mesures/fournisseurs-meteo.md) :
//   instant   « 2026-11-07T12:00:00Z »  ou  « 2026-11-07T01:00:00+00:00 » (décalage ±HH:MM) ; pas de fraction de seconde
//   durée     « PT3H », « P1DT3H », « P8D », « P7DT23H », « PT30M » : jours, heures, minutes, secondes entiers, au moins un
//   intervalle « 2026-11-07T01:00:00+00:00/PT3H » : début puis durée (la forme des couches de la grille du NWS)
//   date HTTP « Sat, 07 Nov 2026 13:27:13 GMT » : les en-têtes `Expires` et `Last-Modified` des deux services
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/heure_fr.hpp"

namespace ecran::iso {

namespace detail {

// `n` chiffres décimaux à partir de `p` ; faux si un caractère n'est pas un chiffre.
constexpr bool chiffres(std::string_view t, std::size_t p, std::size_t n, int &v) noexcept {
    if (p + n > t.size())
        return false;
    int r = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const char c = t[p + i];
        if (c < '0' || c > '9')
            return false;
        r = r * 10 + (c - '0');
    }
    v = r;
    return true;
}

} // namespace detail

// Instant UNIX (secondes) d'une date-heure ISO 8601. Faux si la forme n'est pas l'une de celles qui sont lues, si une
// composante sort de sa plage (mois 1..12, jour du mois, heure 0..23, minute 0..59, seconde 0..59) ou si le décalage dépasse
// ±14 h.
constexpr bool instant(std::string_view t, std::int64_t &sortie) noexcept {
    int an = 0, mois = 0, jour = 0, h = 0, mn = 0, s = 0;
    if (t.size() < 20 || t[4] != '-' || t[7] != '-' || t[10] != 'T' || t[13] != ':' || t[16] != ':')
        return false;
    if (!detail::chiffres(t, 0, 4, an) || !detail::chiffres(t, 5, 2, mois) || !detail::chiffres(t, 8, 2, jour) ||
        !detail::chiffres(t, 11, 2, h) || !detail::chiffres(t, 14, 2, mn) || !detail::chiffres(t, 17, 2, s))
        return false;
    if (mois < 1 || mois > 12 || jour < 1 || jour > heure::detail::jours_du_mois(an, mois) || h > 23 || mn > 59 ||
        s > 59)
        return false;
    std::int64_t decalage = 0;
    if (t.size() == 20 && t[19] == 'Z') {
        decalage = 0;
    } else if (t.size() == 25 && (t[19] == '+' || t[19] == '-') && t[22] == ':') {
        int dh = 0, dm = 0;
        if (!detail::chiffres(t, 20, 2, dh) || !detail::chiffres(t, 23, 2, dm) || dm > 59 || dh * 60 + dm > 14 * 60)
            return false;
        decalage = (dh * 3600 + dm * 60) * (t[19] == '-' ? -1 : 1);
    } else {
        return false;
    }
    sortie = heure::detail::jours_depuis_epoch(an, mois, jour) * 86400 + h * 3600 + mn * 60 + s - decalage;
    return true;
}

// Durée ISO 8601 en secondes (« PT3H » -> 10800). Au moins une composante ; chaque composante une seule fois et dans l'ordre
// jours, (T) heures, minutes, secondes ; « T » seul ou en queue est refusé. Plafond : 366 jours.
constexpr bool duree(std::string_view t, std::int64_t &sortie) noexcept {
    if (t.size() < 3 || t[0] != 'P')
        return false;
    std::size_t p = 1;
    bool apres_t = false;
    int etape = 0; // 0 : jours attendus, 1 : heures, 2 : minutes, 3 : secondes
    std::int64_t total = 0;
    bool au_moins_un = false;
    while (p < t.size()) {
        if (t[p] == 'T') {
            if (apres_t || p + 1 >= t.size())
                return false;
            apres_t = true;
            etape = 1;
            ++p;
            continue;
        }
        std::int64_t n = 0;
        std::size_t debut = p;
        while (p < t.size() && t[p] >= '0' && t[p] <= '9') {
            n = n * 10 + (t[p] - '0');
            if (n > 40000000)
                return false;
            ++p;
        }
        if (p == debut || p >= t.size())
            return false;
        const char u = t[p++];
        if (u == 'D' && !apres_t && etape == 0) {
            total += n * 86400;
            etape = 1;
        } else if (u == 'H' && apres_t && etape == 1) {
            total += n * 3600;
            etape = 2;
        } else if (u == 'M' && apres_t && etape <= 2) {
            total += n * 60;
            etape = 3;
        } else if (u == 'S' && apres_t && etape <= 3) {
            total += n;
            etape = 4;
        } else {
            return false;
        }
        au_moins_un = true;
    }
    if (!au_moins_un || total > 366LL * 86400)
        return false;
    sortie = total;
    return true;
}

// « début/durée » : l'intervalle d'une couche du NWS.
constexpr bool intervalle(std::string_view t, std::int64_t &debut_s, std::int64_t &duree_s) noexcept {
    std::size_t barre = std::string_view::npos;
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (t[i] == '/') {
            if (barre != std::string_view::npos)
                return false; // une seule barre
            barre = i;
        }
    }
    if (barre == std::string_view::npos)
        return false;
    std::int64_t d = 0, l = 0;
    if (!instant(t.substr(0, barre), d) || !duree(t.substr(barre + 1), l) || l <= 0)
        return false;
    debut_s = d;
    duree_s = l;
    return true;
}

// Date HTTP (RFC 9110, « IMF-fixdate ») des en-têtes `Expires` et `Last-Modified` : « Sat, 07 Nov 2026 13:27:13 GMT ». Seule cette
// forme, longue de 29 caractères, est lue (les deux services la donnent ; les formes obsolètes de la RFC, « Saturday, 07-Nov-26 »
// et « Sat Nov  7 13:27:13 2026 », sont refusées, de même que tout fuseau autre que GMT). Le jour de la semaine est contrôlé : une
// date dont le nom du jour ne va pas avec la date est refusée. Aucune sortie n'est touchée si la date est refusée.
constexpr bool date_http(std::string_view t, std::int64_t &sortie) noexcept {
    constexpr std::string_view JOURS = "SunMonTueWedThuFriSat";
    constexpr std::string_view MOIS = "JanFebMarAprMayJunJulAugSepOctNovDec";
    int jour = 0, an = 0, h = 0, mn = 0, s = 0;
    if (t.size() != 29 || t[3] != ',' || t[4] != ' ' || t[7] != ' ' || t[11] != ' ' || t[16] != ' ' || t[19] != ':' ||
        t[22] != ':' || t.substr(25) != " GMT")
        return false;
    if (!detail::chiffres(t, 5, 2, jour) || !detail::chiffres(t, 12, 4, an) || !detail::chiffres(t, 17, 2, h) ||
        !detail::chiffres(t, 20, 2, mn) || !detail::chiffres(t, 23, 2, s))
        return false;
    int mois = 0;
    for (int m = 0; m < 12 && mois == 0; ++m) {
        if (t.substr(8, 3) == MOIS.substr(static_cast<std::size_t>(m) * 3, 3))
            mois = m + 1;
    }
    int nom_du_jour = -1;
    for (int d = 0; d < 7 && nom_du_jour < 0; ++d) {
        if (t.substr(0, 3) == JOURS.substr(static_cast<std::size_t>(d) * 3, 3))
            nom_du_jour = d;
    }
    if (mois == 0 || nom_du_jour < 0 || jour < 1 || jour > heure::detail::jours_du_mois(an, mois) || h > 23 ||
        mn > 59 || s > 59)
        return false;
    const std::int64_t jours = heure::detail::jours_depuis_epoch(an, mois, jour);
    if (heure::detail::jour_semaine_de(jours) != nom_du_jour)
        return false;
    sortie = jours * 86400 + h * 3600 + mn * 60 + s;
    return true;
}

} // namespace ecran::iso
