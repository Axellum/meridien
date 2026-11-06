#include "boot_info.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace ecran {

namespace {

const char *TAG = "boot_info";
const char *kNamespace = "ecran";
const char *kKeyBootCount = "boots";

const char *reset_reason_fr(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:
        return "mise sous tension";
    case ESP_RST_SW:
        return "redémarrage logiciel";
    case ESP_RST_PANIC:
        return "plantage (exception)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return "chien de garde";
    case ESP_RST_BROWNOUT:
        return "chute de tension";
    case ESP_RST_DEEPSLEEP:
        return "sortie de veille profonde";
    case ESP_RST_USB:
        return "réinitialisation par l'USB";
    case ESP_RST_EXT:
        return "broche de réinitialisation";
    default:
        return "cause inconnue";
    }
}

}  // namespace

BootInfo boot_info_load()
{
    BootInfo info = {0, false, reset_reason_fr(esp_reset_reason())};
    ESP_LOGI(TAG, "cause du redémarrage : %s (code %d)", info.reset_reason, static_cast<int>(esp_reset_reason()));

    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        // Partition jamais formatée (premier démarrage après un flash) ou contenu étranger : en phase 1a elle ne porte que ce
        // compteur, on la reformate. À ne plus faire dès qu'elle portera des réglages (phase 3).
        ESP_LOGW(TAG, "NVS illisible (%s) : reformatage (le compteur repart de zéro)", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS inutilisable : %s", esp_err_to_name(err));
        return info;
    }

    nvs_handle_t handle = 0;
    err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open : %s", esp_err_to_name(err));
        return info;
    }
    uint32_t count = 0;
    err = nvs_get_u32(handle, kKeyBootCount, &count);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "lecture du compteur : %s", esp_err_to_name(err));
    }
    count += 1;
    err = nvs_set_u32(handle, kKeyBootCount, count);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    info.boot_count = count;
    info.counter_persistent = (err == ESP_OK);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "écriture du compteur : %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "démarrage n° %u", static_cast<unsigned>(count));
    return info;
}

}  // namespace ecran
