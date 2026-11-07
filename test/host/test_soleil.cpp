// Tests du noyau pur : lever et coucher du soleil par le calcul (soleil.hpp, algorithme de la NOAA, ADR-0012). C'est le SEUL calcul
// du soleil de l'appareil : l'écran principal et les lecteurs de MET Norway et du NWS (ADR-0014) l'emploient, donc toutes les
// preuves sont ici. Sources de vérité, indépendantes du code C++ :
//   - donnees/attendu_soleil.inc, section PYTHON : le même algorithme réécrit en Python avec `math` (120 jours, vingt lieux) ;
//   - donnees/attendu_soleil.inc, section OPEN-METEO : 63 jours RÉELS capturés le 06/11/2026 (neuf réponses, huit lieux) ;
//   - donnees/attendu_riche_*.inc : le lever et le coucher qu'Open-Meteo a donnés pour sept lieux et sept jours (valeurs lues dans
//     les réponses réelles par controle_python_riche.py) : Lyon, Longyearbyen (nuit polaire), Seattle, Singapour, Sydney,
//     Katmandou (+5 h 45), Terre-Neuve (-3 h 30) ;
//   - le produit « Sunrise 3.0 » de MET Norway, pour 79 jours de 24 lieux, des cercles polaires à l'équateur, changements d'heure
//     et ligne de changement de date compris (donnees/soleil_metno_reference.json, capturer_soleil_metno.py) ;
//   - un second calcul écrit en Python avec un AUTRE algorithme, « Almanac for Computers » de l'US Naval Observatory
//     (donnees/attendu_soleil_almanac.inc, controle_python_fournisseurs.py) ;
//   - des constantes mathématiques connues pour les fonctions écrites à la main (sinus, arc tangente, racine : pas de <cmath> en
//     constexpr).
// Le jour polaire et la nuit polaire sont testés sur Longyearbyen, Tromsø et sur les pôles.
#include <cstddef>
#include <cstdint>
#include <limits>

#include "ecran/heure_lieu.hpp"
#include "ecran/soleil.hpp"
#include "mini.hpp"

using namespace ecran::soleil;

