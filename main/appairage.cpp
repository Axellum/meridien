// Code d'appairage courant (contrat main/appairage.h). Écrit une fois par le serveur de la page, lu par l'interface.
#include "appairage.h"

#include <atomic>
#include <cstring>

namespace {

char g_code[8] = {};
std::atomic<bool> g_pret{false};

} // namespace

const char *appairage_jeton() { return g_pret.load(std::memory_order_acquire) ? g_code : ""; }

namespace ecran {

void appairage_publier_code(const char *six_chiffres) {
    if (six_chiffres == nullptr || std::strlen(six_chiffres) != 6) {
        return; // rien d'autre qu'un code à 6 chiffres n'est publié
    }
    std::memcpy(g_code, six_chiffres, 7); // 6 chiffres et le zéro final
    g_pret.store(true, std::memory_order_release);
}

} // namespace ecran
