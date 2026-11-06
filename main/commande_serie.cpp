#include "commande_serie.h"

#include <cctype>
#include <cstddef>
#include <cstring>
#include <unistd.h>

#include "board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ecran {

namespace {

const char *TAG = "commande_serie";

constexpr const char *kCommandeRedemarrer = "redemarrer";
constexpr const char *kCommandeMemoire = "memoire";

void traiter(const char *ligne) {
    if (std::strcmp(ligne, kCommandeMemoire) == 0) {
        // Une ligne « MEMOIRE … » par demande, lue par le banc du PC pour suivre la mémoire libre dans le temps (provisoire, comme
        // « redemarrer »). Rien ne tourne tant qu'elle n'est pas demandée : la mesure n'ajoute aucune activité à la carte.
        ESP_LOGI(TAG, "MEMOIRE t_s=%lld interne_libre=%u plus_gros_bloc=%u interne_min=%u psram_libre=%u",
                 static_cast<long long>(esp_timer_get_time() / 1000000),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
        return;
    }
    if (std::strcmp(ligne, kCommandeRedemarrer) == 0) {
        ESP_LOGW(TAG, "redémarrage logiciel demandé sur la console (esp_rom_software_reset_system)");
        vTaskDelay(pdMS_TO_TICKS(200)); // laisser partir le journal sur l'USB
        board_restart();
    }
    ESP_LOGI(TAG, "commande inconnue ignorée");
}

// Console USB-Serial/JTAG sans pilote installé : stdin se lit en sondage, read() rend 0 (ou -1) quand rien n'est arrivé.
void tache_lecture(void *) {
    char ligne[32];
    std::size_t n = 0;
    for (;;) {
        char c = 0;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (c == '\r' || c == '\n') {
            ligne[n] = '\0';
            if (n > 0) {
                traiter(ligne);
            }
            n = 0;
        } else if (n < sizeof(ligne) - 1) {
            ligne[n++] = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            n = 0; // ligne trop longue pour être une commande : on la jette
        }
    }
}

} // namespace

void commande_serie_demarrer() {
    if (xTaskCreate(tache_lecture, "commande_serie", 3072, nullptr, 2, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "tâche de lecture de la console non créée");
    }
}

} // namespace ecran
