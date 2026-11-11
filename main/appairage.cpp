// Code d'appairage courant (contrat main/appairage.h). Le serveur publie, l'interface lit et demande un nouveau code.
//
// Le code affiché tient dans un entier atomique (code + 1 ; 0 = pas encore de code) : la lecture par l'interface ne voit jamais un
// code à moitié écrit, quelle que soit la tâche qui publie. La demande du bouton passe par deux atomiques (la valeur, puis le
// drapeau) ; si deux demandes se croisent, le serveur applique la dernière valeur écrite et l'écran la montre.
#include "appairage.h"

#include <atomic>

namespace {

std::atomic<std::uint32_t> g_affiche{0};                // code affiché + 1 ; 0 tant que le serveur n'a rien publié
std::atomic<std::uint32_t> g_demande_valeur{0};         // valeur aléatoire du bouton
std::atomic<bool> g_demande{false};                     // une demande attend le serveur
std::uint32_t g_generation_publiee = 0;                 // fil du serveur seulement
char g_texte[ecran::appairage::CODE_CHIFFRES + 1] = {}; // tâche de l'interface seulement

void afficher(std::uint32_t aleatoire) { g_affiche.store(aleatoire % 1000000u + 1u, std::memory_order_release); }

// Les 6 chiffres d'un code déjà écrit par le gestionnaire -> sa valeur.
std::uint32_t valeur(std::string_view code) {
    std::uint32_t v = 0;
    for (char c : code)
        v = v * 10u + static_cast<std::uint32_t>(c - '0');
    return v;
}

} // namespace

const char *appairage_jeton() {
    const std::uint32_t v = g_affiche.load(std::memory_order_acquire);
    if (v == 0)
        return "";
    ecran::appairage::ecrire_code(v - 1u, g_texte);
    return g_texte;
}

namespace ecran {

bool appairage_demander_nouveau_code(std::uint32_t aleatoire) {
    if (g_affiche.load(std::memory_order_acquire) == 0)
        return false; // le serveur n'a pas démarré : il n'y a ni code ni session
    g_demande_valeur.store(aleatoire, std::memory_order_relaxed);
    g_demande.store(true, std::memory_order_release);
    afficher(aleatoire);
    return true;
}

void appairage_synchroniser(appairage::Gestionnaire &g) {
    if (g_demande.exchange(false, std::memory_order_acq_rel))
        g.nouveau_code(g_demande_valeur.load(std::memory_order_relaxed));
    if (g.generation() != g_generation_publiee) {
        g_generation_publiee = g.generation();
        afficher(valeur(g.code()));
    }
}

} // namespace ecran
