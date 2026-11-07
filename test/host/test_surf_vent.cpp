// Tests du noyau pur « surf » : le vent par rapport à la plage (surf_vent.hpp) et le cosinus entier (surf_base.hpp). La formule
// est la nôtre (voir l'en-tête de surf_vent.hpp) : on la compare à l'expression écrite autrement (l'écart angulaire) sur tous les
// angles, et on la fait passer par 359°/0°, le vent nul et les rafales. Chaque test est évalué à la compilation (static_assert) ET à
// l'exécution (voir mini.hpp).
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "ecran/surf_base.hpp"
#include "ecran/surf_vent.hpp"
#include "mini.hpp"

using namespace ecran::surf;

namespace {

constexpr ClasseVent classe(int direction, int orientation) noexcept {
    return classe_de_direction(static_cast<std::int16_t>(direction), static_cast<std::int16_t>(orientation));
}

// ---- le cosinus entier ---------------------------------------------------------------------------------------------------

constexpr int cosinus_aux_angles_remarquables() {
    VERIFIER(cosinus_pm(0) == 1000 && cosinus_pm(60) == 500 && cosinus_pm(90) == 0 && cosinus_pm(120) == -500);
    VERIFIER(cosinus_pm(180) == -1000 && cosinus_pm(270) == 0 && cosinus_pm(360) == 1000);
    VERIFIER(cosinus_pm(30) == 866 && cosinus_pm(45) == 707 && cosinus_pm(135) == -707 && cosinus_pm(150) == -866);
    VERIFIER(cosinus_pm(1) == 1000 && cosinus_pm(2) == 999 && cosinus_pm(89) == 17 && cosinus_pm(91) == -17);
    VERIFIER(cosinus_pm(179) == -1000 && cosinus_pm(181) == -1000 && cosinus_pm(359) == 1000);
    return 0;
}

constexpr int cosinus_est_pair_periodique_et_symetrique_autour_de_90() {
    for (int d = -720; d <= 720; ++d) {
        VERIFIER(cosinus_pm(d) == cosinus_pm(-d));       // cos(-x) = cos(x)
        VERIFIER(cosinus_pm(d) == cosinus_pm(d + 360));  // période 360°
        VERIFIER(cosinus_pm(180 - d) == -cosinus_pm(d)); // cos(180° - x) = -cos(x)
        VERIFIER(cosinus_pm(d) >= -1000 && cosinus_pm(d) <= 1000);
    }
    return 0;
}

constexpr int cosinus_decroit_de_0_a_180_degres_sans_saut() {
    for (int d = 0; d < 180; ++d) {
        VERIFIER(cosinus_pm(d) >= cosinus_pm(d + 1));
        VERIFIER(cosinus_pm(d) - cosinus_pm(d + 1) <= 18); // la pente maximale est 17,5 millièmes par degré
    }
    return 0;
}

// ---- classe de la direction ----------------------------------------------------------------------------------------------

constexpr int plage_vers_l_ouest_les_directions_cardinales() {
    // β = 270 : l'océan est à l'ouest. Vent d'ouest = de mer ; vent d'est = de terre ; nord et sud = travers.
    VERIFIER(classe(270, 270) == ClasseVent::Onshore);
    VERIFIER(classe(90, 270) == ClasseVent::Offshore);
    VERIFIER(classe(0, 270) == ClasseVent::Travers);
    VERIFIER(classe(180, 270) == ClasseVent::Travers);
    return 0;
}

constexpr int les_frontieres_a_60_et_120_degres_sont_inclusives() {
    VERIFIER(classe(330, 270) == ClasseVent::Onshore); // 60° : cos = 0,5 : de mer (inclusif)
    VERIFIER(classe(210, 270) == ClasseVent::Onshore);
    VERIFIER(classe(331, 270) == ClasseVent::Travers); // 61°
    VERIFIER(classe(209, 270) == ClasseVent::Travers);
    VERIFIER(classe(30, 270) == ClasseVent::Offshore); // 120° : cos = -0,5 : de terre (inclusif)
    VERIFIER(classe(150, 270) == ClasseVent::Offshore);
    VERIFIER(classe(29, 270) == ClasseVent::Travers); // 119°
    VERIFIER(classe(151, 270) == ClasseVent::Travers);
    return 0;
}

constexpr int le_nord_est_un_cercle_359_et_0_sont_voisins() {
    // Orientation 0 (plage face au nord) : un vent de 359° vient presque pile de la mer.
    VERIFIER(classe(359, 0) == ClasseVent::Onshore && classe(1, 0) == ClasseVent::Onshore &&
             classe(0, 0) == ClasseVent::Onshore);
    VERIFIER(classe(300, 0) == ClasseVent::Onshore && classe(60, 0) == ClasseVent::Onshore); // 60° de chaque côté
    VERIFIER(classe(299, 0) == ClasseVent::Travers && classe(61, 0) == ClasseVent::Travers);
    VERIFIER(classe(240, 0) == ClasseVent::Offshore && classe(120, 0) == ClasseVent::Offshore);
    VERIFIER(classe(241, 0) == ClasseVent::Travers && classe(119, 0) == ClasseVent::Travers);
    VERIFIER(classe(180, 0) == ClasseVent::Offshore);
    // Orientation 359 : le vent de 0° est à 1°.
    VERIFIER(classe(0, 359) == ClasseVent::Onshore && classe(179, 359) == ClasseVent::Offshore);
    VERIFIER(classe(180, 359) == ClasseVent::Offshore && classe(178, 359) == ClasseVent::Offshore);
    VERIFIER(classe(359, 359) == ClasseVent::Onshore);
    // Orientation 1 : 181° est à 180° d'écart.
    VERIFIER(classe(181, 1) == ClasseVent::Offshore && classe(359, 1) == ClasseVent::Onshore);
    return 0;
}

// La même classification écrite avec l'écart angulaire (en degrés, sans cosinus) sur tout le cercle : si la table de cosinus ou les
// seuils se décalent d'un degré, ce test le voit.
constexpr int la_classe_suit_l_ecart_angulaire_pour_toutes_les_directions() {
    for (int orientation = 0; orientation < 360; orientation += 7) {
        for (int direction = 0; direction < 360; ++direction) {
            const int e = ecart_angulaire(direction, orientation);
            const ClasseVent attendu =
                e <= 60 ? ClasseVent::Onshore : (e >= 120 ? ClasseVent::Offshore : ClasseVent::Travers);
            VERIFIER(classe(direction, orientation) == attendu);
        }
    }
    for (const int orientation : {0, 1, 59, 60, 89, 90, 179, 180, 181, 269, 270, 271, 358, 359}) {
        for (int direction = 0; direction < 360; ++direction) {
            const int e = ecart_angulaire(direction, orientation);
            const ClasseVent attendu =
                e <= 60 ? ClasseVent::Onshore : (e >= 120 ? ClasseVent::Offshore : ClasseVent::Travers);
            VERIFIER(classe(direction, orientation) == attendu);
        }
    }
    return 0;
}

constexpr int la_classe_est_symetrique_gauche_droite() {
    for (int ecart = 0; ecart <= 180; ++ecart) {
        VERIFIER(classe(270 + ecart, 270) == classe(270 - ecart, 270));
        VERIFIER(classe(ecart, 0) == classe(360 - ecart, 0));
    }
    return 0;
}

// ---- composante vers la plage --------------------------------------------------------------------------------------------

constexpr int composante_signe_et_valeurs_remarquables() {
    // 20,0 km/h (200 dixièmes), plage à l'ouest.
    VERIFIER(composante_vers_plage_dixiemes(200, 270, 270) == 200); // de mer pile : tout le vent vers la plage
    VERIFIER(composante_vers_plage_dixiemes(200, 90, 270) == -200); // de terre pile : tout le vent vers le large
    VERIFIER(composante_vers_plage_dixiemes(200, 0, 270) == 0);     // le vent longe la plage
    VERIFIER(composante_vers_plage_dixiemes(200, 180, 270) == 0);
    VERIFIER(composante_vers_plage_dixiemes(200, 330, 270) == 100); // 60° : la moitié
    VERIFIER(composante_vers_plage_dixiemes(200, 30, 270) == -100); // 120° : la moitié, vers le large
    VERIFIER(composante_vers_plage_dixiemes(200, 315, 270) == 141); // 45° : 200 × 0,707 = 141,4
    return 0;
}

constexpr int composante_arrondie_la_moitie_s_eloigne_de_zero() {
    VERIFIER(composante_vers_plage_dixiemes(5, 330, 270) == 3); // 5 × 0,5 = 2,5 -> 3
    VERIFIER(composante_vers_plage_dixiemes(5, 30, 270) == -3); // -2,5 -> -3
    VERIFIER(composante_vers_plage_dixiemes(4, 330, 270) == 2); // 2,0
    VERIFIER(composante_vers_plage_dixiemes(1, 330, 270) == 1); // 0,5 -> 1
    VERIFIER(composante_vers_plage_dixiemes(1, 30, 270) == -1);
    VERIFIER(composante_vers_plage_dixiemes(1, 0, 270) == 0);
    return 0;
}

constexpr int composante_aux_bords_du_cercle() {
    VERIFIER(composante_vers_plage_dixiemes(300, 359, 0) == 300); // cos 1° = 1,000
    VERIFIER(composante_vers_plage_dixiemes(300, 0, 359) == 300);
    VERIFIER(composante_vers_plage_dixiemes(300, 180, 0) == -300);
    VERIFIER(composante_vers_plage_dixiemes(300, 180, 359) == -300);
    VERIFIER(composante_vers_plage_dixiemes(300, 359, 180) == -300);
    VERIFIER(composante_vers_plage_dixiemes(0, 270, 270) == 0); // vent nul : composante nulle, pas absente
    return 0;
}

constexpr int composante_absente_si_la_vitesse_ou_la_direction_manque() {
    VERIFIER(composante_vers_plage_dixiemes(ABSENT, 270, 270) == ABSENT);
    VERIFIER(composante_vers_plage_dixiemes(200, ABSENT, 270) == ABSENT);
    VERIFIER(composante_vers_plage_dixiemes(ABSENT, ABSENT, 270) == ABSENT);
    return 0;
}

constexpr int composante_sur_toute_la_plage_de_vitesse() {
    // Jusqu'à 500,0 km/h (la limite acceptée par l'analyseur) : tient dans un int16_t, du signe attendu.
    VERIFIER(composante_vers_plage_dixiemes(5000, 270, 270) == 5000);
    VERIFIER(composante_vers_plage_dixiemes(5000, 90, 270) == -5000);
    return 0;
}

// ---- vent effectif et calme ----------------------------------------------------------------------------------------------

constexpr int vent_effectif_moyenne_du_vent_et_de_la_rafale() {
    VERIFIER(vent_effectif_dixiemes(80, 300) == 190);   // (80 + 300) / 2
    VERIFIER(vent_effectif_dixiemes(80, ABSENT) == 80); // sans rafale : le vent moyen
    VERIFIER(vent_effectif_dixiemes(80, 50) == 80);     // une rafale plus faible que le vent est ignorée
    VERIFIER(vent_effectif_dixiemes(80, 80) == 80);
    VERIFIER(vent_effectif_dixiemes(0, 0) == 0);
    VERIFIER(vent_effectif_dixiemes(79, 120) == 100); // 99,5 -> 100 (la moitié monte)
    VERIFIER(vent_effectif_dixiemes(78, 120) == 99);
    VERIFIER(vent_effectif_dixiemes(ABSENT, 120) == ABSENT); // pas de vitesse : pas d'effectif, même avec une rafale
    VERIFIER(vent_effectif_dixiemes(ABSENT, ABSENT) == ABSENT);
    return 0;
}

constexpr int calme_sous_10_km_h_effectifs_seuil_inclus_dans_le_non_calme() {
    VERIFIER(VENT_CALME_DIXIEMES_KMH == 100);
    VERIFIER(classer_vent(99, ABSENT, 270, 270).classe == ClasseVent::Calme);    // 9,9 km/h
    VERIFIER(classer_vent(100, ABSENT, 270, 270).classe == ClasseVent::Onshore); // 10,0 km/h : plus calme
    VERIFIER(classer_vent(100, ABSENT, 90, 270).classe == ClasseVent::Offshore);
    VERIFIER(classer_vent(100, ABSENT, 0, 270).classe == ClasseVent::Travers);
    return 0;
}

constexpr int vent_nul_est_calme_avec_une_composante_nulle() {
    const VentSurf r = classer_vent(0, 0, 270, 270);
    VERIFIER(r.classe == ClasseVent::Calme && r.composante_dixiemes_kmh == 0 && r.effectif_dixiemes_kmh == 0);
    const VentSurf r2 = classer_vent(0, ABSENT, 0, 90);
    VERIFIER(r2.classe == ClasseVent::Calme);
    return 0;
}

constexpr int les_rafales_peuvent_sortir_le_vent_du_calme() {
    // 8 km/h en rafales à 30 : l'eau n'est pas lisse (effectif 19 km/h).
    const VentSurf r = classer_vent(80, 300, 270, 270);
    VERIFIER(r.classe == ClasseVent::Onshore && r.effectif_dixiemes_kmh == 190);
    VERIFIER(r.composante_dixiemes_kmh == 80); // la composante reste celle du vent MOYEN (la formule c = v · cos)
    // sans la rafale, le même vent moyen est calme
    VERIFIER(classer_vent(80, ABSENT, 270, 270).classe == ClasseVent::Calme);
    // une rafale ne change pas la classe de direction d'un vent déjà soutenu
    VERIFIER(classer_vent(200, 600, 90, 270).classe == ClasseVent::Offshore);
    return 0;
}

constexpr int sans_donnee_la_classe_est_inconnue_jamais_calme() {
    VERIFIER(classer_vent(ABSENT, ABSENT, 270, 270).classe == ClasseVent::Inconnue);
    VERIFIER(classer_vent(ABSENT, 200, 270, 270).classe == ClasseVent::Inconnue);
    VERIFIER(classer_vent(200, 300, ABSENT, 270).classe == ClasseVent::Inconnue);
    VERIFIER(classer_vent(0, 0, ABSENT, 270).classe == ClasseVent::Inconnue); // vitesse nulle mais direction inconnue
    const VentSurf r = classer_vent(ABSENT, ABSENT, ABSENT, 270);
    VERIFIER(r.composante_dixiemes_kmh == ABSENT && r.effectif_dixiemes_kmh == ABSENT);
    return 0;
}

constexpr int les_seuils_publics_sont_ceux_de_l_adr() {
    VERIFIER(OFFSHORE_COSINUS_PM_MAX == -500 && ONSHORE_COSINUS_PM_MIN == 500);
    VERIFIER(cosinus_pm(60) == ONSHORE_COSINUS_PM_MIN && cosinus_pm(120) == OFFSHORE_COSINUS_PM_MAX);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(cosinus_aux_angles_remarquables)                                                                                 \
    X(cosinus_est_pair_periodique_et_symetrique_autour_de_90)                                                          \
    X(cosinus_decroit_de_0_a_180_degres_sans_saut)                                                                     \
    X(plage_vers_l_ouest_les_directions_cardinales)                                                                    \
    X(les_frontieres_a_60_et_120_degres_sont_inclusives)                                                               \
    X(le_nord_est_un_cercle_359_et_0_sont_voisins)                                                                     \
    X(la_classe_suit_l_ecart_angulaire_pour_toutes_les_directions)                                                     \
    X(la_classe_est_symetrique_gauche_droite)                                                                          \
    X(composante_signe_et_valeurs_remarquables)                                                                        \
    X(composante_arrondie_la_moitie_s_eloigne_de_zero)                                                                 \
    X(composante_aux_bords_du_cercle)                                                                                  \
    X(composante_absente_si_la_vitesse_ou_la_direction_manque)                                                         \
    X(composante_sur_toute_la_plage_de_vitesse)                                                                        \
    X(vent_effectif_moyenne_du_vent_et_de_la_rafale)                                                                   \
    X(calme_sous_10_km_h_effectifs_seuil_inclus_dans_le_non_calme)                                                     \
    X(vent_nul_est_calme_avec_une_composante_nulle)                                                                    \
    X(les_rafales_peuvent_sortir_le_vent_du_calme)                                                                     \
    X(sans_donnee_la_classe_est_inconnue_jamais_calme)                                                                 \
    X(les_seuils_publics_sont_ceux_de_l_adr)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
