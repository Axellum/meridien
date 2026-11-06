// Wi-Fi du Tab5 : lien P4 <-> ESP32-C6 par esp_hosted (SDIO), version du coprocesseur, reprise sur panne.
// Implemente la partie Wi-Fi de board.h (ADR-0005). Sources lues le 06/11/2026 dans le dossier de construction d'ESPHome
// (lecture seule) : esp_hosted 2.12.12 (host/api/src/esp_hosted_api.c, host/drivers/transport/transport_drv.c et sdio_drv.c)
// et esp_wifi_remote 1.6.3, couple deja en service sur cette tablette sous ESPHome.
//
// Ce que ces sources montrent et que ce fichier respecte :
//  - esp_hosted_init() tourne dans un constructeur, avant app_main : il ne prepare que le bus SDIO, il ne parle PAS au
//    coprocesseur. La remise a zero du C6 (GPIO15) et l'initialisation de la carte SDIO se font a la premiere demande de
//    connexion (esp_hosted_connect_to_slave(), ou esp_wifi_init() qui l'appelle). L'alimentation du C6 (expandeur 0x44, bit 0,
//    allumee dans tab5_io_init()) doit donc etre en place avant cet appel, pas avant app_main.
//  - esp_hosted_connect_to_slave() ne rend PAS son echec : elle journalise l'erreur et rend ESP_OK. Le seul signe fiable que le
//    lien est etabli est l'evenement ESP_HOSTED_EVENT_TRANSPORT_UP, attendu ici.
//  - Avec ESP_HOSTED_TRANSPORT_RESTART_ON_FAILURE active, le pilote appelle esp_restart(), qui sur ESP32-P4 ne remet a zero que
//    les coeurs (AGENTS.md, regle de redemarrage). Cette option est desactivee (boards/tab5/sdkconfig.defaults.board) : le pilote
//    envoie ESP_HOSTED_EVENT_TRANSPORT_FAILURE et c'est ce fichier qui redemarre, par board_restart().
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "tab5_internal.h"

static const char *TAG = "tab5_wifi";

#define EV_TRANSPORT_UP BIT0

// Attente maximale de l'evenement "transport actif" apres que esp_hosted_connect_to_slave() a rendu la main.
#define EVENT_WAIT_MS 4000
// Coupure puis remise de l'alimentation du coprocesseur lors d'un second essai (500 ms de coupure : valeur citee par la
// recherche docs/recherche/05, issue esp-hosted-mcu #240 ; non mesuree ici).
#define POWER_CYCLE_OFF_MS 500
#define POWER_CYCLE_SETTLE_MS 300
// Une panne du transport ne redemarre pas la carte avant ce delai (laisse partir le journal ; limite une boucle de redemarrages).
#define RESTART_DELAY_MS 2000

static board_wifi_info_t s_info = {.power_on_ms = -1, .link_up_ms = -1};
static EventGroupHandle_t s_events;
static bool s_handler_registered;
static volatile bool s_restart_pending;

static int32_t now_ms(void) { return (int32_t)(esp_timer_get_time() / 1000); }

void tab5_wifi_note_power_on_ms(int32_t ms) { s_info.power_on_ms = ms; }

static void restart_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
    ESP_LOGW(TAG, "redémarrage logiciel après une panne du lien avec le coprocesseur (esp_rom_software_reset_system)");
    vTaskDelay(pdMS_TO_TICKS(200));
    board_restart();
}

static void on_hosted_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)base;
    switch (id) {
    case ESP_HOSTED_EVENT_CP_INIT: {
        const esp_hosted_event_init_t *init = (const esp_hosted_event_init_t *)data;
        ESP_LOGI(TAG, "coprocesseur démarré (cause de sa remise à zéro : %d)", init != NULL ? (int)init->reason : -1);
        break;
    }
    case ESP_HOSTED_EVENT_TRANSPORT_UP:
        s_info.link_up = true;
        s_info.link_up_ms = now_ms();
        if (s_events != NULL) {
            xEventGroupSetBits(s_events, EV_TRANSPORT_UP);
        }
        ESP_LOGI(TAG, "lien avec le coprocesseur établi à %d ms", (int)s_info.link_up_ms);
        break;
    case ESP_HOSTED_EVENT_TRANSPORT_DOWN:
        s_info.link_up = false;
        if (s_events != NULL) {
            xEventGroupClearBits(s_events, EV_TRANSPORT_UP);
        }
        ESP_LOGW(TAG, "lien avec le coprocesseur coupé");
        break;
    case ESP_HOSTED_EVENT_TRANSPORT_FAILURE:
        s_info.transport_failures++;
        ESP_LOGE(TAG, "panne du transport avec le coprocesseur (n° %u)", (unsigned)s_info.transport_failures);
        if (!s_restart_pending) {
            s_restart_pending = true;
            if (xTaskCreate(restart_task, "wifi_restart", 3072, NULL, 5, NULL) != pdPASS) {
                s_restart_pending = false;
                ESP_LOGE(TAG, "tâche de redémarrage non créée");
            }
        }
        break;
    default: // ESP_HOSTED_EVENT_CP_HEARTBEAT, ESP_HOSTED_EVENT_MEM_MONITOR : sans intérêt ici
        break;
    }
}

