// Tests du noyau pur « mer » : ce qu'on lit de la mer à un instant donné (mer_vue.hpp) — l'heure des bandes, l'âge, la meilleure activité,
// la prochaine marée, la hauteur de la mer entre deux marées, le coefficient.
//
// Les réponses attendues sur les six VRAIES trames sont calculées par Python seul (donnees/attendu_mer_<spot>.inc, controle_python_mer.py),
// pour une trentaine d'instants pris autour des bornes (le premier instant, l'âge de 600 s et de 6 h, la fin des 24 heures, chaque marée).
// La hauteur de la mer est comparée à un calcul À VIRGULE (`math.cos`) : le noyau, entier, doit en rester à moins de 8 mm. Les cas limites
// (frontières, égalités, ordre du temps, valeur inconnue) sont des mers construites à la main.
//
// Chaque test est évalué à la compilation (static_assert) ET à l'exécution (voir mini.hpp).
#include <cstddef>
#include <cstdint>

#include "ecran/mer_trame.hpp"
#include "ecran/mer_vue.hpp"
#include "mer_donnees.hpp"
#include "mini.hpp"

using namespace mer_test;

namespace {

constexpr long long valeur_absolue(long long v) noexcept { return v < 0 ? -v : v; }

constexpr Mer mer_de_base() {
    Mer m{};
    m.valide = true;
    m.genere_s = 1000000;
    m.premiere_heure_s = 996400;
    return m;
}

constexpr Maree maree(bool haute, std::int64_t instant, std::int32_t cm) {
    Maree e{};
    e.haute = haute;
    e.instant_s = instant;
    e.hauteur_cm = cm;
    return e;
}

constexpr void avec_verdict(Mer &m, Activite a, Verdict v, bool presente = true, bool disponible = true) {
    Prevision &p = m.activites[static_cast<int>(a)];
    p.presente = presente;
    p.disponible = disponible;
    p.verdict = v;
}

// ---- les six trames réelles -----------------------------------------------------------------------------------------------------
constexpr int les_vues_des_trames_reelles_sont_celles_que_python_a_calculees() {
    for (int s = 0; s < NB_SPOTS; ++s) {
        const Att &a = SPOTS[s].attendu;
        Mer m{};
        VERIFIER(decoder(SPOTS[s].trame, m));
        VERIFIER(a.nb_points > 20);
        for (int i = 0; i < a.nb_points; ++i) {
            const AttPoint &p = a.points[i];
            VERIFIER(indice_heure(m, p.maintenant) == p.heure);
            VERIFIER(static_cast<int>(fraicheur(m, p.maintenant)) == p.fraicheur);
            VERIFIER(meilleure_activite(m) == p.meilleure);
            VERIFIER(prochaine_maree(m, p.maintenant) == p.prochaine_maree);
            VERIFIER(coefficient_pour(m, p.maintenant) == p.coefficient);
        }
    }
    return 0;
}

constexpr int la_hauteur_de_la_mer_reste_a_moins_de_8_mm_du_calcul_a_virgule() {
    for (int s = 0; s < NB_SPOTS; ++s) {
        const Att &a = SPOTS[s].attendu;
        Mer m{};
        VERIFIER(decoder(SPOTS[s].trame, m));
        VERIFIER(a.nb_hauteurs >= 30);
        for (int i = 0; i < a.nb_hauteurs; ++i) {
            std::int32_t cm = INCONNU;
            VERIFIER(hauteur_maree_cm(m, a.hauteurs[i].instant, cm));
            VERIFIER(valeur_absolue(static_cast<long long>(cm) * 1000 - a.hauteurs[i].milli_cm) < 800);
        }
    }
    return 0;
}

constexpr int avant_la_premiere_maree_et_apres_la_derniere_il_n_y_a_pas_de_hauteur() {
    for (int s = 0; s < NB_SPOTS; ++s) {
        const Att &a = SPOTS[s].attendu;
        Mer m{};
        VERIFIER(decoder(SPOTS[s].trame, m));
        VERIFIER(a.nb_hors == 2);
        for (int i = 0; i < a.nb_hors; ++i) {
            std::int32_t cm = 12345;
            VERIFIER(!hauteur_maree_cm(m, a.hors[i], cm));
            VERIFIER(cm == 12345); // la sortie n'a pas bougé
        }
    }
    return 0;
}

constexpr int la_mer_monte_sans_redescendre_de_la_basse_a_la_pleine_mer() {
    Mer m{};
    VERIFIER(decoder(HOSSEGOR, m) && m.nb_marees >= 3);
    for (int k = 0; k + 1 < m.nb_marees; ++k) {
        const bool monte = m.marees[k + 1].haute;
        std::int32_t avant = m.marees[k].hauteur_cm;
        for (std::int64_t t = m.marees[k].instant_s; t <= m.marees[k + 1].instant_s; t += 300) {
            std::int32_t cm = INCONNU;
            VERIFIER(hauteur_maree_cm(m, t, cm));
            VERIFIER(monte ? cm >= avant : cm <= avant);
            avant = cm;
        }
        std::int32_t fin = INCONNU;
        VERIFIER(hauteur_maree_cm(m, m.marees[k + 1].instant_s, fin) && fin == m.marees[k + 1].hauteur_cm);
    }
    return 0;
}

constexpr int la_prochaine_maree_d_une_trame_reelle_est_la_premiere_qui_n_est_pas_passee() {
    Mer m{};
    VERIFIER(decoder(HOSSEGOR, m) && m.nb_marees == 6);
    VERIFIER(prochaine_maree(m, 1794143738) == 1); // L:…126780 est passée, H:…149460 est la suivante
    VERIFIER(m.marees[1].haute);
    VERIFIER(prochaine_maree(m, 1794149460) == 1);  // à l'instant même : elle compte
    VERIFIER(prochaine_maree(m, 1794149461) == 2);  // une seconde après : la suivante
    VERIFIER(prochaine_maree(m, 1794238020) == 5);  // la dernière
    VERIFIER(prochaine_maree(m, 1794238021) == -1); // plus aucune
    return 0;
}

// ---- l'heure des bandes ---------------------------------------------------------------------------------------------------------
constexpr int l_heure_courante_va_de_0_a_23_et_sort_des_bandes_aux_bornes() {
    Mer m = mer_de_base();
    const std::int64_t p = m.premiere_heure_s;
    VERIFIER(indice_heure(m, p - 1) == -1 && indice_heure(m, 0) == -1);
    VERIFIER(indice_heure(m, p) == 0 && indice_heure(m, p + 3599) == 0);
    VERIFIER(indice_heure(m, p + 3600) == 1 && indice_heure(m, p + 7199) == 1);
    VERIFIER(indice_heure(m, p + 23 * 3600) == 23 && indice_heure(m, p + 24 * 3600 - 1) == 23);
    VERIFIER(indice_heure(m, p + 24 * 3600) == -1 && indice_heure(m, p + 1000 * 3600) == -1);
    m.valide = false;
    VERIFIER(indice_heure(m, p) == -1);
    m.valide = true;
    m.premiere_heure_s = 0; // aucune activité n'a d'heure
    VERIFIER(indice_heure(m, 5) == -1 && indice_heure(m, 0) == -1);
    return 0;
}

// ---- l'âge ----------------------------------------------------------------------------------------------------------------------
constexpr int l_age_de_la_mer_passe_de_fraiche_a_vieillie_a_expiree_aux_bornes() {
    Mer m = mer_de_base();
    m.premiere_heure_s = 0; // pas de bandes : seule la date de la trame compte
    const std::int64_t g = m.genere_s;
    VERIFIER(fraicheur(m, g) == Fraicheur::Fraiche);
    VERIFIER(fraicheur(m, g - 50) == Fraicheur::Fraiche); // une horloge en retard : l'âge ne devient pas négatif
    VERIFIER(fraicheur(m, g + 600) == Fraicheur::Fraiche);
    VERIFIER(fraicheur(m, g + 601) == Fraicheur::Vieillie);
    VERIFIER(fraicheur(m, g + 21600) == Fraicheur::Vieillie);
    VERIFIER(fraicheur(m, g + 21601) == Fraicheur::Expiree);
    return 0;
}

constexpr int sans_mer_ou_sans_horloge_rien_n_est_affirme_frais() {
    Mer m = mer_de_base();
    VERIFIER(fraicheur(m, 0) == Fraicheur::Vieillie); // heure non réglée : l'âge est inconnu
    m.valide = false;
    VERIFIER(fraicheur(m, 1000000) == Fraicheur::Rien && fraicheur(m, 0) == Fraicheur::Rien);
    return 0;
}

constexpr int une_mer_gardee_par_l_api_est_vieillie_meme_toute_neuve() {
    Mer m = mer_de_base();
    m.donnees_gardees = true;
    VERIFIER(fraicheur(m, m.genere_s) == Fraicheur::Vieillie);
    m.genere_s = m.premiere_heure_s + 1;
    VERIFIER(fraicheur(m, m.genere_s + 100000) == Fraicheur::Expiree); // l'expiration passe devant
    return 0;
}

constexpr int les_24_heures_passees_la_mer_est_expiree_meme_si_elle_est_jeune() {
    Mer m = mer_de_base();
    m.genere_s = m.premiere_heure_s + 23 * 3600; // une trame qui date d'il y a un instant, dans la dernière heure
    const std::int64_t fin = m.premiere_heure_s + 24 * 3600;
    VERIFIER(fraicheur(m, fin - 1) == Fraicheur::Vieillie); // 3599 s : plus de 600 s mais encore dans les bandes
    VERIFIER(fraicheur(m, m.genere_s) == Fraicheur::Fraiche);
    VERIFIER(fraicheur(m, fin) == Fraicheur::Expiree);
    return 0;
}

// ---- la meilleure activité ------------------------------------------------------------------------------------------------------
constexpr int la_meilleure_activite_est_celle_du_meilleur_verdict_la_premiere_en_cas_d_egalite() {
    Mer m = mer_de_base();
    VERIFIER(meilleure_activite(m) == -1); // aucune activité reçue
    avec_verdict(m, Activite::Surf, Verdict::Inconnu);
    VERIFIER(meilleure_activite(m) == -1); // un verdict inconnu n'est pas un verdict
    avec_verdict(m, Activite::Kite, Verdict::Deconseille);
    VERIFIER(meilleure_activite(m) == 1); // « déconseillé » est un verdict : il passe devant « inconnu »
    avec_verdict(m, Activite::Bateau, Verdict::Possible);
    VERIFIER(meilleure_activite(m) == 2);
    avec_verdict(m, Activite::PecheAPied, Verdict::Possible);
    VERIFIER(meilleure_activite(m) == 2); // égalité : la première reste
    avec_verdict(m, Activite::Surf, Verdict::Bon);
    VERIFIER(meilleure_activite(m) == 0);
    avec_verdict(m, Activite::PecheAPied, Verdict::Bon);
    VERIFIER(meilleure_activite(m) == 0); // égalité à « bon » : le surf reste
    return 0;
}

constexpr int une_activite_absente_ou_indisponible_ne_peut_pas_etre_la_meilleure() {
    Mer m = mer_de_base();
    avec_verdict(m, Activite::Surf, Verdict::Bon, /*presente=*/false);
    avec_verdict(m, Activite::Kite, Verdict::Bon, /*presente=*/true, /*disponible=*/false);
    VERIFIER(meilleure_activite(m) == -1);
    avec_verdict(m, Activite::Bateau, Verdict::Deconseille);
    VERIFIER(meilleure_activite(m) == 2);
    return 0;
}

// ---- le coefficient -------------------------------------------------------------------------------------------------------------
constexpr int le_coefficient_est_celui_de_la_premiere_pleine_mer_a_venir() {
    Mer m = mer_de_base();
    m.nb_coefficients = 3;
    m.coefficients[0] = {1000, 80};
    m.coefficients[1] = {2000, 0}; // une valeur nulle n'est pas un coefficient
    m.coefficients[2] = {3000, 90};
    VERIFIER(coefficient_pour(m, 0) == 0 && coefficient_pour(m, 1000) == 0);
    VERIFIER(coefficient_pour(m, 1001) == 2);
    VERIFIER(coefficient_pour(m, 3000) == 2);
    VERIFIER(coefficient_pour(m, 3001) == -1);
    m.nb_coefficients = 0;
    VERIFIER(coefficient_pour(m, 0) == -1);
    return 0;
}

// ---- la hauteur, cas limites ----------------------------------------------------------------------------------------------------
constexpr Mer deux_marees(std::int32_t bas_cm, std::int32_t haut_cm) {
    Mer m = mer_de_base();
    m.nb_marees = 2;
    m.marees[0] = maree(false, 10000, bas_cm);
    m.marees[1] = maree(true, 10000 + 21600, haut_cm);
    return m;
}

constexpr int la_hauteur_aux_extremes_au_milieu_et_au_quart() {
    const Mer m = deux_marees(-200, 300);
    std::int32_t cm = INCONNU;
    VERIFIER(hauteur_maree_cm(m, 10000, cm) && cm == -200);
    VERIFIER(hauteur_maree_cm(m, 31600, cm) && cm == 300);
    VERIFIER(hauteur_maree_cm(m, 10000 + 10800, cm) && cm == 50); // à mi-temps : la moyenne exacte (cos 90° = 0)
    VERIFIER(hauteur_maree_cm(m, 10000 + 5400, cm) &&
             cm == -127); // au quart : -200 + 500 x (1 - 0,707) / 2 = -126,8 -> -127
    VERIFIER(hauteur_maree_cm(m, 10000 + 16200, cm) &&
             cm == 227); // aux trois quarts : -200 + 500 x (1 + 0,707) / 2 = 226,8 -> 227
    VERIFIER(hauteur_maree_cm(m, 10000 + 5430, cm) && cm >= -127 && cm <= -125); // entre deux degrés : interpolé
    return 0;
}

constexpr int une_mer_qui_descend_et_des_hauteurs_negatives_sont_arrondies_loin_de_zero() {
    Mer m = deux_marees(300, -200); // pleine mer puis basse mer
    std::int32_t cm = INCONNU;
    VERIFIER(hauteur_maree_cm(m, 10000 + 10800, cm) && cm == 50);
    VERIFIER(hauteur_maree_cm(m, 10000 + 5400, cm) && cm == 227); // 300 - 500 x 0,1465 = 226,8
    VERIFIER(hauteur_maree_cm(m, 10000 + 16200, cm) && cm == -127);
    return 0;
}

constexpr int la_courbe_se_raccorde_au_passage_d_une_maree_a_la_suivante() {
    Mer m = deux_marees(-200, 300);
    m.nb_marees = 3;
    m.marees[2] = maree(false, 31600 + 21000, -250);
    std::int32_t cm = INCONNU;
    VERIFIER(hauteur_maree_cm(m, 31600, cm) && cm == 300); // la pleine mer appartient aux deux paires
    VERIFIER(hauteur_maree_cm(m, 31601, cm) && cm >= 299 && cm <= 300);
    VERIFIER(hauteur_maree_cm(m, 52600, cm) && cm == -250);
    VERIFIER(!hauteur_maree_cm(m, 52601, cm));
    VERIFIER(!hauteur_maree_cm(m, 9999, cm));
    return 0;
}

constexpr int on_ne_dessine_pas_une_courbe_dont_les_marees_ne_sont_pas_dans_l_ordre_du_temps() {
    std::int32_t cm = 777;
    Mer m = deux_marees(-200, 300);
    m.marees[1].instant_s = m.marees[0].instant_s; // la seconde n'est pas plus tard que la première
    VERIFIER(!hauteur_maree_cm(m, m.marees[0].instant_s, cm) && cm == 777);
    m = deux_marees(-200, 300);
    m.marees[1].instant_s = m.marees[0].instant_s - 1;
    VERIFIER(!hauteur_maree_cm(m, m.marees[0].instant_s, cm) && cm == 777);
    m = deux_marees(-200, 300);
    m.marees[0].instant_s = 0; // un instant inconnu
    VERIFIER(!hauteur_maree_cm(m, 5, cm) && cm == 777);
    // une paire hors de l'ordre PLUS LOIN que l'instant cherché ne l'empêche pas ; une paire hors de l'ordre AVANT, si
    m = deux_marees(-200, 300);
    m.nb_marees = 3;
    m.marees[2] = maree(false, 31600 - 5, -250); // avant la seconde
    VERIFIER(hauteur_maree_cm(m, 10000 + 10800, cm) && cm == 50);
    VERIFIER(!hauteur_maree_cm(m, 31600 + 1, cm));
    return 0;
}

constexpr int une_hauteur_inconnue_ou_une_seule_maree_ne_donne_pas_de_courbe() {
    std::int32_t cm = 777;
    Mer m = deux_marees(-200, 300);
    m.marees[1].hauteur_cm = INCONNU;
    VERIFIER(!hauteur_maree_cm(m, 10000 + 10800, cm) && cm == 777);
    m = deux_marees(-200, 300);
    m.marees[0].hauteur_cm = INCONNU;
    VERIFIER(!hauteur_maree_cm(m, 10000 + 10800, cm) && cm == 777);
    m = deux_marees(-200, 300);
    m.nb_marees = 1;
    VERIFIER(!hauteur_maree_cm(m, 10000, cm) && cm == 777);
    m.nb_marees = 0;
    VERIFIER(!hauteur_maree_cm(m, 10000, cm) && cm == 777);
    return 0;
}

constexpr int une_tres_grande_amplitude_ne_deborde_pas() {
    const Mer m = deux_marees(-999999, 999999); // 20 km : bien au-delà de toute marée
    std::int32_t cm = INCONNU;
    VERIFIER(hauteur_maree_cm(m, 10000 + 10800, cm) && cm == 0);
    VERIFIER(hauteur_maree_cm(m, 10000, cm) && cm == -999999);
    VERIFIER(hauteur_maree_cm(m, 31600, cm) && cm == 999999);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(les_vues_des_trames_reelles_sont_celles_que_python_a_calculees)                                                  \
    X(la_hauteur_de_la_mer_reste_a_moins_de_8_mm_du_calcul_a_virgule)                                                  \
    X(avant_la_premiere_maree_et_apres_la_derniere_il_n_y_a_pas_de_hauteur)                                            \
    X(la_mer_monte_sans_redescendre_de_la_basse_a_la_pleine_mer)                                                       \
    X(la_prochaine_maree_d_une_trame_reelle_est_la_premiere_qui_n_est_pas_passee)                                      \
    X(l_heure_courante_va_de_0_a_23_et_sort_des_bandes_aux_bornes)                                                     \
    X(l_age_de_la_mer_passe_de_fraiche_a_vieillie_a_expiree_aux_bornes)                                                \
    X(sans_mer_ou_sans_horloge_rien_n_est_affirme_frais)                                                               \
    X(une_mer_gardee_par_l_api_est_vieillie_meme_toute_neuve)                                                          \
    X(les_24_heures_passees_la_mer_est_expiree_meme_si_elle_est_jeune)                                                 \
    X(la_meilleure_activite_est_celle_du_meilleur_verdict_la_premiere_en_cas_d_egalite)                                \
    X(une_activite_absente_ou_indisponible_ne_peut_pas_etre_la_meilleure)                                              \
    X(le_coefficient_est_celui_de_la_premiere_pleine_mer_a_venir)                                                      \
    X(la_hauteur_aux_extremes_au_milieu_et_au_quart)                                                                   \
    X(une_mer_qui_descend_et_des_hauteurs_negatives_sont_arrondies_loin_de_zero)                                       \
    X(la_courbe_se_raccorde_au_passage_d_une_maree_a_la_suivante)                                                      \
    X(on_ne_dessine_pas_une_courbe_dont_les_marees_ne_sont_pas_dans_l_ordre_du_temps)                                  \
    X(une_hauteur_inconnue_ou_une_seule_maree_ne_donne_pas_de_courbe)                                                  \
    X(une_tres_grande_amplitude_ne_deborde_pas)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
