// Wi-Fi de la Waveshare 7B : lien P4 <-> ESP32-C6 par esp_hosted (SDIO), reprise sur panne.
// Implemente la partie Wi-Fi de board.h (ADR-0005), sur le modele de components/bsp_tab5/tab5_wifi.c (memes sources lues, memes
// raisons : voir son en-tete pour les details du fonctionnement d'esp_hosted 2.12.12). Ce qui change sur la 7B :
//  - le C6 est alimente en permanence (aucun interrupteur d'alimentation, aucun expandeur) : il n'y a donc pas de « coupure puis
//    remise de l'alimentation » au second essai ; le second essai refait la remise a zero du C6 par sa broche (GPIO54, R33 0 ohm vers
//    CHIP_PU), ce qu'esp_hosted_connect_to_slave() fait a chaque appel ;
//  - la version du micrologiciel du C6 n'est PAS demandee par defaut (WS7B_READ_C6_VERSION=0) : le C6 d'origine ne repond pas a la
//    requete de version d'esp_hosted (memoire d'Axel waveshare-7b-materiel-c6, 16 au 19/10/2026 ; le firmware ESPHome de cette carte
//    a retire cet appel). `board_wifi_info_t.version_known` reste donc `false` et la ligne de journal le dit ;
//  - le C6 n'est JAMAIS mis a jour par ce code (decision d'Axel du 16/10/2026 : « j'ai peur de bloquer l'ecran »).
//
// Broches et reglages SDIO : boards/waveshare-7b/sdkconfig.defaults.board (CLK 18, CMD 19, D0 a D3 = 14 a 17, reset GPIO54,
// 20 MHz, tampons en PSRAM). Ils viennent du schema et du firmware ESPHome en service ; AUCUN n'a encore ete constate par ce BSP
// sur la carte tant que docs/mesures/7b-*.md ne le dit pas.
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

#include "ws7b_internal.h"

static const char *TAG = "ws7b_wifi";

#define EV_TRANSPORT_UP BIT0

// Attente maximale de l'evenement « transport actif » apres que esp_hosted_connect_to_slave() a rendu la main.
#define EVENT_WAIT_MS 4000
// Pause avant un second essai (laisse le C6 finir son demarrage ; valeur du Tab5, a mesurer sur la 7B).
#define SECOND_TRY_SETTLE_MS 500
// Une panne du transport ne redemarre pas la carte avant ce delai (laisse partir le journal ; limite une boucle de redemarrages).
#define RESTART_DELAY_MS 2000

static board_wifi_info_t s_info = {.power_on_ms = -1, .link_up_ms = -1};
static EventGroupHandle_t s_events;
static bool s_handler_registered;
static volatile bool s_restart_pending;

static int32_t now_ms(void) { return (int32_t)(esp_timer_get_time() / 1000); }

// Le C6 est alimente des la mise sous tension de la carte : on note l'instant ou board_init() demarre.
void ws7b_wifi_note_boot_ms(int32_t ms) { s_info.power_on_ms = ms; }

static void restart_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
    ESP_LOGW(TAG, "redemarrage logiciel apres une panne du lien avec le coprocesseur (esp_rom_software_reset_system)");
    vTaskDelay(pdMS_TO_TICKS(200));
    board_restart();
}

static void on_hosted_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)base;
    switch (id) {
    case ESP_HOSTED_EVENT_CP_INIT: {
        const esp_hosted_event_init_t *init = (const esp_hosted_event_init_t *)data;
        ESP_LOGI(TAG, "coprocesseur demarre (cause de sa remise a zero : %d)", init != NULL ? (int)init->reason : -1);
        break;
    }
    case ESP_HOSTED_EVENT_TRANSPORT_UP:
        s_info.link_up = true;
        s_info.link_up_ms = now_ms();
        if (s_events != NULL) {
            xEventGroupSetBits(s_events, EV_TRANSPORT_UP);
        }
        ESP_LOGI(TAG, "lien avec le coprocesseur etabli a %d ms", (int)s_info.link_up_ms);
        break;
    case ESP_HOSTED_EVENT_TRANSPORT_DOWN:
        s_info.link_up = false;
        if (s_events != NULL) {
            xEventGroupClearBits(s_events, EV_TRANSPORT_UP);
        }
        ESP_LOGW(TAG, "lien avec le coprocesseur coupe");
        break;
    case ESP_HOSTED_EVENT_TRANSPORT_FAILURE:
        s_info.transport_failures++;
        ESP_LOGE(TAG, "panne du transport avec le coprocesseur (n° %u)", (unsigned)s_info.transport_failures);
        if (!s_restart_pending) {
            s_restart_pending = true;
            if (xTaskCreate(restart_task, "wifi_restart", 3072, NULL, 5, NULL) != pdPASS) {
                s_restart_pending = false;
                ESP_LOGE(TAG, "tache de redemarrage non creee");
            }
        }
        break;
    default: // ESP_HOSTED_EVENT_CP_HEARTBEAT, ESP_HOSTED_EVENT_MEM_MONITOR : sans interet ici
        break;
    }
}

