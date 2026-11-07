// Tests du noyau pur : lever et coucher du soleil calculés sur l'appareil (soleil_calcule.hpp).
//
// Trois références qui ne viennent pas de ce code :
//   - le lever et le coucher qu'Open-Meteo a donnés pour sept lieux et sept jours (donnees/attendu_riche_*.inc, valeurs lues
//     dans les réponses réelles par controle_python_riche.py) : Lyon, Longyearbyen (nuit polaire), Seattle, Singapour, Sydney,
//     Katmandou (+5 h 45), Terre-Neuve (-3 h 30) ;
//   - le produit « Sunrise 3.0 » de MET Norway, pour 79 jours de 24 lieux, des cercles polaires à l'équateur, changements
//     d'heure et ligne de changement de date compris (donnees/soleil_metno_reference.json, capturer_soleil_metno.py) ;
//   - un second calcul écrit en Python avec un autre algorithme (donnees/attendu_soleil_almanac.inc, controle_python_fournisseurs.py).
// Tolérance : deux minutes jusqu'à 60 ° de latitude, cinq minutes au-delà (les sources ne s'accordent pas mieux entre elles :
// MET Norway donne des minutes entières, la réfraction et la position de la grille du modèle diffèrent d'un service à l'autre).
#include <cstddef>
#include <cstdint>

#include "ecran/soleil_calcule.hpp"
#include "mini.hpp"

using namespace ecran::astro;

