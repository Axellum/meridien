// Bus I2C, expandeur PI4IOE5V6408 (0x43), retroeclairage, reset de l'ecran et du tactile.
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_io_expander_pi4ioe5v6408.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tab5_internal.h"

static const char *TAG = "tab5_io";

#define BACKLIGHT_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_TIMER LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_1
#define BACKLIGHT_LEDC_BITS LEDC_TIMER_10_BIT
// 1 kHz comme le firmware ESPHome en service (ledc, tab5-hardware.yaml) ; le BSP d'Espressif prend 5 kHz.
#define BACKLIGHT_LEDC_HZ 1000

static i2c_master_bus_handle_t s_bus;
static esp_io_expander_handle_t s_expander;

i2c_master_bus_handle_t tab5_i2c_bus(void)
{
    return s_bus;
}

static esp_err_t backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = BACKLIGHT_LEDC_BITS,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = BACKLIGHT_LEDC_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "timer retroeclairage");
    ledc_channel_config_t channel = {
        .gpio_num = TAB5_PIN_BACKLIGHT,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .duty = 0,  // eteint tant que l'application n'a pas dessine sa premiere image
        .hpoint = 0,
    };
    return ledc_channel_config(&channel);
}

esp_err_t tab5_backlight_set_percent(int percent)
{
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    const uint32_t max_duty = (1U << BACKLIGHT_LEDC_BITS) - 1U;
    ESP_RETURN_ON_ERROR(ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, max_duty * (uint32_t)percent / 100U), TAG,
                        "duty");
    return ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
}

esp_err_t tab5_io_init(void)
{
    // Retroeclairage a 0 % d'abord : la broche flotte tant que le LEDC n'est pas configure.
    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "retroeclairage");

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = TAB5_I2C_PORT,
        .sda_io_num = TAB5_PIN_I2C_SDA,
        .scl_io_num = TAB5_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "bus I2C");

    // Attention : creer le pilote de l'expandeur le REMET A ZERO (registre 0x01, puis tous les registres par defaut :
    // sorties a bas, haute impedance). Au demarrage c'est l'etat voulu ; ce n'est pas un appel a refaire en marche.
    ESP_RETURN_ON_ERROR(esp_io_expander_new_i2c_pi4ioe5v6408(s_bus, ESP_IO_EXPANDER_I2C_PI4IOE5V6408_ADDRESS_LOW, &s_expander),
                        TAG, "expandeur 0x43");
    return ESP_OK;
}

// Reset de la dalle. La ligne LCD_RST est tenue a bas en push-pull, puis rendue en ENTREE a pull-up (drain ouvert) pour la
// remonter : la laisser en sortie haute rend l'I2C instable (docs/recherche/01-pile-et-qualite.md ; meme sequence que le BSP
// d'Espressif, bsp_feature_en.c, cas BSP_FEATURE_LCD).
esp_err_t tab5_lcd_reset_pulse(void)
{
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_expander, TAB5_EXPANDER_LCD_RST, IO_EXPANDER_OUTPUT), TAG, "dir");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, TAB5_EXPANDER_LCD_RST, 0), TAG, "niveau bas");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_output_mode(s_expander, TAB5_EXPANDER_LCD_RST, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL),
                        TAG, "push-pull");
    vTaskDelay(pdMS_TO_TICKS(10));  // espp : 10 ms bas ; ESPHome : 5 ms
    ESP_RETURN_ON_ERROR(esp_io_expander_set_pullupdown(s_expander, TAB5_EXPANDER_LCD_RST, IO_EXPANDER_PULL_UP), TAG,
                        "pull-up");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_expander, TAB5_EXPANDER_LCD_RST, IO_EXPANDER_INPUT), TAG, "entree");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_output_mode(s_expander, TAB5_EXPANDER_LCD_RST, IO_EXPANDER_OUTPUT_MODE_OPEN_DRAIN),
                        TAG, "drain ouvert");
    vTaskDelay(pdMS_TO_TICKS(120));  // le controleur n'accepte SLPOUT que ~120 ms apres le reset
    return ESP_OK;
}

esp_err_t tab5_touch_enable(void)
{
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_expander, TAB5_EXPANDER_TOUCH_RST, IO_EXPANDER_OUTPUT), TAG, "dir");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, TAB5_EXPANDER_TOUCH_RST, 1), TAG, "niveau haut");
    return esp_io_expander_set_output_mode(s_expander, TAB5_EXPANDER_TOUCH_RST, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL);
}

esp_err_t tab5_touch_reset_pulse(void)
{
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_expander, TAB5_EXPANDER_TOUCH_RST, IO_EXPANDER_OUTPUT), TAG, "dir");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, TAB5_EXPANDER_TOUCH_RST, 0), TAG, "niveau bas");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_output_mode(s_expander, TAB5_EXPANDER_TOUCH_RST, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL),
                        TAG, "push-pull");
    vTaskDelay(pdMS_TO_TICKS(20));  // espp : 20 ms bas puis 50 ms avant de parler a la puce
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, TAB5_EXPANDER_TOUCH_RST, 1), TAG, "niveau haut");
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}
