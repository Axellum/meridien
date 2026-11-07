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

#ifdef __cplusplus
extern "C" {
#endif

// --- Broches (les memes sur les trois revisions d'ecran) ---
#define TAB5_PIN_I2C_SDA GPIO_NUM_31
#define TAB5_PIN_I2C_SCL GPIO_NUM_32
#define TAB5_PIN_BACKLIGHT GPIO_NUM_22 // LEDC
#define TAB5_PIN_TOUCH_INT GPIO_NUM_23
#define TAB5_I2C_PORT I2C_NUM_1

// Expandeur PI4IOE5V6408 en 0x43 : bit 4 = reset de l'ecran (LCD_RST), bit 5 = reset / activation du tactile.
#define TAB5_EXPANDER_LCD_RST IO_EXPANDER_PIN_NUM_4
#define TAB5_EXPANDER_TOUCH_RST IO_EXPANDER_PIN_NUM_5

// Expandeur 0x43, autres bits (firmware ESPHome du depot Tab5, paquets/tab5-hardware.yaml et tab5-sensors-*.yaml, lus le 07/11/2026) :
// bit 0 = choix de l'antenne interne / externe (NON touche), bit 1 = SPEAKER_EN (amplificateur du haut-parleur), bit 2 = 5 V externe
// (NON touche), bit 7 = detection de la prise casque (entree, 1 = fiche inseree).
#define TAB5_EXPANDER_SPEAKER_EN IO_EXPANDER_PIN_NUM_1
#define TAB5_EXPANDER_HEADPHONE_DETECT IO_EXPANDER_PIN_NUM_7

// Expandeur PI4IOE5V6408 en 0x44 : bit 0 = WIFI_EN, alimentation du coprocesseur ESP32-C6 (docs/tab5-pinout.md du depot Tab5,
// "Wi-Fi power" ; M5Stack l'appelle BSP_WIFI_EN). Le bit 3 (5 V USB) n'est PAS touche par ce projet ; les bits 5 (nCHG_QC_EN,
// charge rapide, actif bas), 6 (CHG_STAT, entree) et 7 (CHG_EN, chargeur) sont lus ou forces a l'arret par tab5_alim.cpp.
#define TAB5_EXPANDER_POWER_WIFI_EN IO_EXPANDER_PIN_NUM_0
#define TAB5_EXPANDER_POWER_NCHG_QC_EN IO_EXPANDER_PIN_NUM_5
#define TAB5_EXPANDER_POWER_CHG_STAT IO_EXPANDER_PIN_NUM_6
#define TAB5_EXPANDER_POWER_CHG_EN IO_EXPANDER_PIN_NUM_7

// Peripheriques I2C du bus commun (GPIO31/32, 400 kHz) : adresses lues dans le firmware ESPHome du depot Tab5 le 07/11/2026.
#define TAB5_I2C_ADDR_ES8388 0x10 // DAC audio (haut-parleur, casque)
#define TAB5_I2C_ADDR_ES7210 0x40 // CAN audio des microphones
#define TAB5_I2C_ADDR_INA226 0x41 // moniteur de courant du connecteur de batterie (shunt 5 mohm)
#define TAB5_I2C_ADDR_RX8130 0x32 // horloge materielle (UTC, supercondensateur de secours)

// Bus audio I2S partage (un seul port) : MCLK, BCLK, WS (LRCK), DOUT vers l'ES8388, DIN depuis l'ES7210.
#define TAB5_PIN_I2S_MCLK GPIO_NUM_30
#define TAB5_PIN_I2S_BCLK GPIO_NUM_27
#define TAB5_PIN_I2S_WS GPIO_NUM_29
#define TAB5_PIN_I2S_DOUT GPIO_NUM_26
#define TAB5_PIN_I2S_DIN GPIO_NUM_28
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
// Retroeclairage : registre brut du PWM (resolution TAB5_BACKLIGHT_BITS). La courbe de luminosite PERCUE (niveau 0..100 -> registre)
// est dans tab5_ecran.cpp ; ici on n'ecrit et ne relit que le registre.
#define TAB5_BACKLIGHT_BITS 10
esp_err_t tab5_backlight_set_duty(uint32_t duty);
uint32_t tab5_backlight_get_duty(void); // relu dans le LEDC (pas une copie logicielle)
// Sorties et entrees des expandeurs, avec un verrou commun (les pilotes de l'expandeur ne sont pas reentrants : le Wi-Fi, l'audio et
// l'alimentation y ecrivent depuis des taches differentes). Ecriture : le niveau est pose AVANT de piloter la broche (pas
// d'impulsion parasite quand elle sort de la haute impedance), push-pull. Lecture : la broche est mise en entree.
typedef enum { TAB5_EXPANDER_ECRAN = 0, TAB5_EXPANDER_ALIM = 1 } tab5_expander_t;
esp_err_t tab5_expander_output(tab5_expander_t expander, uint32_t pin_mask, int level);
esp_err_t tab5_expander_input(tab5_expander_t expander, uint32_t pin_mask, int *level);
esp_err_t tab5_expander_peek(tab5_expander_t expander, uint32_t pin_mask,
                             int *level);                                       // lecture sans changer la direction
esp_err_t tab5_expander_pull_down(tab5_expander_t expander, uint32_t pin_mask); // entree avec tirage vers le bas

// tab5_i2c.c : petits peripheriques du bus commun, registres de 8 bits.
esp_err_t tab5_i2c_attach(uint8_t address, i2c_master_dev_handle_t *dev);
esp_err_t tab5_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len);
esp_err_t tab5_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t *data, size_t len);
esp_err_t tab5_i2c_read_u8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *value);
esp_err_t tab5_i2c_write_u8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value);
esp_err_t tab5_i2c_update_u8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t mask, uint8_t value);
// Sonde sans rien ecrire : true si une puce acquitte son adresse (ex. TAB5_I2C_ADDR_INA226).
bool tab5_i2c_probe(uint8_t address);

