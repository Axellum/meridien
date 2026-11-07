// Noyau pur : la date de l'écran principal dans les sept langues. « vendredi 6 novembre 2026 » en français, « Freitag, 6. November
// 2026 » en allemand, « 6 Kasım 2026 Cuma » en turc : l'ordre des mots et la ponctuation changent d'une langue à l'autre, les noms
// de jours et de mois viennent de la table de traduction (ecran/i18n.hpp, clé = nom français de heure_fr.hpp).
// C++17 standard, aucun include ESP-IDF ni LVGL ; évaluable à la compilation (test/host/test_visuel_dates.cpp).
//
// Ce que ce fichier ne dit pas : que chaque tournure soit celle qu'un locuteur écrirait (non relu par un locuteur natif, comme les
// traductions de la table). Les tournures sont celles des calendriers courants de chaque langue.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/heure_fr.hpp"
#include "ecran/i18n.hpp"

namespace ecran::visuel {

namespace detail {

// Écrit dans un tampon borné ; `trop` passe à vrai au premier octet qui ne tient pas (le résultat est alors vidé).
struct Plume {
    char *sortie;
    std::size_t capacite;
    std::size_t n = 0;
    bool trop = false;

    constexpr void car(char c) noexcept {
        if (sortie == nullptr || n + 1 >= capacite) {
            trop = true;
            return;
        }
        sortie[n++] = c;
    }
    constexpr void texte(std::string_view s) noexcept {
        for (char c : s)
            car(c);
    }
    constexpr void entier(int v) noexcept {
        if (v < 0) {
            trop = true;
            return;
        }
        char chiffres[12] = {};
        int nb = 0;
        do {
            chiffres[nb++] = static_cast<char>('0' + v % 10);
            v /= 10;
        } while (v > 0 && nb < 11);
        while (nb > 0)
            car(chiffres[--nb]);
    }
    constexpr std::size_t finir() noexcept {
        if (sortie == nullptr || capacite == 0)
            return 0;
        if (trop) {
            sortie[0] = '\0';
            return 0;
        }
        sortie[n] = '\0';
        return n;
    }
};

} // namespace detail

// Nom du jour (0 dimanche … 6 samedi) dans la langue ; vide si hors plage.
constexpr std::string_view nom_jour_l(i18n::Langue langue, int jour_semaine) noexcept {
    const std::string_view fr = heure::nom_jour(jour_semaine);
    return fr.empty() ? fr : i18n::tr(fr, langue);
}

// Nom du mois (1 à 12) dans la langue ; vide si hors plage.
constexpr std::string_view nom_mois_l(i18n::Langue langue, int mois) noexcept {
    const std::string_view fr = heure::nom_mois(mois);
    return fr.empty() ? fr : i18n::tr(fr, langue);
}

// Date longue de l'écran. Rend le nombre d'octets écrits (sans le zéro), 0 et un tampon vidé si la date est invalide ou si le
// tampon est trop petit. `avec_annee` faux : « vendredi 6 novembre ».
constexpr std::size_t formater_date(i18n::Langue langue, const heure::HeureLocale &h, char *sortie,
                                    std::size_t capacite, bool avec_annee = true) noexcept {
    detail::Plume p{sortie, capacite};
    if (h.mois < 1 || h.mois > 12 || h.jour < 1 || h.jour > 31 || h.jour_semaine < 0 || h.jour_semaine > 6 ||
        h.an < 0) {
        p.trop = true;
        return p.finir();
    }
    const std::string_view j = nom_jour_l(langue, h.jour_semaine);
    const std::string_view m = nom_mois_l(langue, h.mois);
    using i18n::Langue;
    switch (langue) {
    case Langue::Fr:
        p.texte(j);
        p.car(' ');
        if (h.jour == 1) {
            p.texte("1er");
        } else {
            p.entier(h.jour);
        }
        p.car(' ');
        p.texte(m);
        break;
    case Langue::En:
        p.texte(j);
        p.texte(", ");
        p.entier(h.jour);
        p.car(' ');
        p.texte(m);
        break;
    case Langue::De:
        p.texte(j);
        p.texte(", ");
        p.entier(h.jour);
        p.texte(". ");
        p.texte(m);
        break;
    case Langue::Nl:
    case Langue::It:
        p.texte(j);
        p.car(' ');
        p.entier(h.jour);
        p.car(' ');
        p.texte(m);
        break;
    case Langue::Es:
        p.texte(j);
        p.texte(", ");
        p.entier(h.jour);
        p.texte(" de ");
        p.texte(m);
        break;
    case Langue::Tr:
        p.entier(h.jour);
        p.car(' ');
        p.texte(m);
        if (avec_annee) {
            p.car(' ');
            p.entier(h.an);
        }
        p.car(' ');
        p.texte(j);
        return p.finir();
    }
    if (avec_annee) {
        p.car(' ');
        if (langue == Langue::Es)
            p.texte("de ");
        p.entier(h.an);
    }
    return p.finir();
}

// Jours abrégés de la bande de la semaine, ceux que chaque langue écrit dans ses calendriers (dimanche en premier, comme
// jour_semaine). Une simple troncature donnerait « Paz » pour dimanche ET lundi en turc, et « Mit », « Mon » en allemand, que
// personne n'écrit : d'où la table.
inline constexpr const char *JOURS_COURTS[i18n::NOMBRE_LANGUES][7] = {
    {"dim", "lun", "mar", "mer", "jeu", "ven", "sam"}, // fr
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}, // en
    {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"},        // de
    {"zo", "ma", "di", "wo", "do", "vr", "za"},        // nl
    {"dom", "lun", "mar", "mi\xC3\xA9", "jue", "vie",
     "s\xC3\xA1"
     "b"},                                             // es : mié, sáb
    {"dom", "lun", "mar", "mer", "gio", "ven", "sab"}, // it
    {"Paz", "Pzt", "Sal",
     "\xC3\x87"
     "ar",
     "Per", "Cum", "Cmt"}, // tr : Çar
};

// Jour abrégé pour la bande de la semaine (« ven », « Fr », « Cmt »). Rend 0 si le jour est invalide ou si le tampon est trop petit.
constexpr std::size_t jour_court(i18n::Langue langue, int jour_semaine, char *sortie, std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    const std::size_t l = static_cast<std::size_t>(langue);
    if (jour_semaine < 0 || jour_semaine > 6 || l >= i18n::NOMBRE_LANGUES) {
        p.trop = true;
        return p.finir();
    }
    p.texte(JOURS_COURTS[l][jour_semaine]);
    return p.finir();
}

} // namespace ecran::visuel
