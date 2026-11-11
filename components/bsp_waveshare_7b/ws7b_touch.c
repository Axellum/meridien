// Tactile Goodix GT911 de la Waveshare 7B : reset, recherche de l'adresse, creation du pilote, trace des appuis, auto-test.
//
// Particularites de la carte (sources : en-tete de include/ws7b_pur.h) :
//  - l'interruption (INT) du GT911 n'est PAS reliee au P4 : on interroge la puce a chaque periode de LVGL (mode « timer ») ;
//  - sans INT, l'adresse I2C de la puce (0x5D ou 0x14) n'est pas imposee par le niveau de INT au reset : on essaie les deux ;
//  - le reset (RESET_TP) est sur GPIO23 [schema et ESPHome] ; le BSP Waveshare declare cette broche « NC, partagee avec le reset de
//    la dalle ». Ecart NON tranche (aucun essai sur la carte) : ce fichier pilote GPIO23 ET suppose que le reset de la dalle
//    (GPIO33, fait AVANT, par ws7b_display.c) peut aussi reinitialiser la puce. L'ordre dans board_init() (dalle, puis tactile)
//    garantit que l'adresse trouvee ici est celle qui sera utilisee ;
//  - le GT911 est monte a l'envers de la dalle : miroir X et Y GARDES (BSP Waveshare et ESPHome). Le point rendu au code appelant
//    est dans le repere de la dalle ; c'est LVGL qui applique ensuite la rotation de l'interface (comme sur le Tab5).
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ws7b_internal.h"

static const char *TAG = "ws7b_touch";

// Impulsion de reset du GT911 : bas >= 10 ms, puis ~50 ms avant la premiere parole I2C (fiche technique Goodix, non relue ici :
// valeurs du pilote Espressif, qui attend 10 + 10 ms, rallongees par prudence).
#define RESET_LOW_MS 10
#define RESET_SETTLE_MS 60
#define PROBE_TIMEOUT_MS 100

// Instant (ms, esp_timer) ou la ligne de reset est remontee ; RESET_SETTLE_MS se compte depuis lui.
static uint32_t s_release_ms;
static bool s_reset_started;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

// Impulsion basse puis remontee, SANS attendre RESET_SETTLE_MS : l'attente est faite par wait_settle(), juste avant la premiere parole
// I2C. Entre les deux, board_init() cree la dalle (reset GPIO33, 120 ms, initialisation) : le delai court pendant ce travail au lieu
// d'etre attendu a vide sur le chemin de la premiere image (revue du 12/11/2026). Les valeurs ne changent pas.
static esp_err_t reset_start(void) {
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << WS7B_PIN_TOUCH_RST,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "broche de reset");
    ESP_RETURN_ON_ERROR(gpio_set_level((gpio_num_t)WS7B_PIN_TOUCH_RST, 0), TAG, "reset bas");
    vTaskDelay(pdMS_TO_TICKS(RESET_LOW_MS));
    ESP_RETURN_ON_ERROR(gpio_set_level((gpio_num_t)WS7B_PIN_TOUCH_RST, 1), TAG, "reset haut");
    s_release_ms = now_ms();
    s_reset_started = true;
    return ESP_OK;
}