// tab5_ecran.cpp : luminosite perçue et veille (LVGL). tab5_ecran_init() apres la creation de l'affichage LVGL.
esp_err_t tab5_ecran_init(lv_display_t *disp);
// tab5_rtc.cpp : horloge materielle. tab5_rtc_init() apres le bus I2C ; reprend l'heure de la puce si le systeme n'en a pas.
esp_err_t tab5_rtc_init(void);
// Trame brute de 7 octets (secondes ... annee), pour l'essai sur carte qui restitue la valeur d'origine.
esp_err_t tab5_rtc_raw_read(uint8_t frame[7]);
esp_err_t tab5_rtc_raw_write(const uint8_t frame[7]);
// tab5_i2s.c : bus audio I2S (un port, duplex, 16 bits stereo). Les horloges tournent tant qu'une direction est activee.
bool tab5_i2s_is_open(void);
uint32_t tab5_i2s_rate(void);
esp_err_t tab5_i2s_open(uint32_t rate, uint32_t *ram_taken); // ram_taken : octets de RAM interne pris par les canaux
esp_err_t tab5_i2s_enable(bool tx, bool rx);                 // met chaque direction dans l'etat demande
esp_err_t tab5_i2s_write(const void *data, size_t bytes, size_t *written, uint32_t timeout_ms);
esp_err_t tab5_i2s_read(void *data, size_t bytes, size_t *read, uint32_t timeout_ms);
esp_err_t tab5_i2s_flush_rx(void);
void tab5_i2s_counters(uint32_t *tx_overflows, uint32_t *rx_overflows);
esp_err_t tab5_i2s_close(void);
// tab5_codec.c : ES8388 (DAC) et ES7210 (CAN). tab5_codec_open() bus I2S actif, DAC en sourdine et volume au plus bas.
typedef enum { TAB5_DAC_LINE1 = 1, TAB5_DAC_LINE2 = 2, TAB5_DAC_BOTH = 3 } tab5_dac_line_t;
esp_err_t tab5_codec_open(uint32_t rate);
esp_err_t tab5_dac_set_output(tab5_dac_line_t line);
esp_err_t tab5_dac_set_volume(int step, bool mute); // pas de 0,5 dB, 0..192
esp_err_t tab5_dac_read_reg(uint8_t reg, uint8_t *value);
esp_err_t tab5_adc_read_ids(uint8_t *id1, uint8_t *id2);
esp_err_t tab5_adc_read_reg(uint8_t reg, uint8_t *value);
// tab5_audio.cpp : politique audio (board_audio_*). Rien n'est initialise avant le premier appel d'une fonction audio. Les
// fonctions suivantes ne servent qu'a l'essai sur carte (tab5_autotest.cpp).
typedef struct {
    uint32_t tone_hz;
    int32_t tone_peak_dbfs_x10;
    int volume_level;
    uint32_t duration_ms;
} tab5_duplex_cfg_t;
typedef struct {
    uint32_t frames_written;
    uint32_t frames_read;
    uint32_t tx_overflows;
    uint32_t rx_overflows;
    uint32_t elapsed_ms;
    uint32_t slowest_block_ms;
    int32_t left_rms_dbfs_x10;
    int32_t left_tone_dbfs_x10;
    int32_t right_rms_dbfs_x10;
    int32_t right_tone_dbfs_x10;
    int speaker_en_pin; // niveau relu de SPEAKER_EN (expandeur 0x43, bit 1) PENDANT le son : 1 attendu, -1 illisible
} tab5_duplex_result_t;
esp_err_t tab5_audio_duplex_probe(const tab5_duplex_cfg_t *cfg, tab5_duplex_result_t *result);
esp_err_t tab5_audio_set_line(tab5_dac_line_t line);
esp_err_t tab5_audio_close(void);
// Niveaux des deux microphones separement (la premiere demi-seconde est jetee), pour reperer un micro mort.
esp_err_t tab5_audio_mic_levels(uint32_t duration_ms, uint32_t tone_hz, int32_t *left_rms_x10, int32_t *right_rms_x10,
                                int32_t *left_tone_x10, int32_t *right_tone_x10);
// tab5_alim.cpp : moniteur de courant et chargeur. tab5_alim_init() apres le bus I2C et les expandeurs.
esp_err_t tab5_alim_init(void);
// tab5_autotest.cpp : essai sur carte de tout ce qui precede (option CMake TAB5_AUTOTEST_MATERIEL) ; sans l'option, ne fait rien.
void tab5_autotest_materiel_start(void);

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
// Journal d'une ligne par appui : point brut de la puce et point final apres la rotation de LVGL. Verrou de LVGL pris.
void tab5_orientation_trace_tactile(lv_indev_t *indev);

#ifdef __cplusplus
}
#endif
