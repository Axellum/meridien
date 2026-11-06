// Interne a la couche carte du Tab5 : broches et fonctions partagees entre les fichiers de bsp_tab5.
// Sources lues le 06/11/2026 (aucune reprise de memoire) : BSP esp-bsp m5stack_tab5 1.3.2, BSP communautaire espp
// m5stack-tab5, firmware ESPHome du depot Tab5 (tab5-hardware.yaml, modele mipi_dsi M5STACK-TAB5-ST7121).
#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_io_expander.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

// --- Broches (les memes sur les trois revisions d'ecran) ---
#define TAB5_PIN_I2C_SDA GPIO_NUM_31
#define TAB5_PIN_I2C_SCL GPIO_NUM_32
#define TAB5_PIN_BACKLIGHT GPIO_NUM_22 // LEDC
#define TAB5_PIN_TOUCH_INT GPIO_NUM_23
#define TAB5_I2C_PORT I2C_NUM_1

// Expandeur PI4IOE5V6408 en 0x43 : bit 4 = reset de l'ecran (LCD_RST), bit 5 = reset / activation du tactile.
#define TAB5_EXPANDER_LCD_RST IO_EXPANDER_PIN_NUM_4
#define TAB5_EXPANDER_TOUCH_RST IO_EXPANDER_PIN_NUM_5

#define TAB5_LCD_H_RES 720
#define TAB5_LCD_V_RES 1280
#define TAB5_DSI_LANES 2
// Alimentation de la PHY DSI : canal 3 du regulateur interne, 2,5 V.
#define TAB5_DSI_PHY_LDO_CHANNEL 3
#define TAB5_DSI_PHY_LDO_MV 2500

#define TAB5_I2C_ADDR_SITRONIX_TOUCH 0x55 // ST7121 et ST7123 : meme adresse, version lue au registre 0x0000
#define TAB5_I2C_ADDR_GT911_BACKUP 0x14   // ILI9881C d'origine

typedef enum {
    TAB5_DISPLAY_UNKNOWN = 0,
    TAB5_DISPLAY_ILI9881C, // GT911 en 0x14
    TAB5_DISPLAY_ST7123,   // version tactile Sitronix = 3
    TAB5_DISPLAY_ST7121,   // version tactile Sitronix = 1
} tab5_display_controller_t;

const char *tab5_display_controller_name(tab5_display_controller_t controller);

// tab5_io.c
esp_err_t tab5_io_init(void); // bus I2C, retroeclairage a 0 %, expandeur 0x43
i2c_master_bus_handle_t tab5_i2c_bus(void);
esp_err_t tab5_lcd_reset_pulse(void);   // reset de la dalle (bit 4) : bas en push-pull, puis entree a pull-up
esp_err_t tab5_touch_reset_pulse(void); // reset du tactile (bit 5) : bas puis haut, en push-pull
esp_err_t tab5_touch_enable(void);      // tactile hors reset (bit 5 haut)
esp_err_t tab5_backlight_set_percent(int percent);

// tab5_detect.c : lit la version du tactile Sitronix (ou cherche le GT911) ; attend que la puce reponde.
esp_err_t tab5_detect_controller(tab5_display_controller_t *controller, uint8_t *touch_version, int32_t *elapsed_ms);

// tab5_display.c : cree le bus DSI, la dalle (reset logiciel + initialisation) pour le controleur detecte.
esp_err_t tab5_display_create(tab5_display_controller_t controller, esp_lcd_panel_handle_t *panel,
                              esp_lcd_panel_io_handle_t *io);

// tab5_touch.c : tactile Sitronix (pilote ST7123, valable pour le ST7121 aussi, voir tab5_touch.c).
esp_err_t tab5_touch_create(esp_lcd_touch_handle_t *touch);
