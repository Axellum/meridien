// Tests du noyau pur : courbe de volume perçu (volume.hpp). Références : -50 dB * ((100 - niveau) / 99) ** 2, calcul flottant fait
// à part (Python), et pas de 0,5 dB = 2 pas par dB.
#include "ecran/volume.hpp"
#include "mini.hpp"

using namespace ecran::volume;

namespace {

constexpr int proche(int a, double attendu, double tol) {
    const double d = static_cast<double>(a) - attendu;
    return (d < 0 ? -d : d) <= tol ? 1 : 0;
}

constexpr int le_maximum_est_zero_db_et_le_minimum_moins_cinquante() {
    VERIFIER(db_x10(100) == 0);
    VERIFIER(db_x10(1) == -500);
    VERIFIER(db_x10(150) == 0);   // borne
    VERIFIER(db_x10(-3) == -500); // au plus bas du curseur (le muet est une autre notion)
    return 0;
}

constexpr int valeurs_de_reference_du_calcul_flottant() {
    constexpr int niveaux[] = {1, 2, 10, 25, 50, 60, 75, 90, 99, 100};
    constexpr double attendu[] = {-500.0, -489.95, -413.22, -286.96, -127.54, -81.62, -31.88, -5.10, -0.05, 0.0};
    for (int i = 0; i < 10; ++i) {
        VERIFIER(proche(db_x10(niveaux[i]), attendu[i], 1.0));
    }
    return 0;
}

constexpr int la_courbe_ne_descend_jamais() {
    for (int n = 1; n < 100; ++n) {
        VERIFIER(db_x10(n) <= db_x10(n + 1));
        VERIFIER(pas_pour_niveau(n) >= pas_pour_niveau(n + 1)); // plus fort = pas du convertisseur plus petit
    }
    return 0;
}

// Les 50 premiers crans du curseur changent tous le pas du convertisseur (0,5 dB). Au-dessus de 50 la courbe devient plus fine que le
// convertisseur : deux crans voisins peuvent donner le même pas, et les crans 93 à 100 sont tous à 0 dB (limite du matériel, pas de la
// formule : valeurs de référence calculées à part en Python, voir docs/carte-tab5.md).
constexpr int les_cinquante_premiers_crans_sont_distincts_pour_le_convertisseur() {
    for (int n = 1; n <= 50; ++n) {
        VERIFIER(pas_pour_niveau(n) > pas_pour_niveau(n + 1));
    }
    VERIFIER(pas_pour_niveau(93) == 0);
    VERIFIER(pas_pour_niveau(92) == 1);
    return 0;
}

// Les crans du haut sont plus fins que ceux du bas : c'est l'intention de la courbe quadratique.
constexpr int les_crans_du_haut_sont_fins() {
    VERIFIER(db_x10(100) - db_x10(99) < db_x10(51) - db_x10(50));
    VERIFIER(db_x10(51) - db_x10(50) < db_x10(2) - db_x10(1));
    VERIFIER(db_x10(100) - db_x10(99) <= 5); // moins d'un demi-decibel entre les deux derniers crans
    return 0;
}

constexpr int le_muet_est_a_part() {
    VERIFIER(est_muet(0));
    VERIFIER(est_muet(-1));
    VERIFIER(!est_muet(1));
    VERIFIER(pas_pour_niveau(0) == kPasMax);
    VERIFIER(pas_pour_niveau(-5) == kPasMax);
    VERIFIER(pas_pour_niveau(1) != kPasMax);
    return 0;
}

constexpr int le_plafond_decale_toute_la_courbe() {
    VERIFIER(db_x10(100, -60) == -60);
    VERIFIER(db_x10(50, -60) == db_x10(50) - 60);
    VERIFIER(db_x10(1, -60) == -560);
    VERIFIER(db_x10(100, 40) == 0);              // un « plafond » positif n'amplifie pas
    VERIFIER(db_x10(1, -900) == kDbX10Plancher); // jamais sous -96 dB
    return 0;
}

constexpr int pas_du_convertisseur() {
    VERIFIER(pas_demi_db(0) == 0);
    VERIFIER(pas_demi_db(-5) == 1);     // 0,5 dB
    VERIFIER(pas_demi_db(-10) == 2);    // 1 dB
    VERIFIER(pas_demi_db(-127) == 25);  // 12,7 dB -> 25,4 pas
    VERIFIER(pas_demi_db(-500) == 100); // 50 dB
    VERIFIER(pas_demi_db(-960) == kPasMax);
    VERIFIER(pas_demi_db(-2000) == kPasMax); // jamais au-dela du registre
    VERIFIER(pas_demi_db(10) == 0);          // un « gain » est ramene a 0 dB
    VERIFIER(pas_pour_niveau(100) == 0);
    VERIFIER(pas_pour_niveau(50) == 26); // -12,7 dB -> 25,5 pas -> 26 (arrondi)
    VERIFIER(pas_pour_niveau(1) == 100);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(le_maximum_est_zero_db_et_le_minimum_moins_cinquante)                                                            \
    X(valeurs_de_reference_du_calcul_flottant)                                                                         \
    X(la_courbe_ne_descend_jamais)                                                                                     \
    X(les_cinquante_premiers_crans_sont_distincts_pour_le_convertisseur)                                               \
    X(les_crans_du_haut_sont_fins)                                                                                     \
    X(le_muet_est_a_part)                                                                                              \
    X(le_plafond_decale_toute_la_courbe)                                                                               \
    X(pas_du_convertisseur)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
