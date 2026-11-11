// Tests du noyau pur : l'ordonnanceur de la tâche réseau unique des données (ordonnanceur_reseau.hpp).
// Les délais des scénarios sont ceux des clients de l'appareil : météo 1,5 s d'anti-rebond et 2 s d'attente du réseau, Vigie et mer
// 5 s de sondage des réglages, essai de la liste des spots demandé à l'écran.
#include "ecran/ordonnanceur_reseau.hpp"
#include "mini.hpp"

using namespace ecran::ordonnanceur;

namespace {

constexpr int M = indice(Source::Meteo);
constexpr int V = indice(Source::Vigie);
constexpr int MER = indice(Source::Mer);
constexpr int L = indice(Source::ListeSpots);

// ---- réveil -----------------------------------------------------------------------------------------------------------------

constexpr int rien_ne_passe_sans_reveil() {
    Ordonnanceur o;
    VERIFIER(o.choisir(0) == kAucune);
    VERIFIER(o.choisir(1000000000) == kAucune);
    VERIFIER(o.attente_ms(0, 60000) == 60000); // rien de prévu : la tâche dort jusqu'au plafond
    return 0;
}

constexpr int reveil_sans_anti_rebond_passe_tout_de_suite() {
    Ordonnanceur o;
    o.reveiller(Source::Vigie, 1000);
    VERIFIER(o.choisir(1000) == V);
    VERIFIER(o.attente_ms(1000, 60000) == 0);
    return 0;
}

constexpr int reveil_meteo_attend_l_anti_rebond() {
    Ordonnanceur o;
    o.reveiller(Source::Meteo, 1000);
    VERIFIER(o.choisir(1000) == kAucune);
    VERIFIER(o.choisir(2499) == kAucune);
    VERIFIER(o.attente_ms(1000, 60000) == 1500);
    VERIFIER(o.choisir(2500) == M);
    return 0;
}

constexpr int rafale_de_reveils_ne_repousse_pas() {
    // Trois changements de zone en 1 s : une seule requête, 1,5 s après le PREMIER (avant : vTaskDelay(1500) puis effacement).
    Ordonnanceur o;
    o.reveiller(Source::Meteo, 1000);
    o.reveiller(Source::Meteo, 1500);
    o.reveiller(Source::Meteo, 2000);
    VERIFIER(o.echeance_ms[M] == 2500);
    VERIFIER(o.choisir(2500) == M);
    return 0;
}

constexpr int reveil_avance_un_pas_lointain() {
    // Météo prévue dans 15 min ; un changement de zone la fait passer 1,5 s plus tard.
    Ordonnanceur o;
    o.apres_pas(Source::Meteo, 0, 900000);
    o.reveiller(Source::Meteo, 10000);
    VERIFIER(o.echeance_ms[M] == 11500);
    return 0;
}

constexpr int reveil_n_eloigne_pas_un_pas_proche() {
    // Météo due dans 200 ms : un réveil (anti-rebond 1,5 s) ne la retarde pas.
    Ordonnanceur o;
    o.apres_pas(Source::Meteo, 0, 1200);
    o.reveiller(Source::Meteo, 1000);
    VERIFIER(o.echeance_ms[M] == 1200);
    return 0;
}

constexpr int reveil_signale_une_seule_fois() {
    Ordonnanceur o;
    o.reveiller(Source::Meteo, 0);
    VERIFIER(o.prendre_reveil(Source::Meteo));
    VERIFIER(!o.prendre_reveil(Source::Meteo));
    VERIFIER(!o.prendre_reveil(Source::Vigie)); // un réveil ne touche que sa source
    return 0;
}

constexpr int reveil_pendant_un_pas_est_garde() {
    // Le réveil arrive pendant le pas de la météo (la zone change pendant la requête) : la tâche le lit après le pas, il n'est pas perdu.
    Ordonnanceur o;
    o.reveiller(Source::Meteo, 0);
    VERIFIER(o.prendre_reveil(Source::Meteo));
    o.apres_pas(Source::Meteo, 5000, 900000);
    o.reveiller(Source::Meteo, 5000); // notification lue après le pas
    VERIFIER(o.echeance_ms[M] == 6500);
    VERIFIER(o.prendre_reveil(Source::Meteo));
    return 0;
}

// ---- choix ------------------------------------------------------------------------------------------------------------------

constexpr int seules_les_sources_dues_passent() {
    Ordonnanceur o;
    o.apres_pas(Source::Meteo, 0, 2000);
    o.apres_pas(Source::Vigie, 0, 5000);
    VERIFIER(o.choisir(1999) == kAucune);
    VERIFIER(o.choisir(2000) == M);
    VERIFIER(o.attente_ms(0, 60000) == 2000);
    return 0;
}

constexpr int la_plus_en_retard_passe_d_abord() {
    Ordonnanceur o;
    o.apres_pas(Source::Meteo, 0, 3000);
    o.apres_pas(Source::Vigie, 0, 1000);
    o.apres_pas(Source::Mer, 0, 2000);
    VERIFIER(o.choisir(5000) == V);
    o.apres_pas(Source::Vigie, 5000, 5000);
    VERIFIER(o.choisir(5000) == MER);
    o.apres_pas(Source::Mer, 5000, 5000);
    VERIFIER(o.choisir(5000) == M);
    return 0;
}

constexpr int egalite_dans_l_ordre_de_la_table() {
    Ordonnanceur o;
    o.apres_pas(Source::Mer, 0, 1000);
    o.apres_pas(Source::Vigie, 0, 1000);
    o.apres_pas(Source::Meteo, 0, 1000);
    VERIFIER(o.choisir(1000) == M);
    return 0;
}

constexpr int liste_des_spots_passe_avant_les_autres() {
    // Une personne attend la liste devant l'écran : elle passe dès la fin du pas en cours, avant la météo pourtant plus en retard.
    Ordonnanceur o;
    o.apres_pas(Source::Meteo, 0, 0);
    o.apres_pas(Source::Vigie, 0, 0);
    o.reveiller(Source::ListeSpots, 4000);
    VERIFIER(o.choisir(4000) == L);
    return 0;
}

constexpr int liste_pas_due_ne_bloque_rien() {
    Ordonnanceur o;
    o.apres_pas(Source::Meteo, 0, 0);
    VERIFIER(o.choisir(10) == M); // aucune demande de liste : la météo passe
    return 0;
}

constexpr int une_source_qui_rend_zero_n_affame_pas_les_autres() {
    // La mer rend 0 après un essai (« reboucle tout de suite ») : la météo, due avant, passe quand même la première.
    Ordonnanceur o;
    o.apres_pas(Source::Meteo, 0, 1000);
    o.apres_pas(Source::Mer, 1000, 0);
    VERIFIER(o.choisir(1000) == M);
    // Et dans l'autre sens : la mer due depuis 1000 passe avant la Vigie due depuis 1001.
    Ordonnanceur p;
    p.apres_pas(Source::Mer, 1000, 0);
    p.apres_pas(Source::Vigie, 1001, 0);
    VERIFIER(p.choisir(2000) == MER);
    return 0;
}

constexpr int tour_complet_sans_famine() {
    // Quatre sources toujours dues (chaque pas rend 0) : en quatre pas, chacune est passée une fois.
    Ordonnanceur o;
    std::int64_t t = 0;
    o.apres_pas(Source::Meteo, t, 0);
    o.apres_pas(Source::Vigie, t, 0);
    o.apres_pas(Source::Mer, t, 0);
    bool passee[kNombreSources] = {};
    for (int n = 0; n < 3; ++n) {
        const int s = o.choisir(t);
        if (s == kAucune) {
            return __LINE__;
        }
        passee[s] = true;
        t += 100; // chaque pas dure 100 ms
        o.apres_pas(static_cast<Source>(s), t, 0);
    }
    VERIFIER(passee[M] && passee[V] && passee[MER]);
    return 0;
}

// ---- après un pas -----------------------------------------------------------------------------------------------------------

constexpr int plus_rien_endort_la_source() {
    Ordonnanceur o;
    o.reveiller(Source::ListeSpots, 0);
    o.apres_pas(Source::ListeSpots, 3000, kPlusRien);
    VERIFIER(o.echeance_ms[L] == kJamais);
    VERIFIER(o.choisir(1000000000) == kAucune);
    o.reveiller(Source::ListeSpots, 5000); // nouvelle demande de l'écran
    VERIFIER(o.choisir(5000) == L);
    return 0;
}

constexpr int delai_negatif_vaut_zero() {
    Ordonnanceur o;
    o.apres_pas(Source::Vigie, 1000, -500);
    VERIFIER(o.echeance_ms[V] == 1000);
    return 0;
}

constexpr int delai_compte_depuis_la_fin_du_pas() {
    // Une requête de 12 s puis « prochain essai dans 15 min » : 15 min après la FIN de la requête, comme avant.
    Ordonnanceur o;
    o.apres_pas(Source::Meteo, 12000, 900000);
    VERIFIER(o.echeance_ms[M] == 912000);
    return 0;
}

// ---- attente de la tâche ----------------------------------------------------------------------------------------------------

constexpr int attente_jusqu_a_la_prochaine_echeance() {
    Ordonnanceur o;
    o.apres_pas(Source::Meteo, 0, 30000);
    o.apres_pas(Source::Vigie, 0, 5000);
    VERIFIER(o.attente_ms(1000, 60000) == 4000);
    VERIFIER(o.attente_ms(1000, 2000) == 2000); // plafond
    VERIFIER(o.attente_ms(9000, 60000) == 0);   // échéance dépassée : pas d'attente
    return 0;
}

constexpr int scenario_demarrage() {
    // Démarrage : météo et Vigie réveillées, réseau pas prêt (chaque pas rend 2 s), puis réseau prêt.
    Ordonnanceur o;
    o.reveiller(Source::Meteo, 0);
    o.reveiller(Source::Vigie, 0);
    VERIFIER(o.choisir(0) == V); // la météo attend son anti-rebond
    o.prendre_reveil(Source::Vigie);
    o.apres_pas(Source::Vigie, 0, 2000);
    VERIFIER(o.choisir(0) == kAucune);
    VERIFIER(o.attente_ms(0, 60000) == 1500);
    VERIFIER(o.choisir(1500) == M);
    o.prendre_reveil(Source::Meteo);
    o.apres_pas(Source::Meteo, 1500, 2000);
    VERIFIER(o.choisir(2000) == V);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(rien_ne_passe_sans_reveil)                                                                                       \
    X(reveil_sans_anti_rebond_passe_tout_de_suite)                                                                     \
    X(reveil_meteo_attend_l_anti_rebond)                                                                               \
    X(rafale_de_reveils_ne_repousse_pas)                                                                               \
    X(reveil_avance_un_pas_lointain)                                                                                   \
    X(reveil_n_eloigne_pas_un_pas_proche)                                                                              \
    X(reveil_signale_une_seule_fois)                                                                                   \
    X(reveil_pendant_un_pas_est_garde)                                                                                 \
    X(seules_les_sources_dues_passent)                                                                                 \
    X(la_plus_en_retard_passe_d_abord)                                                                                 \
    X(egalite_dans_l_ordre_de_la_table)                                                                                \
    X(liste_des_spots_passe_avant_les_autres)                                                                          \
    X(liste_pas_due_ne_bloque_rien)                                                                                    \
    X(une_source_qui_rend_zero_n_affame_pas_les_autres)                                                                \
    X(tour_complet_sans_famine)                                                                                        \
    X(plus_rien_endort_la_source)                                                                                      \
    X(delai_negatif_vaut_zero)                                                                                         \
    X(delai_compte_depuis_la_fin_du_pas)                                                                               \
    X(attente_jusqu_a_la_prochaine_echeance)                                                                           \
    X(scenario_demarrage)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
