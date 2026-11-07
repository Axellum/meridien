// Mesure (ce n'est pas un test) : de combien le lever et le coucher du soleil calculés sur l'appareil (soleil_calcule.hpp) s'écartent
// de trois références qui ne viennent pas de ce code. Les chiffres de docs/mesures/fournisseurs-meteo.md, §5, sont ceux qu'elle affiche.
//   - « Sunrise 3.0 » de MET Norway : 79 jours de 24 lieux (donnees/attendu_soleil_metno.inc) ;
//   - un second calcul écrit en Python, algorithme « Almanac for Computers » (donnees/attendu_soleil_almanac.inc) ;
//   - le lever et le coucher donnés par Open-Meteo, 7 jours pour 7 lieux (donnees/attendu_riche_*.inc).
// Compiler et lancer depuis la racine du dépôt, avec le compilateur de tools\env_hote.ps1 :
//   clang++ -std=c++17 -O1 -I components/core/include -I test/host test/host/mesure_ecarts_soleil.cpp -o mesure_ecarts_soleil.exe
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "ecran/soleil_calcule.hpp"

using namespace ecran::astro;

namespace {

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

// Une ligne par jour : {début du jour, lat_e5, lon_e5, situation (0 normal, 1 nuit polaire, 2 jour polaire), lever, coucher}.
void liste(const char *nom, const long long *a, std::size_t n) {
    constexpr int TRANCHES = 4;
    const char *noms[TRANCHES] = {"|lat| <= 45", "45 - 60", "60 - 66,5", "> 66,5"};
    long long pire[TRANCHES] = {};
    double somme[TRANCHES] = {};
    int nb[TRANCHES] = {};
    int nuits = 0, jours = 0, situations_differentes = 0, normaux = 0;
    for (std::size_t i = 0; i < n / 6; ++i) {
        const long long *c = a + i * 6;
        const EvenementsSoleil e =
            evenements_du_jour(c[0], static_cast<std::int32_t>(c[1]), static_cast<std::int32_t>(c[2]));
        if (static_cast<long long>(e.situation) != c[3]) {
            ++situations_differentes;
            continue;
        }
        if (e.situation == Situation::NuitPolaire) {
            ++nuits;
            continue;
        }
        if (e.situation == Situation::JourPolaire) {
            ++jours;
            continue;
        }
        const long long lat = absolu(c[1]);
        const int t = lat <= 4500000 ? 0 : (lat <= 6000000 ? 1 : (lat <= 6650000 ? 2 : 3));
        const long long d1 = absolu(e.lever_s - c[4]);
        const long long d2 = absolu(e.coucher_s - c[5]);
        pire[t] = std::max(pire[t], std::max(d1, d2));
        somme[t] += static_cast<double>(d1 + d2) / 2.0;
        ++nb[t];
        ++normaux;
    }
    std::printf("%s : %d jours normaux, %d nuits polaires, %d jours polaires, %d situations differentes\n", nom,
                normaux, nuits, jours, situations_differentes);
    for (int t = 0; t < TRANCHES; ++t)
        std::printf("   %-12s n=%2d  pire=%4lld s  moyen=%.0f s\n", noms[t], nb[t], pire[t],
                    nb[t] ? somme[t] / nb[t] : 0.0);
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
        const EvenementsSoleil e = evenements_du_jour(date, lat_e5, lon_e5);
        pire = std::max(pire, std::max(absolu(e.lever_s - lever), absolu(e.coucher_s - coucher)));
        ++jours;
    }
    std::printf("Open-Meteo %-13s : %d jours, %d nuits polaires, pire ecart %lld s\n", nom, jours, nuits, pire);
}

} // namespace

int main() {
    liste("MET Norway Sunrise 3.0", ATT_MET, sizeof ATT_MET / sizeof *ATT_MET);
    liste("Second calcul (USNO)", ATT_USNO, sizeof ATT_USNO / sizeof *ATT_USNO);
    open_meteo("Lyon", ATT_LYON, 4574906, 484789);
    open_meteo("Seattle", ATT_SEATTLE, 4760621, -12233207);
    open_meteo("Singapour", ATT_SINGAPOUR, 129000, 10385000);
    open_meteo("Sydney", ATT_SYDNEY, -3386785, 15120732);
    open_meteo("Katmandou", ATT_KATMANDOU, 2771720, 8532400);
    open_meteo("Terre-Neuve", ATT_TERRENEUVE, 4756000, -5271000);
    open_meteo("Longyearbyen", ATT_LONGYEARBYEN, 7822000, 1562000);
    return 0;
}
