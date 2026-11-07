// Noyau pur « surf » : la note de session de 0 à 10, le classement des spots et le meilleur créneau de la journée. C++17 standard,
// aucun include ESP-IDF, entiers seulement. Aucune heure locale n'est formatée ici : les créneaux sont rendus en instants UNIX (UTC)
// et c'est l'écran qui les écrira.
//
// LE BARÈME EST UN BARÈME MAISON. Il n'est ni calibré sur des sessions réelles ni tiré d'un site de prévision : c'est une règle
// simple, lisible et testée, écrite pour classer des spots entre eux et dire « bon jour / mauvais jour » ; ce qu'il dit de la
// QUALITÉ RÉELLE d'un spot précis est NON VÉRIFIÉ (proposition de comparaison dans docs/mesures/surf.md). Voir ADR-0013.
//
// 100 points, trois parts :
//   - HAUTEUR  (40 points) : la houle (à défaut les vagues totales) comparée à la fourchette de hauteur du spot [min, max]. Dans la
//     fourchette : 40. Sous le minimum : rampe jusqu'à 0 à 40 % du minimum. Au-dessus du maximum : rampe jusqu'à 0 à 160 % du maximum.
//   - PÉRIODE  (25 points) : 0 à 5 s ou moins, 25 à 13 s ou plus, linéaire entre les deux. Une période longue = une houle de fond
//     qui porte de l'énergie ; une période courte = du clapot.
//   - VENT     (35 points) : selon la classe (surf_vent.hpp) et la force du vent EFFECTIF (moyenne du vent et des rafales). 35 points
//     jusqu'à 10 km/h, pour toutes les classes (eau lisse) ; puis la perte dépend de la classe : offshore ne perd rien jusqu'à 25
//     km/h puis tombe à 0 à 70 km/h ; travers tombe à 0 à 45 km/h ; onshore tombe à 0 à 30 km/h.
// Note = (points + 5) / 10, de 0 à 10 (95 points -> 10 ; 94 -> 9).
//
// LIMITES ÉCRITES :
//   - La DIRECTION DE LA HOULE par rapport à la plage n'est PAS modélisée. Une houle qui arrive de l'arrière de la plage, ou que
//     le cap, le récif ou l'île du large arrête, est notée comme une houle de face. Il faudrait la fenêtre de houle du spot, que
//     ni Open-Meteo ni la saisie de la personne ne donnent. La direction est lue (surf_donnees.hpp) mais ne sert pas.
//   - Le fond (sable, récif, galets), la marée qui fait marcher ou non un spot, la foule, la température de l'eau, la sécurité :
//     rien de tout cela n'entre dans la note.
//   - Les hauteurs sont celles du LARGE (maille de 5 à 25 km), pas celles de la vague au spot ; le vent est celui d'une maille de
//     la prévision, à 10 m.
//   - Une grandeur absente (`null`) rend la note INDISPONIBLE : jamais 0, jamais une valeur par défaut.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/surf_base.hpp"
#include "ecran/surf_donnees.hpp"
#include "ecran/surf_maree.hpp"
#include "ecran/surf_spot.hpp"
#include "ecran/surf_vent.hpp"