static void read_versions(void) {
    snprintf(s_info.host_version, sizeof(s_info.host_version), "%d.%d.%d", ESP_HOSTED_VERSION_MAJOR_1,
             ESP_HOSTED_VERSION_MINOR_1, ESP_HOSTED_VERSION_PATCH_1);
    esp_hosted_coprocessor_fwver_t version;
    memset(&version, 0, sizeof(version));
    const int err = esp_hosted_get_coprocessor_fwversion(&version); // lecture seule : RPC "version", rien n'est ecrit
    if (err == ESP_OK) {
        snprintf(s_info.coprocessor_version, sizeof(s_info.coprocessor_version), "%u.%u.%u", (unsigned)version.major1,
                 (unsigned)version.minor1, (unsigned)version.patch1);
        s_info.version_known = true;
    } else {
        s_info.coprocessor_version[0] = '\0';
        s_info.version_known = false;
        ESP_LOGW(TAG, "version du coprocesseur non lue (%s)", esp_err_to_name(err));
    }
}

esp_err_t board_wifi_link_up(uint32_t timeout_ms) {
    if (!tab5_wifi_power_available()) {
        return ESP_ERR_INVALID_STATE; // l'alimentation du coprocesseur n'a pas pu etre allumee
    }
    if (s_events == NULL) {
        s_events = xEventGroupCreate();
        ESP_RETURN_ON_FALSE(s_events != NULL, ESP_ERR_NO_MEM, TAG, "groupe d'événements");
    }
    // esp_hosted poste ses evenements sur la boucle par defaut : elle doit exister avant le premier appel au coprocesseur.
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { // ESP_ERR_INVALID_STATE = deja creee
        return err;
    }
    if (!s_handler_registered) {
        ESP_RETURN_ON_ERROR(esp_event_handler_register(ESP_HOSTED_EVENT, ESP_EVENT_ANY_ID, on_hosted_event, NULL), TAG,
                            "gestionnaire d'événements esp_hosted");
        s_handler_registered = true;
    }

    const int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    for (int attempt = 1; attempt <= 2; attempt++) {
        s_info.attempts = attempt;
        if (attempt > 1) {
            ESP_LOGW(TAG, "second essai : coupure puis remise de l'alimentation du coprocesseur");
            tab5_wifi_power_set(false);
            vTaskDelay(pdMS_TO_TICKS(POWER_CYCLE_OFF_MS));
            tab5_wifi_power_set(true);
            vTaskDelay(pdMS_TO_TICKS(POWER_CYCLE_SETTLE_MS));
        }
        if (esp_timer_get_time() >= deadline_us) {
            break;
        }
        // Remise a zero du C6 (GPIO15), initialisation SDIO, attente de son annonce. Rend ESP_OK meme en cas d'echec.
        ESP_LOGI(TAG, "connexion au coprocesseur (essai %d)", attempt);
        esp_hosted_connect_to_slave();

        const int64_t left_ms = (deadline_us - esp_timer_get_time()) / 1000;
        const TickType_t wait = pdMS_TO_TICKS(left_ms > EVENT_WAIT_MS ? EVENT_WAIT_MS : (left_ms > 0 ? left_ms : 0));
        const EventBits_t bits = xEventGroupWaitBits(s_events, EV_TRANSPORT_UP, pdFALSE, pdTRUE, wait);
        if ((bits & EV_TRANSPORT_UP) != 0) {
            read_versions();
            ESP_LOGI(TAG, "coprocesseur : micrologiciel %s (lu, jamais écrit), esp_hosted côté P4 %s ; essai %d",
                     s_info.version_known ? s_info.coprocessor_version : "inconnu", s_info.host_version, attempt);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "lien non établi à l'essai %d", attempt);
    }
    return ESP_ERR_TIMEOUT;
}

void board_wifi_get_info(board_wifi_info_t *info) {
    if (info != NULL) {
        *info = s_info;
    }
}
