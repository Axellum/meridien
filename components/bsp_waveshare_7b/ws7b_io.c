// Entrees/sorties de la Waveshare 7B : retroeclairage (LEDC), bus I2C, ampli audio, extinction et reveil REELS de la dalle.
//
// Rien ici n'est calcule "a la main" : les rapports cycliques viennent de ws7b_backlight_duty() et l'enchainement
// d'extinction/reveil de ws7b_panel_plan() (include/ws7b_pur.h, testes sur le PC). Ce fichier execute le plan.
//
// Particularites de cette carte (sources : voir l'en-tete de ws7b_pur.h) :
//  - pas d'expandeur d'E/S : tout est sur des broches du P4 ;
//  - le retroeclairage est ANALOGIQUE (BL_CTRL -> filtre RC -> AP3032) : a 0 % la dalle reste faiblement eclairee (constate par
//    Axel le 16/10/2026). D'ou l'extinction reelle de la dalle par ses commandes DCS (voir ws7b_backlight_apply) ;
//  - la sortie LEDC est INVERSEE (output_invert), comme dans le BSP Waveshare et sous ESPHome.
#include <stdio.h>

#include "board.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ws7b_internal.h"

static const char *TAG = "ws7b_io";

#define BACKLIGHT_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_TIMER LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_1
#define BACKLIGHT_LEDC_BITS LEDC_TIMER_10_BIT
_Static_assert(WS7B_BACKLIGHT_BITS == 10, "WS7B_BACKLIGHT_BITS doit valoir LEDC_TIMER_10_BIT");

// Delai de reponse d'une adresse I2C lors d'une detection (ms).
#define I2C_PROBE_TIMEOUT_MS 50

static i2c_master_bus_handle_t s_bus;
static esp_lcd_panel_io_handle_t s_panel_io; // canal DCS de la dalle ; NULL tant que LVGL n'est pas pret
static bool s_panel_on = true;               // la dalle est reveillee (0x11 puis 0x29) apres board_init()
static uint32_t s_last_sleep_change_ms;      // dernier 0x10/0x11 envoye (espacement de 120 ms)
static lv_obj_t *s_veil;                     // voile noir pose sur la couche systeme pendant l'extinction

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

i2c_master_bus_handle_t ws7b_i2c_bus(void) { return s_bus; }

// --- Retroeclairage ----------------------------------------------------------------------------------------------------------
static esp_err_t backlight_init(void) {
    const ledc_timer_config_t timer = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = BACKLIGHT_LEDC_BITS,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = WS7B_BACKLIGHT_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "timer retroeclairage");
    const ledc_channel_config_t channel = {
        .gpio_num = WS7B_PIN_BACKLIGHT,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .duty = 0, // eteint (au minimum analogique) tant que l'application n'a pas dessine sa premiere image
        .hpoint = 0,
        .flags.output_invert = 1,
    };
    return ledc_channel_config(&channel);
}

esp_err_t ws7b_backlight_set_percent(int percent) {
    const uint32_t duty = ws7b_backlight_duty(percent, WS7B_BACKLIGHT_BITS);
    ESP_RETURN_ON_ERROR(ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty), TAG, "duty");
    return ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
}

// --- Ampli audio (NS4150B) ---------------------------------------------------------------------------------------------------
esp_err_t ws7b_amp_set(bool on) { return gpio_set_level((gpio_num_t)WS7B_PIN_AMP_CTRL, on ? 1 : 0); }

static esp_err_t amp_init(void) {
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << WS7B_PIN_AMP_CTRL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level((gpio_num_t)WS7B_PIN_AMP_CTRL, 0), TAG,
                        "niveau initial de l'ampli"); // bas AVANT la sortie
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "broche de l'ampli");
    return ws7b_amp_set(false); // ampli coupe jusqu'au premier son
}

// --- Bus I2C -----------------------------------------------------------------------------------------------------------------
esp_err_t ws7b_io_init(void) {
    // Retroeclairage a 0 % d'abord : la broche flotte tant que le LEDC n'est pas configure.
    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "retroeclairage");
    ESP_RETURN_ON_ERROR(amp_init(), TAG, "ampli");

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = WS7B_I2C_PORT,
        .sda_io_num = WS7B_PIN_I2C_SDA,
        .scl_io_num = WS7B_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "bus I2C");
    return ESP_OK;
}

void ws7b_i2c_detect_codecs(bool *es8311, bool *es7210) {
    // Lecture seule : un simple « START + adresse » (aucune ecriture dans les codecs).
    if (es8311 != NULL) {
        *es8311 = i2c_master_probe(s_bus, WS7B_ES8311_I2C_ADDR, I2C_PROBE_TIMEOUT_MS) == ESP_OK;
    }
    if (es7210 != NULL) {
        *es7210 = i2c_master_probe(s_bus, WS7B_ES7210_I2C_ADDR, I2C_PROBE_TIMEOUT_MS) == ESP_OK;
    }
}

