// Tests du noyau pur : lune (phase moyenne), moment de la journée et position du soleil sur son arc.
#include <initializer_list>

#include "ecran/visuel_astres.hpp"
#include "mini.hpp"

using namespace ecran::visuel;

namespace {

// Repères lus le 07/11/2026 sur https://failflow.com/moonphases/2026 (page qui dit calculer avec l'algorithme de Meeus ; une seule
// source, recoupée seulement avec l'éclipse de Lune du 3 mars 2026 et l'éclipse de Soleil du 12 août 2026, qui tombent sur ces
// dates). Pleine lune du 3 mars 2026 à 11:39 UTC, nouvelle lune du 12 août 2026 à 17:38 UTC, pleine lune du 29 juillet 2026 à
// 14:37 UTC, pleine lune du 28 août 2026 à 04:19 UTC.
constexpr std::int64_t PLEINE_2026_03_03 = 1772537940;
constexpr std::int64_t NOUVELLE_2026_08_12 = 1786556280;
constexpr std::int64_t PLEINE_2026_07_29 = 1785335820;
constexpr std::int64_t PLEINE_2026_08_28 = 1787890740;
constexpr std::int64_t JOUR_S = 86400;
// Tolérance : la phase moyenne s'écarte de la vraie d'au plus ~14 h ; on accepte 0,75 jour.
constexpr std::int64_t TOLERANCE_S = 3 * JOUR_S / 4;

constexpr bool proche(std::int64_t a, std::int64_t b, std::int64_t tolerance) {
    return (a > b ? a - b : b - a) <= tolerance;
}

constexpr int nouvelle_lune_est_l_indice_0() {
    const PhaseLune p = phase_lune(NOUVELLE_2026_08_12);
    VERIFIER(p.indice == 0);
    VERIFIER(p.eclairement_pct <= 3 || p.age_dixiemes_j >= 290); // juste avant ou juste après : disque quasi noir
    return 0;
}

constexpr int pleine_lune_est_l_indice_4() {
    for (std::int64_t t : {PLEINE_2026_03_03, PLEINE_2026_07_29, PLEINE_2026_08_28}) {
        const PhaseLune p = phase_lune(t);
        VERIFIER(p.indice == 4);
        VERIFIER(p.eclairement_pct >= 97);
    }
    return 0;
}

constexpr int les_phases_se_suivent_dans_l_ordre_du_calendrier() {
    // Entre la pleine lune du 29/07/2026 et celle du 28/08/2026 : décroissante, dernier quartier, nouvelle, croissante...
    const PhaseLune apres_pleine = phase_lune(PLEINE_2026_07_29 + 3 * JOUR_S);
    VERIFIER(apres_pleine.indice == 5); // gibbeuse décroissante
    VERIFIER(!lune_croissante(apres_pleine));
    const PhaseLune dernier_quartier = phase_lune(PLEINE_2026_07_29 + 7 * JOUR_S + JOUR_S * 2 / 5);
    VERIFIER(dernier_quartier.indice == 6);
    VERIFIER(dernier_quartier.eclairement_pct >= 40 && dernier_quartier.eclairement_pct <= 60);
    const PhaseLune croissant = phase_lune(NOUVELLE_2026_08_12 + 3 * JOUR_S);
    VERIFIER(croissant.indice == 1);
    VERIFIER(lune_croissante(croissant));
    const PhaseLune premier_quartier = phase_lune(NOUVELLE_2026_08_12 + 7 * JOUR_S + JOUR_S * 2 / 5);
    VERIFIER(premier_quartier.indice == 2);
    VERIFIER(premier_quartier.eclairement_pct >= 40 && premier_quartier.eclairement_pct <= 60);
    return 0;
}

constexpr int le_cycle_dure_un_mois_synodique() {
    // Deux pleines lunes consécutives du tableau sont séparées de 29,5 jours +- 0,75 jour.
    VERIFIER(proche(PLEINE_2026_08_28 - PLEINE_2026_07_29, LUNE_MOIS_SYNODIQUE_S, TOLERANCE_S));
    // La phase calculée revient au même point un mois plus tard.
    const PhaseLune a = phase_lune(1760000000);
    const PhaseLune b = phase_lune(1760000000 + LUNE_MOIS_SYNODIQUE_S);
    VERIFIER(a.indice == b.indice);
    VERIFIER(a.age_dixiemes_j == b.age_dixiemes_j);
    return 0;
}

constexpr int avant_la_reference_le_calcul_reste_juste() {
    // Un instant antérieur à la nouvelle lune de référence (modulo négatif) : l'âge reste dans [0, 29,5 j[.
    const PhaseLune p = phase_lune(LUNE_NOUVELLE_REFERENCE_S - 10 * JOUR_S);
    VERIFIER(p.age_dixiemes_j >= 0 && p.age_dixiemes_j < 296);
    VERIFIER(p.indice >= 0 && p.indice < LUNE_NB_PHASES);
    VERIFIER(p.eclairement_pct >= 0 && p.eclairement_pct <= 100);
    return 0;
}

constexpr int toutes_les_valeurs_restent_dans_leurs_bornes() {
    // Un mois entier, toutes les 6 heures : jamais d'indice, d'âge ni d'éclairement hors plage ; l'éclairement monte puis
    // redescend d'une seule traite (une seule pleine lune par mois).
    int montees = 0;
    int precedent = -1;
    for (std::int64_t t = NOUVELLE_2026_08_12; t < NOUVELLE_2026_08_12 + 30 * JOUR_S; t += 6 * 3600) {
        const PhaseLune p = phase_lune(t);
        VERIFIER(p.indice >= 0 && p.indice < LUNE_NB_PHASES);
        VERIFIER(p.age_dixiemes_j >= 0 && p.age_dixiemes_j <= 295);
        VERIFIER(p.eclairement_pct >= 0 && p.eclairement_pct <= 100);
        if (precedent >= 0 && p.eclairement_pct < precedent && montees == 0)
            montees = 1; // début de la descente
        if (montees == 1 && precedent >= 0)
            VERIFIER(p.eclairement_pct <= precedent + 1); // une fois la descente commencée, elle ne remonte pas
        precedent = p.eclairement_pct;
    }
    return 0;
}

constexpr int la_serie_de_taylor_est_assez_juste() {
    // cos(2*pi*x) aux repères : 0 -> 1, 1/4 -> 0, 1/2 -> -1, 3/4 -> 0, 1/6 -> 0,5 (à 1 % près)
    VERIFIER(detail::cos_tour_milliemes(0) >= 995);
    VERIFIER(detail::cos_tour_milliemes(250) >= -10 && detail::cos_tour_milliemes(250) <= 10);
    VERIFIER(detail::cos_tour_milliemes(500) <= -995);
    VERIFIER(detail::cos_tour_milliemes(750) >= -10 && detail::cos_tour_milliemes(750) <= 10);
    VERIFIER(detail::cos_tour_milliemes(167) >= 490 && detail::cos_tour_milliemes(167) <= 510);
    VERIFIER(detail::cos_tour_milliemes(-250) >= -10 && detail::cos_tour_milliemes(-250) <= 10);
    return 0;
}

// ---- soleil ----

constexpr std::int64_t LEVER = 1000000 + 7 * 3600 + 30 * 60;    // 07:30 de ce jour fictif
constexpr std::int64_t COUCHER = 1000000 + 17 * 3600 + 30 * 60; // 17:30

constexpr int le_moment_de_la_journee_suit_le_lever_et_le_coucher() {
    VERIFIER(arc_soleil(LEVER - 2 * 3600, LEVER, COUCHER, false).moment == MomentJour::NUIT);
    VERIFIER(arc_soleil(LEVER - 20 * 60, LEVER, COUCHER, false).moment == MomentJour::AUBE);
    VERIFIER(arc_soleil(LEVER + 20 * 60, LEVER, COUCHER, true).moment == MomentJour::AUBE);
    VERIFIER(arc_soleil(LEVER + 2 * 3600, LEVER, COUCHER, true).moment == MomentJour::JOUR);
    VERIFIER(arc_soleil(COUCHER - 20 * 60, LEVER, COUCHER, true).moment == MomentJour::CREPUSCULE);
    VERIFIER(arc_soleil(COUCHER + 20 * 60, LEVER, COUCHER, false).moment == MomentJour::CREPUSCULE);
    VERIFIER(arc_soleil(COUCHER + 2 * 3600, LEVER, COUCHER, false).moment == MomentJour::NUIT);
    return 0;
}

constexpr int la_position_sur_l_arc_va_de_0_a_1000() {
    VERIFIER(arc_soleil(LEVER, LEVER, COUCHER, true).position_pm == 0);
    VERIFIER(arc_soleil(LEVER + 5 * 3600, LEVER, COUCHER, true).position_pm == 500); // midi solaire
    VERIFIER(arc_soleil(COUCHER, LEVER, COUCHER, true).position_pm == 1000);
    VERIFIER(arc_soleil(LEVER - 1, LEVER, COUCHER, false).position_pm == -1);   // pas encore levé
    VERIFIER(arc_soleil(COUCHER + 1, LEVER, COUCHER, false).position_pm == -1); // déjà couché
    VERIFIER(arc_soleil(LEVER + 5 * 3600, LEVER, COUCHER, true).duree_jour_s == 10 * 3600);
    return 0;
}

constexpr int le_temps_avant_le_lever_est_donne_la_nuit() {
    const ArcSoleil a = arc_soleil(LEVER - 3600, LEVER, COUCHER, false);
    VERIFIER(a.avant_lever_s == 3600);
    VERIFIER(arc_soleil(LEVER + 3600, LEVER, COUCHER, true).avant_lever_s == 0);
    return 0;
}

constexpr int sans_lever_ni_coucher_on_se_fie_a_is_day() {
    // Valeurs absentes (0), égales (nuit polaire constatée à Longyearbyen le 06/11/2026) ou dans le mauvais ordre.
    for (const bool jour : {true, false}) {
        const MomentJour attendu = jour ? MomentJour::JOUR : MomentJour::NUIT;
        VERIFIER(arc_soleil(5000000, 0, 0, jour).moment == attendu);
        VERIFIER(arc_soleil(5000000, 4000000, 4000000, jour).moment == attendu);
        VERIFIER(arc_soleil(5000000, 4100000, 4000000, jour).moment == attendu);
        VERIFIER(!arc_soleil(5000000, 4000000, 4000000, jour).soleil_connu);
        VERIFIER(arc_soleil(5000000, 4000000, 4000000, jour).position_pm == -1);
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(nouvelle_lune_est_l_indice_0)                                                                                    \
    X(pleine_lune_est_l_indice_4)                                                                                      \
    X(les_phases_se_suivent_dans_l_ordre_du_calendrier)                                                                \
    X(le_cycle_dure_un_mois_synodique)                                                                                 \
    X(avant_la_reference_le_calcul_reste_juste)                                                                        \
    X(toutes_les_valeurs_restent_dans_leurs_bornes)                                                                    \
    X(la_serie_de_taylor_est_assez_juste)                                                                              \
    X(le_moment_de_la_journee_suit_le_lever_et_le_coucher)                                                             \
    X(la_position_sur_l_arc_va_de_0_a_1000)                                                                            \
    X(le_temps_avant_le_lever_est_donne_la_nuit)                                                                       \
    X(sans_lever_ni_coucher_on_se_fie_a_is_day)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
