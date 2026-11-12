// La tâche réseau unique des données (contrat main/tache_donnees.h). Ici : la tâche, les réveils et l'attente ; les décisions
// (qui passe, quand) sont dans ecran/ordonnanceur_reseau.hpp.
#include "tache_donnees.h"

#include <atomic>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ecran {

namespace {

const char *TAG = "donnees";

using ordonnanceur::Source;

// La tâche ne dort jamais plus longtemps : une attente sans fin ne se voit pas au journal.
constexpr std::int64_t kPlafondAttenteMs = 60000;

std::atomic<PasDonnees> g_pas[ordonnanceur::kNombreSources]; // stockage statique : tous nuls au démarrage
// Un bit par source réveillée, posé par n'importe quelle tâche, lu et remis à zéro par la tâche réseau. La notification FreeRTOS ne
// sert qu'à la réveiller : un bit posé avant sa création, ou pendant un pas, n'est jamais perdu.
std::atomic<std::uint32_t> g_reveils{0};
std::atomic<TaskHandle_t> g_tache{nullptr};

std::int64_t maintenant_ms() { return esp_timer_get_time() / 1000; }

void tache(void *) {
    ordonnanceur::Ordonnanceur o;
    for (;;) {
        std::int64_t t = maintenant_ms();
        const std::uint32_t bits = g_reveils.exchange(0);
        for (int i = 0; i < ordonnanceur::kNombreSources; ++i) {
            if ((bits & (1U << i)) != 0) {
                o.reveiller(static_cast<Source>(i), t);
            }
        }
        const int choisie = o.choisir(t);
        if (choisie == ordonnanceur::kAucune) {
            std::int64_t attente = o.attente_ms(t, kPlafondAttenteMs);
            if (attente < 1) {
                attente = 1;
            }
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(static_cast<std::uint32_t>(attente)));
            continue;
        }
        const Source s = static_cast<Source>(choisie);
        const bool reveillee = o.prendre_reveil(s);
        const PasDonnees pas = g_pas[choisie].load();
        const std::int64_t delai = pas != nullptr ? pas(reveillee) : ordonnanceur::kPlusRien;
        o.apres_pas(s, maintenant_ms(), delai);
    }
}

} // namespace

void tache_donnees_inscrire(Source source, PasDonnees pas) { g_pas[ordonnanceur::indice(source)].store(pas); }

bool tache_donnees_reveiller(Source source) {
    g_reveils.fetch_or(1U << ordonnanceur::indice(source));
    const TaskHandle_t t = g_tache.load();
    if (t == nullptr) {
        return false;
    }
    xTaskNotifyGive(t);
    return true;
}

void tache_donnees_demarrer() {
    if (g_tache.load() != nullptr) {
        return;
    }
    TaskHandle_t t = nullptr;
    if (xTaskCreate(tache, "donnees", kPileTacheDonnees, nullptr, tskIDLE_PRIORITY, &t) != pdPASS) {
        ESP_LOGE(TAG, "tâche réseau des données non créée (mémoire interne libre : %u octets)",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
        return;
    }
    g_tache.store(t);
    xTaskNotifyGive(t); // les réveils posés avant la création sont lus tout de suite
}

void tache_donnees_journaliser_pile(const char *tag) {
    ESP_LOGI(tag, "pile de la tâche réseau des données : %u octets jamais utilisés sur %u",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)), static_cast<unsigned>(kPileTacheDonnees));
}

} // namespace ecran
