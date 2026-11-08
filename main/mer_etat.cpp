#include "mer_etat.h"

#include <new>
#include <utility>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace ecran {

namespace {

const char *TAG = "mer";

// Mutex statique (aucune allocation) créé au premier usage ; les premiers appels viennent de la seule tâche qui écrit (le client) ou de
// la tâche de l'interface, jamais en même temps au démarrage : la création n'a pas besoin d'être protégée elle-même.
StaticSemaphore_t g_mutex_donnees;
SemaphoreHandle_t g_mutex = nullptr;

MerPubliee *g_publiee = nullptr; // lue par l'interface, sous le verrou
MerPubliee *g_travail = nullptr; // écrite par la tâche réseau, sans verrou : personne d'autre n'y touche
std::int64_t g_recu_s = 0;
bool g_valide = false;

SemaphoreHandle_t verrou() {
    if (g_mutex == nullptr) {
        g_mutex = xSemaphoreCreateMutexStatic(&g_mutex_donnees);
    }
    return g_mutex;
}

MerPubliee *allouer() {
    void *place = heap_caps_calloc(1, sizeof(MerPubliee), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (place == nullptr) {
        return nullptr;
    }
    return new (place) MerPubliee();
}

bool allouer_les_deux() {
    if (g_publiee != nullptr && g_travail != nullptr) {
        return true;
    }
    if (g_publiee == nullptr) {
        g_publiee = allouer();
    }
    if (g_travail == nullptr) {
        g_travail = allouer();
    }
    if (g_publiee == nullptr || g_travail == nullptr) {
        ESP_LOGE(TAG, "pas de place en PSRAM pour les deux tampons de la mer (%u octets chacun)",
                 static_cast<unsigned>(sizeof(MerPubliee)));
        return false;
    }
    ESP_LOGI(TAG, "tampons de la mer : 2 x %u octets en PSRAM", static_cast<unsigned>(sizeof(MerPubliee)));
    return true;
}

} // namespace

MerPubliee *mer_etat_travail() { return allouer_les_deux() ? g_travail : nullptr; }

void mer_etat_publier(std::int64_t recu_s) {
    if (!allouer_les_deux()) {
        return;
    }
    xSemaphoreTake(verrou(), portMAX_DELAY);
    std::swap(g_publiee, g_travail);
    g_recu_s = recu_s;
    g_valide = true;
    xSemaphoreGive(g_mutex);
}

void mer_etat_oublier() {
    xSemaphoreTake(verrou(), portMAX_DELAY);
    g_valide = false;
    g_recu_s = 0;
    xSemaphoreGive(g_mutex);
}

MerLecture::MerLecture() {
    xSemaphoreTake(verrou(), portMAX_DELAY);
    if (g_valide && g_publiee != nullptr) {
        publiee_ = g_publiee;
        recu_s_ = g_recu_s;
    }
}

MerLecture::~MerLecture() { xSemaphoreGive(g_mutex); }

} // namespace ecran
