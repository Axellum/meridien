// Noyau pur « surf » : pleines et basses mers tirées de la série `sea_level_height_msl`. C++17 standard, aucun include ESP-IDF,
// entiers seulement (centimètres, secondes UNIX en UTC). Aucune heure locale n'est formatée ici.
//
// INDICATIF PARTOUT. Open-Meteo écrit que ce niveau de mer, calculé à environ 8 km de maille, est rapporté au niveau moyen
// GLOBAL (pas au zéro des cartes marines) et « ne remplace pas un annuaire des marées » ; il ajoute que la précision baisse près
// des côtes. Les hauteurs de ce fichier sont donc des écarts au niveau moyen, pas des cotes marines, et les heures sont à l'heure
// près (la série est horaire, aucune interpolation n'est faite : voir ADR-0013). Une phrase affichée doit dire « indicatif ».
//
// Méthode :
//   1. extrema locaux de la série horaire. Un palier (plusieurs heures de suite à la même valeur en cm) compte pour UN extremum,
//      à l'heure du milieu du palier. Une valeur absente (`null`) coupe la série : on ne devine pas un extremum de part et d'autre
//      d'un trou, et ni la première ni la dernière heure ne peuvent en être un (il leur manque un voisin) ;
//   2. filtre « en zigzag » : un extremum à moins de MAREE_AMPLITUDE_MIN_CM de l'extremum précédent (de type opposé) est un accident
//      de la courbe (variation météorologique du niveau, bruit du modèle), pas une marée : il est écarté, et deux extrema de même
//      type qui se suivent après un écart écarté se fondent en le plus extrême des deux ;
//   3. la marée est « notable » s'il reste au moins deux extrema (une pleine mer ET une basse mer à plus de 30 cm l'une de l'autre).
// Pourquoi 30 cm : mesuré sur des réponses réelles du 07/11/2026 (docs/mesures/surf.md), la Baltique (Warnemünde, 20 cm) et la
// Méditerranée (Marseille, 27 cm) varient sur 54 heures SANS marée sensible (effet du vent et de la pression), alors qu'Oahu, où la
// marée est faible, varie de 68 cm. 30 cm tombe entre les deux, mais la marge au-dessus de Marseille n'est que de 3 cm et ne
// repose que sur UNE journée de capture : c'est un seuil d'ingénieur pour ces données, pas une propriété de l'océan (les
// mortes-eaux d'un littoral à marée faible peuvent passer dessous : non vérifié).
//
// Tendance : montante si le prochain extremum est une pleine mer, descendante si c'est une basse mer ; « étale » à moins d'une
// demi-heure d'un extremum. Inconnue si la marée n'est pas notable ou si l'instant demandé est hors de la série.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/surf_base.hpp"
#include "ecran/surf_donnees.hpp"

