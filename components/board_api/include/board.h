// Interface commune des cartes (ADR-0005). Le code applicatif n'inclut que ce fichier : il ne sait pas sur quelle carte il
// tourne. Chaque carte (components/bsp_<carte>) fournit ces fonctions ; boards/<carte>/ porte ses reglages de construction.
//
// Convention : orientation native fixe (pas de rotation logicielle, risque de dechirure sur P4) ; l'interface se dessine a
// partir de board_get_info()->width / height, jamais en coordonnees absolues.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_rom_sys.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *board_name; // "M5Stack Tab5"
    const char
        *display_controller;      // detecte a l'execution, avant d'initialiser le DSI : "ST7121", "ST7123", "ILI9881C"
    const char *touch_controller; // puce tactile lue, avec sa version : "Sitronix (version 1)"
    int32_t width;                // pixels, orientation native
    int32_t height;
} board_info_t;

// Jalons du demarrage, en millisecondes depuis le lancement de l'application (esp_timer), pour mesurer avant de sacrifier.
typedef struct {
    int32_t init_start_ms;          // entree dans board_init()
    int32_t expander_ready_ms;      // attente de l'expandeur ecoulee et expandeur initialise
    int32_t controller_detected_ms; // controleur d'ecran identifie (apres le reset du LCD et du tactile)
    int32_t panel_ready_ms;         // DSI + dalle initialises
    int32_t lvgl_ready_ms;          // LVGL, affichage et tactile enregistres
} board_boot_timeline_t;

// Alimentations, reset de l'ecran et du tactile, detection du controleur, DSI, dalle, LVGL, tactile.
// Le retroeclairage reste eteint : l'application l'allume (board_set_backlight_percent) une fois sa premiere image dessinee.
esp_err_t board_init(void);

const board_info_t *board_get_info(void);
const board_boot_timeline_t *board_get_boot_timeline(void);

// Ecran LVGL cree par board_init(). LVGL tourne dans sa propre tache : verrouiller (board_display_lock) avant d'y toucher.
lv_display_t *board_get_display(void);

// Verrou de LVGL. timeout_ms = 0 : attendre sans limite. Rend false si le verrou n'a pas pu etre pris.
bool board_display_lock(uint32_t timeout_ms);
void board_display_unlock(void);

esp_err_t board_set_backlight_percent(int percent);

// Redemarrage logiciel. Sur ESP32-P4, esp_restart() ne remet a zero que les coeurs (boucle du lien C6 vue sur la 7B) :
// la seule forme autorisee dans ce projet est la remise a zero du systeme par la ROM.
static inline void __attribute__((noreturn)) board_restart(void) {
    esp_rom_software_reset_system();
    for (;;) {
    }
}

#ifdef __cplusplus
}
#endif