namespace {

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

// Ordre du fichier attendu (test_meteo_riche.cpp, `aplatir`) : 13 valeurs de tête, puis 8 par jour (date, code, min, max, lever,
// coucher, uv, proba).
constexpr std::size_t DEBUT_JOURS = 13;
constexpr std::size_t PAR_JOUR = 8;
constexpr int NB_JOURS = 7;

constexpr std::int64_t TOLERANCE_S = 120;

constexpr std::int64_t abs64(std::int64_t v) { return v < 0 ? -v : v; }

// Plus grand écart (secondes) entre le calcul et les valeurs d'Open-Meteo pour un lieu ; -1 si un jour a un autre cas que
// celui d'Open-Meteo (nuit polaire d'un côté et pas de l'autre).
template <std::size_t N>
constexpr std::int64_t ecart_maximal(const long long (&att)[N], std::int32_t lat_e5, std::int32_t lon_e5) {
    std::int64_t pire = 0;
    for (int k = 0; k < NB_JOURS; ++k) {
        const std::size_t base = DEBUT_JOURS + PAR_JOUR * static_cast<std::size_t>(k);
        const std::int64_t date = att[base];
        const std::int64_t lever = att[base + 4];
        const std::int64_t coucher = att[base + 5];
        const EvenementsSoleil e = evenements_du_jour(date, lat_e5, lon_e5);
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
    const std::int64_t e = ecart_maximal(ATT_LYON, 4574906, 484789);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}
constexpr int contre_open_meteo_longyearbyen_nuit_polaire() {
    const std::int64_t e = ecart_maximal(ATT_LONGYEARBYEN, 7822000, 1562000);
    VERIFIER(e == 0); // les sept jours sont de la nuit polaire des deux côtés : aucun lever à comparer
    return 0;
}
constexpr int contre_open_meteo_seattle() {
    const std::int64_t e = ecart_maximal(ATT_SEATTLE, 4760621, -12233207);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}
constexpr int contre_open_meteo_singapour() {
    const std::int64_t e = ecart_maximal(ATT_SINGAPOUR, 129000, 10385000);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}
constexpr int contre_open_meteo_sydney() {
    const std::int64_t e = ecart_maximal(ATT_SYDNEY, -3386785, 15120732);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}
constexpr int contre_open_meteo_katmandou() {
    const std::int64_t e = ecart_maximal(ATT_KATMANDOU, 2771720, 8532400);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}
constexpr int contre_open_meteo_terreneuve() {
    const std::int64_t e = ecart_maximal(ATT_TERRENEUVE, 4756000, -5271000);
    VERIFIER(e >= 0 && e <= TOLERANCE_S);
    return 0;
}

// Les deux listes de jours qui suivent : une ligne par jour local {début du jour, lat_e5, lon_e5, situation (0 normal, 1 nuit
// polaire, 2 jour polaire), lever, coucher}.
//   - Sunrise 3.0 de MET Norway : réponses réelles du 07/11/2026 (capturer_soleil_metno.py), à la minute ;
//   - l'algorithme « Almanac for Computers » de l'US Naval Observatory, écrit en Python (controle_python_fournisseurs.py).
constexpr long long ATT_MET[] = {
#include "donnees/attendu_soleil_metno.inc"
};
constexpr long long ATT_USNO[] = {
#include "donnees/attendu_soleil_almanac.inc"
};
constexpr std::size_t COLONNES = 6;

// Tolérance d'après les écarts constatés (docs/mesures/fournisseurs-meteo.md) : jusqu'à 60 ° de latitude, 2 minutes ; au-delà,
// où quelques centièmes de degré de hauteur du soleil font plusieurs minutes, 5 minutes.
constexpr std::int64_t tolerance_pour(std::int64_t lat_e5) { return abs64(lat_e5) <= 6000000 ? TOLERANCE_S : 300; }

// `tolerance_usno` : le second algorithme est moins précis que celui du noyau (jusqu'à 3 minutes d'écart avec MET Norway aux
// latitudes tempérées, davantage près des pôles) : on ne compare ses heures que jusqu'à 60 ° de latitude et à 5 minutes.
template <std::size_t N>
constexpr int comparer_aux_jours(const long long (&att)[N], bool heures_larges, int &normaux, int &nuits, int &jours) {
    static_assert(N % COLONNES == 0, "colonnes");
    for (std::size_t i = 0; i < N / COLONNES; ++i) {
        const long long *c = &att[i * COLONNES];
        const EvenementsSoleil e =
            evenements_du_jour(c[0], static_cast<std::int32_t>(c[1]), static_cast<std::int32_t>(c[2]));
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

// Cas limites écrits à la main : le soleil à l'horizon et sa hauteur.
constexpr int hauteur_du_soleil() {
    // Midi solaire au 21 juin à Quito (équateur, 78,5 °O) : le soleil est presque au zénith (déclinaison 23,4 °, latitude -0,2 °).
    // 2026-06-21 17:00 UTC = 12:00 local (UTC-5) ; le midi solaire est vers 17:14 UTC.
    constexpr std::int64_t juin = 1782061200; // 2026-06-21 17:00:00 UTC
    VERIFIER(hauteur_soleil_deg(juin, -20000, -7850000) > 60.0);
    VERIFIER(hauteur_soleil_deg(juin, -20000, -7850000) < 70.0);
    // Même instant à l'antipode (lat +0,2, lon +101,5) : nuit profonde, soleil à plus de 60 ° sous l'horizon.
    VERIFIER(hauteur_soleil_deg(juin, 20000, 10150000) < -60.0);
    VERIFIER(!soleil_leve(juin, 20000, 10150000));
    VERIFIER(soleil_leve(juin, -20000, -7850000));
    return 0;
}

constexpr int fonctions_trigonometriques() {
    // Quelques valeurs exactes, à 1e-9 près.
    const auto proche = [](double a, double b) { return (a > b ? a - b : b - a) < 1e-9; };
    VERIFIER(proche(detail::sinus(0.0), 0.0));
    VERIFIER(proche(detail::sinus(PI / 2.0), 1.0));
    VERIFIER(proche(detail::sinus(PI / 6.0), 0.5));
    VERIFIER(proche(detail::sinus(-PI / 6.0), -0.5));
    VERIFIER(proche(detail::cosinus(0.0), 1.0));
    VERIFIER(proche(detail::cosinus(PI / 3.0), 0.5));
    VERIFIER(proche(detail::cosinus(PI), -1.0));
    VERIFIER(proche(detail::sinus(100.0 * PI + PI / 6.0), 0.5)); // grand argument : la réduction d'angle
    VERIFIER(proche(detail::sinus(-100.0 * PI + PI / 6.0), 0.5));
    VERIFIER(proche(detail::arc_sinus(0.5), PI / 6.0));
    VERIFIER(proche(detail::arc_sinus(-1.0), -PI / 2.0));
    VERIFIER(proche(detail::arc_sinus(2.0), PI / 2.0)); // hors de [-1, 1] : bornée, pas de valeur absurde
    VERIFIER(proche(detail::arc_cosinus(0.5), PI / 3.0));
    VERIFIER(proche(detail::arc_cosinus(1.0), 0.0));
    VERIFIER(proche(detail::arc_cosinus(-3.0), PI));
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
                const EvenementsSoleil e = evenements_du_jour(j, lat, lon);
                if (e.situation != Situation::Normal)
                    continue;
                VERIFIER(e.lever_s < e.coucher_s);
                VERIFIER(e.coucher_s - e.lever_s <= 86400);
                VERIFIER(e.lever_s > j - 86400 && e.coucher_s < j + 2 * 86400);
            }
        }
    }
    // Les pôles : jamais de division par zéro ; au pôle Nord en juin, le soleil ne se couche pas.
    VERIFIER(evenements_du_jour(1782000000, 9000000, 0).situation == Situation::JourPolaire);
    VERIFIER(evenements_du_jour(1797000000, 9000000, 0).situation == Situation::NuitPolaire);
    VERIFIER(evenements_du_jour(1782000000, -9000000, 0).situation == Situation::NuitPolaire);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(contre_open_meteo_lyon)                                                                                          \
    X(contre_open_meteo_longyearbyen_nuit_polaire)                                                                     \
    X(contre_open_meteo_seattle)                                                                                       \
    X(contre_open_meteo_singapour)                                                                                     \
    X(contre_open_meteo_sydney)                                                                                        \
    X(contre_open_meteo_katmandou)                                                                                     \
    X(contre_open_meteo_terreneuve)                                                                                    \
    X(contre_sunrise_de_met_norway)                                                                                    \
    X(contre_le_second_calcul)                                                                                         \
    X(hauteur_du_soleil)                                                                                               \
    X(fonctions_trigonometriques)                                                                                      \
    X(pas_de_valeur_absurde)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
