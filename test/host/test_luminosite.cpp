// Tests du noyau pur : courbe de luminosité perçue -> rapport cyclique (luminosite.hpp).
// Les valeurs de référence viennent d'un calcul flottant fait à part (Python : 20 + 980 * (niveau / 100) ** 2,5, et la même
// formule avec un plafond de 800 pour mille), pas du code testé.
#include "ecran/luminosite.hpp"
#include "mini.hpp"

using namespace ecran::luminosite;

namespace {

constexpr int proche(std::int32_t a, double attendu, double tolerance) {
    const double d = static_cast<double>(a) - attendu;
    return (d < 0 ? -d : d) <= tolerance ? 1 : 0;
}

constexpr int zero_est_eteint_quelle_que_soit_la_courbe() {
    VERIFIER(duty_pour_mille(0) == 0);
    VERIFIER(duty_pour_mille(-5) == 0);
    VERIFIER(duty_pour_mille(0, Courbe{300, 900}) == 0); // jamais le plancher
    VERIFIER(duty_pour_bits(0, 10) == 0);
    VERIFIER(duty_pour_bits(0, 8, Courbe{300, 900}) == 0);
    return 0;
}

constexpr int un_est_le_plancher_et_cent_le_plafond() {
    VERIFIER(duty_pour_mille(1) == 20);
    VERIFIER(duty_pour_mille(100) == 1000);
    VERIFIER(duty_pour_mille(150) == 1000); // au-dela de 100 : borne
    const Courbe c{35, 800};
    VERIFIER(duty_pour_mille(1, c) == 35);
    VERIFIER(duty_pour_mille(100, c) == 800);
    // Le plafond d'une carte n'est jamais depasse, meme a la resolution du registre.
    VERIFIER(duty_pour_bits(100, 10, c) == 818); // 800 * 1023 / 1000, arrondi
    VERIFIER(duty_pour_bits(100, 10) == 1023);
    VERIFIER(duty_pour_bits(100, 8) == 255);
    return 0;
}

constexpr int valeurs_de_reference_du_calcul_flottant() {
    constexpr int niveaux[] = {1, 5, 10, 20, 30, 40, 50, 60, 70, 75, 80, 90, 99, 100};
    constexpr double attendu[] = {20.01,  20.55,  23.10,  37.53,  68.31,  119.17, 193.24,
                                  293.28, 421.76, 497.40, 580.98, 773.06, 975.68, 1000.0};
    for (int i = 0; i < 14; ++i) {
        VERIFIER(proche(duty_pour_mille(niveaux[i]), attendu[i], 1.0));
    }
    constexpr double attendu_800[] = {20.01,  20.44,  22.47,  33.95,  58.45,  98.93,  157.89,
                                      237.51, 339.77, 399.97, 466.50, 619.38, 780.65, 800.0};
    for (int i = 0; i < 14; ++i) {
        VERIFIER(proche(duty_pour_mille(niveaux[i], Courbe{20, 800}), attendu_800[i], 1.0));
    }
    return 0;
}

// Valeurs EXACTES (pas de tolérance) aux niveaux où l'arrondi et la troncature diffèrent : le pour mille arrondi vient de
// 20 + 980 * (n / 100) ** 2,5 calculé en flottant à part, le registre à 10 bits de (pour mille * 1023 + 500) // 1000.
constexpr int les_arrondis_sont_exacts() {
    VERIFIER(duty_pour_mille(6) == 21);
    VERIFIER(duty_pour_mille(8) == 22);
    VERIFIER(duty_pour_mille(12) == 25);
    VERIFIER(duty_pour_mille(17) == 32);
    VERIFIER(duty_pour_mille(21) == 40);
    VERIFIER(duty_pour_mille(25) == 51);
    VERIFIER(duty_pour_mille(32) == 77);
    VERIFIER(duty_pour_mille(49) == 185);
    VERIFIER(duty_pour_bits(14, 10) == 28);
    VERIFIER(duty_pour_bits(16, 10) == 31);
    VERIFIER(duty_pour_bits(31, 10) == 74);
    VERIFIER(duty_pour_bits(56, 10) == 256);
    VERIFIER(duty_pour_bits(60, 10) == 300);
    return 0;
}

constexpr int la_courbe_ne_descend_jamais() {
    for (int n = 0; n < 100; ++n) {
        VERIFIER(duty_pour_mille(n) <= duty_pour_mille(n + 1));
        VERIFIER(duty_pour_bits(n, 10) <= duty_pour_bits(n + 1, 10));
    }
    return 0;
}

// Au-dela du plancher, chaque cran du curseur doit se voir : deux niveaux voisins ne donnent pas le meme registre.
constexpr int chaque_niveau_utile_change_le_registre_a_dix_bits() {
    for (int n = 20; n < 100; ++n) {
        VERIFIER(duty_pour_bits(n, 10) < duty_pour_bits(n + 1, 10));
    }
    return 0;
}

// La courbe est PERCEPTUELLE : le niveau 50 emet nettement moins que la moitie de la lumiere du niveau 100.
constexpr int le_milieu_du_curseur_n_est_pas_la_moitie_de_la_lumiere() {
    VERIFIER(duty_pour_mille(50) < 250);
    VERIFIER(duty_pour_mille(50) > 100);
    VERIFIER(duty_pour_mille(25) < 100);
    return 0;
}

constexpr int borne_et_plafond() {
    VERIFIER(borner_niveau(-1) == 0);
    VERIFIER(borner_niveau(101) == 100);
    VERIFIER(borner_niveau(42) == 42);
    VERIFIER(niveau_plafonne(80, 30) == 30);
    VERIFIER(niveau_plafonne(10, 30) == 10);
    VERIFIER(niveau_plafonne(500, 700) == 100);
    VERIFIER(niveau_plafonne(-3, 30) == 0);
    return 0;
}

constexpr int racine_entiere_exacte() {
    VERIFIER(racine_entiere(0) == 0);
    VERIFIER(racine_entiere(1) == 1);
    VERIFIER(racine_entiere(15) == 3);
    VERIFIER(racine_entiere(16) == 4);
    VERIFIER(racine_entiere(2147483648ull) == 46340);
    VERIFIER(racine_entiere(18446744065119617025ull) == 4294967295ull);
    return 0;
}

constexpr int bits_hors_plage_retombent_sur_dix() {
    VERIFIER(duty_pour_bits(100, 0) == 1023);
    VERIFIER(duty_pour_bits(100, 17) == 1023);
    VERIFIER(duty_pour_bits(100, 16) == 65535);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(zero_est_eteint_quelle_que_soit_la_courbe)                                                                       \
    X(un_est_le_plancher_et_cent_le_plafond)                                                                           \
    X(valeurs_de_reference_du_calcul_flottant)                                                                         \
    X(les_arrondis_sont_exacts)                                                                                        \
    X(la_courbe_ne_descend_jamais)                                                                                     \
    X(chaque_niveau_utile_change_le_registre_a_dix_bits)                                                               \
    X(le_milieu_du_curseur_n_est_pas_la_moitie_de_la_lumiere)                                                          \
    X(borne_et_plafond)                                                                                                \
    X(racine_entiere_exacte)                                                                                           \
    X(bits_hors_plage_retombent_sur_dix)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
