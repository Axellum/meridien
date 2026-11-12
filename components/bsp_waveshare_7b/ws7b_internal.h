// Interne a la couche carte de la Waveshare 7B : constantes ESP-IDF et fonctions partagees entre les fichiers de bsp_waveshare_7b.
// Les VALEURS (broches, minutages, tables) sont dans include/ws7b_pur.h (testees sur le PC) ; ce fichier n'y ajoute que leur
// habillage ESP-IDF (types de broches, ports) et les options de compilation de CMakeLists.txt.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include "ws7b_pur.h"

#define WS7B_I2C_PORT I2C_NUM_1
#define WS7B_I2C_HZ 400000 // 400 kHz : [B] BSP_I2C_CLK_SPEED_HZ et [H] frequency: 400kHz

// --- Options de compilation (definies par CMakeLists.txt de ce composant ; voir docs/carte-7b.md) -----------------------------
// Rotation de l'interface : 0 = 7B d'Axel (montee a l'envers par rapport aux demos Waveshare), 180 = montage des demos Waveshare.
// `.\tools\idf.ps1 "-DWS7B_ROTATION=180" build`. Le CMake refuse toute autre valeur ; le controle ci-dessous le redit au compilateur.
#ifndef WS7B_UI_ROTATION_DEGREES
#define WS7B_UI_ROTATION_DEGREES 0
#endif
#if WS7B_UI_ROTATION_DEGREES != 0 && WS7B_UI_ROTATION_DEGREES != 180
#error "WS7B_UI_ROTATION_DEGREES : 0 ou 180 seulement (ws7b_rotation_valid)"
#endif

// Hauteur (en lignes de 1024 px) du tampon de dessin LVGL, en RAM interne (DMA), en double tampon. `-DWS7B_DRAW_BUFFER_LINES=n`.
#ifndef WS7B_DRAW_BUFFER_LINES
#define WS7B_DRAW_BUFFER_LINES WS7B_DRAW_BUFFER_LINES_DEFAULT
#endif

// 1 (defaut) : board_set_backlight_percent(0) eteint AUSSI la dalle (voile noir, 0x28, 0x10) et un pourcentage > 0 la reveille.
// 0 : le pourcentage ne commande que le retroeclairage (qui reste, sur cette carte, tres legerement allume a 0 %).
#ifndef WS7B_REAL_PANEL_OFF
#define WS7B_REAL_PANEL_OFF 1
#endif

// 1 : lire la version du micrologiciel du C6 (RPC esp_hosted). 0 (defaut) : ne pas la demander : le C6 d'origine n'y repond pas
// (memoire waveshare-7b-materiel-c6 ; le firmware ESPHome de cette carte a retire cet appel). Voir ws7b_wifi.c.
#ifndef WS7B_READ_C6_VERSION
#define WS7B_READ_C6_VERSION 0
#endif

// Binaires d'ESSAI (absents du binaire normal) : WS7B_AUTOTEST_TACTILE (cinq appuis simules), WS7B_ESSAI_AUDIO (tonalite + micro).

// ws7b_io.c ------------------------------------------------------------------------------------------------------------------
esp_err_t ws7b_io_init(void); // retroeclairage a 0 %, bus I2C, ampli coupe
i2c_master_bus_handle_t ws7b_i2c_bus(void);
// Reglage direct du retroeclairage (LEDC) : ne touche pas a la dalle. Sert a board_init() et a ws7b_backlight_apply().
esp_err_t ws7b_backlight_set_percent(int percent);
// Corps de board_set_backlight_percent() : retroeclairage + (si WS7B_REAL_PANEL_OFF) extinction ou reveil reels de la dalle.
esp_err_t ws7b_backlight_apply(int percent);
// A appeler une fois LVGL pret : `io` est le canal DCS de la dalle. Avant cet appel, ws7b_backlight_apply() ne touche pas a la dalle.
void ws7b_panel_bind(esp_lcd_panel_io_handle_t io);
esp_err_t ws7b_amp_set(bool on); // ampli NS4150B (GPIO53)
// Detection a l'execution : les codecs audio repondent-ils sur le bus I2C ? (lecture seule)
void ws7b_i2c_detect_codecs(bool *es8311, bool *es7210);

// ws7b_display.c -------------------------------------------------------------------------------------------------------------
// Alimente la PHY DSI, cree le bus DSI, la dalle EK79007, la reinitialise (broche 33) et l'initialise par WS7B_EK79007_INIT.
esp_err_t ws7b_display_create(esp_lcd_panel_handle_t *panel, esp_lcd_panel_io_handle_t *io);

// ws7b_touch.c ---------------------------------------------------------------------------------------------------------------
// Reset du GT911 (GPIO23), recherche de son adresse (0x5D puis 0x14), lecture de l'identifiant produit, creation du pilote.
// `product_id` : texte lu ("911") ; `address` : adresse I2C trouvee.
// Lance le reset du GT911 (bas 10 ms, puis remontee) sans attendre son delai de reprise : ws7b_touch_create() n'attend ensuite que ce
// qui reste. Facultatif : sans lui, ws7b_touch_create() fait l'impulsion complete.
esp_err_t ws7b_touch_reset_start(void);
esp_err_t ws7b_touch_create(esp_lcd_touch_handle_t *touch, uint8_t *address, char product_id[8]);
// Trace d'une ligne par appui (point brut -> point logique) et, si WS7B_AUTOTEST_TACTILE, cinq appuis simules.
// Verrou de LVGL pris par l'appelant, une fois l'indev cree.
void ws7b_touch_trace(lv_indev_t *indev);

// ws7b_audio.c ---------------------------------------------------------------------------------------------------------------
// Codecs ES8311 (sortie) et ES7210 (entrees) sur le bus I2C et le bus I2S de la carte. Non appele par board_init() : l'audio
// n'est pas dans board.h (voir docs/carte-7b.md). Utilise par l'essai matiere (WS7B_ESSAI_AUDIO) et, plus tard, par la couche
// audio commune.
esp_err_t ws7b_audio_init(void);
esp_err_t ws7b_audio_set_volume_percent(int percent);         // 0 = coupe ; 100 = 0 dB (jamais de gain positif)
esp_err_t ws7b_audio_play_tone(int freq_hz, int duration_ms); // sinus 16 bits mono 16 kHz, au volume courant
esp_err_t ws7b_audio_mic_level(int32_t *rms, int32_t *peak,
                               int duration_ms); // niveau du micro (voie gauche de l'ES7210)
void ws7b_audio_selftest(void);                  // tonalite puis niveau du micro, journalises (essai matiere)

// ws7b_wifi.c ----------------------------------------------------------------------------------------------------------------
void ws7b_wifi_note_boot_ms(int32_t ms);
