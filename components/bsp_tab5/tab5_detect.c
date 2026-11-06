// Detection du controleur d'ecran du Tab5, AVANT d'initialiser le DSI : les trois revisions ont des sequences d'init et des
// debits DSI differents, et se tromper donne un ecran noir (docs/recherche/01-pile-et-qualite.md).
//
// Methode (identique dans le BSP d'Espressif et dans espp) :
//   - une puce Sitronix repond en 0x55 : sa version de firmware est au registre 16 bits 0x0000 ; 1 = ST7121, 3 = ST7123 ;
//   - sinon un GT911 en 0x14 = ILI9881C d'origine.
// La puce tactile met jusqu'a ~50 ms a repondre apres son reset : on interroge toutes les 10 ms, jusqu'a une seconde et demie.
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tab5_internal.h"

static const char *TAG = "tab5_detect";

#define DETECT_PERIOD_MS 10
#define DETECT_MAX_ATTEMPTS 150
#define SITRONIX_VERSION_REG_HIGH 0x00
#define SITRONIX_VERSION_REG_LOW 0x00
#define SITRONIX_VERSION_ST7121 1
#define SITRONIX_VERSION_ST7123 3

const char *tab5_display_controller_name(tab5_display_controller_t controller) {
    switch (controller) {
    case TAB5_DISPLAY_ILI9881C:
        return "ILI9881C";
    case TAB5_DISPLAY_ST7123:
        return "ST7123";
    case TAB5_DISPLAY_ST7121:
        return "ST7121";
    default:
        return "inconnu";
    }
}

esp_err_t tab5_detect_controller(tab5_display_controller_t *controller, uint8_t *touch_version, int32_t *elapsed_ms) {
    *controller = TAB5_DISPLAY_UNKNOWN;
    *touch_version = 0;
    i2c_master_bus_handle_t bus = tab5_i2c_bus();

    // Peripherique temporaire pour la lecture du registre ; supprime avant de creer le pilote tactile (meme adresse 0x55).
    const i2c_device_config_t device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TAB5_I2C_ADDR_SITRONIX_TOUCH,
        .scl_speed_hz = 100000, // meme vitesse que la configuration du pilote tactile ST7123
    };
    i2c_master_dev_handle_t device = NULL;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &device_cfg, &device), TAG, "peripherique 0x55");

    const int64_t start_us = esp_timer_get_time();
    int last_logged_version = -1;
    bool sitronix_acked = false;
    for (int attempt = 0; attempt < DETECT_MAX_ATTEMPTS && *controller == TAB5_DISPLAY_UNKNOWN; ++attempt) {
        if (i2c_master_probe(bus, TAB5_I2C_ADDR_SITRONIX_TOUCH, 20) == ESP_OK) {
            sitronix_acked = true;
            const uint8_t reg[2] = {SITRONIX_VERSION_REG_HIGH, SITRONIX_VERSION_REG_LOW};
            uint8_t version = 0;
            if (i2c_master_transmit_receive(device, reg, sizeof(reg), &version, 1, 50) == ESP_OK) {
                if (version != last_logged_version) {
                    last_logged_version = version;
                    ESP_LOGI(TAG, "version du tactile Sitronix lue : %u (tentative %d)", (unsigned)version,
                             attempt + 1);
                }
                if (version == SITRONIX_VERSION_ST7121) {
                    *controller = TAB5_DISPLAY_ST7121;
                    *touch_version = version;
                } else if (version == SITRONIX_VERSION_ST7123) {
                    *controller = TAB5_DISPLAY_ST7123;
                    *touch_version = version;
                }
                // Autre valeur : la puce demarre peut-etre encore, on continue d'interroger.
            }
        } else if (i2c_master_probe(bus, TAB5_I2C_ADDR_GT911_BACKUP, 20) == ESP_OK) {
            *controller = TAB5_DISPLAY_ILI9881C;
        }
        if (*controller == TAB5_DISPLAY_UNKNOWN) {
            vTaskDelay(pdMS_TO_TICKS(DETECT_PERIOD_MS));
        }
    }
    i2c_master_bus_rm_device(device);
    *elapsed_ms = (int32_t)((esp_timer_get_time() - start_us) / 1000);

    if (*controller == TAB5_DISPLAY_UNKNOWN) {
        // Pas de supposition (espp retombe sur ST7123) : un mauvais choix donne un ecran noir sans explication.
        ESP_LOGE(TAG, "controleur non identifie apres %d ms (%s)", (int)*elapsed_ms,
                 sitronix_acked ? "puce Sitronix presente mais version illisible ou inconnue"
                                : "aucune puce tactile ne repond");
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}
