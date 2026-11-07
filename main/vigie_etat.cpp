#include "vigie_etat.h"

#include <new>
#include <utility>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace ecran {

namespace {

const char *TAG = "vigie";

// Mutex statique (aucune allocation) créé au premier usage ; les premiers appels viennent de la seule tâche qui écrit (le client) ou
// de la tâche de l'interface, jamais en même temps au démarrage : la création n'a pas besoin d'être protégée elle-même.
StaticSemaphore_t g_mutex_donnees;
SemaphoreHandle_t g_mutex = nullptr;

vigie::Alertes *g_publie = nullptr;  // lu par l'interface, sous le verrou
vigie::Alertes *g_travail = nullptr; // écrit par la tâche réseau, sans verrou : personne d'autre n'y touche
std::int64_t g_recu_s = 0;
bool g_valide = false;

SemaphoreHandle_t verrou() {
    if (g_mutex == nullptr) {
        g_mutex = xSemaphoreCreateMutexStatic(&g_mutex_donnees);
    }
    return g_mutex;
}

vigie::Alertes *allouer() {
    void *place = heap_caps_calloc(1, sizeof(vigie::Alertes), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (place == nullptr) {
        return nullptr;
    }
    auto *a = new (place) vigie::Alertes();
    vigie::vider(*a);
    return a;
}

bool allouer_les_deux() {
    if (g_publie != nullptr && g_travail != nullptr) {
        return true;
    }
    if (g_publie == nullptr) {
        g_publie = allouer();
    }
    if (g_travail == nullptr) {
        g_travail = allouer();
    }
    if (g_publie == nullptr || g_travail == nullptr) {
        ESP_LOGE(TAG, "pas de place en PSRAM pour les deux tampons des alertes (%u octets chacun)",
                 static_cast<unsigned>(sizeof(vigie::Alertes)));
        return false;
    }
    ESP_LOGI(TAG, "tampons des alertes : 2 x %u octets en PSRAM", static_cast<unsigned>(sizeof(vigie::Alertes)));
    return true;
}

} // namespace

vigie::Alertes *vigie_etat_travail() { return allouer_les_deux() ? g_travail : nullptr; }

void vigie_etat_publier(std::int64_t recu_s) {
    if (!allouer_les_deux()) {
        return;
    }
    xSemaphoreTake(verrou(), portMAX_DELAY);
    std::swap(g_publie, g_travail);
    g_recu_s = recu_s;
    g_valide = true;
    xSemaphoreGive(g_mutex);
}

void vigie_etat_oublier() {
    xSemaphoreTake(verrou(), portMAX_DELAY);
    g_valide = false;
    g_recu_s = 0;
    xSemaphoreGive(g_mutex);
}

VigieLecture::VigieLecture() {
    xSemaphoreTake(verrou(), portMAX_DELAY);
    if (g_valide && g_publie != nullptr) {
        alertes_ = g_publie;
        recu_s_ = g_recu_s;
    }
}

VigieLecture::~VigieLecture() { xSemaphoreGive(g_mutex); }

} // namespace ecran
