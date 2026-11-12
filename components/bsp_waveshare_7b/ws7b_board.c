// Couche carte de la Waveshare ESP32-P4-WIFI6-Touch-LCD-7B : implementation de board.h (interface commune, ADR-0005).
//
// Meme interface, memes jalons de demarrage que components/bsp_tab5. Ce qui est different sur cette carte :
//  - dalle EK79007 1024 x 600, DEJA en paysage : l'interface a la taille de la dalle ; rotation 0 (7B d'Axel) ou 180 (montage des
//    demos Waveshare), choisie a la compilation (WS7B_ROTATION) ;
//  - aucun expandeur d'E/S : les jalons « expandeur » et « controleur detecte » sont ceux de « E/S pretes » et « dalle creee »
//    (voir les commentaires de board_init) ;
//  - tactile GT911 interroge (INT non relie) ;
//  - retroeclairage analogique : un pourcentage nul eteint aussi la dalle (WS7B_REAL_PANEL_OFF).
// Aucun `if (tab5)` ailleurs dans le projet : le code applicatif ne connait que board.h.
#include <stdio.h>

#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ws7b_internal.h"

static const char *TAG = "ws7b";

static board_info_t s_info = {
    .board_name = "Waveshare 7B",
    .display_controller = "EK79007", // seul modele de cette carte : annonce, pas detecte (voir ws7b_display.c)
    .touch_controller = "inconnu",
    .width = 0, // fixees par ui_size() puis relues de LVGL
    .height = 0,
    .panel_width = WS7B_LCD_H_RES,
    .panel_height = WS7B_LCD_V_RES,
    .rotation_degrees = WS7B_UI_ROTATION_DEGREES,
};
static board_boot_timeline_t s_timeline;
static lv_display_t *s_display;
static char s_touch_label[40];

static int32_t now_ms(void) { return (int32_t)(esp_timer_get_time() / 1000); }

const board_info_t *board_get_info(void) { return &s_info; }

const board_boot_timeline_t *board_get_boot_timeline(void) { return &s_timeline; }

lv_display_t *board_get_display(void) { return s_display; }

bool board_display_lock(uint32_t timeout_ms) { return lvgl_port_lock(timeout_ms); }

void board_display_unlock(void) { lvgl_port_unlock(); }

esp_err_t board_set_backlight_percent(int percent) { return ws7b_backlight_apply(percent); }

static lv_display_rotation_t lvgl_rotation(void) {
#if WS7B_UI_ROTATION_DEGREES == 180
    return LV_DISPLAY_ROTATION_180;
#else
    return LV_DISPLAY_ROTATION_0;
#endif
}

static esp_err_t lvgl_attach(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io, esp_lcd_touch_handle_t touch) {
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    // Pile de la tache LVGL : 8192 octets au lieu des 7168 par defaut d'esp_lvgl_port. En -O2 la marge minimale mesuree au demarrage
    // tombait a 716 o (Tab5) et 584 o (7B) ; +1 Ko de memoire interne (docs/mesures/optimisation-o2.md).
    port_cfg.task_stack = 8192;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl_port_init");

    // Taille LOGIQUE attendue apres la rotation (0 et 180 la laissent en 1024 x 600) : controlee ci-dessous.
    int ui_w = 0;
    int ui_h = 0;
    ws7b_ui_size(WS7B_UI_ROTATION_DEGREES, &ui_w, &ui_h);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = WS7B_LCD_H_RES * WS7B_DRAW_BUFFER_LINES,
        .double_buffer = true,
        .hres = WS7B_LCD_H_RES,
        .vres = WS7B_LCD_V_RES,
        .monochrome = false,
        .rotation = {.swap_xy = false, .mirror_x = false, .mirror_y = false}, // sans objet : la rotation est logicielle
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags =
            {
                .buff_dma = true,
                .buff_spiram = false,
                .sw_rotate = (WS7B_UI_ROTATION_DEGREES !=
                              0), // rotation LOGICIELLE par le PPA du P4 ; rien a faire en rotation 0
                .swap_bytes = false,
                .full_refresh = false,
                .direct_mode = false,
            },
    };
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {.flags = {.avoid_tearing = false}};
    s_display = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    ESP_RETURN_ON_FALSE(s_display, ESP_FAIL, TAG, "lvgl_port_add_disp_dsi");

    lvgl_port_lock(0);
    if (WS7B_UI_ROTATION_DEGREES != 0) {
        lv_display_set_rotation(s_display, lvgl_rotation());
    }
    s_info.width = lv_display_get_horizontal_resolution(s_display);
    s_info.height = lv_display_get_vertical_resolution(s_display);
    lvgl_port_unlock();
    ESP_RETURN_ON_FALSE(s_info.width == ui_w && s_info.height == ui_h, ESP_FAIL, TAG,
                        "taille logique inattendue apres rotation : %d x %d (attendu %d x %d)", (int)s_info.width,
                        (int)s_info.height, ui_w, ui_h);
    ESP_LOGI(TAG, "rotation LVGL %d° : interface %d x %d sur une dalle %d x %d", WS7B_UI_ROTATION_DEGREES,
             (int)s_info.width, (int)s_info.height, WS7B_LCD_H_RES, WS7B_LCD_V_RES);

    if (touch == NULL) {
        return ESP_OK; // pas de tactile (voir board_init) : l'ecran s'affiche quand meme
    }
    const lvgl_port_touch_cfg_t touch_port_cfg = {.disp = s_display, .handle = touch};
    lv_indev_t *indev = lvgl_port_add_touch(&touch_port_cfg);
    ESP_RETURN_ON_FALSE(indev, ESP_FAIL, TAG, "lvgl_port_add_touch");
    // INT n'est pas relie au P4 : on interroge la puce a chaque periode de LVGL.
    lvgl_port_lock(0);
    lv_indev_set_mode(indev, LV_INDEV_MODE_TIMER);
    // Lecture toutes les 10 ms au lieu des 33 ms de LVGL (LV_DEF_REFR_PERIOD) : un appui est vu plus tot. Effet NON mesure sur la
    // carte. LVGL 9.5 n'a pas de lv_indev_set_read_period : on regle la minuterie de lecture de l'indev.
    lv_timer_set_period(lv_indev_get_read_timer(indev), 10);
    ws7b_touch_trace(indev);
    lvgl_port_unlock();
    return ESP_OK;
}