// Ce qui reste de RESET_SETTLE_MS depuis la remontee (rien s'il est ecoule). Le +1 couvre l'arrondi de now_ms() a la milliseconde.
static void wait_settle(void) {
    const uint32_t wait = ws7b_wait_remaining(now_ms(), s_release_ms, RESET_SETTLE_MS + 1);
    if (wait > 0) {
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

static esp_err_t reset_pulse(void) {
    ESP_RETURN_ON_ERROR(reset_start(), TAG, "reset du tactile");
    wait_settle();
    return ESP_OK;
}

esp_err_t ws7b_touch_reset_start(void) { return reset_start(); }

// Adresse a laquelle la puce repond (0 si aucune). Lecture seule : un « START + adresse ».
static uint8_t find_address(void) {
    if (i2c_master_probe(ws7b_i2c_bus(), WS7B_GT911_ADDR, PROBE_TIMEOUT_MS) == ESP_OK) {
        return WS7B_GT911_ADDR;
    }
    if (i2c_master_probe(ws7b_i2c_bus(), WS7B_GT911_ADDR_BACKUP, PROBE_TIMEOUT_MS) == ESP_OK) {
        return WS7B_GT911_ADDR_BACKUP;
    }
    return 0;
}

esp_err_t ws7b_touch_create(esp_lcd_touch_handle_t *touch, uint8_t *address, char product_id[8]) {
    product_id[0] = '\0';
    *address = 0;

    // Reset puis recherche de l'adresse ; un second reset si la puce ne repond pas du tout. Le premier reset a pu etre lance plus tot
    // par ws7b_touch_reset_start() : on n'attend alors que ce qui reste de RESET_SETTLE_MS.
    uint8_t found = 0;
    for (int attempt = 1; attempt <= 2 && found == 0; attempt++) {
        if (attempt == 1 && s_reset_started) {
            wait_settle();
        } else {
            ESP_RETURN_ON_ERROR(reset_pulse(), TAG, "reset du tactile");
        }
        found = find_address();
        if (found == 0) {
            ESP_LOGW(TAG, "GT911 muet aux adresses 0x%02x et 0x%02x (essai %d/2)", WS7B_GT911_ADDR,
                     WS7B_GT911_ADDR_BACKUP, attempt);
        }
    }
    if (found == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    *address = found;

    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.dev_addr = found;
    io_cfg.scl_speed_hz = WS7B_I2C_HZ;
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(ws7b_i2c_bus(), &io_cfg, &io), TAG, "E/S I2C du tactile");

    // Identifiant produit (4 octets : '9' '1' '1' 0), lu AVANT de creer le pilote : une puce qui repond a l'adresse mais n'est pas
    // un GT911 est signalee clairement au lieu d'etre prise pour une autre.
    uint8_t id[4] = {0, 0, 0, 0};
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_rx_param(io, WS7B_GT911_PRODUCT_ID_REG, id, sizeof(id)), TAG,
                        "identifiant produit");
    snprintf(product_id, 8, "%c%c%c", (id[0] >= 32 && id[0] < 127) ? id[0] : '?',
             (id[1] >= 32 && id[1] < 127) ? id[1] : '?', (id[2] >= 32 && id[2] < 127) ? id[2] : '?');
    if (!ws7b_gt911_product_id_ok(id)) {
        ESP_LOGW(TAG, "identifiant produit inattendu : %02x %02x %02x %02x (attendu '9' '1' '1' 0)", id[0], id[1],
                 id[2], id[3]);
    }

    // x_max / y_max = dernier pixel (1023, 599) : le miroir du pilote calcule `max - valeur`, donc le point reste dans la dalle.
    esp_lcd_touch_config_t cfg = {0};
    cfg.x_max = WS7B_LCD_H_RES - 1;
    cfg.y_max = WS7B_LCD_V_RES - 1;
    cfg.rst_gpio_num =
        GPIO_NUM_NC; // le reset est fait ci-dessus : le pilote n'en refait pas (l'adresse ne doit plus changer)
    cfg.int_gpio_num = GPIO_NUM_NC; // INT non relie au P4
    cfg.levels.reset = 0;
    cfg.levels.interrupt = 0;
    cfg.flags.swap_xy = WS7B_GT911_SWAP_XY;
    cfg.flags.mirror_x = WS7B_GT911_MIRROR_X;
    cfg.flags.mirror_y = WS7B_GT911_MIRROR_Y;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(io, &cfg, touch), TAG, "pilote GT911");
    return ESP_OK;
}

// --- Trace des appuis et auto-test -------------------------------------------------------------------------------------------
// Une ligne par appui avec le point BRUT (repere de la dalle, tel que rendu par le pilote, ramene dans la dalle) et le point
// LOGIQUE (apres la rotation de LVGL). Meme texte que sur le Tab5 : « appui : brut (x, y) ... -> logique (x, y) ... ».
//
// AUTOTEST (WS7B_AUTOTEST_TACTILE=1 : `.\tools\idf.ps1 "-DWS7B_AUTOTEST_TACTILE=1" build`, `=0` pour revenir au binaire normal) :
// 10 s apres le demarrage, cinq appuis SIMULES passent par le meme chemin que ceux du doigt (lecture tracee -> LVGL -> evenement
// PRESSED). Chacun journalise « AUTOTEST ... OK/ECART ». Cela prouve le calcul de la rotation SANS doigt ; cela ne prouve pas que
// la puce renvoie les bons points (il faut un doigt : voir docs/mesures/7b-*.md).
#ifndef WS7B_AUTOTEST_TACTILE
#define WS7B_AUTOTEST_TACTILE 0
#endif

static lv_indev_read_cb_t s_read_origin;
static bool s_pressed;
static lv_point_t s_raw;

#if WS7B_AUTOTEST_TACTILE
#define AUTOTEST_START_TICKS 100   // 100 ms par tick : 10 s apres l'installation de la trace
#define AUTOTEST_TICKS_PER_PRESS 8 // 3 ticks appuye (300 ms, sous l'appui long de 400 ms) puis 5 relache
static int s_auto_tick;
static int s_auto_current = -1;
static bool s_auto_active;
static bool s_auto_pressed;
static lv_point_t s_auto_raw;