namespace {

struct Ligne {
    double latitude, longitude;
    int an, mois, jour, decalage;
    int cas;
    long long lever, coucher;
};
constexpr Ligne PYTHON[] = {
#define ATTENDU_SOLEIL_PYTHON
#include "donnees/attendu_soleil.inc"
#undef ATTENDU_SOLEIL_PYTHON
};
constexpr std::size_t NOMBRE_PYTHON = sizeof(PYTHON) / sizeof(PYTHON[0]);
constexpr Ligne OPEN_METEO[] = {
#define ATTENDU_SOLEIL_OPEN_METEO
#include "donnees/attendu_soleil.inc"
#undef ATTENDU_SOLEIL_OPEN_METEO
};
constexpr std::size_t NOMBRE_OPEN_METEO = sizeof(OPEN_METEO) / sizeof(OPEN_METEO[0]);

// Open-Meteo, sept lieux et sept jours chacun : le lever et le coucher réels de la réponse (donnees/attendu_riche_*.inc).
constexpr long long ATT_LYON[] = {
#include "donnees/attendu_riche_lyon.inc"
};
constexpr long long ATT_LONGYEARBYEN[] = {
#include "donnees/attendu_riche_longyearbyen.inc"
};
constexpr long long ATT_SEATTLE[] = {
#include "donnees/attendu_riche_seattle.inc"
};
constexpr long long ATT_SINGAPOUR[] = {
#include "donnees/attendu_riche_singapour.inc"
};
constexpr long long ATT_SYDNEY[] = {
#include "donnees/attendu_riche_sydney.inc"
};
constexpr long long ATT_KATMANDOU[] = {
#include "donnees/attendu_riche_katmandou.inc"
};
constexpr long long ATT_TERRENEUVE[] = {
#include "donnees/attendu_riche_terreneuve.inc"
};

// Les deux listes de jours qui suivent : une ligne par jour local {début du jour, lat_e5, lon_e5, situation (même numérotation que
// `Situation` : 0 normal, 1 jour polaire, 2 nuit polaire), lever, coucher}.
//   - Sunrise 3.0 de MET Norway : réponses réelles du 07/11/2026 (capturer_soleil_metno.py), à la minute ;
//   - l'algorithme « Almanac for Computers » de l'US Naval Observatory, écrit en Python (controle_python_fournisseurs.py).
constexpr long long ATT_MET[] = {
#include "donnees/attendu_soleil_metno.inc"
};
constexpr long long ATT_USNO[] = {
#include "donnees/attendu_soleil_almanac.inc"
};
constexpr std::size_t COLONNES = 6;

constexpr double absolu(double x) { return x < 0.0 ? -x : x; }
constexpr long long absolu(long long x) { return x < 0 ? -x : x; }
constexpr bool proche(double a, double b, double tolerance) { return absolu(a - b) <= tolerance; }

constexpr int les_lignes_de_reference_sont_la() {
    VERIFIER(NOMBRE_PYTHON == 120);
    VERIFIER(NOMBRE_OPEN_METEO == 63);
    VERIFIER(sizeof(ATT_MET) / sizeof(ATT_MET[0]) == 79 * COLONNES); // aucun jour réel de MET Norway perdu
    VERIFIER(sizeof(ATT_USNO) / sizeof(ATT_USNO[0]) == 79 * COLONNES);
    return 0;
}

// ---- 1. les fonctions écrites à la main -------------------------------------------------------------------------------------

constexpr int la_racine_carree() {
    VERIFIER(proche(detail::racine(2.0), 1.4142135623730951, 1e-14));
    VERIFIER(proche(detail::racine(0.25), 0.5, 1e-15));
    VERIFIER(proche(detail::racine(1e10), 1e5, 1e-8));
    VERIFIER(proche(detail::racine(1e-8), 1e-4, 1e-18));
    VERIFIER(detail::racine(0.0) == 0.0);
    VERIFIER(detail::racine(-4.0) == 0.0); // jamais de NaN
    return 0;
}

constexpr int le_sinus_et_le_cosinus() {
    constexpr double PI = detail::PI;
    VERIFIER(proche(detail::sinus(0.0), 0.0, 1e-15));
    VERIFIER(proche(detail::sinus(PI / 6.0), 0.5, 1e-13));
    VERIFIER(proche(detail::sinus(PI / 2.0), 1.0, 1e-13));
    VERIFIER(proche(detail::sinus(PI), 0.0, 1e-13));
    VERIFIER(proche(detail::sinus(-PI / 2.0), -1.0, 1e-13));
    VERIFIER(proche(detail::cosinus(0.0), 1.0, 1e-13));
    VERIFIER(proche(detail::cosinus(PI / 3.0), 0.5, 1e-13));
    VERIFIER(proche(detail::cosinus(PI), -1.0, 1e-13));
    // des angles loin de zéro sont ramenés dans [-pi, pi] (l'équation du temps passe des angles de plusieurs milliers de degrés)
    VERIFIER(proche(detail::sinus(100.0 * PI + PI / 6.0), 0.5, 1e-11));
    VERIFIER(proche(detail::sinus(-100.0 * PI + PI / 6.0), 0.5, 1e-11));
    VERIFIER(proche(detail::cosinus(5000.0), 0.15466840618074712, 1e-9)); // math.cos(5000.0) de Python
    VERIFIER(proche(detail::tangente(PI / 4.0), 1.0, 1e-12));
    return 0;
}

constexpr int les_arcs() {
    constexpr double PI = detail::PI;
    VERIFIER(proche(detail::arc_tangente(1.0), PI / 4.0, 1e-13));
    VERIFIER(proche(detail::arc_tangente(-1.0), -PI / 4.0, 1e-13));
    VERIFIER(proche(detail::arc_tangente(0.0), 0.0, 1e-15));
    VERIFIER(proche(detail::arc_tangente(1e6), PI / 2.0, 1e-5));
    VERIFIER(proche(detail::arc_tangente(2.0), 1.1071487177940904, 1e-13));
    VERIFIER(proche(detail::arc_tangente(0.3), 0.2914567944778671, 1e-13));
    VERIFIER(proche(detail::arc_sinus(0.5), PI / 6.0, 1e-12));
    VERIFIER(proche(detail::arc_sinus(-0.5), -PI / 6.0, 1e-12));
    VERIFIER(proche(detail::arc_sinus(1.0), PI / 2.0, 1e-15));
    VERIFIER(proche(detail::arc_sinus(5.0), PI / 2.0, 1e-15)); // hors domaine : bornée, jamais NaN
    VERIFIER(proche(detail::arc_sinus(-5.0), -PI / 2.0, 1e-15));
    VERIFIER(proche(detail::arc_cosinus(0.5), PI / 3.0, 1e-12));
    VERIFIER(proche(detail::arc_cosinus(-1.0), PI, 1e-12));
    VERIFIER(proche(detail::arc_cosinus(1.0), 0.0, 1e-12));
    VERIFIER(proche(detail::arc_cosinus(-3.0), PI, 1e-12)); // hors domaine : bornée aussi
    VERIFIER(proche(detail::arc_cosinus(3.0), 0.0, 1e-12));
    return 0;
}

// ---- 2. le calcul face à Python ---------------------------------------------------------------------------------------------

constexpr int le_calcul_est_celui_de_python_a_quelques_secondes_pres() {
    for (const Ligne &l : PYTHON) {
        const LeverCoucher r = lever_coucher(l.latitude, l.longitude, l.an, l.mois, l.jour, l.decalage);
        VERIFIER(static_cast<int>(r.situation) == l.cas);
        if (l.cas == 0) {
            VERIFIER(absolu(r.lever_s - l.lever) <= 3);
            VERIFIER(absolu(r.coucher_s - l.coucher) <= 3);
        } else {
            VERIFIER(r.lever_s == 0 && r.coucher_s == 0);
        }
    }
    return 0;
}

constexpr int le_lever_tombe_le_jour_demande_et_avant_le_coucher() {
    for (const Ligne &l : PYTHON) {
        if (l.cas != 0) {
            continue;
        }
        const LeverCoucher r = lever_coucher(l.latitude, l.longitude, l.an, l.mois, l.jour, l.decalage);
        VERIFIER(r.situation == Situation::Normal);
        VERIFIER(r.lever_s < r.coucher_s);
        // en heure locale (décalage du jour), le lever et le coucher sont bien le jour civil demandé
        const ecran::heure::Fuseau f = ecran::heure::fuseau_fixe(l.decalage);
        const ecran::heure::HeureLocale lever = ecran::heure::en_heure_locale(r.lever_s, f);
        const ecran::heure::HeureLocale coucher = ecran::heure::en_heure_locale(r.coucher_s, f);
        VERIFIER(lever.an == l.an && lever.mois == l.mois && lever.jour == l.jour);
        // Près du cercle polaire le coucher d'été passe minuit (Reykjavik : 00 h 04 le lendemain) : on ne le vérifie qu'en dessous de 55°.
        if (absolu(l.latitude) < 55.0) {
            VERIFIER(coucher.an == l.an && coucher.mois == l.mois && coucher.jour == l.jour);
        }
    }
    return 0;
}

// ---- 3. le calcul face à une source de production ---------------------------------------------------------------------------

constexpr int le_calcul_est_celui_d_open_meteo_a_deux_minutes_pres() {
    // Mesuré à la génération (controle_python_fuseaux.py) : l'écart le plus grand est de 8 s sur ces 63 jours. La tolérance est
    // volontairement plus large (deux minutes) : Open-Meteo peut changer de modèle, et ce n'est pas à ce test de le suivre à la seconde.
    int normaux = 0;
    int polaires = 0;
    for (const Ligne &l : OPEN_METEO) {
        const LeverCoucher r = lever_coucher(l.latitude, l.longitude, l.an, l.mois, l.jour, l.decalage);
        if (l.cas == 2) { // Open-Meteo écrit lever == coucher en nuit polaire
            VERIFIER(r.situation == Situation::NuitPolaire);
            ++polaires;
            continue;
        }
        VERIFIER(r.situation == Situation::Normal);
        VERIFIER(absolu(r.lever_s - l.lever) <= 120);
        VERIFIER(absolu(r.coucher_s - l.coucher) <= 120);
        ++normaux;
    }
    VERIFIER(normaux == 49 &&
             polaires == 14); // Longyearbyen : 7 jours de nuit polaire dans chacune de ses deux réponses
    return 0;
}

// ---- 3 bis. les autres références : Open-Meteo par lieu, MET Norway, un second calcul ---------------------------------------
// Ces preuves venaient de la branche des lecteurs de MET Norway et du NWS (ADR-0014), qui avait écrit son propre calcul du soleil
// avec la même méthode ; il a été supprimé et ses preuves sont ici, sur le calcul unique. Elles passent par `lever_coucher_e5`, la
// forme en entiers (position en e5, début du jour local en secondes) que lisent ces lecteurs.

constexpr std::int64_t TOLERANCE_S = 120;
constexpr std::int64_t abs64(std::int64_t v) { return v < 0 ? -v : v; }

// Ordre du fichier attendu d'Open-Meteo (test_meteo_riche.cpp, `aplatir`) : 13 valeurs de tête, puis 8 par jour (date, code, min,
// max, lever, coucher, uv, proba).
constexpr std::size_t DEBUT_JOURS = 13;
constexpr std::size_t PAR_JOUR = 8;
constexpr int NB_JOURS = 7;

// Plus grand écart (secondes) entre le calcul et les valeurs d'Open-Meteo pour un lieu ; -1 si un jour a un autre cas que
// celui d'Open-Meteo (nuit polaire d'un côté et pas de l'autre).
template <std::size_t N>
constexpr std::int64_t ecart_maximal_open_meteo(const long long (&att)[N], std::int32_t lat_e5, std::int32_t lon_e5) {
    static_assert(N >= DEBUT_JOURS + PAR_JOUR * NB_JOURS, "sept jours");
    std::int64_t pire = 0;
    for (int k = 0; k < NB_JOURS; ++k) {
        const std::size_t base = DEBUT_JOURS + PAR_JOUR * static_cast<std::size_t>(k);
        const std::int64_t date = att[base];
        const std::int64_t lever = att[base + 4];
        const std::int64_t coucher = att[base + 5];
        const LeverCoucher e = lever_coucher_e5(date, lat_e5, lon_e5);
        const bool nuit_om = lever == date && coucher == date;
        if (nuit_om != (e.situation == Situation::NuitPolaire))
            return -1;
        if (nuit_om)
            continue;
        if (e.situation != Situation::Normal)
            return -1;
        const std::int64_t d1 = abs64(e.lever_s - lever);
        const std::int64_t d2 = abs64(e.coucher_s - coucher);
        if (d1 > pire)
            pire = d1;
        if (d2 > pire)
            pire = d2;
    }
    return pire;
}

constexpr int contre_open_meteo_lyon() {
    const std::int64_t e = ecart_maximal_open_meteo(ATT_LYON, 4574906, 484789);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}
constexpr int contre_open_meteo_longyearbyen_nuit_polaire() {
    const std::int64_t e = ecart_maximal_open_meteo(ATT_LONGYEARBYEN, 7822000, 1562000);
    VERIFIER(e == 0); // les sept jours sont de la nuit polaire des deux côtés : aucun lever à comparer
    return 0;
}
constexpr int contre_open_meteo_seattle() {
    const std::int64_t e = ecart_maximal_open_meteo(ATT_SEATTLE, 4760621, -12233207);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}
constexpr int contre_open_meteo_singapour() {
    const std::int64_t e = ecart_maximal_open_meteo(ATT_SINGAPOUR, 129000, 10385000);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}
constexpr int contre_open_meteo_sydney() {
    const std::int64_t e = ecart_maximal_open_meteo(ATT_SYDNEY, -3386785, 15120732);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}
constexpr int contre_open_meteo_katmandou() {
    const std::int64_t e = ecart_maximal_open_meteo(ATT_KATMANDOU, 2771720, 8532400);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}
constexpr int contre_open_meteo_terreneuve() {
    const std::int64_t e = ecart_maximal_open_meteo(ATT_TERRENEUVE, 4756000, -5271000);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}

// Tolérance d'après les écarts constatés (docs/mesures/fournisseurs-meteo.md) : jusqu'à 60 ° de latitude, 2 minutes ; au-delà,
// où quelques centièmes de degré de hauteur du soleil font plusieurs minutes, 5 minutes. Ce ne sont pas des valeurs à relâcher pour
// faire passer un test : le calcul est à moins d'une minute de MET Norway sous 60 ° (une minute entière : la précision du service).
constexpr std::int64_t tolerance_pour(std::int64_t lat_e5) { return abs64(lat_e5) <= 6000000 ? TOLERANCE_S : 300; }

// `heures_larges` : le second algorithme est moins précis que celui du noyau (jusqu'à 3 minutes d'écart avec MET Norway aux
// latitudes tempérées, davantage près des pôles) : on ne compare ses heures que jusqu'à 60 ° de latitude et à 5 minutes.
template <std::size_t N>
constexpr int comparer_aux_jours(const long long (&att)[N], bool heures_larges, int &normaux, int &nuits, int &jours) {
    static_assert(N % COLONNES == 0, "colonnes");
    for (std::size_t i = 0; i < N / COLONNES; ++i) {
        const long long *c = &att[i * COLONNES];
        const LeverCoucher e = lever_coucher_e5(c[0], static_cast<std::int32_t>(c[1]), static_cast<std::int32_t>(c[2]));
        VERIFIER(static_cast<long long>(e.situation) == c[3]);
        if (e.situation == Situation::Normal) {
            ++normaux;
            VERIFIER(e.lever_s < e.coucher_s);
            const std::int64_t tol = heures_larges ? 300 : tolerance_pour(c[1]);
            if (!heures_larges || abs64(c[1]) <= 6000000) {
                VERIFIER(abs64(e.lever_s - c[4]) <= tol);
                VERIFIER(abs64(e.coucher_s - c[5]) <= tol);
            }
        } else if (e.situation == Situation::NuitPolaire) {
            ++nuits;
        } else {
            ++jours;
        }
    }
    return 0;
}

constexpr int contre_sunrise_de_met_norway() {
    int normaux = 0, nuits = 0, jours = 0;
    VERIFIER(comparer_aux_jours(ATT_MET, false, normaux, nuits, jours) == 0);
    VERIFIER(normaux >= 60 && nuits >= 3 && jours >= 6); // le fichier couvre bien les trois situations
    return 0;
}

constexpr int contre_le_second_calcul() {
    int normaux = 0, nuits = 0, jours = 0;
    VERIFIER(comparer_aux_jours(ATT_USNO, true, normaux, nuits, jours) == 0);
    VERIFIER(normaux >= 60 && nuits >= 3 && jours >= 6);
    return 0;
}

// ---- 3 ter. la forme en entiers et la hauteur du soleil -----------------------------------------------------------------------

constexpr int les_degres_viennent_des_e5() {
    VERIFIER(proche(detail::degres_depuis_e5(4885660), 48.8566, 1e-12));
    VERIFIER(proche(detail::degres_depuis_e5(-7850000), -78.5, 1e-12));
    VERIFIER(proche(detail::degres_depuis_e5(0), 0.0, 1e-15));
    VERIFIER(proche(detail::degres_depuis_e5(18000000), 180.0, 1e-12));
    return 0;
}

// La forme en entiers n'est pas un second calcul : sur les 120 lignes de Python (coordonnées à quatre décimales, donc exactes en
// e5), elle rend EXACTEMENT les mêmes secondes que la forme en degrés avec la date civile et le décalage.
constexpr int la_forme_en_entiers_est_celle_en_degres() {
    int comparees = 0;
    for (const Ligne &l : PYTHON) {
        const std::int64_t debut = ecran::heure::detail::jours_depuis_epoch(l.an, l.mois, l.jour) * 86400 - l.decalage;
        const std::int32_t lat_e5 = static_cast<std::int32_t>(l.latitude * 100000.0 + (l.latitude >= 0.0 ? 0.5 : -0.5));
        const std::int32_t lon_e5 =
            static_cast<std::int32_t>(l.longitude * 100000.0 + (l.longitude >= 0.0 ? 0.5 : -0.5));
        const LeverCoucher a = lever_coucher(l.latitude, l.longitude, l.an, l.mois, l.jour, l.decalage);
        const LeverCoucher b = lever_coucher_e5(debut, lat_e5, lon_e5);
        VERIFIER(a.situation == b.situation);
        VERIFIER(a.lever_s == b.lever_s && a.coucher_s == b.coucher_s);
        ++comparees;
    }
    VERIFIER(comparees == 120);
    return 0;
}

constexpr int une_position_en_e5_hors_du_globe_ne_donne_aucune_heure() {
    constexpr std::int64_t debut = 1782000000; // 2026-06-20
    VERIFIER(lever_coucher_e5(debut, 9000001, 0).situation != Situation::Normal);
    VERIFIER(lever_coucher_e5(debut, -9000001, 0).situation != Situation::Normal);
    VERIFIER(lever_coucher_e5(debut, 0, 18000001).situation != Situation::Normal);
    VERIFIER(lever_coucher_e5(debut, 0, -18000001).situation != Situation::Normal);
    VERIFIER(lever_coucher_e5(debut, 4885660, 235220).situation == Situation::Normal); // Paris
    VERIFIER(lever_coucher_e5(debut, 9000000, 0).situation ==
             Situation::JourPolaire); // le pôle Nord lui-même est un lieu valable
    return 0;
}

// Cas limites écrits à la main : le soleil à l'horizon et sa hauteur.
constexpr int hauteur_du_soleil() {
    // Midi solaire au 21 juin à Quito (équateur, 78,5 °O) : le soleil est presque au zénith (déclinaison 23,4 °, latitude -0,2 °).
    // 2026-06-21 17:00 UTC = 12:00 local (UTC-5) ; le midi solaire est vers 17:14 UTC.
    constexpr std::int64_t juin = 1782061200; // 2026-06-21 17:00:00 UTC
    VERIFIER(hauteur_deg(juin, -0.2, -78.5) > 60.0);
    VERIFIER(hauteur_deg(juin, -0.2, -78.5) < 70.0);
    // Même instant à l'antipode (lat +0,2, lon +101,5) : nuit profonde, soleil à plus de 60 ° sous l'horizon.
    VERIFIER(hauteur_deg(juin, 0.2, 101.5) < -60.0);
    VERIFIER(!est_leve_e5(juin, 20000, 10150000));
    VERIFIER(est_leve_e5(juin, -20000, -7850000));
    return 0;
}

// `est_leve_e5` et le lever/coucher sont le même seuil (-0,833 °) : à cinq minutes de part et d'autre de chaque événement des jours
// de MET Norway, le soleil est couché avant le lever et levé après, levé avant le coucher et couché après. (Sous 60 ° de latitude :
// plus haut, la hauteur change trop lentement pour que cinq minutes la séparent du seuil.)
constexpr int le_jour_et_la_nuit_suivent_le_lever_et_le_coucher() {
    int controles = 0;
    for (std::size_t i = 0; i < sizeof(ATT_MET) / sizeof(ATT_MET[0]) / COLONNES; ++i) {
        const long long *c = &ATT_MET[i * COLONNES];
        if (c[3] != 0 || abs64(c[1]) > 6000000)
            continue;
        const std::int32_t lat = static_cast<std::int32_t>(c[1]);
        const std::int32_t lon = static_cast<std::int32_t>(c[2]);
        const LeverCoucher e = lever_coucher_e5(c[0], lat, lon);
        VERIFIER(e.situation == Situation::Normal);
        VERIFIER(!est_leve_e5(e.lever_s - 300, lat, lon));
        VERIFIER(est_leve_e5(e.lever_s + 300, lat, lon));
        VERIFIER(est_leve_e5(e.coucher_s - 300, lat, lon));
        VERIFIER(!est_leve_e5(e.coucher_s + 300, lat, lon));
        ++controles;
    }
    VERIFIER(controles >= 50);
    return 0;
}

// Le lever et le coucher sont, par construction, les instants où la hauteur du soleil vaut -0,833 ° : `hauteur_deg` y rend ce seuil
// à moins d'un centième de degré, aux 69 jours « normaux » de MET Norway, de l'équateur aux cercles polaires. Mesuré : l'écart le
// plus grand est de 0,0017 ° (l'arrondi à la seconde du lever). Ce test lie les deux chemins du fichier, qui lisent la même position
// du soleil : un décalage du jour julien, de la longitude ou de l'angle horaire dans l'un des deux ne passe pas.
constexpr int la_hauteur_au_lever_et_au_coucher_est_le_seuil() {
    int controles = 0;
    for (std::size_t i = 0; i < sizeof(ATT_MET) / sizeof(ATT_MET[0]) / COLONNES; ++i) {
        const long long *c = &ATT_MET[i * COLONNES];
        if (c[3] != 0)
            continue;
        const std::int32_t lat = static_cast<std::int32_t>(c[1]);
        const std::int32_t lon = static_cast<std::int32_t>(c[2]);
        const LeverCoucher e = lever_coucher_e5(c[0], lat, lon);
        VERIFIER(e.situation == Situation::Normal);
        const double lat_deg = detail::degres_depuis_e5(lat);
        const double lon_deg = detail::degres_depuis_e5(lon);
        VERIFIER(proche(hauteur_deg(e.lever_s, lat_deg, lon_deg), 90.0 - detail::ZENITH_LEVER_DEG, 0.01));
        VERIFIER(proche(hauteur_deg(e.coucher_s, lat_deg, lon_deg), 90.0 - detail::ZENITH_LEVER_DEG, 0.01));
        ++controles;
    }
    VERIFIER(controles == 69);
    return 0;
}

// Positions extrêmes : aucun résultat absurde (ni lever après coucher, ni jour de plus de 24 h).
constexpr int pas_de_valeur_absurde() {
    constexpr std::int32_t LATS[] = {-9000000, -8000000, -6657000, -4000000, 0, 3000000, 6657000, 7822000, 9000000};
    constexpr std::int32_t LONS[] = {-18000000, -9000000, 0, 9000000, 18000000};
    constexpr std::int64_t JOURS[] = {1782000000, 1790000000, 1797000000, 1805000000}; // été et hiver
    for (const std::int32_t lat : LATS) {
        for (const std::int32_t lon : LONS) {
            for (const std::int64_t j : JOURS) {
                const LeverCoucher e = lever_coucher_e5(j, lat, lon);
                if (e.situation != Situation::Normal)
                    continue;
                VERIFIER(e.lever_s < e.coucher_s);
                VERIFIER(e.coucher_s - e.lever_s <= 86400);
                VERIFIER(e.lever_s > j - 86400 && e.coucher_s < j + 2 * 86400);
            }
        }
    }
    // Les pôles : jamais de division par zéro ; au pôle Nord en juin, le soleil ne se couche pas.
    VERIFIER(lever_coucher_e5(1782000000, 9000000, 0).situation == Situation::JourPolaire);
    VERIFIER(lever_coucher_e5(1797000000, 9000000, 0).situation == Situation::NuitPolaire);
    VERIFIER(lever_coucher_e5(1782000000, -9000000, 0).situation == Situation::NuitPolaire);
    return 0;
}

// ---- 4. jour polaire, nuit polaire, limites ---------------------------------------------------------------------------------

constexpr int jour_polaire_et_nuit_polaire() {
    // Longyearbyen (78,2 N), Tromsø (69,6 N), cap Nord… : le soleil ne se couche pas en juin, ne se lève pas en décembre.
    VERIFIER(lever_coucher(78.2232, 15.6267, 2026, 6, 21, 7200).situation == Situation::JourPolaire);
    VERIFIER(lever_coucher(78.2232, 15.6267, 2026, 12, 21, 3600).situation == Situation::NuitPolaire);
    VERIFIER(lever_coucher(69.6492, 18.9553, 2026, 6, 21, 7200).situation == Situation::JourPolaire);
    VERIFIER(lever_coucher(69.6492, 18.9553, 2026, 12, 21, 3600).situation == Situation::NuitPolaire);
    // Le même lieu, hors saison polaire : un lever et un coucher.
    VERIFIER(lever_coucher(69.6492, 18.9553, 2026, 3, 20, 3600).situation == Situation::Normal);
    VERIFIER(lever_coucher(69.6492, 18.9553, 2026, 9, 23, 7200).situation == Situation::Normal);
    // Les pôles : été = jour, hiver = nuit (le dénominateur de l'angle horaire est nul).
    VERIFIER(lever_coucher(90.0, 0.0, 2026, 6, 21, 0).situation == Situation::JourPolaire);
    VERIFIER(lever_coucher(90.0, 0.0, 2026, 12, 21, 0).situation == Situation::NuitPolaire);
    VERIFIER(lever_coucher(-90.0, 0.0, 2026, 12, 21, 0).situation == Situation::JourPolaire);
    VERIFIER(lever_coucher(-90.0, 0.0, 2026, 6, 21, 0).situation == Situation::NuitPolaire);
    // Sans heure : aucune valeur à utiliser.
    const LeverCoucher r = lever_coucher(78.2232, 15.6267, 2026, 12, 21, 3600);
    VERIFIER(r.lever_s == 0 && r.coucher_s == 0);
    return 0;
}

constexpr int des_coordonnees_invraisemblables_ne_donnent_aucune_heure() {
    VERIFIER(lever_coucher(91.0, 0.0, 2026, 6, 21, 0).situation != Situation::Normal);
    VERIFIER(lever_coucher(-91.0, 0.0, 2026, 6, 21, 0).situation != Situation::Normal);
    VERIFIER(lever_coucher(0.0, 181.0, 2026, 6, 21, 0).situation != Situation::Normal);
    VERIFIER(lever_coucher(0.0, -181.0, 2026, 6, 21, 0).situation != Situation::Normal);
    constexpr double non_nombre = std::numeric_limits<double>::quiet_NaN();
    VERIFIER(lever_coucher(non_nombre, 0.0, 2026, 6, 21, 0).situation != Situation::Normal);
    VERIFIER(lever_coucher(0.0, non_nombre, 2026, 6, 21, 0).situation != Situation::Normal);
    return 0;
}

constexpr int l_equateur_a_un_jour_de_douze_heures() {
    // Quito (0,18 S), à l'équinoxe : 12 h à quelques minutes près (réfraction et rayon du soleil rallongent le jour de ~7 minutes).
    const LeverCoucher r = lever_coucher(-0.1807, -78.4678, 2026, 3, 20, -18000);
    VERIFIER(r.situation == Situation::Normal);
    const long long duree = r.coucher_s - r.lever_s;
    VERIFIER(duree > 12 * 3600 && duree < 12 * 3600 + 15 * 60);
    return 0;
}

constexpr int les_jours_s_allongent_vers_l_ete_et_se_raccourcissent_vers_l_hiver() {
    // Paris : le jour le plus long (21 juin) fait plus de 16 h, le plus court (21 décembre) moins de 8 h 30.
    const LeverCoucher ete = lever_coucher(48.8566, 2.3522, 2026, 6, 21, 7200);
    const LeverCoucher hiver = lever_coucher(48.8566, 2.3522, 2026, 12, 21, 3600);
    VERIFIER(ete.coucher_s - ete.lever_s > 16 * 3600);
    VERIFIER(hiver.coucher_s - hiver.lever_s < 8 * 3600 + 1800);
    // L'hémisphère sud est inversé : Sydney a le jour le plus long en décembre.
    const LeverCoucher sydney_ete = lever_coucher(-33.8688, 151.2093, 2026, 12, 21, 39600);
    const LeverCoucher sydney_hiver = lever_coucher(-33.8688, 151.2093, 2026, 6, 21, 36000);
    VERIFIER(sydney_ete.coucher_s - sydney_ete.lever_s > sydney_hiver.coucher_s - sydney_hiver.lever_s + 3 * 3600);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(les_lignes_de_reference_sont_la)                                                                                 \
    X(la_racine_carree)                                                                                                \
    X(le_sinus_et_le_cosinus)                                                                                          \
    X(les_arcs)                                                                                                        \
    X(le_calcul_est_celui_de_python_a_quelques_secondes_pres)                                                          \
    X(le_lever_tombe_le_jour_demande_et_avant_le_coucher)                                                              \
    X(le_calcul_est_celui_d_open_meteo_a_deux_minutes_pres)                                                            \
    X(contre_open_meteo_lyon)                                                                                          \
    X(contre_open_meteo_longyearbyen_nuit_polaire)                                                                     \
    X(contre_open_meteo_seattle)                                                                                       \
    X(contre_open_meteo_singapour)                                                                                     \
    X(contre_open_meteo_sydney)                                                                                        \
    X(contre_open_meteo_katmandou)                                                                                     \
    X(contre_open_meteo_terreneuve)                                                                                    \
    X(contre_sunrise_de_met_norway)                                                                                    \
    X(contre_le_second_calcul)                                                                                         \
    X(les_degres_viennent_des_e5)                                                                                      \
    X(la_forme_en_entiers_est_celle_en_degres)                                                                         \
    X(une_position_en_e5_hors_du_globe_ne_donne_aucune_heure)                                                          \
    X(hauteur_du_soleil)                                                                                               \
    X(le_jour_et_la_nuit_suivent_le_lever_et_le_coucher)                                                               \
    X(la_hauteur_au_lever_et_au_coucher_est_le_seuil)                                                                  \
    X(pas_de_valeur_absurde)                                                                                           \
    X(jour_polaire_et_nuit_polaire)                                                                                    \
    X(des_coordonnees_invraisemblables_ne_donnent_aucune_heure)                                                        \
    X(l_equateur_a_un_jour_de_douze_heures)                                                                            \
    X(les_jours_s_allongent_vers_l_ete_et_se_raccourcissent_vers_l_hiver)

} // namespace

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
