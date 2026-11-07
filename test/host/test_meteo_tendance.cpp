// Tests de la tendance de pression sur 3 h, mesurée par l'appareil : meteo_tendance.hpp.
//
// Pas de réponse réelle d'Open-Meteo ici : la tendance ne lit que des couples (instant de la mesure, pression) que l'appareil a
// lui-même notés ; chaque cas est écrit à la main avec son résultat calculé de tête (pas par le code testé). Les valeurs
// physiques sont celles d'une pression réaliste (1 013,2 hPa = 10132 dixièmes).
#include <cstddef>
#include <cstdint>

#include "ecran/meteo_tendance.hpp"
#include "mini.hpp"

using namespace ecran::meteo;

namespace {

constexpr std::int64_t T = 1'794'000'000; // un instant quelconque (début novembre 2026)
constexpr std::int64_t QUART = 900;
constexpr std::int64_t TROIS_H = 10800;

constexpr int sans_historique_la_tendance_est_inconnue() {
    HistoPression h;
    VERIFIER(h.n == 0);
    VERIFIER(!tendance_3h(h, T, 10132).connue);
    // Une seule mesure, celle qu'on interroge : elle a 0 s d'âge, pas 3 h.
    tendance_noter(h, T, 10132);
    VERIFIER(h.n == 1);
    VERIFIER(!tendance_3h(h, T, 10132).connue);
    return 0;
}

constexpr int trois_heures_pile_donnent_la_difference() {
    HistoPression h;
    tendance_noter(h, T, 10132);
    tendance_noter(h, T + TROIS_H, 10098); // 1 013,2 -> 1 009,8 : -3,4 hPa en 3 h
    const Tendance t = tendance_3h(h, T + TROIS_H, 10098);
    VERIFIER(t.connue);
    VERIFIER(t.dixiemes_hpa == -34);

    HistoPression m;
    tendance_noter(m, T, 10080);
    tendance_noter(m, T + TROIS_H, 10105); // +2,5 hPa
    VERIFIER(tendance_3h(m, T + TROIS_H, 10105).dixiemes_hpa == 25);

    HistoPression stable;
    tendance_noter(stable, T, 10132);
    VERIFIER(tendance_3h(stable, T + TROIS_H, 10132).connue);
    VERIFIER(tendance_3h(stable, T + TROIS_H, 10132).dixiemes_hpa == 0);
    return 0;
}

constexpr int la_fenetre_va_de_2h30_a_3h30_bornes_comprises() {
    HistoPression h;
    tendance_noter(h, T, 10000);
    VERIFIER(!tendance_3h(h, T + 9000 - 1, 10010).connue); // 2 h 29 min 59 s
    VERIFIER(tendance_3h(h, T + 9000, 10010).connue);      // 2 h 30 pile
    VERIFIER(tendance_3h(h, T + 9000, 10010).dixiemes_hpa == 10);
    VERIFIER(tendance_3h(h, T + 12600, 10010).connue);      // 3 h 30 pile
    VERIFIER(!tendance_3h(h, T + 12600 + 1, 10010).connue); // 3 h 30 min 1 s
    return 0;
}

constexpr int la_mesure_la_plus_proche_de_3h_est_retenue() {
    HistoPression h;
    tendance_noter(h, T, 10000);
    tendance_noter(h, T + 1000, 10050);
    // cible = T + 900 : T est à 900 s, T + 1000 à 100 s -> la seconde.
    VERIFIER(tendance_3h(h, T + TROIS_H + 900, 10090).dixiemes_hpa == 40);
    // cible = T + 100 : T est à 100 s, T + 1000 à 900 s -> la première.
    VERIFIER(tendance_3h(h, T + TROIS_H + 100, 10090).dixiemes_hpa == 90);

    HistoPression e;
    tendance_noter(e, T, 10000);
    tendance_noter(e, T + 1800, 10200);
    // cible = T + 900 : à 900 s des deux -> la plus ancienne reste.
    VERIFIER(tendance_3h(e, T + TROIS_H + 900, 10300).dixiemes_hpa == 300);
    return 0;
}

constexpr int la_meme_mesure_deux_fois_ne_fait_qu_un_point() {
    HistoPression h;
    tendance_noter(h, T, 10000);
    tendance_noter(h, T, 10010); // même `current.time` reçu par deux réponses de suite : la dernière valeur fait foi
    VERIFIER(h.n == 1);
    VERIFIER(h.instants_s[0] == T);
    VERIFIER(h.dixiemes_hpa[0] == 10010);
    tendance_noter(h, T + QUART, 10020);
    VERIFIER(h.n == 2);
    VERIFIER(h.dixiemes_hpa[0] == 10010);
    return 0;
}

constexpr int une_mesure_plus_ancienne_repart_de_zero() {
    HistoPression h;
    tendance_noter(h, T + QUART, 10000);
    tendance_noter(h, T + 2 * QUART, 10010);
    tendance_noter(h, T, 9990); // en arrière : autre lieu ou serveur qui revient en arrière
    VERIFIER(h.n == 1);
    VERIFIER(h.instants_s[0] == T);
    VERIFIER(h.dixiemes_hpa[0] == 9990);
    // et la suite s'empile normalement
    tendance_noter(h, T + QUART, 9995);
    VERIFIER(h.n == 2);
    return 0;
}

constexpr int l_historique_est_borne_et_garde_les_plus_recentes() {
    HistoPression h;
    for (int i = 0; i < 20; ++i)
        tendance_noter(h, T + i * QUART, static_cast<int>(10000 + i));
    VERIFIER(h.n == TENDANCE_MESURES_MAX);
    VERIFIER(h.instants_s[0] == T + 4 * QUART); // les 4 plus anciennes sont sorties
    VERIFIER(h.dixiemes_hpa[0] == 10004);
    VERIFIER(h.instants_s[TENDANCE_MESURES_MAX - 1] == T + 19 * QUART);
    VERIFIER(h.dixiemes_hpa[TENDANCE_MESURES_MAX - 1] == 10019);
    for (std::size_t i = 1; i < h.n; ++i) // toujours rangées, strictement croissantes
        VERIFIER(h.instants_s[i] > h.instants_s[i - 1]);
    // Pleine, elle sait encore répondre : 3 h avant la mesure 19 est la mesure 7, gardée.
    const Tendance t = tendance_3h(h, T + 19 * QUART, 10019);
    VERIFIER(t.connue);
    VERIFIER(t.dixiemes_hpa == 12);
    // Et 3 h 30 avant (mesure 5) l'est aussi : la borne haute de la fenêtre reste couverte quand l'historique est plein.
    VERIFIER(h.instants_s[0] <= T + 19 * QUART - 12600);
    return 0;
}

constexpr int une_panne_de_plus_de_3h30_rend_la_tendance_inconnue() {
    HistoPression h;
    tendance_noter(h, T, 10000);
    tendance_noter(h, T + 4 * 3600, 10040); // plus aucune mesure entre 2 h 30 et 3 h 30 avant
    VERIFIER(!tendance_3h(h, T + 4 * 3600, 10040).connue);
    // mais 3 h plus tard une mesure existe de nouveau dans la fenêtre
    tendance_noter(h, T + 4 * 3600 + TROIS_H, 10100);
    VERIFIER(tendance_3h(h, T + 4 * 3600 + TROIS_H, 10100).dixiemes_hpa == 60);
    return 0;
}

constexpr int les_pressions_extremes_tiennent() {
    HistoPression h;
    tendance_noter(h, T, 10850);          // 1 085,0 hPa, la limite haute plausible du noyau
    tendance_noter(h, T + TROIS_H, 8700); // 870,0 hPa, la limite basse
    VERIFIER(h.dixiemes_hpa[0] == 10850);
    VERIFIER(h.dixiemes_hpa[1] == 8700);
    VERIFIER(tendance_3h(h, T + TROIS_H, 8700).dixiemes_hpa == -2150);
    return 0;
}

constexpr int la_tendance_est_relative_a_la_mesure_interrogee_pas_a_la_derniere() {
    // L'historique peut contenir des mesures plus récentes que celle qu'on convertit (publication d'un cache après une réponse).
    HistoPression h;
    tendance_noter(h, T, 10000);
    tendance_noter(h, T + TROIS_H, 10030);
    tendance_noter(h, T + TROIS_H + 3 * QUART, 10040);
    VERIFIER(tendance_3h(h, T + TROIS_H, 10030).dixiemes_hpa == 30); // la mesure du milieu, pas la dernière
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(sans_historique_la_tendance_est_inconnue)                                                                        \
    X(trois_heures_pile_donnent_la_difference)                                                                         \
    X(la_fenetre_va_de_2h30_a_3h30_bornes_comprises)                                                                   \
    X(la_mesure_la_plus_proche_de_3h_est_retenue)                                                                      \
    X(la_meme_mesure_deux_fois_ne_fait_qu_un_point)                                                                    \
    X(une_mesure_plus_ancienne_repart_de_zero)                                                                         \
    X(l_historique_est_borne_et_garde_les_plus_recentes)                                                               \
    X(une_panne_de_plus_de_3h30_rend_la_tendance_inconnue)                                                             \
    X(les_pressions_extremes_tiennent)                                                                                 \
    X(la_tendance_est_relative_a_la_mesure_interrogee_pas_a_la_derniere)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
