// Squelette : prouve que la chaine de compilation ESP32-P4 fonctionne. Aucune fonction produit ici.
// Le vrai demarrage (carte, ecran, reseau, reglages) arrive en phase 1 (docs/plan.md).
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "ecran";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "ecran autonome - squelette, ESP-IDF %s", esp_get_idf_version());
}
