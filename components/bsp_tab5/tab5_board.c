// Couche carte du M5Stack Tab5 : implementation de board.h (interface commune, ADR-0005).
#include <stdio.h>

#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tab5_internal.h"

static const char *TAG = "tab5";

// Le PI4IOE5V6408 (0x43) a besoin d'environ 1 s apres le demarrage avant que son reset de l'ecran soit fiable : sans cette
// attente l'ecran reste noir apres un redemarrage logiciel alors que tout le reste marche (cause confirmee le 06/07/2026 par
// une preuve par elimination, voir tab5-hardware.yaml du depot Tab5 ; ADR-0005 du depot Tab5). Delai a mesurer et a reduire
// plus tard seulement si un essai le permet, jamais a l'oeil.
#define EXPANDER_SETTLE_MS 1000

// Tampon de dessin LVGL : 50 lignes de la LARGEUR NATIVE de la dalle, soit 36 000 pixels (72 Ko), en RAM interne (DMA) et en double
// tampon (valeurs du BSP d'Espressif). Apres la rotation, LVGL rend des bandes de 1280 px de large : 28 lignes par bande, donc
// 26 bandes pour un ecran entier. Le tampon de sortie du PPA (meme taille) est cree par esp_lvgl_port.
#define LVGL_DRAW_BUF_LINES 50

static board_info_t s_info = {
    .board_name = "M5Stack Tab5",
    .display_controller = "inconnu",
    .touch_controller = "inconnu",
    .width = TAB5_UI_WIDTH, // taille logique, apres la rotation (voir tab5_orientation.c)
    .height = TAB5_UI_HEIGHT,
    .panel_width = TAB5_LCD_H_RES,
    .panel_height = TAB5_LCD_V_RES,
    .rotation_degrees = TAB5_UI_ROTATION_DEGREES,
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

// board_set_backlight_percent(), la veille, la RTC, l'audio et l'alimentation : tab5_ecran.cpp, tab5_rtc.cpp, tab5_audio.cpp,
// tab5_alim.cpp (leur logique pure est dans components/core).

static esp_err_t lvgl_attach(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io, esp_lcd_touch_handle_t touch) {
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    // Pile de la tache LVGL : 8192 octets au lieu des 7168 par defaut d'esp_lvgl_port. En -O2 la marge minimale mesuree au demarrage
    // tombait a 716 o (Tab5) et 584 o (7B) ; +1 Ko de memoire interne (docs/mesures/optimisation-o2.md).
    port_cfg.task_stack = 8192;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl_port_init");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = TAB5_LCD_H_RES * LVGL_DRAW_BUF_LINES,
        .double_buffer = true,
        .hres = TAB5_LCD_H_RES,
        .vres = TAB5_LCD_V_RES,
        .monochrome = false,
        .rotation = {.swap_xy = false, .mirror_x = false, .mirror_y = false}, // sans objet : la rotation est logicielle
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags =
            {
                .buff_dma = true,
                .buff_spiram = false,
                .sw_rotate = true, // rotation LOGICIELLE, par le PPA du P4 (CONFIG_LVGL_PORT_ENABLE_PPA, boards/tab5/)
                .swap_bytes = false,
                .full_refresh = false,
                .direct_mode = false,
            },
    };
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {.flags = {.avoid_tearing = false}};
    s_display = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    ESP_RETURN_ON_FALSE(s_display, ESP_FAIL, TAG, "lvgl_port_add_disp_dsi");
    // Paysage : LVGL tourne l'image de 90 degres (convention LVGL ; = `rotation: 270` d'ESPHome). La taille logique devient
    // 1280 x 720 ; le tactile est tourne dans le meme sens par tab5_touch_create().
    lvgl_port_lock(0);
    tab5_orientation_apply(s_display);
    s_info.width = lv_display_get_horizontal_resolution(s_display);
    s_info.height = lv_display_get_vertical_resolution(s_display);
    lvgl_port_unlock();
    ESP_RETURN_ON_FALSE(s_info.width == TAB5_UI_WIDTH && s_info.height == TAB5_UI_HEIGHT, ESP_FAIL, TAG,
                        "taille logique inattendue apres rotation : %d x %d", (int)s_info.width, (int)s_info.height);

    const lvgl_port_touch_cfg_t touch_port_cfg = {.disp = s_display, .handle = touch};
    lv_indev_t *indev = lvgl_port_add_touch(&touch_port_cfg);
    ESP_RETURN_ON_FALSE(indev, ESP_FAIL, TAG, "lvgl_port_add_touch");
    // L'interruption (GPIO23) reveille la tache LVGL, mais on lit aussi le tactile a chaque periode de LVGL : en mode
    // "evenement seul" un relachement dont l'interruption serait perdue laisserait le doigt "appuye" indefiniment.
    lvgl_port_lock(0);
    lv_indev_set_mode(indev, LV_INDEV_MODE_TIMER);
    // Lecture toutes les 10 ms au lieu des 33 ms de LVGL (LV_DEF_REFR_PERIOD) : un appui est vu plus tot. Effet NON mesure sur la
    // carte. LVGL 9.5 n'a pas de lv_indev_set_read_period : on regle la minuterie de lecture de l'indev.
    lv_timer_set_period(lv_indev_get_read_timer(indev), 10);
    tab5_orientation_trace_tactile(indev);
    lvgl_port_unlock();
    return ESP_OK;
}