esp_err_t board_init(void) {
    s_timeline.init_start_ms = now_ms();
    ws7b_wifi_note_boot_ms(s_timeline.init_start_ms); // le C6 est alimente des la mise sous tension

    // 1. Retroeclairage a 0 %, ampli coupe, bus I2C. Pas d'expandeur a attendre : ce jalon est celui de « E/S pretes ».
    ESP_RETURN_ON_ERROR(ws7b_io_init(), TAG, "E/S");
    s_timeline.expander_ready_ms = now_ms();

    // 2. DSI + dalle (le reset materiel de la dalle, GPIO33, est fait par la creation). Le controleur n'est pas detecte : jalon
    // confondu avec « dalle prete ». Le reset du tactile est lance juste avant : ses 60 ms de reprise courent pendant la creation de
    // la dalle (plus de 120 ms d'attentes propres) au lieu de s'ajouter apres elle. L'adresse du GT911 est toujours cherchee APRES
    // la dalle, donc apres le dernier reset qui a pu toucher la puce (voir l'en-tete de ws7b_touch.c). Un echec n'arrete rien :
    // ws7b_touch_create() refait alors l'impulsion complete.
    if (ws7b_touch_reset_start() != ESP_OK) {
        ESP_LOGW(TAG, "reset anticipé du tactile refusé : impulsion complète plus loin");
    }
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(ws7b_display_create(&panel, &io), TAG, "dalle");
    s_timeline.panel_ready_ms = now_ms();
    s_timeline.controller_detected_ms = s_timeline.panel_ready_ms;
    ESP_LOGI(TAG, "Contrôleur d'écran : %s (annoncé, non détecté : seul modèle de cette carte)",
             s_info.display_controller);

    // 3. Tactile : reset, adresse, identifiant produit. Une puce muette n'arrete PAS le demarrage : la carte affiche son ecran
    // (horloge, meteo) sans tactile, et le journal le dit en erreur. (Le Tab5, lui, s'arrete : sa detection du controleur d'ecran
    // passe par le tactile.)
    esp_lcd_touch_handle_t touch = NULL;
    uint8_t touch_address = 0;
    char product_id[8] = "";
    const esp_err_t touch_err = ws7b_touch_create(&touch, &touch_address, product_id);
    if (touch_err == ESP_OK) {
        snprintf(s_touch_label, sizeof(s_touch_label), "Goodix GT%s (0x%02x)", product_id, (unsigned)touch_address);
        s_info.touch_controller = s_touch_label;
        ESP_LOGI(TAG, "Tactile : %s", s_touch_label);
    } else {
        touch = NULL;
        s_info.touch_controller = "absent";
        ESP_LOGE(TAG, "Tactile non initialise (%s) : l'ecran demarre sans tactile", esp_err_to_name(touch_err));
    }

    // 4. LVGL, puis la dalle devient pilotable (extinction reelle) une fois LVGL pret.
    ESP_RETURN_ON_ERROR(lvgl_attach(panel, io, touch), TAG, "LVGL");
    ws7b_panel_bind(io);
    s_timeline.lvgl_ready_ms = now_ms();

    // 5. Detection a l'execution des codecs audio (lecture seule) : consignee, pas utilisee par board.h.
    bool es8311 = false;
    bool es7210 = false;
    ws7b_i2c_detect_codecs(&es8311, &es7210);
    ESP_LOGI(TAG, "Codecs audio sur le bus I2C : ES8311 %s, ES7210 %s", es8311 ? "present" : "ABSENT",
             es7210 ? "present" : "ABSENT");

    ESP_LOGI(TAG,
             "Carte prête : %s, écran %s, interface %dx%d (dalle %dx%d, rotation %d°), jalons ms : E/S %d, dalle %d, "
             "LVGL %d",
             s_info.board_name, s_info.display_controller, (int)s_info.width, (int)s_info.height,
             (int)s_info.panel_width, (int)s_info.panel_height, (int)s_info.rotation_degrees,
             (int)s_timeline.expander_ready_ms, (int)s_timeline.panel_ready_ms, (int)s_timeline.lvgl_ready_ms);

#ifdef WS7B_ESSAI_AUDIO
    ws7b_audio_selftest(); // binaire d'essai seulement : tonalite puis niveau du micro
#endif
    return ESP_OK;
}
