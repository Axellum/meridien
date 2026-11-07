// Noyau pur « surf » : le vent par rapport à la plage. C++17 standard, aucun include ESP-IDF, entiers seulement (le cosinus vient
// de la table de surf_base.hpp, calculée à la compilation).
//
// CETTE FORMULE EST DE NOUS : elle ne vient d'aucune source. Elle est l'usage courant des prévisions de surf, écrit ici avec des
// seuils nommés et justifiés ; ni Open-Meteo ni un site de surf ne la fournit. Le test la compare à `math.cos` de Python (table
// des 91 cosinus) et la fait passer par 359°/0°, vent nul et rafales (test_surf_vent.cpp).
//
// Définitions. d = direction D'OÙ VIENT le vent, en degrés (convention d'Open-Meteo : 0 = vent du nord, 90 = vent d'est). β =
// orientation de la plage = direction VERS LAQUELLE elle fait face, vers le large (surf_spot.hpp). Un vent dont d = β vient du
// large et souffle vers la terre : il est « de mer » (on-shore). Un vent dont d = β + 180° vient de la terre et souffle vers le
// large : « de terre » (off-shore). La composante vers la plage est
//
//        c = v · cos(d − β)        (> 0 : vers la plage ; < 0 : vers le large ; 0 : le vent longe la plage)
//
// avec v la vitesse. Elle est arrondie au dixième de km/h.
//
// Classes (sur cos(d − β) seul, la FORCE est jugée à part) :
//   - offshore (vent de terre) : cos(d − β) ≤ −0,5, c'est-à-dire un vent à 60° ou moins de la direction exactement de terre ;
//   - onshore (vent de mer)    : cos(d − β) ≥ +0,5, un vent à 60° ou moins de la direction exactement de mer ;
//   - travers (cross-shore)    : entre les deux (de 60° à 120° de la direction de mer).
// Pourquoi 0,5 (60°) : trois secteurs de 120° égaux, le plus petit découpage qui garde un « travers » aussi large que chacun des
// deux autres ; un vent à 60° de la terre garde au moins la moitié de sa vitesse dirigée vers le large, ce qui lisse encore la
// face de la vague. Le seuil est inclusif des deux côtés (60° exactement = onshore, 120° exactement = offshore) ; le choix de
// 0,5 plutôt que 0,707 (45°) est un parti pris, à reprendre sur des spots réels (non vérifié).
//
// Calme : si le vent EFFECTIF est sous 10 km/h, l'eau reste lisse quelle que soit la direction ; la classe est alors `Calme`.
// 10 km/h est un choix maison appuyé sur l'échelle de Beaufort (force 2, « brise légère », 6 à 11 km/h ; la force 3 commence à
// 12 km/h et fait des vaguelettes déferlantes) : au-dessous de la force 3 la surface n'est pas encore marquée. Le vent EFFECTIF
// est la moyenne du vent moyen et de la rafale ((v + max(v, rafale)) / 2) : 8 km/h en rafales à 30 n'est pas un vent calme.
// Sans rafale (`ABSENT`), l'effectif est le vent moyen.
#pragma once

#include <cstdint>

#include "ecran/surf_base.hpp"

namespace ecran::surf {

enum class ClasseVent : std::uint8_t {
    Inconnue, // vitesse ou direction absente
    Calme,    // vent effectif sous VENT_CALME_DIXIEMES_KMH : direction sans importance
    Offshore, // vent de terre
    Travers,
    Onshore, // vent de mer
};

// Seuils (voir l'en-tête pour le raisonnement).
inline constexpr std::int16_t VENT_CALME_DIXIEMES_KMH = 100; // 10,0 km/h
inline constexpr int OFFSHORE_COSINUS_PM_MAX = -500;         // cos ≤ -0,5
inline constexpr int ONSHORE_COSINUS_PM_MIN = 500;           // cos ≥ +0,5

struct VentSurf {
    ClasseVent classe = ClasseVent::Inconnue;
    std::int16_t composante_dixiemes_kmh = ABSENT; // v · cos(d − β) ; > 0 vers la plage
    std::int16_t effectif_dixiemes_kmh = ABSENT;
};

// Vent effectif en dixièmes de km/h (voir l'en-tête). ABSENT si la vitesse l'est.
constexpr std::int16_t vent_effectif_dixiemes(std::int16_t vitesse, std::int16_t rafale) noexcept {
    if (!present(vitesse))
        return ABSENT;
    const std::int64_t plein = (present(rafale) && rafale > vitesse) ? rafale : vitesse;
    return static_cast<std::int16_t>(division_arrondie(static_cast<std::int64_t>(vitesse) + plein, 2));
}

// c = v · cos(d − β) en dixièmes de km/h, arrondi au plus proche (la moitié s'éloigne de zéro). Les directions sont des angles :
// 359° et 1° sont à 2°, pas à 358°.
constexpr std::int16_t composante_vers_plage_dixiemes(std::int16_t vitesse, std::int16_t direction,
                                                      std::int16_t orientation) noexcept {
    if (!present(vitesse) || !present(direction))
        return ABSENT;
    const std::int64_t c =
        static_cast<std::int64_t>(vitesse) * cosinus_pm(static_cast<std::int64_t>(direction) - orientation);
    return static_cast<std::int16_t>(division_arrondie(c, 1000));
}

// Classe de la seule DIRECTION (sans tenir compte du calme).
constexpr ClasseVent classe_de_direction(std::int16_t direction, std::int16_t orientation) noexcept {
    const int c = cosinus_pm(static_cast<std::int64_t>(direction) - orientation);
    if (c <= OFFSHORE_COSINUS_PM_MAX)
        return ClasseVent::Offshore;
    if (c >= ONSHORE_COSINUS_PM_MIN)
        return ClasseVent::Onshore;
    return ClasseVent::Travers;
}

// Classe complète. `Inconnue` si la vitesse ou la direction manque (jamais « calme » par défaut : pas de donnée n'est pas pas de vent).
constexpr VentSurf classer_vent(std::int16_t vitesse, std::int16_t rafale, std::int16_t direction,
                                std::int16_t orientation) noexcept {
    VentSurf r{};
    if (!present(vitesse) || !present(direction))
        return r;
    r.effectif_dixiemes_kmh = vent_effectif_dixiemes(vitesse, rafale);
    r.composante_dixiemes_kmh = composante_vers_plage_dixiemes(vitesse, direction, orientation);
    r.classe = r.effectif_dixiemes_kmh < VENT_CALME_DIXIEMES_KMH ? ClasseVent::Calme
                                                                 : classe_de_direction(direction, orientation);
    return r;
}

} // namespace ecran::surf
