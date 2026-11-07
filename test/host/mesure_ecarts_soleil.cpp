// Mesure (ce n'est pas un test) : de combien le lever et le coucher du soleil calculés sur l'appareil (soleil.hpp, le seul calcul
// du soleil, ADR-0012) s'écartent de cinq références qui ne viennent pas de ce code. Les chiffres de docs/mesures/fuseaux.md et de
// docs/mesures/fournisseurs-meteo.md (§ Soleil) sont ceux qu'elle affiche.
//   - « Sunrise 3.0 » de MET Norway : 79 jours de 24 lieux (donnees/attendu_soleil_metno.inc) ;
//   - un second calcul écrit en Python, algorithme « Almanac for Computers » (donnees/attendu_soleil_almanac.inc) ;
//   - le lever et le coucher donnés par Open-Meteo, 7 jours pour 7 lieux (donnees/attendu_riche_*.inc) ;
//   - le même algorithme NOAA réécrit en Python (120 jours, donnees/attendu_soleil.inc, section PYTHON) ;
//   - 63 jours réels d'Open-Meteo (donnees/attendu_soleil.inc, section OPEN-METEO).
// Compiler et lancer depuis la racine du dépôt, avec le compilateur de tools\env_hote.ps1 :
//   clang++ -std=c++17 -O1 -I components/core/include -I test/host test/host/mesure_ecarts_soleil.cpp -o mesure_ecarts_soleil.exe
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "ecran/soleil.hpp"

using namespace ecran::soleil;

