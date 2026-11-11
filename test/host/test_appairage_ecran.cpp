// Test de main/appairage.cpp (le code d'appairage montré à l'écran) compilé tel quel sur le PC, avec le gestionnaire du noyau.
// Vérifie le chemin du bouton « Nouveau code » de l'écran jusqu'à la page : le code affiché change tout de suite, et dès que le
// serveur se synchronise (début de chaque requête de l'API), l'ancien code est refusé, le nouveau accepté, les sessions fermées et
// le verrou levé. Vérifie aussi qu'un code tiré par une série d'essais ratés est montré à l'écran.
//
// Ce que ce test attrape et que test_appairage ne voit pas : un bouton qui ne fait rien (le défaut d'origine), un écran qui montre
// un autre code que celui que la page accepte, une demande perdue ou appliquée deux fois avec des valeurs différentes.
// Les fonctions de ce module gardent leur état dans des variables globales : les tests s'enchaînent dans l'ordre de la liste,
// à l'exécution seulement (pas de static_assert).
#include <cstdint>
#include <string_view>

#include "../../main/appairage.cpp"
#include "mini.hpp"

namespace ap = ecran::appairage;
using sv = std::string_view;

namespace {

struct Alea {
    std::uint8_t graine = 40;
    void operator()(std::uint8_t *sortie, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            sortie[i] = static_cast<std::uint8_t>(graine + i);
        graine = static_cast<std::uint8_t>(graine + 13);
    }
};

ap::Gestionnaire g_serveur; // celui du serveur de la page
Alea g_alea;

int rien_avant_le_premier_code() {
    VERIFIER(sv(appairage_jeton()).empty());
    VERIFIER(!ecran::appairage_demander_nouveau_code(111111u)); // pas de page : rien à renouveler
    ecran::appairage_synchroniser(g_serveur);                   // gestionnaire jamais tiré : rien n'est publié
    VERIFIER(sv(appairage_jeton()).empty());
    return 0;
}

int le_code_du_demarrage_est_affiche() {
    g_serveur.nouveau_code(4217u);
    ecran::appairage_synchroniser(g_serveur);
    VERIFIER(sv(appairage_jeton()) == "004217");
    VERIFIER(sv(appairage_jeton()) == g_serveur.code());
    return 0;
}

int nouveau_code_affiche_tout_de_suite_puis_applique_par_le_serveur() {
    const ap::Resultat ouverte = g_serveur.essayer("004217", 0, g_alea);
    VERIFIER(ouverte.issue == ap::Issue::Accepte);
    for (int i = 0; i < ap::ESSAIS_MAX; ++i)
        g_serveur.essayer("999999", 1000, g_alea);              // un verrou en cours
    VERIFIER(ecran::appairage_demander_nouveau_code(1654321u)); // le bouton de l'écran
    VERIFIER(sv(appairage_jeton()) == "654321");                // affiché tout de suite
    // le serveur prend la demande au début de sa requête suivante
    ecran::appairage_synchroniser(g_serveur);
    VERIFIER(g_serveur.code() == "654321" && sv(appairage_jeton()) == "654321");
    VERIFIER(!g_serveur.jeton_valide(ouverte.jeton) && g_serveur.sessions_ouvertes() == 0);
    VERIFIER(g_serveur.essayer("004217", 2000, g_alea).issue == ap::Issue::Refuse); // ancien code, et plus de verrou
    VERIFIER(g_serveur.essayer("654321", 2000, g_alea).issue == ap::Issue::Accepte);
    // la demande n'est appliquée qu'une fois : la session ouverte avec le nouveau code reste
    ecran::appairage_synchroniser(g_serveur);
    VERIFIER(g_serveur.sessions_ouvertes() == 1);
    return 0;
}

int le_code_tire_par_des_essais_rates_est_affiche() {
    std::int64_t t = 10000;
    ap::Resultat r;
    for (int serie = 1; serie <= ap::VERROUS_AVANT_NOUVEAU_CODE; ++serie) {
        for (int i = 0; i < ap::ESSAIS_MAX; ++i)
            r = g_serveur.essayer("999999", t, g_alea);
        ecran::appairage_synchroniser(g_serveur); // fin de la requête
        t += ap::duree_verrou(serie);
    }
    VERIFIER(r.code_change && g_serveur.code() != "654321");
    VERIFIER(sv(appairage_jeton()) == g_serveur.code());
    VERIFIER(g_serveur.sessions_ouvertes() == 1); // le code a changé, la session de la personne reste
    return 0;
}

} // namespace

int main() {
    static const Cas cas[] = {
        {"rien_avant_le_premier_code", rien_avant_le_premier_code},
        {"le_code_du_demarrage_est_affiche", le_code_du_demarrage_est_affiche},
        {"nouveau_code_affiche_tout_de_suite_puis_applique_par_le_serveur",
         nouveau_code_affiche_tout_de_suite_puis_applique_par_le_serveur},
        {"le_code_tire_par_des_essais_rates_est_affiche", le_code_tire_par_des_essais_rates_est_affiche},
    };
    return lancer_tous(cas);
}