// --- Extinction et reveil reels de la dalle ----------------------------------------------------------------------------------
// Le plan (quelles etapes, dans quel ordre, avec quels delais) est ws7b_panel_plan() ; on l'execute ici. Verrou de LVGL PRIS.
static void veil_show(bool show) {
    if (s_veil == NULL) {
        if (!show) {
            return;
        }
        s_veil = lv_obj_create(lv_layer_sys());
        lv_obj_remove_style_all(s_veil);
        lv_obj_set_size(s_veil, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(s_veil, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(s_veil, LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_veil,
                           LV_OBJ_FLAG_CLICKABLE); // les appuis continuent d'arriver aux objets de dessous (reveil)
        lv_obj_remove_flag(s_veil, LV_OBJ_FLAG_SCROLLABLE);
    }
    if (show) {
        lv_obj_remove_flag(s_veil, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_veil, LV_OBJ_FLAG_HIDDEN);
    }
    lv_refr_now(NULL); // dessine tout de suite : l'etape suivante suppose que la dalle a recu l'image
}

static esp_err_t panel_dcs(uint8_t cmd) {
    const esp_err_t err = esp_lcd_panel_io_tx_param(s_panel_io, cmd, NULL, 0);
    if (cmd == WS7B_DCS_SLEEP_IN || cmd == WS7B_DCS_SLEEP_OUT) {
        s_last_sleep_change_ms = now_ms();
    }
    return err;
}

static esp_err_t panel_transition(bool want_on) {
    ws7b_step_t plan[WS7B_PLAN_MAX];
    const size_t n = ws7b_panel_plan(s_panel_on ? 1 : 0, want_on ? 1 : 0, plan);
    for (size_t i = 0; i < n; i++) {
        esp_err_t err = ESP_OK;
        switch (plan[i].kind) {
        case WS7B_STEP_VEIL_ON:
            veil_show(true);
            break;
        case WS7B_STEP_VEIL_OFF:
            veil_show(false);
            break;
        case WS7B_STEP_DCS:
            err = panel_dcs(plan[i].dcs);
            break;
        case WS7B_STEP_WAIT:
            vTaskDelay(pdMS_TO_TICKS(plan[i].ms));
            break;
        case WS7B_STEP_SPACE: {
            const uint32_t wait = ws7b_wait_remaining(now_ms(), s_last_sleep_change_ms, WS7B_PANEL_SLEEP_SPACING_MS);
            if (wait > 0) {
                vTaskDelay(pdMS_TO_TICKS(wait));
            }
            break;
        }
        default:
            break;
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "dalle : etape %u (kind %u, dcs 0x%02x) refusee : %s", (unsigned)i, (unsigned)plan[i].kind,
                     (unsigned)plan[i].dcs, esp_err_to_name(err));
            return err;
        }
    }
    if (n > 0) {
        s_panel_on = want_on;
        ESP_LOGI(TAG, "dalle %s (%u etapes)", want_on ? "reveillee" : "eteinte", (unsigned)n);
    }
    return ESP_OK;
}

void ws7b_panel_bind(esp_lcd_panel_io_handle_t io) {
    s_panel_io = io;
    s_panel_on = true;
    s_last_sleep_change_ms = now_ms(); // le « sleep out » de l'initialisation vient d'etre envoye
}

// Corps de board_set_backlight_percent(). Une transition de la dalle se fait sous le verrou de LVGL (recursif : l'appelant peut deja
// le tenir) ; un simple changement de luminosite ne prend aucun verrou.
esp_err_t ws7b_backlight_apply(int percent) {
#if WS7B_REAL_PANEL_OFF
    const bool want_on = percent > 0;
    if (s_panel_io != NULL && want_on != s_panel_on) {
        if (!lvgl_port_lock(2000)) {
            ESP_LOGE(TAG, "verrou de LVGL non pris en 2 s : la dalle n'est pas %s", want_on ? "reveillee" : "eteinte");
            return ESP_ERR_TIMEOUT;
        }
        // Reveil AVANT d'allumer le retroeclairage (pas d'image parasite) ; extinction AVANT de le baisser.
        const esp_err_t err = (want_on != s_panel_on) ? panel_transition(want_on) : ESP_OK; // re-test sous verrou
        lvgl_port_unlock();
        ESP_RETURN_ON_ERROR(err, TAG, "transition de la dalle");
    }
#endif
    return ws7b_backlight_set_percent(percent);
}