namespace {

struct Ligne {
    double latitude, longitude;
    int an, mois, jour, decalage;
    int cas;
    long long lever, coucher;
};
const Ligne PYTHON[] = {
#define ATTENDU_SOLEIL_PYTHON
#include "donnees/attendu_soleil.inc"
#undef ATTENDU_SOLEIL_PYTHON
};
const Ligne OPEN_METEO_63[] = {
#define ATTENDU_SOLEIL_OPEN_METEO
#include "donnees/attendu_soleil.inc"
#undef ATTENDU_SOLEIL_OPEN_METEO
};
const long long ATT_MET[] = {
#include "donnees/attendu_soleil_metno.inc"
};
const long long ATT_USNO[] = {
#include "donnees/attendu_soleil_almanac.inc"
};
const long long ATT_LYON[] = {
#include "donnees/attendu_riche_lyon.inc"
};
const long long ATT_SEATTLE[] = {
#include "donnees/attendu_riche_seattle.inc"
};
const long long ATT_SINGAPOUR[] = {
#include "donnees/attendu_riche_singapour.inc"
};
const long long ATT_SYDNEY[] = {
#include "donnees/attendu_riche_sydney.inc"
};
const long long ATT_KATMANDOU[] = {
#include "donnees/attendu_riche_katmandou.inc"
};
const long long ATT_TERRENEUVE[] = {
#include "donnees/attendu_riche_terreneuve.inc"
};
const long long ATT_LONGYEARBYEN[] = {
#include "donnees/attendu_riche_longyearbyen.inc"
};

long long absolu(long long v) { return v < 0 ? -v : v; }

constexpr int TRANCHES = 4;
const char *const NOMS_TRANCHES[TRANCHES] = {"|lat| <= 45", "45 - 60", "60 - 66,5", "> 66,5"};

int tranche(long long lat_e5) {
    const long long lat = absolu(lat_e5);
    return lat <= 4500000 ? 0 : (lat <= 6000000 ? 1 : (lat <= 6650000 ? 2 : 3));
}

// Cumul des écarts par tranche de latitude.
struct Cumul {
    long long pire[TRANCHES] = {};
    double somme[TRANCHES] = {};
    int nb[TRANCHES] = {};
    int nuits = 0, jours = 0, situations_differentes = 0, normaux = 0;
    void ajouter(long long lat_e5, long long d1, long long d2) {
        const int t = tranche(lat_e5);
        pire[t] = std::max(pire[t], std::max(d1, d2));
        somme[t] += static_cast<double>(d1 + d2) / 2.0;
        ++nb[t];
        ++normaux;
    }
    void imprimer(const char *nom) const {
        std::printf("%s : %d jours normaux, %d nuits polaires, %d jours polaires, %d situations differentes\n", nom,
                    normaux, nuits, jours, situations_differentes);
        long long pire_tout = 0;
        for (int t = 0; t < TRANCHES; ++t) {
            std::printf("   %-12s n=%2d  pire=%4lld s  moyen=%.0f s\n", NOMS_TRANCHES[t], nb[t], pire[t],
                        nb[t] ? somme[t] / nb[t] : 0.0);
            pire_tout = std::max(pire_tout, pire[t]);
        }
        std::printf("   pire ecart toutes tranches : %lld s\n", pire_tout);
    }
};

// Une ligne par jour : {début du jour, lat_e5, lon_e5, situation (même numérotation que `Situation`), lever, coucher}.
void liste(const char *nom, const long long *a, std::size_t n) {
    Cumul c;
    for (std::size_t i = 0; i < n / 6; ++i) {
        const long long *l = a + i * 6;
        const LeverCoucher e = lever_coucher_e5(l[0], static_cast<std::int32_t>(l[1]), static_cast<std::int32_t>(l[2]));
        if (static_cast<long long>(e.situation) != l[3]) {
            ++c.situations_differentes;
            continue;
        }
        if (e.situation == Situation::NuitPolaire) {
            ++c.nuits;
            continue;
        }
        if (e.situation == Situation::JourPolaire) {
            ++c.jours;
            continue;
        }
        c.ajouter(l[1], absolu(e.lever_s - l[4]), absolu(e.coucher_s - l[5]));
    }
    c.imprimer(nom);
}

// Les sections de attendu_soleil.inc : forme en degrés, date civile et décalage (celle de l'écran principal). Dans la section
// OPEN-METEO, la situation 2 est une nuit polaire écrite « lever == coucher » (comme Open-Meteo).
void section(const char *nom, const Ligne *a, std::size_t n) {
    Cumul c;
    for (std::size_t i = 0; i < n; ++i) {
        const Ligne &l = a[i];
        const LeverCoucher e = lever_coucher(l.latitude, l.longitude, l.an, l.mois, l.jour, l.decalage);
        if (static_cast<int>(e.situation) != l.cas) {
            ++c.situations_differentes;
            continue;
        }
        if (e.situation == Situation::NuitPolaire) {
            ++c.nuits;
            continue;
        }
        if (e.situation == Situation::JourPolaire) {
            ++c.jours;
            continue;
        }
        const long long lat_e5 = std::llround(l.latitude * 100000.0);
        c.ajouter(lat_e5, absolu(e.lever_s - l.lever), absolu(e.coucher_s - l.coucher));
    }
    c.imprimer(nom);
}

// Open-Meteo : 13 valeurs de tête, puis 8 par jour (date, code, min, max, lever, coucher, uv, proba), sept jours.
template <std::size_t N>
void open_meteo(const char *nom, const long long (&att)[N], std::int32_t lat_e5, std::int32_t lon_e5) {
    long long pire = 0;
    int jours = 0, nuits = 0;
    for (int k = 0; k < 7; ++k) {
        const std::size_t base = 13 + 8 * static_cast<std::size_t>(k);
        const long long date = att[base], lever = att[base + 4], coucher = att[base + 5];
        if (lever == date && coucher == date) {
            ++nuits;
            continue;
        }
        const LeverCoucher e = lever_coucher_e5(date, lat_e5, lon_e5);
        pire = std::max(pire, std::max(absolu(e.lever_s - lever), absolu(e.coucher_s - coucher)));
        ++jours;
    }
    std::printf("Open-Meteo %-13s : %d jours, %d nuits polaires, pire ecart %lld s\n", nom, jours, nuits, pire);
}

} // namespace

int main() {
    liste("MET Norway Sunrise 3.0", ATT_MET, sizeof ATT_MET / sizeof *ATT_MET);
    liste("Second calcul (USNO)", ATT_USNO, sizeof ATT_USNO / sizeof *ATT_USNO);
    section("Python (NOAA reecrit), 120 jours", PYTHON, sizeof PYTHON / sizeof *PYTHON);
    section("Open-Meteo, 63 jours", OPEN_METEO_63, sizeof OPEN_METEO_63 / sizeof *OPEN_METEO_63);
    open_meteo("Lyon", ATT_LYON, 4574906, 484789);
    open_meteo("Seattle", ATT_SEATTLE, 4760621, -12233207);
    open_meteo("Singapour", ATT_SINGAPOUR, 129000, 10385000);
    open_meteo("Sydney", ATT_SYDNEY, -3386785, 15120732);
    open_meteo("Katmandou", ATT_KATMANDOU, 2771720, 8532400);
    open_meteo("Terre-Neuve", ATT_TERRENEUVE, 4756000, -5271000);
    open_meteo("Longyearbyen", ATT_LONGYEARBYEN, 7822000, 1562000);
    return 0;
}
