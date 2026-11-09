// Tests de la bande de pluie de l'accueil : accueil_bande_pluie.hpp (ADR-0027) — une barre par quart d'heure, le quart en cours, « maintenant »,
// le repère « dans une heure », la fin de la pluie en cours, la hauteur d'une barre.
//
// Les quarts d'heure du cas principal sont ceux d'Open-Meteo pour la maison le 09/11/2026 à 12 h 18 (relevés à la main, voir
// docs/mesures/bande-pluie.md) : le quart écoulé de 12 h 00 (1,9 mm), puis 0,7 / 0,1 / 0,1 mm, puis sec. Les autres cas sont écrits à la main.
// Chaque test est évalué à la compilation ET à l'exécution.
#include <cstddef>
#include <cstdint>

#include "ecran/accueil_bande_pluie.hpp"
#include "mini.hpp"

using namespace ecran::accueil;

namespace {

constexpr int INCONNU = ecran::visuel::INCONNU_DIX;
// 2026-11-09 à Paris (UTC+1) : minuit = 1 794 092 400 + 86 400.
constexpr std::int64_t MINUIT = 1'794'092'400 + 86'400;
constexpr std::int64_t T(int h, int m, int s = 0) { return MINUIT + h * 3600 + m * 60 + s; }

// Le premier quart gardé commence à 12 h 00 (le quart écoulé), neuf quarts : jusqu'à 14 h 15.
constexpr int REEL[9] = {19, 7, 1, 1, 0, 0, 0, 0, 0};

constexpr BandePluie reel(std::int64_t maintenant) { return composer_bande_pluie(T(12, 0), REEL, 9, maintenant); }

// ---- les constantes -------------------------------------------------------------------------------------------------------------

constexpr int les_constantes_de_la_bande() {
    VERIFIER(QUARTS_BANDE_MAX == 8);
    VERIFIER(QUART_S == 900);
    VERIFIER(SEUIL_QUART_DIX == 1);
    VERIFIER(PLANCHER_ECHELLE_QUART_DIX_H == 30);
    return 0;
}

// ---- le cas réel ----------------------------------------------------------------------------------------------------------------

constexpr int la_bande_commence_au_quart_en_cours_pas_au_quart_ecoule() {
    const BandePluie b = reel(T(12, 18));
    VERIFIER(b.presente && b.n == 8);
    VERIFIER(b.barre[0].debut_s == T(12, 15) && b.barre[0].dix_mm == 7);
    VERIFIER(b.barre[1].debut_s == T(12, 30) && b.barre[1].dix_mm == 1);
    VERIFIER(b.barre[7].debut_s == T(14, 0) && b.barre[7].dix_mm == 0);
    return 0;
}

constexpr int le_quart_change_pile_au_quart_d_heure() {
    VERIFIER(reel(T(12, 15)).barre[0].debut_s == T(12, 15));     // pile : le quart qui commence
    VERIFIER(reel(T(12, 29, 59)).barre[0].debut_s == T(12, 15)); // une seconde avant : encore lui
    VERIFIER(reel(T(12, 30)).barre[0].debut_s == T(12, 30));     // pile : le suivant
    VERIFIER(reel(T(12, 30)).n == 7);                            // il ne reste que sept quarts
    return 0;
}

constexpr int la_pluie_en_cours_a_sa_force_sa_fin_et_ses_reperes() {
    const BandePluie b = reel(T(12, 18));
    VERIFIER(b.en_cours && b.premier == 0);
    VERIFIER(b.force == NiveauPluie::MOYEN); // 0,7 mm sur le quart = 2,8 mm/h
    VERIFIER(b.fin_dans_min == 42);          // 13 h 00 moins 12 h 18
    VERIFIER(b.maintenant_pm == 25);         // 3 min sur 120 min = 25 pour mille
    VERIFIER(b.plus_une_heure_pm == 525);    // 13 h 18 : 63 min sur 120
    VERIFIER(b.echelle_dix_h == 30);         // 2,8 mm/h < le plancher de 3 mm/h
    return 0;
}

constexpr int la_fin_est_arrondie_a_la_minute_au_dessus() {
    VERIFIER(reel(T(12, 18, 30)).fin_dans_min == 42); // 41 min 30 s
    VERIFIER(reel(T(12, 18, 1)).fin_dans_min == 42);  // 41 min 59 s
    VERIFIER(reel(T(12, 17, 59)).fin_dans_min == 43); // 42 min 1 s
    VERIFIER(reel(T(12, 59, 59)).fin_dans_min == 1);  // il reste une seconde : « dans 1 min », jamais 0 avant la fin
    return 0;
}

// ---- la force et l'échelle -----------------------------------------------------------------------------------------------------

constexpr int le_niveau_est_celui_de_la_pluie_ramenee_a_l_heure() {
    constexpr int q[8] = {2, 3, 7, 8, 1, 0, 0, 0}; // 0,8 / 1,2 / 2,8 / 3,2 mm/h
    const BandePluie b = composer_bande_pluie(T(12, 0), q, 8, T(12, 1));
    VERIFIER(b.barre[0].niveau == NiveauPluie::FAIBLE && b.barre[0].dix_mm_h == 8);
    VERIFIER(b.barre[1].niveau == NiveauPluie::MOYEN && b.barre[1].dix_mm_h == 12);
    VERIFIER(b.barre[2].niveau == NiveauPluie::MOYEN && b.barre[2].dix_mm_h == 28);
    VERIFIER(b.barre[3].niveau == NiveauPluie::FORT && b.barre[3].dix_mm_h == 32);
    return 0;
}

constexpr int l_echelle_a_un_plancher_de_trois_millimetres_et_suit_la_plus_forte_barre() {
    constexpr int bruine[8] = {1, 1, 0, 0, 0, 0, 0, 0};
    VERIFIER(composer_bande_pluie(T(12, 0), bruine, 8, T(12, 1)).echelle_dix_h == 30);
    constexpr int forte[8] = {0, 9, 0, 0, 0, 0, 0, 0}; // 0,9 mm sur le quart = 3,6 mm/h
    VERIFIER(composer_bande_pluie(T(12, 0), forte, 8, T(12, 1)).echelle_dix_h == 36);
    return 0;
}

constexpr int une_pluie_qui_commence_plus_tard_prend_la_force_du_premier_quart_pluvieux() {
    constexpr int q[8] = {0, 0, 0, 2, 9, 0, 0, 0}; // sec, puis 0,8 mm/h, puis 3,6 mm/h
    const BandePluie b = composer_bande_pluie(T(12, 0), q, 8, T(12, 1));
    VERIFIER(b.presente && !b.en_cours && b.premier == 3);
    VERIFIER(b.force == NiveauPluie::FAIBLE); // le premier quart, pas le plus fort
    VERIFIER(b.fin_dans_min == -1);           // la fin n'est dite que pour une pluie en cours
    return 0;
}

// ---- la présence de la bande ----------------------------------------------------------------------------------------------------

constexpr int sans_pluie_dans_la_bande_il_n_y_a_pas_de_bande() {
    constexpr int sec[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const BandePluie b = composer_bande_pluie(T(12, 0), sec, 8, T(12, 1));
    VERIFIER(!b.presente && b.premier == -1 && !b.en_cours);
    VERIFIER(b.n == 8); // les barres existent : c'est l'écran qui décide de ne rien dessiner
    constexpr int une_goutte[8] = {0, 0, 0, 0, 0, 0, 0, 1}; // 0,1 mm au dernier quart : c'est de la pluie
    VERIFIER(composer_bande_pluie(T(12, 0), une_goutte, 8, T(12, 1)).presente);
    return 0;
}

constexpr int la_pluie_du_quart_ecoule_ne_fait_pas_de_bande() {
    // 1,9 mm est tombé de 12 h 00 à 12 h 15 : à 12 h 16 le quart est écoulé, la pluie suivante est de 0,7 mm (en cours).
    constexpr int q[9] = {19, 0, 0, 0, 0, 0, 0, 0, 0};
    VERIFIER(!composer_bande_pluie(T(12, 0), q, 9, T(12, 16)).presente);
    return 0;
}

constexpr int des_quarts_qui_ne_couvrent_pas_le_moment_ne_donnent_jamais_de_bande() {
    VERIFIER(!reel(T(14, 15)).presente && reel(T(14, 15)).n == 0); // tous passés (neuf quarts : jusqu'à 14 h 15)
    VERIFIER(!reel(T(15, 0)).presente);
    VERIFIER(!reel(T(11, 59, 59)).presente);                                       // pas encore commencés
    VERIFIER(reel(T(14, 14, 59)).presente == false && reel(T(14, 14, 59)).n == 1); // le dernier quart, sec
    return 0;
}

constexpr int les_entrees_invalides_ne_donnent_rien() {
    VERIFIER(!composer_bande_pluie(T(12, 0), nullptr, 9, T(12, 18)).presente);
    VERIFIER(composer_bande_pluie(T(12, 0), REEL, 0, T(12, 18)).n == 0);
    VERIFIER(composer_bande_pluie(0, REEL, 9, T(12, 18)).n == 0);
    VERIFIER(composer_bande_pluie(T(12, 0), REEL, 9, 0).n == 0);
    VERIFIER(composer_bande_pluie(T(12, 0), REEL, 9, -5).n == 0);
    return 0;
}

constexpr int la_bande_ne_depasse_jamais_deux_heures() {
    constexpr int q[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    const BandePluie b = composer_bande_pluie(T(12, 0), q, 12, T(12, 1));
    VERIFIER(b.n == QUARTS_BANDE_MAX);
    return 0;
}

// ---- l'inconnu n'est jamais un zéro ----------------------------------------------------------------------------------------------

constexpr int un_quart_sans_valeur_est_une_barre_vide_jamais_une_barre_seche() {
    constexpr int q[8] = {7, INCONNU, 0, 0, 0, 0, 0, 0};
    const BandePluie b = composer_bande_pluie(T(12, 0), q, 8, T(12, 1));
    VERIFIER(b.barre[1].dix_mm == INCONNU && b.barre[1].dix_mm_h == INCONNU && !b.barre[1].pluvieux);
    VERIFIER(b.barre[2].dix_mm == 0 && b.barre[2].dix_mm_h == 0); // un vrai zéro, lui, est sec
    return 0;
}

constexpr int un_quart_inconnu_coupe_la_fin_de_la_pluie_il_ne_dit_pas_qu_elle_finit() {
    constexpr int q[8] = {7, INCONNU, 0, 0, 0, 0, 0, 0};
    const BandePluie b = composer_bande_pluie(T(12, 0), q, 8, T(12, 1));
    VERIFIER(b.presente && b.en_cours && b.fin_dans_min == -1);
    return 0;
}

constexpr int une_pluie_qui_dure_jusqu_au_bout_de_la_bande_n_a_pas_de_fin() {
    constexpr int q[8] = {3, 3, 3, 3, 3, 3, 3, 3};
    const BandePluie b = composer_bande_pluie(T(12, 0), q, 8, T(12, 1));
    VERIFIER(b.presente && b.en_cours && b.fin_dans_min == -1);
    return 0;
}

// ---- le repère « dans une heure » -----------------------------------------------------------------------------------------------

constexpr int le_repere_dans_une_heure_sort_de_la_bande_quand_elle_est_courte() {
    const BandePluie b = reel(T(13, 50)); // il reste les quarts de 13 h 45 et 14 h 00 : 30 min de bande
    VERIFIER(b.n == 2);
    VERIFIER(b.plus_une_heure_pm == -1);
    VERIFIER(b.maintenant_pm == 166); // 5 min sur 30 min

    // Pile au bord : quatre quarts = une heure de bande, « maintenant » au début. « Dans une heure » tombe sur la fin, hors de la bande ;
    // avec un quart de plus il tombe à 800 ‰ (60 min sur 75).
    constexpr int PLUIE[5] = {5, 5, 5, 5, 5};
    const BandePluie une_heure = composer_bande_pluie(T(13, 0), PLUIE, 4, T(13, 0));
    VERIFIER(une_heure.presente && une_heure.n == 4);
    VERIFIER(une_heure.maintenant_pm == 0);
    VERIFIER(une_heure.plus_une_heure_pm == -1);
    const BandePluie plus_longue = composer_bande_pluie(T(13, 0), PLUIE, 5, T(13, 0));
    VERIFIER(plus_longue.n == 5);
    VERIFIER(plus_longue.plus_une_heure_pm == 800);
    return 0;
}

constexpr int le_repere_dans_une_heure_est_a_la_fraction_exacte() {
    const BandePluie b = reel(T(12, 15)); // pile au début : maintenant à 0, +1 h à la moitié de la bande de 2 h
    VERIFIER(b.maintenant_pm == 0);
    VERIFIER(b.plus_une_heure_pm == 500);
    return 0;
}

// ---- la hauteur d'une barre ---------------------------------------------------------------------------------------------------------

constexpr int la_hauteur_est_en_racine_avec_un_plancher_et_un_plafond() {
    VERIFIER(hauteur_de_barre_quart(0, 30, 100, 10) == 0);       // sec : rien
    VERIFIER(hauteur_de_barre_quart(INCONNU, 30, 100, 10) == 0); // inconnu : rien
    VERIFIER(hauteur_de_barre_quart(30, 30, 100, 10) == 100);    // l'échelle pleine : toute la hauteur
    VERIFIER(hauteur_de_barre_quart(60, 30, 100, 10) == 100);    // au-delà : le plafond
    VERIFIER(hauteur_de_barre_quart(3, 30, 100, 10) == 38);      // 0,3 mm/h : 10 + 90 x racine(0,1) = 38
    VERIFIER(hauteur_de_barre_quart(1, 30, 100, 10) >= 10);      // une goutte se voit
    VERIFIER(hauteur_de_barre_quart(15, 30, 100, 10) == 73);     // 10 + 90 x racine(0,5) = 73,6 : arrondi vers le bas
    return 0;
}

constexpr int la_hauteur_tient_dans_les_cas_limites() {
    VERIFIER(hauteur_de_barre_quart(10, 30, 0, 5) == 0);  // pas de place
    VERIFIER(hauteur_de_barre_quart(10, 0, 100, 5) == 0); // échelle invalide
    VERIFIER(hauteur_de_barre_quart(10, 30, 4, 10) == 4); // plancher plus haut que la place : la place
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(les_constantes_de_la_bande)                                                                                      \
    X(la_bande_commence_au_quart_en_cours_pas_au_quart_ecoule)                                                         \
    X(le_quart_change_pile_au_quart_d_heure)                                                                           \
    X(la_pluie_en_cours_a_sa_force_sa_fin_et_ses_reperes)                                                              \
    X(la_fin_est_arrondie_a_la_minute_au_dessus)                                                                       \
    X(le_niveau_est_celui_de_la_pluie_ramenee_a_l_heure)                                                               \
    X(l_echelle_a_un_plancher_de_trois_millimetres_et_suit_la_plus_forte_barre)                                        \
    X(une_pluie_qui_commence_plus_tard_prend_la_force_du_premier_quart_pluvieux)                                       \
    X(sans_pluie_dans_la_bande_il_n_y_a_pas_de_bande)                                                                  \
    X(la_pluie_du_quart_ecoule_ne_fait_pas_de_bande)                                                                   \
    X(des_quarts_qui_ne_couvrent_pas_le_moment_ne_donnent_jamais_de_bande)                                             \
    X(les_entrees_invalides_ne_donnent_rien)                                                                           \
    X(la_bande_ne_depasse_jamais_deux_heures)                                                                          \
    X(un_quart_sans_valeur_est_une_barre_vide_jamais_une_barre_seche)                                                  \
    X(un_quart_inconnu_coupe_la_fin_de_la_pluie_il_ne_dit_pas_qu_elle_finit)                                           \
    X(une_pluie_qui_dure_jusqu_au_bout_de_la_bande_n_a_pas_de_fin)                                                     \
    X(le_repere_dans_une_heure_sort_de_la_bande_quand_elle_est_courte)                                                 \
    X(le_repere_dans_une_heure_est_a_la_fraction_exacte)                                                               \
    X(la_hauteur_est_en_racine_avec_un_plancher_et_un_plafond)                                                         \
    X(la_hauteur_tient_dans_les_cas_limites)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
