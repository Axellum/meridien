// Tactile Sitronix du Tab5 (ST7121 et ST7123 : meme puce tactile integree, meme adresse 0x55, meme protocole).
// Le registre du registre Espressif n'a pas de pilote "st7121" : le BSP d'Espressif et ESPHome utilisent le pilote ST7123
// pour les deux (esp_lcd_touch_st7123 1.0.2).
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_st7123.h"
#include "esp_log.h"

#include "tab5_internal.h"

static const char *TAG = "tab5_touch";

esp_err_t tab5_touch_create(esp_lcd_touch_handle_t *touch)
{
    // Orientation native de la dalle (portrait) : ni echange ni miroir. Le reset est fait par l'expandeur
    // (tab5_touch_reset_pulse) : le pilote ne gere donc aucune broche de reset.
    esp_lcd_touch_config_t touch_cfg = {0};
    touch_cfg.x_max = TAB5_LCD_H_RES;
    touch_cfg.y_max = TAB5_LCD_V_RES;
    touch_cfg.rst_gpio_num = GPIO_NUM_NC;
    touch_cfg.int_gpio_num = TAB5_PIN_TOUCH_INT;
    touch_cfg.levels.reset = 0;
    touch_cfg.levels.interrupt = 0;  // front descendant (comme le BSP d'Espressif)

    const esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(tab5_i2c_bus(), &io_cfg, &io), TAG, "E/S I2C du tactile");
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_st7123(io, &touch_cfg, touch), TAG, "pilote tactile");
    return ESP_OK;
}
