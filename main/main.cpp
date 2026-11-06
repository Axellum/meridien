// Phase 1b : heure et date réelles sur l'écran, par le Wi-Fi du coprocesseur ESP32-C6 et SNTP (docs/plan.md). Les lignes de
// diagnostic de la phase 1a (écran, tactile, compteur de démarrages) restent sous l'heure.
#include "board.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "boot_info.h"
#include "commande_serie.h"
#include "meteo_client.h"
#include "reseau.h"
#include "ui_bonjour.h"

static const char *TAG = "ecran";

// Rétroéclairage à 75 % au premier démarrage, comme le firmware ESPHome du Tab5 (moins de courant et de chaleur que 100 % ;
// un brownout a été vu à 100 % sur une alimentation juste, audit du 16/10/2026).
static constexpr int kBacklightPercent = 75;

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "écran autonome, phase 1b ; ESP-IDF %s ; application démarrée à %d ms", esp_get_idf_version(),
             static_cast<int>(esp_timer_get_time() / 1000));

    const ecran::BootInfo boot = ecran::boot_info_load();

    const esp_err_t err = board_init();
    if (err != ESP_OK) {
        // Pas de redémarrage automatique ici : une boucle de redémarrages masquerait la cause et userait le matériel.
        ESP_LOGE(TAG, "carte non initialisée : %s ; arrêt (l'écran reste éteint)", esp_err_to_name(err));
        return;
    }

    if (board_display_lock(0)) {
        ecran::ui_bonjour_creer(boot);
        lv_refr_now(board_get_display()); // première image dessinée avant d'allumer le rétroéclairage
        board_display_unlock();
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // laisser finir le transfert DMA de la dernière bande
    board_set_backlight_percent(kBacklightPercent);
    ESP_LOGI(TAG, "rétroéclairage allumé à %d %% à %d ms ; première image affichée", kBacklightPercent,
             static_cast<int>(esp_timer_get_time() / 1000));

    // Le réseau démarre après la première image : l'écran garde le temps de démarrage mesuré en 1a (docs/mesures/1b.md dit
    // si le Wi-Fi l'a changé).
    ecran::reseau_demarrer(boot);
    ecran::
        meteo_client_demarrer(); // météo Open-Meteo : cache NVS lu tout de suite, requêtes dès que le Wi-Fi et l'heure sont là
    ecran::commande_serie_demarrer();
}