namespace ecran::surf {

// 54 h de marée semi-diurnale en font 9 ; la marge couvre le bruit filtré.
inline constexpr std::size_t MAREE_EXTREMA_MAX = 16;
inline constexpr std::int16_t MAREE_AMPLITUDE_MIN_CM = 30;
inline constexpr std::int64_t MAREE_ETALE_S = 30 * 60; // « étale » : à moins d'une demi-heure d'un extremum

struct Extremum {
    std::int64_t instant_s = 0; // UTC ; à l'heure du milieu d'un palier (peut tomber sur une demi-heure)
    std::int16_t niveau_cm = 0; // écart au niveau moyen global (pas une cote marine)
    bool pleine = false;        // vrai : pleine mer ; faux : basse mer
};

enum class Tendance : std::uint8_t { Inconnue, Montante, Descendante, Etale };

struct Maree {
    std::uint8_t nb = 0;
    Extremum extrema[MAREE_EXTREMA_MAX] = {};
    bool notable = false;           // au moins deux extrema à 30 cm ou plus l'un de l'autre
    std::int16_t marnage_cm = 0;    // plus haut moins plus bas parmi les extrema gardés (0 si aucun)
    std::int64_t serie_debut_s = 0; // première et dernière heure de la série lue (pour dire « hors de la série »)
    std::int64_t serie_fin_s = 0;   // dernière étiquette + 3 600 s : l'instant où la série cesse de couvrir
};

namespace detail {

// Ajoute un extremum brut à la liste filtrée « en zigzag » (voir l'en-tête).
constexpr void zigzag(Maree &m, const Extremum &x) noexcept {
    if (m.nb == 0) {
        m.extrema[m.nb++] = x;
        return;
    }
    Extremum &dernier = m.extrema[m.nb - 1];
    if (x.pleine == dernier.pleine) { // même type : on garde le plus extrême
        const bool plus_extreme = x.pleine ? x.niveau_cm > dernier.niveau_cm : x.niveau_cm < dernier.niveau_cm;
        if (plus_extreme)
            dernier = x;
        return;
    }
    const int ecart = x.niveau_cm - dernier.niveau_cm;
    if ((ecart < 0 ? -ecart : ecart) < MAREE_AMPLITUDE_MIN_CM)
        return; // accident de la courbe
    if (m.nb < MAREE_EXTREMA_MAX)
        m.extrema[m.nb++] = x;
}

} // namespace detail

constexpr Maree extraire_maree(const Marine &s) noexcept {
    Maree m{};
    if (s.nb == 0)
        return m;
    m.serie_debut_s = s.heures[0].debut_s;
    m.serie_fin_s = s.heures[s.nb - 1].debut_s + 3600;
    if (!s.a_maree || s.nb < 3)
        return m;
    std::size_t i = 1;
    while (i + 1 < s.nb) {
        const std::int16_t avant = s.heures[i - 1].maree_cm;
        const std::int16_t ici = s.heures[i].maree_cm;
        if (!present(avant) || !present(ici)) {
            ++i;
            continue;
        }
        std::size_t j = i; // fin du palier de valeurs égales
        while (j + 1 < s.nb && s.heures[j + 1].maree_cm == ici)
            ++j;
        if (j + 1 >= s.nb || !present(s.heures[j + 1].maree_cm)) {
            i = j + 1; // pas de voisin lisible après le palier
            continue;
        }
        const std::int16_t apres = s.heures[j + 1].maree_cm;
        if (ici > avant && ici > apres) {
            detail::zigzag(m, Extremum{(s.heures[i].debut_s + s.heures[j].debut_s) / 2, ici, true});
        } else if (ici < avant && ici < apres) {
            detail::zigzag(m, Extremum{(s.heures[i].debut_s + s.heures[j].debut_s) / 2, ici, false});
        }
        i = j + 1;
    }
    std::int16_t haut = INT16_MIN;
    std::int16_t bas = INT16_MAX;
    for (std::size_t k = 0; k < m.nb; ++k) {
        haut = m.extrema[k].niveau_cm > haut ? m.extrema[k].niveau_cm : haut;
        bas = m.extrema[k].niveau_cm < bas ? m.extrema[k].niveau_cm : bas;
    }
    if (m.nb >= 2) {
        m.notable = true;
        m.marnage_cm = static_cast<std::int16_t>(haut - bas);
    }
    return m;
}

// Prochaine pleine mer (`pleine` vrai) ou basse mer (faux) à `maintenant_s` ou après. nullptr si la marée n'est pas notable ou
// si la série n'en contient plus. Le pointeur vise `m`, qui doit survivre à l'appelant.
constexpr const Extremum *prochain_extremum(const Maree &m, bool pleine, std::int64_t maintenant_s) noexcept {
    if (!m.notable)
        return nullptr;
    for (std::size_t k = 0; k < m.nb; ++k) {
        if (m.extrema[k].pleine == pleine && m.extrema[k].instant_s >= maintenant_s)
            return &m.extrema[k];
    }
    return nullptr;
}

constexpr Tendance tendance_a(const Maree &m, std::int64_t maintenant_s) noexcept {
    if (!m.notable || maintenant_s < m.serie_debut_s || maintenant_s >= m.serie_fin_s)
        return Tendance::Inconnue;
    const Extremum *avant = nullptr; // dernier extremum à ou avant `maintenant_s`
    const Extremum *apres = nullptr; // premier extremum après
    for (std::size_t k = 0; k < m.nb; ++k) {
        if (m.extrema[k].instant_s <= maintenant_s) {
            avant = &m.extrema[k];
        } else if (apres == nullptr) {
            apres = &m.extrema[k];
        }
    }
    if ((avant != nullptr && maintenant_s - avant->instant_s <= MAREE_ETALE_S) ||
        (apres != nullptr && apres->instant_s - maintenant_s <= MAREE_ETALE_S)) {
        return Tendance::Etale;
    }
    if (apres != nullptr)
        return apres->pleine ? Tendance::Montante : Tendance::Descendante;
    if (avant != nullptr)
        return avant->pleine ? Tendance::Descendante : Tendance::Montante;
    return Tendance::Inconnue;
}

} // namespace ecran::surf