namespace ecran::surf {

// ---- barème (toutes les constantes du calcul, au même endroit) ----------------------------------------------------------

inline constexpr int POINTS_HAUTEUR = 40;
inline constexpr int POINTS_PERIODE = 25;
inline constexpr int POINTS_VENT = 35;
static_assert(POINTS_HAUTEUR + POINTS_PERIODE + POINTS_VENT == 100, "le barème fait 100 points");

// Hauteur : zéro point à 40 % du minimum du spot et à 160 % de son maximum (en millièmes).
inline constexpr std::int64_t HAUTEUR_SOUS_MIN_NUL_PM = 400;
inline constexpr std::int64_t HAUTEUR_SUR_MAX_NUL_PM = 1600;

// Période, en dixièmes de seconde : 0 point à 5,0 s ou moins, tous les points à 13,0 s ou plus.
inline constexpr std::int64_t PERIODE_NULLE_DS = 50;
inline constexpr std::int64_t PERIODE_PLEINE_DS = 130;

// Vent, en dixièmes de km/h : tous les points jusqu'à VENT_CALME_DIXIEMES_KMH (surf_vent.hpp, 10 km/h), puis, selon la classe,
// le palier de force qui garde les 35 points et la force à laquelle il n'en reste aucun.
inline constexpr std::int64_t VENT_OFFSHORE_PLEIN_JUSQU_A_DIXIEMES = 250; // 25 km/h
inline constexpr std::int64_t VENT_OFFSHORE_NUL_A_DIXIEMES = 700;         // 70 km/h
inline constexpr std::int64_t VENT_TRAVERS_NUL_A_DIXIEMES = 450;          // 45 km/h
inline constexpr std::int64_t VENT_ONSHORE_NUL_A_DIXIEMES = 300;          // 30 km/h

// Verdict en mots (le texte affiché viendra de la table de traduction) : seuils sur la note 0..10.
inline constexpr int NOTE_BON_MIN = 7;
inline constexpr int NOTE_CORRECT_MIN = 5;
inline constexpr int NOTE_MOYEN_MIN = 3;
inline constexpr int NOTE_EXCELLENT_MIN = 9;

// « Facteur limitant » : la part qui manque le plus de points, à condition qu'il en manque au moins ce nombre.
inline constexpr int LIMITANT_SEUIL_POINTS = 8;

enum class PositionHauteur : std::uint8_t { Inconnue, Sous, Dans, Sur }; // par rapport à la fourchette du spot
enum class Facteur : std::uint8_t { Aucun, Hauteur, Periode, Vent };
enum class Verdict : std::uint8_t { Indisponible, Mauvais, Moyen, Correct, Bon, Excellent };

// Ce qu'on sait d'une heure à un spot, valeurs absentes comprises.
struct Conditions {
    std::int64_t debut_s = 0;
    std::int16_t hauteur_cm = ABSENT; // houle si elle est présente, sinon vagues totales (voir `conditions_a`)
    std::int16_t periode_ds = ABSENT;
    std::int16_t vent_dixiemes_kmh = ABSENT;
    std::int16_t rafale_dixiemes_kmh = ABSENT;
    std::int16_t vent_direction_deg = ABSENT;
};

struct Note {
    bool disponible = false;
    std::uint8_t points = 0; // 0..100
    std::uint8_t note = 0;   // 0..10
    std::uint8_t points_hauteur = 0;
    std::uint8_t points_periode = 0;
    std::uint8_t points_vent = 0;
    PositionHauteur hauteur = PositionHauteur::Inconnue;
    VentSurf vent{};
    Facteur limitant = Facteur::Aucun;
};

// Note arrondie sur 10 à partir des points. 0..100 -> 0..10.
constexpr std::uint8_t note_de_points(int points) noexcept {
    return static_cast<std::uint8_t>(limiter((static_cast<std::int64_t>(points) + 5) / 10, 0, 10));
}

constexpr Verdict verdict_de(const Note &n) noexcept {
    if (!n.disponible)
        return Verdict::Indisponible;
    if (n.note >= NOTE_EXCELLENT_MIN)
        return Verdict::Excellent;
    if (n.note >= NOTE_BON_MIN)
        return Verdict::Bon;
    if (n.note >= NOTE_CORRECT_MIN)
        return Verdict::Correct;
    if (n.note >= NOTE_MOYEN_MIN)
        return Verdict::Moyen;
    return Verdict::Mauvais;
}

namespace detail {

constexpr int points_de_hauteur(std::int64_t hauteur_cm, const Spot &spot, PositionHauteur &position) noexcept {
    const std::int64_t bas = spot.hauteur_min_cm;
    const std::int64_t haut = spot.hauteur_max_cm;
    if (hauteur_cm < bas) {
        position = PositionHauteur::Sous;
        return static_cast<int>(
            rampe(hauteur_cm, division_arrondie(bas * HAUTEUR_SOUS_MIN_NUL_PM, 1000), 0, bas, POINTS_HAUTEUR));
    }
    if (hauteur_cm > haut) {
        position = PositionHauteur::Sur;
        return static_cast<int>(
            rampe(hauteur_cm, haut, POINTS_HAUTEUR, division_arrondie(haut * HAUTEUR_SUR_MAX_NUL_PM, 1000), 0));
    }
    position = PositionHauteur::Dans;
    return POINTS_HAUTEUR;
}

constexpr int points_de_periode(std::int64_t periode_ds) noexcept {
    return static_cast<int>(rampe(periode_ds, PERIODE_NULLE_DS, 0, PERIODE_PLEINE_DS, POINTS_PERIODE));
}

constexpr int points_de_vent(const VentSurf &v) noexcept {
    const std::int64_t f = v.effectif_dixiemes_kmh;
    switch (v.classe) {
    case ClasseVent::Offshore:
        return static_cast<int>(
            rampe(f, VENT_OFFSHORE_PLEIN_JUSQU_A_DIXIEMES, POINTS_VENT, VENT_OFFSHORE_NUL_A_DIXIEMES, 0));
    case ClasseVent::Travers:
        return static_cast<int>(rampe(f, VENT_CALME_DIXIEMES_KMH, POINTS_VENT, VENT_TRAVERS_NUL_A_DIXIEMES, 0));
    case ClasseVent::Onshore:
        return static_cast<int>(rampe(f, VENT_CALME_DIXIEMES_KMH, POINTS_VENT, VENT_ONSHORE_NUL_A_DIXIEMES, 0));
    case ClasseVent::Calme:
        return POINTS_VENT;
    case ClasseVent::Inconnue:
        break;
    }
    return 0;
}

} // namespace detail

// La note d'une heure à un spot. Indisponible (jamais 0) si la hauteur, la période, la vitesse ou la direction du vent manque ;
// une rafale absente n'empêche pas la note (le vent effectif est alors le vent moyen).
constexpr Note noter(const Spot &spot, const Conditions &c) noexcept {
    Note n{};
    if (!present(c.hauteur_cm) || !present(c.periode_ds) || !present(c.vent_dixiemes_kmh) ||
        !present(c.vent_direction_deg))
        return n;
    n.vent = classer_vent(c.vent_dixiemes_kmh, c.rafale_dixiemes_kmh, c.vent_direction_deg, spot.orientation_deg);
    const int ph = detail::points_de_hauteur(c.hauteur_cm, spot, n.hauteur);
    const int pp = detail::points_de_periode(c.periode_ds);
    const int pv = detail::points_de_vent(n.vent);
    n.points_hauteur = static_cast<std::uint8_t>(ph);
    n.points_periode = static_cast<std::uint8_t>(pp);
    n.points_vent = static_cast<std::uint8_t>(pv);
    n.points = static_cast<std::uint8_t>(ph + pp + pv);
    n.note = note_de_points(n.points);
    n.disponible = true;
    // La part qui manque le plus de points ; à égalité, le vent d'abord (c'est lui qu'on peut attendre qu'il tourne), puis la
    // hauteur, puis la période.
    const int manque_vent = POINTS_VENT - pv;
    const int manque_hauteur = POINTS_HAUTEUR - ph;
    const int manque_periode = POINTS_PERIODE - pp;
    int pire = manque_vent;
    n.limitant = Facteur::Vent;
    if (manque_hauteur > pire) {
        pire = manque_hauteur;
        n.limitant = Facteur::Hauteur;
    }
    if (manque_periode > pire) {
        pire = manque_periode;
        n.limitant = Facteur::Periode;
    }
    if (pire < LIMITANT_SEUIL_POINTS)
        n.limitant = Facteur::Aucun;
    return n;
}

// ---- assemblage des deux réponses ---------------------------------------------------------------------------------------

// Les conditions de l'heure n° `i` de la réponse MER, avec le vent de la MÊME étiquette dans la réponse VENT (jointure par
// l'instant, pas par le rang : les deux réponses peuvent ne pas commencer à la même heure). Houle d'abord : le clapot fait par le
// vent local n'est pas ce qui fait une vague de surf, et la houle porte la période ; sans houle (modèle qui ne la sépare pas) on
// retombe sur les vagues totales et leur période. Une houle de 0 cm est une mesure (mer sans houle), pas une absence.
constexpr Conditions conditions_a(const Marine &mer, const Vent &vent, std::size_t i) noexcept {
    Conditions c{};
    if (i >= mer.nb)
        return c;
    const MarineHeure &h = mer.heures[i];
    c.debut_s = h.debut_s;
    if (present(h.houle_cm)) {
        c.hauteur_cm = h.houle_cm;
        c.periode_ds = present(h.houle_periode_ds) ? h.houle_periode_ds : h.vagues_periode_ds;
    } else {
        c.hauteur_cm = h.vagues_cm;
        c.periode_ds = h.vagues_periode_ds;
    }
    for (std::size_t k = 0; k < vent.nb; ++k) {
        if (vent.heures[k].debut_s == h.debut_s) {
            c.vent_dixiemes_kmh = vent.heures[k].vitesse_dixiemes_kmh;
            c.rafale_dixiemes_kmh = vent.heures[k].rafale_dixiemes_kmh;
            c.vent_direction_deg = vent.heures[k].direction_deg;
            break;
        }
    }
    return c;
}

// Rang de l'heure de la réponse MER qui contient `instant_s` ([étiquette, étiquette + 1 h)), ou -1 si l'instant est hors de la série.
constexpr int heure_de(const Marine &mer, std::int64_t instant_s) noexcept {
    for (std::size_t i = 0; i < mer.nb; ++i) {
        if (instant_s >= mer.heures[i].debut_s && instant_s < mer.heures[i].debut_s + detail::PAS_S)
            return static_cast<int>(i);
    }
    return -1;
}

// Décalage à utiliser pour dire « 6 h à 21 h locales » : celui de la réponse VENT (elle vient d'un point terrestre, son fuseau
// est celui du lieu), sinon celui de la réponse MER. Au large Open-Meteo rend le fuseau `Etc/GMT` (décalage 0, constaté) : la mer
// seule peut donc se tromper de plusieurs heures.
constexpr std::int32_t decalage_a_retenir(const Marine &mer, const Vent &vent) noexcept {
    return vent.nb > 0 ? vent.decalage_utc_s : mer.decalage_utc_s;
}

// ---- meilleur créneau ---------------------------------------------------------------------------------------------------

// Fenêtre de jour : de 6 h à 21 h locales. Un créneau est fait d'heures entières ENTIÈREMENT dans cette fenêtre (la dernière
// commence à 20 h au plus tard). 2 heures au moins, 6 au plus.
inline constexpr int CRENEAU_DEBUT_H = 6;
inline constexpr int CRENEAU_FIN_H = 21;
inline constexpr int CRENEAU_HEURES_MIN = 2;
inline constexpr int CRENEAU_HEURES_MAX = 6;
// Une heure voisine s'ajoute au créneau si elle a au plus ce nombre de points de moins que la MOYENNE des deux meilleures heures
// de départ (10 points = une note).
inline constexpr int CRENEAU_TOLERANCE_POINTS = 10;

struct Creneau {
    bool trouve = false;
    std::int64_t debut_s = 0; // UTC, début de la première heure
    std::int64_t fin_s = 0;   // UTC, fin de la dernière heure
    std::uint8_t heures = 0;
    std::uint8_t points = 0; // moyenne des heures, 0..100
    std::uint8_t note = 0;   // 0..10
};

// Meilleur créneau d'un jour LOCAL : `jour_decale` 0 = le jour de `maintenant_s`, 1 = demain. Le jour local est calculé avec
// `decalage_utc_s` (fixé à la requête : il ne suit pas un changement d'heure survenu dans les 48 h, voir surf_donnees.hpp).
// Les heures déjà écoulées (qui se terminent avant `maintenant_s`) sont écartées. Le créneau est la fenêtre de 2 heures
// consécutives de plus forte moyenne (la plus tôt à égalité), élargie une heure à la fois, du côté de la meilleure voisine, tant
// que celle-ci reste dans la tolérance et que le créneau tient en CRENEAU_HEURES_MAX. Une heure sans note (valeur absente)
// coupe le créneau. `trouve` faux : aucun créneau de 2 heures ce jour-là (soir trop tardif, données manquantes).
constexpr Creneau meilleur_creneau(const Spot &spot, const Marine &mer, const Vent &vent, std::int64_t maintenant_s,
                                   std::int32_t decalage_utc_s, int jour_decale) noexcept {
    Creneau r{};
    std::int16_t pts[HEURES_MAX] = {};
    const std::int64_t jour_cible = division_plancher(maintenant_s + decalage_utc_s, 86400) + jour_decale;
    for (std::size_t i = 0; i < mer.nb; ++i) {
        pts[i] = -1;
        const std::int64_t t = mer.heures[i].debut_s;
        const std::int64_t local = t + decalage_utc_s;
        const std::int64_t jour = division_plancher(local, 86400);
        const std::int64_t seconde_du_jour = local - jour * 86400;
        const bool dans_la_fenetre =
            seconde_du_jour >= CRENEAU_DEBUT_H * 3600 && seconde_du_jour + 3600 <= CRENEAU_FIN_H * 3600;
        if (jour != jour_cible || !dans_la_fenetre || t + 3600 <= maintenant_s)
            continue;
        const Note n = noter(spot, conditions_a(mer, vent, i));
        if (n.disponible)
            pts[i] = n.points;
    }
    // 1. la meilleure fenêtre de 2 heures consécutives
    int meilleur = -1;
    int somme0 = -1;
    for (std::size_t i = 0; i + 1 < mer.nb; ++i) {
        if (pts[i] < 0 || pts[i + 1] < 0)
            continue;
        const int somme = pts[i] + pts[i + 1];
        if (somme > somme0) { // strict : à égalité on garde la plus tôt
            somme0 = somme;
            meilleur = static_cast<int>(i);
        }
    }
    if (meilleur < 0)
        return r;
    // 2. élargissement
    int bas = meilleur;
    int haut = meilleur + 1;
    int somme = somme0;
    while (haut - bas + 1 < CRENEAU_HEURES_MAX) {
        const bool gauche_ok =
            bas > 0 && pts[bas - 1] >= 0 && 2 * pts[bas - 1] + 2 * CRENEAU_TOLERANCE_POINTS >= somme0;
        const bool droite_ok = static_cast<std::size_t>(haut) + 1 < mer.nb && pts[haut + 1] >= 0 &&
                               2 * pts[haut + 1] + 2 * CRENEAU_TOLERANCE_POINTS >= somme0;
        if (!gauche_ok && !droite_ok)
            break;
        if (gauche_ok && (!droite_ok || pts[bas - 1] >= pts[haut + 1])) {
            --bas;
            somme += pts[bas];
        } else {
            ++haut;
            somme += pts[haut];
        }
    }
    const int heures = haut - bas + 1;
    r.trouve = true;
    r.debut_s = mer.heures[static_cast<std::size_t>(bas)].debut_s;
    r.fin_s = mer.heures[static_cast<std::size_t>(haut)].debut_s + 3600;
    r.heures = static_cast<std::uint8_t>(heures);
    r.points = static_cast<std::uint8_t>(division_arrondie(somme, heures));
    r.note = note_de_points(r.points);
    return r;
}

// ---- classement ---------------------------------------------------------------------------------------------------------

// Range les indices 0..n-1 par clé décroissante dans `ordre` (les `n` cases doivent exister). Une clé négative ou `ABSENT` = pas de
// donnée : ce spot passe APRÈS tous les autres, jamais devant sur un 0 inventé. Tri stable : à clés égales, l'ordre de saisie
// de la personne est gardé (deux spots à 70 points ne s'échangent pas d'un rafraîchissement à l'autre).
constexpr void classer(const std::int16_t *cles, std::size_t n, std::uint8_t *ordre) noexcept {
    for (std::size_t i = 0; i < n; ++i)
        ordre[i] = static_cast<std::uint8_t>(i);
    for (std::size_t i = 1; i < n; ++i) { // tri par insertion, stable
        const std::uint8_t courant = ordre[i];
        // Une clé négative ou ABSENT ne passe jamais devant : `autre` (normalisée à -1 au plus bas) est alors toujours >= `cle`.
        const std::int16_t cle = cles[courant];
        std::size_t k = i;
        while (k > 0) {
            const std::int16_t autre = cles[ordre[k - 1]] < 0 ? std::int16_t{-1} : cles[ordre[k - 1]];
            if (autre >= cle)
                break;
            ordre[k] = ordre[k - 1];
            --k;
        }
        ordre[k] = courant;
    }
}

// ---- tout ce que la liste « Mes spots » et le détail ont besoin de savoir d'un spot -------------------------------------

struct EvaluationSpot {
    bool donnees = false; // une réponse mer avec des vagues ET une réponse vent exploitables
    Note maintenant{};    // l'heure en cours
    Creneau creneau{};    // meilleur créneau du jour demandé
    Maree maree{};
    Tendance tendance = Tendance::Inconnue;
    std::int64_t prochaine_pleine_s = 0; // 0 = inconnue (marée non notable ou plus d'extremum dans la série)
    std::int64_t prochaine_basse_s = 0;
    // Clé du classement « Mes spots » : les points du meilleur créneau, à défaut ceux de l'heure en cours, à défaut -1 (pas de donnée).
    std::int16_t cle = -1;
};

constexpr EvaluationSpot evaluer_spot(const Spot &spot, const Marine &mer, const Vent &vent, std::int64_t maintenant_s,
                                      int jour_decale = 0) noexcept {
    EvaluationSpot e{};
    e.donnees = mer.a_vagues && vent.nb > 0;
    e.maree = extraire_maree(mer);
    e.tendance = tendance_a(e.maree, maintenant_s);
    if (const Extremum *p = prochain_extremum(e.maree, true, maintenant_s))
        e.prochaine_pleine_s = p->instant_s;
    if (const Extremum *b = prochain_extremum(e.maree, false, maintenant_s))
        e.prochaine_basse_s = b->instant_s;
    if (!e.donnees)
        return e;
    const int i = heure_de(mer, maintenant_s);
    if (i >= 0)
        e.maintenant = noter(spot, conditions_a(mer, vent, static_cast<std::size_t>(i)));
    e.creneau = meilleur_creneau(spot, mer, vent, maintenant_s, decalage_a_retenir(mer, vent), jour_decale);
    e.cle = e.creneau.trouve
                ? static_cast<std::int16_t>(e.creneau.points)
                : (e.maintenant.disponible ? static_cast<std::int16_t>(e.maintenant.points) : std::int16_t{-1});
    return e;
}

} // namespace ecran::surf
