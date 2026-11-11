// Phase 1b : heure et date réelles sur l'écran, par le Wi-Fi du coprocesseur ESP32-C6 et SNTP (docs/plan.md). Les lignes de
// diagnostic de la phase 1a (écran, tactile, compteur de démarrages) restent sous l'heure.
#include "board.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "src/display/lv_display_private.h" // le drapeau `flushing` (voir attendre_fin_du_dernier_transfert)

#include "boot_info.h"
#include "commande_serie.h"
#include "ecran_principal.h"
#include "mer_client.h"
#include "meteo_client.h"
#include "reglages.h"
#include "reseau.h"
#include "serveur_web.h"
#include "tache_donnees.h"
#include "vigie_client.h"

static const char *TAG = "ecran";

// Rétroéclairage : la luminosité réglée par la page (reglages.h) ; 75 % tant qu'elle n'a jamais été réglée, comme le firmware
// ESPHome du Tab5 (moins de courant et de chaleur que 100 % ; un brownout a été vu à 100 % sur une alimentation juste, audit du
// 16/10/2026).

// lv_refr_now() rend la main avant la fin du transfert DMA2D de la dernière bande vers le tampon d'image : esp_lvgl_port ne
// signale « terminé » (lv_display_flush_ready, qui baisse `flushing`) que dans le rappel de fin de transfert. LVGL n'offre pas de
// lecture publique de ce drapeau, d'où l'en-tête privé (déjà utilisé par main/ui_details_ouvrir.cpp pour les tampons). Avant, une
// attente fixe de 100 ms couvrait ce transfert. Ici : on attend la fin réelle (au plus 100 ms, l'ancienne attente, si le rappel ne
// vient jamais), puis une période de balayage de la dalle (60 Hz, ~17 ms) pour que l'image entière ait été envoyée au moins une
// fois quand la lumière s'allume. Renvoie la durée attendue, en ms, pour le journal.
static int attendre_fin_du_dernier_transfert() {
    constexpr int64_t kAttenteMaxUs = 100 * 1000;
    const int64_t debut_us = esp_timer_get_time();
    lv_display_t *const ecran = board_get_display();
    while (ecran->flushing && (esp_timer_get_time() - debut_us) < kAttenteMaxUs) {
        vTaskDelay(1);
    }
    vTaskDelay(pdMS_TO_TICKS(20)); // une période de balayage à 60 Hz
    return static_cast<int>((esp_timer_get_time() - debut_us) / 1000);
}

static int maintenant_ms() { return static_cast<int>(esp_timer_get_time() / 1000); }

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "écran autonome, phase 1b ; ESP-IDF %s ; application démarrée à %d ms", esp_get_idf_version(),
             static_cast<int>(esp_timer_get_time() / 1000));

    ecran::memoire_jalon("début de app_main");
    const ecran::BootInfo boot = ecran::boot_info_load();
    ecran::reglages_demarrer(); // après la NVS : langue de l'écran et luminosité réglées par la page
    const int t_reglages = maintenant_ms();
    ecran::memoire_jalon("après la NVS et les réglages");

    const esp_err_t err = board_init();
    if (err != ESP_OK) {
        // Pas de redémarrage automatique ici : une boucle de redémarrages masquerait la cause et userait le matériel.
        ESP_LOGE(TAG, "carte non initialisée : %s ; arrêt (l'écran reste éteint)", esp_err_to_name(err));
        return;
    }
    const int t_carte = maintenant_ms();
    ecran::memoire_jalon("après board_init (écran, tactile, LVGL, tampons)");

    // La météo du cache NVS est relue AVANT l'écran principal : sa première image la montre au lieu de « météo indisponible »
    // jusqu'au passage suivant du minuteur. Sans Wi-Fi ni heure, comme le reste de la relecture du cache.
    ecran::meteo_client_relire_cache();
    const int t_cache = maintenant_ms();

    int t_ecran = -1;
    int t_image = -1;
    if (board_display_lock(0)) {
        // Écran principal (heure, date, météo) chargé ; l'écran « Bonjour » des phases 1a/1b devient l'écran de diagnostic
        // (appui long sur le fond pour passer de l'un à l'autre).
        ecran::ecran_principal_creer(boot);
        t_ecran = maintenant_ms();
        lv_refr_now(board_get_display()); // première image dessinée avant d'allumer le rétroéclairage
        t_image = maintenant_ms();
        board_display_unlock();
    }
    ecran::memoire_jalon("après l'écran principal (objets, police, première image)");
    const int attente_ms = attendre_fin_du_dernier_transfert();
    const int luminosite = ecran::reglages_affichage().luminosite;
    board_set_backlight_percent(luminosite);
    const int t_lumiere = maintenant_ms();
    ESP_LOGI(TAG, "rétroéclairage allumé à %d %% à %d ms ; première image affichée (dernier transfert attendu %d ms)",
             luminosite, t_lumiere, attente_ms);
    // Une ligne par démarrage avec chaque étape jusqu'à la lumière (ms depuis le début de l'application, esp_timer) : de quoi comparer
    // deux binaires sur la carte sans recouper plusieurs lignes. -1 : étape non faite.
    const board_boot_timeline_t *jalons = board_get_boot_timeline();
    ESP_LOGI(
        TAG,
        "DEMARRAGE ms : réglages %d, carte %d (début %d, E/S %d, contrôleur %d, dalle %d, LVGL %d), cache météo %d, "
        "écran créé %d, première image rendue %d, lumière %d",
        t_reglages, t_carte, (int)jalons->init_start_ms, (int)jalons->expander_ready_ms,
        (int)jalons->controller_detected_ms, (int)jalons->panel_ready_ms, (int)jalons->lvgl_ready_ms, t_cache, t_ecran,
        t_image, t_lumiere);

    // Le réseau démarre après la première image : l'écran garde le temps de démarrage mesuré en 1a (docs/mesures/1b.md dit
    // si le Wi-Fi l'a changé).
    ecran::reseau_demarrer(boot);
    ecran::memoire_jalon("après le lancement de la tâche réseau (pile 8 Ko)");
    // Une seule tâche pour les requêtes de données (météo, alertes, mer, liste des spots) : tache_donnees.h.
    ecran::tache_donnees_demarrer();
    ecran::memoire_jalon("après le lancement de la tâche réseau des données (pile 12 Ko)");
    ecran::
        meteo_client_demarrer(); // météo Open-Meteo : cache NVS déjà relu avant la première image, requêtes dès le Wi-Fi et l'heure
    ecran::memoire_jalon("après le démarrage du client météo (cache NVS)");
    ecran::
        vigie_client_demarrer(); // alertes officielles de Vigie : premier essai 60 à 120 s après que le réseau et l'heure soient prêts
    ecran::memoire_jalon("après le démarrage du client des alertes");
    // Mer du spot choisi (ADR-0019) : le client ne démarre que si le registre des spots n'est pas vide (appareil neuf : rien n'est
    // demandé à Vigie). Premier essai 120 à 180 s après que le réseau et l'heure soient prêts.
    ecran::mer_client_demarrer();
    ecran::serveur_web_demarrer(); // page de réglages sur le port 80 dès que l'adresse IP est obtenue
    ecran::memoire_jalon("après le lancement de la tâche du serveur web");
    ecran::commande_serie_demarrer();
    ecran::memoire_jalon("fin de app_main (commande série lancée)");
}
