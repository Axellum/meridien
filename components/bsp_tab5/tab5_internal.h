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
#include "lvgl.h"

// --- Broches (les memes sur les trois revisions d'ecran) ---
#define TAB5_PIN_I2C_SDA GPIO_NUM_31
#define TAB5_PIN_I2C_SCL GPIO_NUM_32
#define TAB5_PIN_BACKLIGHT GPIO_NUM_22 // LEDC
#define TAB5_PIN_TOUCH_INT GPIO_NUM_23
#define TAB5_I2C_PORT I2C_NUM_1

// Expandeur PI4IOE5V6408 en 0x43 : bit 4 = reset de l'ecran (LCD_RST), bit 5 = reset / activation du tactile.
#define TAB5_EXPANDER_LCD_RST IO_EXPANDER_PIN_NUM_4
#define TAB5_EXPANDER_TOUCH_RST IO_EXPANDER_PIN_NUM_5

// Expandeur PI4IOE5V6408 en 0x44 : bit 0 = WIFI_EN, alimentation du coprocesseur ESP32-C6 (docs/tab5-pinout.md du depot Tab5,
// "Wi-Fi power" ; M5Stack l'appelle BSP_WIFI_EN). Les autres bits (alimentation USB, charge de la batterie, extinction) ne sont
// PAS touches par ce projet.
#define TAB5_EXPANDER_POWER_WIFI_EN IO_EXPANDER_PIN_NUM_0
// Reset du coprocesseur : GPIO15, impulsion generee par esp_hosted (reglages de boards/tab5/sdkconfig.defaults.board).

// Dalle NATIVE : portrait 720 x 1280 (le DSI et le tampon d'image sont dans cette orientation).
#define TAB5_LCD_H_RES 720
#define TAB5_LCD_V_RES 1280
// Orientation de l'interface : paysage, comme le firmware ESPHome du Tab5 (`carte_rotation: "270"`, geometrie-tab5.yaml).
// 90 est la meme rotation exprimee dans la convention de LVGL / d'esp_lvgl_port (lv_display_set_rotation) : memes formules
// de zone et de tactile que ESPHome, voir docs/mesures/orientation.md. La rotation materielle de la dalle ne marche pas sur
// ST7121 (espp) : elle est logicielle, par l'accelerateur PPA du P4.
#define TAB5_UI_ROTATION_DEGREES 90
#define TAB5_UI_WIDTH TAB5_LCD_V_RES  // largeur logique apres rotation
#define TAB5_UI_HEIGHT TAB5_LCD_H_RES // hauteur logique apres rotation
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
// Alimentation du coprocesseur radio (expandeur 0x44, bit 0). tab5_wifi_power_init() est appelee par tab5_io_init() ; une
// erreur n'empeche pas l'ecran de demarrer (le Wi-Fi sera simplement absent).
bool tab5_wifi_power_available(void);
esp_err_t tab5_wifi_power_set(bool on);

// tab5_wifi.c
void tab5_wifi_note_power_on_ms(int32_t ms);

// tab5_detect.c : lit la version du tactile Sitronix (ou cherche le GT911) ; attend que la puce reponde.
esp_err_t tab5_detect_controller(tab5_display_controller_t *controller, uint8_t *touch_version, int32_t *elapsed_ms);

// tab5_display.c : cree le bus DSI, la dalle (reset logiciel + initialisation) pour le controleur detecte.
esp_err_t tab5_display_create(tab5_display_controller_t controller, esp_lcd_panel_handle_t *panel,
                              esp_lcd_panel_io_handle_t *io);

// tab5_touch.c : tactile Sitronix (pilote ST7123, valable pour le ST7121 aussi, voir tab5_touch.c).
esp_err_t tab5_touch_create(esp_lcd_touch_handle_t *touch);

// tab5_orientation.c : rotation logicielle de LVGL (paysage), mesures de l'affichage, banc PPA contre processeur.
// tab5_orientation_apply : verrou de LVGL pris par l'appelant, a appeler une fois l'affichage cree.
void tab5_orientation_apply(lv_display_t *disp);
void tab5_orientation_bench(void); // une seule fois, au demarrage (mesure, retire quand le choix est consigne)
