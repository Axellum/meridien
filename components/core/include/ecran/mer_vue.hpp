// Noyau pur : ce qu'on lit dans la mer d'un spot (mer_trame.hpp) à un instant donné. C++17 standard, aucun include ESP-IDF ni LVGL, aucun
// flottant à l'exécution, évaluable à la compilation (test/host/test_mer_vue.cpp).
//
// Tout part d'un instant UNIX UTC `maintenant_s` ; 0 veut dire « pas d'horloge » (heure non réglée) : rien n'est alors affirmé « frais ».
// Ces fonctions ne décident de rien d'autre que ce qu'elles rendent : les mots, les couleurs et la mise en page sont dans la page.
#pragma once

#include <cstdint>

#include "ecran/mer_trame.hpp"
#include "ecran/surf_base.hpp"

namespace ecran::mer {

// Au-delà de ce vieillissement (secondes), la page dit l'âge de ce qu'elle montre ; au-delà du second, elle ne montre plus les verdicts.
// (Mêmes seuils que la page de vigie-display : un choix de Vigie, à ajuster à l'essai.)
inline constexpr std::int64_t VIEILLIE_APRES_S = 600;
inline constexpr std::int64_t EXPIREE_APRES_S = 21600;

// Quelle heure des bandes est « maintenant » : 0 à 23, ou -1 (pas de mer, pas d'heure de départ, avant la première, après la dernière).
constexpr int indice_heure(const Mer &mer, std::int64_t maintenant_s) noexcept {
    if (!mer.valide || mer.premiere_heure_s == 0 || maintenant_s < mer.premiere_heure_s)
        return -1;
    const std::int64_t indice = (maintenant_s - mer.premiere_heure_s) / 3600;
    return indice < HEURES ? static_cast<int>(indice) : -1;
}

// L'âge de la mer. « Expirée » aussi quand les 24 heures des bandes sont passées : les verdicts ne disent plus rien de maintenant.
constexpr Fraicheur fraicheur(const Mer &mer, std::int64_t maintenant_s) noexcept {
    if (!mer.valide)
        return Fraicheur::Rien;
    if (maintenant_s == 0)
        return Fraicheur::Vieillie; // pas d'horloge : l'âge n'est pas connu
    const std::int64_t age = maintenant_s > mer.genere_s ? maintenant_s - mer.genere_s : 0;
    if (age > EXPIREE_APRES_S)
        return Fraicheur::Expiree;
    if (mer.premiere_heure_s != 0 && maintenant_s >= mer.premiere_heure_s + static_cast<std::int64_t>(HEURES) * 3600)
        return Fraicheur::Expiree;
    if (mer.donnees_gardees || age > VIEILLIE_APRES_S)
        return Fraicheur::Vieillie;
    return Fraicheur::Fraiche;
}

namespace detail {
constexpr int rang_verdict(Verdict v) noexcept {
    switch (v) {
    case Verdict::Bon:
        return 3;
    case Verdict::Possible:
        return 2;
    case Verdict::Deconseille:
        return 1;
    default:
        return 0;
    }
}
} // namespace detail

// L'activité au meilleur verdict (le surf, le kite, le bateau, la pêche à pied dans cet ordre en cas d'égalité) parmi celles qu'on peut juger
// ici ; -1 quand aucune n'a de verdict. « Déconseillé » passe devant « inconnu » : une activité qu'on déconseille est un verdict.
constexpr int meilleure_activite(const Mer &mer) noexcept {
    int meilleure = -1;
    int meilleur_rang = 0;
    for (int i = 0; i < NB_ACTIVITES; ++i) {
        const Prevision &a = mer.activites[i];
        if (!a.presente || !a.disponible)
            continue;
        const int rang = detail::rang_verdict(a.verdict);
        if (rang > meilleur_rang) {
            meilleure = i;
            meilleur_rang = rang;
        }
    }
    return meilleure;
}

// La première marée (pleine ou basse) qui n'est pas passée ; -1 quand il n'en reste aucune (ou aucun modèle de marée près du spot).
constexpr int prochaine_maree(const Mer &mer, std::int64_t maintenant_s) noexcept {
    for (int i = 0; i < mer.nb_marees && i < MAREES_MAX; ++i)
        if (mer.marees[i].instant_s >= maintenant_s)
            return i;
    return -1;
}

// Hauteur de la mer (centimètres) à `instant_s`, par la demi-cosinusoïde qui joint deux marées qui se suivent : h = a + (b - a) (1 - cos(pi t)) / 2.
// Faux (cm intacte) quand l'instant n'est entre aucune paire de marées, quand les marées ne sont pas dans l'ordre du temps (on ne dessine
// pas), ou qu'une hauteur est inconnue. Calcul entier : le cosinus est celui, au millième, de surf_base.hpp, interpolé entre deux degrés
// entiers ; l'écart avec le calcul à virgule est sous 1 cm pour une amplitude de 10 m (test_mer_vue.cpp).
constexpr bool hauteur_maree_cm(const Mer &mer, std::int64_t instant_s, std::int32_t &cm) noexcept {
    const int n = mer.nb_marees < MAREES_MAX ? mer.nb_marees : MAREES_MAX;
    for (int i = 0; i + 1 < n; ++i) {
        const Maree &a = mer.marees[i];
        const Maree &b = mer.marees[i + 1];
        if (a.instant_s == 0 || b.instant_s <= a.instant_s)
            return false;
        if (instant_s < a.instant_s || instant_s > b.instant_s)
            continue;
        if (a.hauteur_cm == INCONNU || b.hauteur_cm == INCONNU)
            return false;
        const std::int64_t duree = b.instant_s - a.instant_s;
        const std::int64_t ecoule = instant_s - a.instant_s;
        const std::int64_t numerateur = 180 * ecoule; // l'angle, en degrés x duree
        const std::int64_t deg = numerateur / duree;
        const std::int64_t reste = numerateur % duree;
        const std::int64_t c0 = surf::cosinus_pm(deg);
        const std::int64_t c1 = surf::cosinus_pm(deg + 1);
        const std::int64_t cosinus_x_duree = c0 * duree + (c1 - c0) * reste; // en millièmes x duree
        const std::int64_t ecart = static_cast<std::int64_t>(b.hauteur_cm) - a.hauteur_cm;
        cm = static_cast<std::int32_t>(a.hauteur_cm +
                                       surf::division_arrondie(ecart * (1000 * duree - cosinus_x_duree), 2000 * duree));
        return true;
    }
    return false;
}

// Le coefficient (estimé) de la première pleine mer qui n'est pas passée ; -1 quand il n'y en a pas.
constexpr int coefficient_pour(const Mer &mer, std::int64_t maintenant_s) noexcept {
    const int n = mer.nb_coefficients < COEFFICIENTS_MAX ? mer.nb_coefficients : COEFFICIENTS_MAX;
    for (int i = 0; i < n; ++i)
        if (mer.coefficients[i].valeur != 0 && mer.coefficients[i].pleine_mer_s >= maintenant_s)
            return i;
    return -1;
}

} // namespace ecran::mer