static void autotest_step(lv_timer_t *timer) {
    s_auto_tick++;
    if (s_auto_tick < AUTOTEST_START_TICKS) {
        return;
    }
    const int rel = s_auto_tick - AUTOTEST_START_TICKS;
    const int k = rel / AUTOTEST_TICKS_PER_PRESS;
    const int phase = rel % AUTOTEST_TICKS_PER_PRESS;
    if (k >= WS7B_AUTOTEST_POINTS) {
        s_auto_active = false;
        s_auto_pressed = false;
        s_auto_current = -1;
        ESP_LOGI(TAG, "AUTOTEST tactile : fin des %d appuis simules", WS7B_AUTOTEST_POINTS);
        lv_timer_delete(timer);
        return;
    }
    if (phase == 0) {
        // Point BRUT de la dalle qui doit devenir le point logique voulu apres la rotation de LVGL. 0 et 180 degres sont leur propre
        // inverse (ws7b_rotate_point).
        int rx = 0;
        int ry = 0;
        ws7b_rotate_point(WS7B_UI_ROTATION_DEGREES, WS7B_AUTOTEST_X[k], WS7B_AUTOTEST_Y[k], WS7B_LCD_H_RES,
                          WS7B_LCD_V_RES, &rx, &ry);
        s_auto_current = k;
        s_auto_raw.x = rx;
        s_auto_raw.y = ry;
        s_auto_active = true;
        s_auto_pressed = true;
    } else if (phase == 3) {
        s_auto_pressed = false;
    }
}
#endif

static void traced_read(lv_indev_t *indev, lv_indev_data_t *data) {
#if WS7B_AUTOTEST_TACTILE
    if (s_auto_active) {
        data->point = s_auto_raw;
        data->state = s_auto_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        data->continue_reading = false;
    } else {
        s_read_origin(indev, data);
    }
#else
    s_read_origin(indev, data);
#endif
    // Ramene le point dans la dalle (le GT911 peut deborder d'un pixel au bord apres le miroir) avant que LVGL ne le tourne.
    data->point.x = ws7b_clamp(data->point.x, 0, WS7B_LCD_H_RES - 1);
    data->point.y = ws7b_clamp(data->point.y, 0, WS7B_LCD_V_RES - 1);
    const bool pressed = data->state == LV_INDEV_STATE_PRESSED;
    if (pressed && !s_pressed) {
        s_raw = data->point; // premier point de l'appui, avant la rotation
    }
    s_pressed = pressed;
}

static void on_press(lv_event_t *e) {
    // La CIBLE de l'evenement est l'indev (voir tab5_orientation.c : lv_event_get_indev() lirait l'objet touche et planterait).
    lv_indev_t *indev = (lv_indev_t *)lv_event_get_target(e);
    lv_point_t logical = {0, 0};
    lv_indev_get_point(indev, &logical);
    lv_display_t *disp = lv_indev_get_display(indev);
    ESP_LOGI(TAG, "appui : brut (%d, %d) sur la dalle %d x %d -> logique (%d, %d) sur l'interface %d x %d",
             (int)s_raw.x, (int)s_raw.y, WS7B_LCD_H_RES, WS7B_LCD_V_RES, (int)logical.x, (int)logical.y,
             (int)lv_display_get_horizontal_resolution(disp), (int)lv_display_get_vertical_resolution(disp));
#if WS7B_AUTOTEST_TACTILE
    if (s_auto_active && s_auto_current >= 0) {
        const bool ok = logical.x == WS7B_AUTOTEST_X[s_auto_current] && logical.y == WS7B_AUTOTEST_Y[s_auto_current];
        ESP_LOGI(TAG, "AUTOTEST appui %d/%d : cible logique (%d, %d), brut injecte (%d, %d), obtenu (%d, %d) : %s",
                 s_auto_current + 1, WS7B_AUTOTEST_POINTS, WS7B_AUTOTEST_X[s_auto_current],
                 WS7B_AUTOTEST_Y[s_auto_current], (int)s_raw.x, (int)s_raw.y, (int)logical.x, (int)logical.y,
                 ok ? "OK" : "ECART");
    }
#endif
}

void ws7b_touch_trace(lv_indev_t *indev) {
    s_read_origin = lv_indev_get_read_cb(indev);
    if (s_read_origin == NULL) {
        return;
    }
    lv_indev_set_read_cb(indev, traced_read);
    lv_indev_add_event_cb(indev, on_press, LV_EVENT_PRESSED, NULL);
#if WS7B_AUTOTEST_TACTILE
    lv_timer_create(autotest_step, 100, NULL);
    ESP_LOGW(
        TAG,
        "AUTOTEST tactile compile : %d appuis simules 10 s apres le demarrage (binaire d'essai, pas le binaire normal)",
        WS7B_AUTOTEST_POINTS);
#endif
}
