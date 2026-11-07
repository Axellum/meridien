// Tests du noyau pur « surf » : pleines et basses mers tirées de la série horaire du niveau de la mer (surf_maree.hpp), sur des séries
// synthétiques dont chaque heure est choisie (les réponses réelles sont dans test_surf_reponses.cpp). Chaque test est évalué à la
// compilation (static_assert) ET à l'exécution (voir mini.hpp).
#include <cstddef>
#include <cstdint>

#include "ecran/surf_donnees.hpp"
#include "ecran/surf_maree.hpp"
#include "mini.hpp"
#include "surf_outils.hpp"

using namespace ecran::surf;
using namespace surf_test;

namespace {

constexpr int N_ = ABSENT; // « pas de donnée » dans une série écrite à la main

// Une mer dont seul le niveau est renseigné : une heure par valeur, à partir de `debut`.
template <std::size_t N>
constexpr Marine niveaux(const int (&v)[N], std::int64_t debut = MINUIT, std::int32_t decalage = 0) noexcept {
    Marine m = mer_vide(N, debut, decalage);
    for (std::size_t i = 0; i < N; ++i) {
        m.heures[i].maree_cm = static_cast<std::int16_t>(v[i]);
        if (v[i] != N_)
            m.a_maree = true;
    }
    return m;
}

// ---- une marée régulière -------------------------------------------------------------------------------------------------

// Deux pleines mers et une basse mer, sans palier : 17 heures.
constexpr int REGULIERE[] = {0, 100, 180, 220, 180, 100, 0, -100, -180, -220, -180, -100, 0, 100, 180, 220, 180};

constexpr int les_pleines_et_les_basses_mers_sont_trouvees_aux_bonnes_heures() {
    const Maree m = extraire_maree(niveaux(REGULIERE));
    VERIFIER(m.nb == 3 && m.notable && m.marnage_cm == 440);
    VERIFIER(m.extrema[0].pleine && m.extrema[0].instant_s == MINUIT + 3 * HEURE && m.extrema[0].niveau_cm == 220);
    VERIFIER(!m.extrema[1].pleine && m.extrema[1].instant_s == MINUIT + 9 * HEURE && m.extrema[1].niveau_cm == -220);
    VERIFIER(m.extrema[2].pleine && m.extrema[2].instant_s == MINUIT + 15 * HEURE && m.extrema[2].niveau_cm == 220);
    VERIFIER(m.serie_debut_s == MINUIT && m.serie_fin_s == MINUIT + 17 * HEURE); // dernière étiquette (16 h) + 1 h
    return 0;
}

constexpr int le_fuseau_de_la_reponse_ne_change_pas_les_instants_utc() {
    const Maree a = extraire_maree(niveaux(REGULIERE));
    const Maree b = extraire_maree(niveaux(REGULIERE, MINUIT, 7200));
    VERIFIER(a.nb == b.nb);
    for (std::size_t k = 0; k < a.nb; ++k)
        VERIFIER(a.extrema[k].instant_s == b.extrema[k].instant_s);
    return 0;
}

constexpr int prochaine_pleine_et_prochaine_basse_mer() {
    const Maree m = extraire_maree(niveaux(REGULIERE));
    const Extremum *p = prochain_extremum(m, true, MINUIT);
    VERIFIER(p != nullptr && p->instant_s == MINUIT + 3 * HEURE);
    p = prochain_extremum(m, true, MINUIT + 3 * HEURE); // à l'instant même : compte (≥)
    VERIFIER(p != nullptr && p->instant_s == MINUIT + 3 * HEURE);
    p = prochain_extremum(m, true, MINUIT + 3 * HEURE + 1); // une seconde après : la suivante
    VERIFIER(p != nullptr && p->instant_s == MINUIT + 15 * HEURE);
    p = prochain_extremum(m, false, MINUIT + 4 * HEURE);
    VERIFIER(p != nullptr && p->instant_s == MINUIT + 9 * HEURE && !p->pleine);
    VERIFIER(prochain_extremum(m, false, MINUIT + 9 * HEURE + 1) == nullptr); // plus de basse mer dans la série
    VERIFIER(prochain_extremum(m, true, MINUIT + 15 * HEURE + 1) == nullptr);
    return 0;
}

constexpr int la_tendance_suit_le_prochain_extremum() {
    const Maree m = extraire_maree(niveaux(REGULIERE));
    VERIFIER(tendance_a(m, MINUIT) == Tendance::Montante); // début de la série : la pleine mer de 3 h vient
    VERIFIER(tendance_a(m, MINUIT + 1 * HEURE) == Tendance::Montante);
    VERIFIER(tendance_a(m, MINUIT + 5 * HEURE) == Tendance::Descendante); // entre la pleine de 3 h et la basse de 9 h
    VERIFIER(tendance_a(m, MINUIT + 12 * HEURE) == Tendance::Montante);   // entre la basse de 9 h et la pleine de 15 h
    VERIFIER(tendance_a(m, MINUIT + 16 * HEURE) ==
             Tendance::Descendante); // après le dernier extremum : l'inverse de celui-ci
    VERIFIER(tendance_a(m, MINUIT + 17 * HEURE - 1) == Tendance::Descendante);
    return 0;
}

constexpr int etale_a_moins_d_une_demi_heure_d_un_extremum_seuil_inclus() {
    const Maree m = extraire_maree(niveaux(REGULIERE));
    const std::int64_t pleine = MINUIT + 3 * HEURE;
    VERIFIER(MAREE_ETALE_S == 1800);
    VERIFIER(tendance_a(m, pleine) == Tendance::Etale);
    VERIFIER(tendance_a(m, pleine - 1800) == Tendance::Etale);
    VERIFIER(tendance_a(m, pleine - 1801) == Tendance::Montante);
    VERIFIER(tendance_a(m, pleine + 1800) == Tendance::Etale);
    VERIFIER(tendance_a(m, pleine + 1801) == Tendance::Descendante);
    const std::int64_t basse = MINUIT + 9 * HEURE;
    VERIFIER(tendance_a(m, basse + 1800) == Tendance::Etale);
    VERIFIER(tendance_a(m, basse + 1801) == Tendance::Montante);
    return 0;
}

constexpr int hors_de_la_serie_la_tendance_est_inconnue() {
    const Maree m = extraire_maree(niveaux(REGULIERE));
    VERIFIER(tendance_a(m, MINUIT - 1) == Tendance::Inconnue);
    VERIFIER(tendance_a(m, MINUIT) != Tendance::Inconnue);                  // première seconde couverte
    VERIFIER(tendance_a(m, MINUIT + 17 * HEURE - 1) != Tendance::Inconnue); // dernière seconde couverte
    VERIFIER(tendance_a(m, MINUIT + 17 * HEURE) == Tendance::Inconnue);     // la série couvre jusqu'à 17 h exclu
    VERIFIER(tendance_a(m, MINUIT + 100 * HEURE) == Tendance::Inconnue);
    return 0;
}

// ---- paliers -------------------------------------------------------------------------------------------------------------

constexpr int un_palier_est_un_seul_extremum_a_l_heure_du_milieu() {
    // Pleine mer sur deux heures (2 h et 3 h), basse mer sur trois heures (6 h, 7 h, 8 h).
    constexpr int V[] = {0, 100, 200, 200, 100, 0, -100, -100, -100, 0, 100};
    const Maree m = extraire_maree(niveaux(V));
    VERIFIER(m.nb == 2 && m.notable && m.marnage_cm == 300);
    VERIFIER(m.extrema[0].pleine && m.extrema[0].instant_s == MINUIT + 9000); // 2 h 30
    VERIFIER(!m.extrema[1].pleine && m.extrema[1].instant_s == MINUIT + 7 * HEURE);
    return 0;
}

constexpr int un_palier_au_bord_de_la_serie_n_est_pas_un_extremum() {
    // Il manque un voisin de l'autre côté : on ne sait pas si la mer monte encore.
    constexpr int V[] = {100, 100, 50, -100, 50, 100, 100};
    const Maree m = extraire_maree(niveaux(V));
    VERIFIER(m.nb == 1 && !m.extrema[0].pleine && m.extrema[0].instant_s == MINUIT + 3 * HEURE);
    VERIFIER(!m.notable && m.marnage_cm == 0);
    // et l'inverse : une basse mer étalée aux deux bords, une seule pleine mer entre les deux
    constexpr int W[] = {-100, -100, 50, 100, 50, -100, -100};
    const Maree n = extraire_maree(niveaux(W));
    VERIFIER(n.nb == 1 && n.extrema[0].pleine && n.extrema[0].instant_s == MINUIT + 3 * HEURE && !n.notable);
    return 0;
}

constexpr int la_premiere_et_la_derniere_heure_ne_sont_jamais_un_extremum() {
    constexpr int V[] = {300, 100, -100, 100, 300};
    const Maree m = extraire_maree(niveaux(V));
    VERIFIER(m.nb == 1 && !m.extrema[0].pleine && m.extrema[0].niveau_cm == -100);
    VERIFIER(!m.notable);
    return 0;
}

constexpr int une_mer_plate_ou_monotone_n_a_pas_d_extremum() {
    constexpr int PLAT[] = {50, 50, 50, 50, 50};
    constexpr int MONTE[] = {0, 10, 20, 30, 40, 50};
    VERIFIER(extraire_maree(niveaux(PLAT)).nb == 0);
    VERIFIER(extraire_maree(niveaux(MONTE)).nb == 0);
    VERIFIER(!extraire_maree(niveaux(MONTE)).notable);
    return 0;
}

// ---- données absentes ----------------------------------------------------------------------------------------------------

constexpr int une_valeur_absente_coupe_la_serie() {
    // La pleine mer de 2 h n'a pas de voisin lisible après elle ; la basse mer de 7 h est entière.
    constexpr int V[] = {0, 100, 200, N_, 100, 0, -100, -200, -100, 0};
    const Maree m = extraire_maree(niveaux(V));
    VERIFIER(m.nb == 1 && !m.extrema[0].pleine && m.extrema[0].niveau_cm == -200 && !m.notable);
    // Un trou au sommet : ni l'heure absente ni ses voisines ne deviennent un extremum.
    constexpr int W[] = {0, 100, N_, 100, 0};
    VERIFIER(extraire_maree(niveaux(W)).nb == 0);
    // Un trou au début : la première heure lisible n'a pas de voisin avant elle.
    constexpr int X[] = {N_, 100, 50, 100, N_};
    VERIFIER(extraire_maree(niveaux(X)).nb == 1); // seule la basse mer de 2 h a deux voisins lisibles
    return 0;
}

constexpr int un_trou_n_invente_pas_la_basse_mer_cachee_dedans() {
    // Deux pleines mers (1 h et 5 h) de part et d'autre d'un trou : la basse mer entre elles n'est pas lisible, donc pas inventée ;
    // les deux pleines mers, de même type et sans basse mer entre elles, se fondent en la plus haute.
    constexpr int V[] = {0, 200, 0, N_, 0, 210, 0};
    const Maree m = extraire_maree(niveaux(V));
    VERIFIER(m.nb == 1 && m.extrema[0].pleine && m.extrema[0].niveau_cm == 210);
    VERIFIER(m.extrema[0].instant_s == MINUIT + 5 * HEURE && !m.notable);
    return 0;
}

constexpr int sans_niveau_de_mer_rien_n_est_dit() {
    const Maree m = extraire_maree(mer_vide(54));
    VERIFIER(m.nb == 0 && !m.notable && m.marnage_cm == 0);
    VERIFIER(m.serie_debut_s == MINUIT && m.serie_fin_s == MINUIT + 54 * HEURE); // la série existe, la marée non
    VERIFIER(tendance_a(m, MINUIT + 3 * HEURE) == Tendance::Inconnue);
    VERIFIER(prochain_extremum(m, true, MINUIT) == nullptr);
    return 0;
}

constexpr int moins_de_trois_heures_ne_disent_rien() {
    constexpr int V[] = {0, 100};
    const Maree m = extraire_maree(niveaux(V));
    VERIFIER(m.nb == 0 && !m.notable);
    VERIFIER(m.serie_debut_s == MINUIT && m.serie_fin_s == MINUIT + 2 * HEURE);
    const Maree vide = extraire_maree(Marine{});
    VERIFIER(vide.nb == 0 && !vide.notable && vide.serie_debut_s == 0 && vide.serie_fin_s == 0);
    return 0;
}

// ---- le filtre en zigzag -------------------------------------------------------------------------------------------------

constexpr int un_petit_accident_de_la_courbe_n_est_pas_une_maree() {
    // 200 puis 190 puis 195 : deux accidents de moins de 30 cm, la pleine mer de 2 h reste seule.
    constexpr int V[] = {0, 100, 200, 190, 195, 100, 0, -100, -200, -100, 0};
    const Maree m = extraire_maree(niveaux(V));
    VERIFIER(m.nb == 2 && m.notable && m.marnage_cm == 400);
    VERIFIER(m.extrema[0].pleine && m.extrema[0].instant_s == MINUIT + 2 * HEURE && m.extrema[0].niveau_cm == 200);
    VERIFIER(!m.extrema[1].pleine && m.extrema[1].niveau_cm == -200);
    return 0;
}

constexpr int deux_pleines_mers_separees_par_un_accident_gardent_la_plus_haute() {
    constexpr int V[] = {0, 100, 200, 190, 205, 100, 0, -100, -200, -100, 0};
    const Maree m = extraire_maree(niveaux(V));
    VERIFIER(m.nb == 2 && m.extrema[0].pleine && m.extrema[0].niveau_cm == 205);
    VERIFIER(m.extrema[0].instant_s == MINUIT + 4 * HEURE); // l'heure de la plus haute, pas de la première
    // l'inverse pour deux basses mers
    constexpr int W[] = {0, -100, -200, -190, -205, -100, 0, 100, 200, 100, 0};
    const Maree n = extraire_maree(niveaux(W));
    VERIFIER(n.nb == 2 && !n.extrema[0].pleine && n.extrema[0].niveau_cm == -205);
    VERIFIER(n.extrema[0].instant_s == MINUIT + 4 * HEURE);
    return 0;
}

constexpr int le_seuil_de_30_cm_est_inclus_29_cm_est_ecarte() {
    VERIFIER(MAREE_AMPLITUDE_MIN_CM == 30);
    // 100 -> 70 -> 100 : un creux de 30 cm est une basse mer ; de 29 cm, un accident.
    constexpr int A30[] = {0, 50, 100, 70, 100, 50, 0, -100, 0};
    constexpr int A29[] = {0, 50, 100, 71, 100, 50, 0, -100, 0};
    const Maree a = extraire_maree(niveaux(A30));
    VERIFIER(a.nb == 4 && a.extrema[1].niveau_cm == 70 && !a.extrema[1].pleine);
    const Maree b = extraire_maree(niveaux(A29));
    VERIFIER(b.nb == 2 && b.extrema[0].instant_s == MINUIT + 2 * HEURE && b.extrema[1].niveau_cm == -100);
    // Deux extrema à 30 cm exactement font une marée notable ; à 29 cm non.
    constexpr int N30[] = {20, 10, 0, 10, 30, 10, 20};
    constexpr int N29[] = {20, 10, 0, 10, 29, 10, 20};
    VERIFIER(extraire_maree(niveaux(N30)).notable && extraire_maree(niveaux(N30)).marnage_cm == 30);
    VERIFIER(!extraire_maree(niveaux(N29)).notable && extraire_maree(niveaux(N29)).nb == 1);
    return 0;
}

constexpr int une_mer_sans_maree_reste_non_notable() {
    // Le profil des mers fermées de nos mesures : une ondulation de 20 à 27 cm sur 54 h.
    constexpr int V[] = {0, 8, 15, 22, 15, 8, 3, 0, 5, 12, 20, 27, 20, 11, 4, 0, 6, 14, 21, 14, 8, 3, 1, 8, 15};
    const Maree m = extraire_maree(niveaux(V));
    VERIFIER(!m.notable && m.marnage_cm == 0);
    VERIFIER(prochain_extremum(m, true, MINUIT) == nullptr);
    VERIFIER(tendance_a(m, MINUIT + 3 * HEURE) == Tendance::Inconnue);
    return 0;
}

// ---- capacité ------------------------------------------------------------------------------------------------------------

constexpr int la_liste_est_plafonnee_a_seize_extrema_sans_deborder() {
    VERIFIER(MAREE_EXTREMA_MAX == 16);
    int v[54] = {};
    for (int i = 0; i < 54; ++i)
        v[i] = (i % 2 == 0) ? 0 : 100; // un extremum à chaque heure intérieure : 52 bruts
    const Maree m = extraire_maree(niveaux(v));
    VERIFIER(m.nb == MAREE_EXTREMA_MAX && m.notable && m.marnage_cm == 100);
    VERIFIER(m.extrema[0].instant_s == MINUIT + 1 * HEURE && m.extrema[0].pleine);
    VERIFIER(m.extrema[15].instant_s == MINUIT + 16 * HEURE &&
             !m.extrema[15].pleine); // les seize premiers, dans l'ordre
    return 0;
}

constexpr int une_reponse_de_60_heures_est_lue_en_entier() {
    int v[60] = {};
    for (int i = 0; i < 60; ++i)
        v[i] = (i < 30) ? i * 10 : (59 - i) * 10; // monte puis descend : 290 aux heures 29 et 30, une seule pleine mer
    const Maree m = extraire_maree(niveaux(v));
    VERIFIER(m.serie_fin_s == MINUIT + 60 * HEURE);
    VERIFIER(m.nb == 1 && m.extrema[0].pleine && m.extrema[0].niveau_cm == 290);
    VERIFIER(m.extrema[0].instant_s == MINUIT + 29 * HEURE + 1800); // le milieu du palier de 29 h à 30 h
    VERIFIER(!m.notable); // une pleine mer sans basse mer ne fait pas une marée notable
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(les_pleines_et_les_basses_mers_sont_trouvees_aux_bonnes_heures)                                                  \
    X(le_fuseau_de_la_reponse_ne_change_pas_les_instants_utc)                                                          \
    X(prochaine_pleine_et_prochaine_basse_mer)                                                                         \
    X(la_tendance_suit_le_prochain_extremum)                                                                           \
    X(etale_a_moins_d_une_demi_heure_d_un_extremum_seuil_inclus)                                                       \
    X(hors_de_la_serie_la_tendance_est_inconnue)                                                                       \
    X(un_palier_est_un_seul_extremum_a_l_heure_du_milieu)                                                              \
    X(un_palier_au_bord_de_la_serie_n_est_pas_un_extremum)                                                             \
    X(la_premiere_et_la_derniere_heure_ne_sont_jamais_un_extremum)                                                     \
    X(une_mer_plate_ou_monotone_n_a_pas_d_extremum)                                                                    \
    X(une_valeur_absente_coupe_la_serie)                                                                               \
    X(un_trou_n_invente_pas_la_basse_mer_cachee_dedans)                                                                \
    X(sans_niveau_de_mer_rien_n_est_dit)                                                                               \
    X(moins_de_trois_heures_ne_disent_rien)                                                                            \
    X(un_petit_accident_de_la_courbe_n_est_pas_une_maree)                                                              \
    X(deux_pleines_mers_separees_par_un_accident_gardent_la_plus_haute)                                                \
    X(le_seuil_de_30_cm_est_inclus_29_cm_est_ecarte)                                                                   \
    X(une_mer_sans_maree_reste_non_notable)                                                                            \
    X(la_liste_est_plafonnee_a_seize_extrema_sans_deborder)                                                            \
    X(une_reponse_de_60_heures_est_lue_en_entier)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
