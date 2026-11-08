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
#include "ecran_principal.h"
#include "mer_client.h"
#include "meteo_client.h"
#include "reglages.h"
#include "reseau.h"
#include "serveur_web.h"
#include "vigie_client.h"

static const char *TAG = "ecran";

// Rétroéclairage : la luminosité réglée par la page (reglages.h) ; 75 % tant qu'elle n'a jamais été réglée, comme le firmware
// ESPHome du Tab5 (moins de courant et de chaleur que 100 % ; un brownout a été vu à 100 % sur une alimentation juste, audit du
// 16/10/2026).

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "écran autonome, phase 1b ; ESP-IDF %s ; application démarrée à %d ms", esp_get_idf_version(),
             static_cast<int>(esp_timer_get_time() / 1000));

    ecran::memoire_jalon("début de app_main");
    const ecran::BootInfo boot = ecran::boot_info_load();
    ecran::reglages_demarrer(); // après la NVS : langue de l'écran et luminosité réglées par la page
    ecran::memoire_jalon("après la NVS et les réglages");

    const esp_err_t err = board_init();
    if (err != ESP_OK) {
        // Pas de redémarrage automatique ici : une boucle de redémarrages masquerait la cause et userait le matériel.
        ESP_LOGE(TAG, "carte non initialisée : %s ; arrêt (l'écran reste éteint)", esp_err_to_name(err));
        return;
    }
    ecran::memoire_jalon("après board_init (écran, tactile, LVGL, tampons)");

    if (board_display_lock(0)) {
        // Écran principal (heure, date, météo) chargé ; l'écran « Bonjour » des phases 1a/1b devient l'écran de diagnostic
        // (appui long sur le fond pour passer de l'un à l'autre).
        ecran::ecran_principal_creer(boot);
        lv_refr_now(board_get_display()); // première image dessinée avant d'allumer le rétroéclairage
        board_display_unlock();
    }
    ecran::memoire_jalon("après l'écran principal (objets, police, première image)");
    vTaskDelay(pdMS_TO_TICKS(100)); // laisser finir le transfert DMA de la dernière bande
    const int luminosite = ecran::reglages_affichage().luminosite;
    board_set_backlight_percent(luminosite);
    ESP_LOGI(TAG, "rétroéclairage allumé à %d %% à %d ms ; première image affichée", luminosite,
             static_cast<int>(esp_timer_get_time() / 1000));

    // Le réseau démarre après la première image : l'écran garde le temps de démarrage mesuré en 1a (docs/mesures/1b.md dit
    // si le Wi-Fi l'a changé).
    ecran::reseau_demarrer(boot);
    ecran::memoire_jalon("après le lancement de la tâche réseau (pile 8 Ko)");
    ecran::
        meteo_client_demarrer(); // météo Open-Meteo : cache NVS lu tout de suite, requêtes dès que le Wi-Fi et l'heure sont là
    ecran::memoire_jalon("après le lancement de la tâche météo (pile 12 Ko, cache NVS)");
    ecran::
        vigie_client_demarrer(); // alertes officielles de Vigie : premier essai 60 à 120 s après que le réseau et l'heure soient prêts
    ecran::memoire_jalon("après le lancement de la tâche des alertes (pile 12 Ko)");
    // Mer du spot choisi (ADR-0019) : la tâche n'est créée que si le registre des spots n'est pas vide (appareil neuf : rien n'est créé,
    // rien n'est demandé à Vigie, aucune mémoire de plus). Premier essai 120 à 180 s après que le réseau et l'heure soient prêts.
    ecran::mer_client_demarrer();
    ecran::serveur_web_demarrer(); // page de réglages sur le port 80 dès que l'adresse IP est obtenue
    ecran::memoire_jalon("après le lancement de la tâche du serveur web");
    ecran::commande_serie_demarrer();
    ecran::memoire_jalon("fin de app_main (commande série lancée)");
}