static void read_versions(void) {
    snprintf(s_info.host_version, sizeof(s_info.host_version), "%d.%d.%d", ESP_HOSTED_VERSION_MAJOR_1,
             ESP_HOSTED_VERSION_MINOR_1, ESP_HOSTED_VERSION_PATCH_1);
#if WS7B_READ_C6_VERSION
    esp_hosted_coprocessor_fwver_t version;
    memset(&version, 0, sizeof(version));
    const int err = esp_hosted_get_coprocessor_fwversion(&version); // lecture seule : RPC « version », rien n'est ecrit
    if (err == ESP_OK) {
        snprintf(s_info.coprocessor_version, sizeof(s_info.coprocessor_version), "%u.%u.%u", (unsigned)version.major1,
                 (unsigned)version.minor1, (unsigned)version.patch1);
        s_info.version_known = true;
    } else {
        s_info.coprocessor_version[0] = '\0';
        s_info.version_known = false;
        ESP_LOGW(TAG, "version du coprocesseur non lue (%s)", esp_err_to_name(err));
    }
#else
    s_info.coprocessor_version[0] = '\0';
    s_info.version_known = false; // non demandee : le C6 d'origine n'y repond pas (voir l'en-tete)
#endif
}

esp_err_t board_wifi_link_up(uint32_t timeout_ms) {
    if (s_events == NULL) {
        s_events = xEventGroupCreate();
        ESP_RETURN_ON_FALSE(s_events != NULL, ESP_ERR_NO_MEM, TAG, "groupe d'evenements");
    }
    // esp_hosted poste ses evenements sur la boucle par defaut : elle doit exister avant le premier appel au coprocesseur.
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { // ESP_ERR_INVALID_STATE = deja creee
        return err;
    }
    if (!s_handler_registered) {
        ESP_RETURN_ON_ERROR(esp_event_handler_register(ESP_HOSTED_EVENT, ESP_EVENT_ANY_ID, on_hosted_event, NULL), TAG,
                            "gestionnaire d'evenements esp_hosted");
        s_handler_registered = true;
    }

    const int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    for (int attempt = 1; attempt <= 2; attempt++) {
        s_info.attempts = attempt;
        if (attempt > 1) {
            ESP_LOGW(TAG, "second essai : nouvelle remise a zero du coprocesseur (pas d'interrupteur d'alimentation "
                          "sur cette carte)");
            vTaskDelay(pdMS_TO_TICKS(SECOND_TRY_SETTLE_MS));
        }
        if (esp_timer_get_time() >= deadline_us) {
            break;
        }
        // Remise a zero du C6 (GPIO54), initialisation SDIO, attente de son annonce. Rend ESP_OK meme en cas d'echec.
        ESP_LOGI(TAG, "connexion au coprocesseur (essai %d)", attempt);
        esp_hosted_connect_to_slave();

        const int64_t left_ms = (deadline_us - esp_timer_get_time()) / 1000;
        const TickType_t wait = pdMS_TO_TICKS(left_ms > EVENT_WAIT_MS ? EVENT_WAIT_MS : (left_ms > 0 ? left_ms : 0));
        const EventBits_t bits = xEventGroupWaitBits(s_events, EV_TRANSPORT_UP, pdFALSE, pdTRUE, wait);
        if ((bits & EV_TRANSPORT_UP) != 0) {
            read_versions();
            ESP_LOGI(TAG, "coprocesseur : micrologiciel %s (jamais ecrit), esp_hosted cote P4 %s ; essai %d",
                     s_info.version_known ? s_info.coprocessor_version : "non demande", s_info.host_version, attempt);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "lien non etabli a l'essai %d", attempt);
    }
    return ESP_ERR_TIMEOUT;
}

void board_wifi_get_info(board_wifi_info_t *info) {
    if (info != NULL) {
        *info = s_info;
    }
}
