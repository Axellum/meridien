// Tests du noyau pur : lever et coucher du soleil par le calcul (soleil.hpp, algorithme de la NOAA, ADR-0012).
// Trois sources de vérité, indépendantes du code C++ :
//   - donnees/attendu_soleil.inc, section PYTHON : le même algorithme réécrit en Python avec `math` (120 jours, vingt lieux) ;
//   - donnees/attendu_soleil.inc, section OPEN-METEO : 63 jours RÉELS capturés le 06/11/2026 (neuf réponses, huit lieux) ;
//   - des constantes mathématiques connues pour les fonctions écrites à la main (sinus, arc tangente, racine : pas de <cmath> en
//     constexpr).
// Le jour polaire et la nuit polaire sont testés sur Longyearbyen, Tromsø et sur les pôles.
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

constexpr double absolu(double x) { return x < 0.0 ? -x : x; }
constexpr long long absolu(long long x) { return x < 0 ? -x : x; }
constexpr bool proche(double a, double b, double tolerance) { return absolu(a - b) <= tolerance; }

constexpr int les_lignes_de_reference_sont_la() {
    VERIFIER(NOMBRE_PYTHON == 120);
    VERIFIER(NOMBRE_OPEN_METEO == 63);
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
    VERIFIER(proche(detail::arc_cosinus(0.5), PI / 3.0, 1e-12));
    VERIFIER(proche(detail::arc_cosinus(-1.0), PI, 1e-12));
    VERIFIER(proche(detail::arc_cosinus(1.0), 0.0, 1e-12));
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
