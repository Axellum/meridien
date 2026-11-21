#include "climat_etat.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace ecran {

namespace {

// Mutex statique (aucune allocation) créé au premier usage. Le premier appel vient de la seule tâche qui écrit (le client) ou de la
// tâche de l'interface, jamais en même temps au démarrage : la création n'a pas besoin d'être protégée elle-même (même choix que vigie_etat.cpp).
StaticSemaphore_t g_mutex_donnees;
SemaphoreHandle_t g_mutex = nullptr;

climat::Normales g_normales;
std::int64_t g_recu_s = 0;
bool g_valide = false;

SemaphoreHandle_t verrou() {
    if (g_mutex == nullptr) {
        g_mutex = xSemaphoreCreateMutexStatic(&g_mutex_donnees);
    }
    return g_mutex;
}

} // namespace

void climat_etat_publier(const climat::Normales &normales, std::int64_t recu_s) {
    xSemaphoreTake(verrou(), portMAX_DELAY);
    g_normales = normales;
    g_recu_s = recu_s;
    g_valide = true;
    xSemaphoreGive(g_mutex);
}

void climat_etat_oublier() {
    xSemaphoreTake(verrou(), portMAX_DELAY);
    g_valide = false;
    g_recu_s = 0;
    g_normales = climat::Normales{};
    xSemaphoreGive(g_mutex);
}

bool climat_etat_lire(climat::Normales &sortie, std::int64_t &recu_s) {
    xSemaphoreTake(verrou(), portMAX_DELAY);
    const bool valide = g_valide;
    if (valide) {
        sortie = g_normales;
        recu_s = g_recu_s;
    }
    xSemaphoreGive(g_mutex);
    return valide;
}

} // namespace ecran