esp_err_t board_init(void) {
    s_timeline.init_start_ms = now_ms();

    // 1. Laisser l'expandeur se stabiliser (voir EXPANDER_SETTLE_MS), puis initialiser I2C, retroeclairage a 0 %, expandeur.
    vTaskDelay(pdMS_TO_TICKS(EXPANDER_SETTLE_MS));
    ESP_RETURN_ON_ERROR(tab5_io_init(), TAG, "E/S");
    s_timeline.expander_ready_ms = now_ms();
    // Horloge materielle (reprend l'heure du systeme si elle n'en a pas) et alimentation (chargeur a l'arret) : une erreur est
    // journalisee mais n'empeche jamais l'ecran de demarrer.
    if (tab5_rtc_init() != ESP_OK) {
        ESP_LOGW(TAG, "horloge matérielle indisponible");
    }
    if (tab5_alim_init() != ESP_OK) {
        ESP_LOGW(TAG, "moniteur d'alimentation indisponible");
    }

    // 2. Tactile hors reset, puis reset de la dalle, puis detection du controleur : AVANT de toucher au DSI.
    ESP_RETURN_ON_ERROR(tab5_touch_enable(), TAG, "activation du tactile");
    ESP_RETURN_ON_ERROR(tab5_lcd_reset_pulse(), TAG, "reset de la dalle");
    tab5_display_controller_t controller = TAB5_DISPLAY_UNKNOWN;
    uint8_t touch_version = 0;
    int32_t detect_ms = 0;
    esp_err_t err = tab5_detect_controller(&controller, &touch_version, &detect_ms);
    if (err != ESP_OK) {
        // La puce tactile ne repond pas toujours apres le premier reset : un second, sur le tactile seul.
        ESP_LOGW(TAG, "controleur non identifie, nouveau reset du tactile puis seconde detection");
        ESP_RETURN_ON_ERROR(tab5_touch_reset_pulse(), TAG, "reset du tactile");
        err = tab5_detect_controller(&controller, &touch_version, &detect_ms);
    }
    ESP_RETURN_ON_ERROR(err, TAG, "detection du controleur d'ecran");
    s_info.display_controller = tab5_display_controller_name(controller);
    if (controller == TAB5_DISPLAY_ILI9881C) {
        snprintf(s_touch_label, sizeof(s_touch_label), "%s", "Goodix GT911");
    } else {
        snprintf(s_touch_label, sizeof(s_touch_label), "Sitronix (version %u)", (unsigned)touch_version);
    }
    s_info.touch_controller = s_touch_label;
    s_timeline.controller_detected_ms = now_ms();
    // Ligne attendue par le critere de 1a ("le contrôleur est identifié dans le journal série").
    ESP_LOGI(TAG, "Contrôleur d'écran détecté : %s (tactile : %s ; détection en %d ms)", s_info.display_controller,
             s_info.touch_controller, (int)detect_ms);

    // 3. DSI + dalle.
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(tab5_display_create(controller, &panel, &io), TAG, "dalle");
    s_timeline.panel_ready_ms = now_ms();

    // 4. Tactile (pilote) puis LVGL.
    esp_lcd_touch_handle_t touch = NULL;
    ESP_RETURN_ON_ERROR(tab5_touch_create(&touch), TAG, "tactile");
    ESP_RETURN_ON_ERROR(lvgl_attach(panel, io, touch), TAG, "LVGL");
    s_timeline.lvgl_ready_ms = now_ms();
    tab5_orientation_bench(); // mesure unique (hors jalons du demarrage), voir tab5_orientation.c
    // Luminosite percue et veille : le minuteur et le voile de la veille vivent dans LVGL, verrou pris.
    lvgl_port_lock(0);
    const esp_err_t ecran_err = tab5_ecran_init(s_display);
    lvgl_port_unlock();
    ESP_RETURN_ON_ERROR(ecran_err, TAG, "luminosité et veille");

    ESP_LOGI(
        TAG,
        "Carte prête : %s, écran %s, interface %dx%d (dalle %dx%d tournée de %d°), jalons ms : attente expandeur %d, "
        "détection %d, dalle %d, LVGL %d",
        s_info.board_name, s_info.display_controller, (int)s_info.width, (int)s_info.height, (int)s_info.panel_width,
        (int)s_info.panel_height, (int)s_info.rotation_degrees, (int)s_timeline.expander_ready_ms,
        (int)s_timeline.controller_detected_ms, (int)s_timeline.panel_ready_ms, (int)s_timeline.lvgl_ready_ms);
    tab5_autotest_materiel_start(); // ne fait rien sans l'option TAB5_AUTOTEST_MATERIEL
    return ESP_OK;
}
