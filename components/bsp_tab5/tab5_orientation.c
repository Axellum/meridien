// Orientation paysage du Tab5 : rotation LOGICIELLE de LVGL, mesures de l'affichage.
//
// Pourquoi logicielle : la rotation materielle de la dalle (swap_xy / miroir du controleur) ne marche pas sur ST7121 (signale par
// espp ; esp_lcd_panel_swap_xy repond "not supported" sur le panneau DSI). ESPHome fait pareil : sa composante LVGL, avec
// `rotation: 270`, rend chaque zone dans une orientation paysage puis la tourne avec l'accelerateur PPA du P4 avant de l'envoyer
// au tampon d'image (lvgl_esphome.cpp, LvglComponent::ppa_rotate_ / draw_buffer_ ; ADR-0012 du depot Tab5).
// Ici c'est esp_lvgl_port (sw_rotate + CONFIG_LVGL_PORT_ENABLE_PPA) qui le fait dans son flush ; ce fichier regle le sens et
// mesure (docs/mesures/orientation.md).
#include <stdint.h>

#include "driver/ppa.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "tab5_internal.h"

static const char *TAG = "tab5_orientation";

static lv_display_rotation_t lvgl_rotation(void) {
#if TAB5_UI_ROTATION_DEGREES == 90
    return LV_DISPLAY_ROTATION_90;
#elif TAB5_UI_ROTATION_DEGREES == 180
    return LV_DISPLAY_ROTATION_180;
#elif TAB5_UI_ROTATION_DEGREES == 270
    return LV_DISPLAY_ROTATION_270;
#else
    return LV_DISPLAY_ROTATION_0;
#endif
}

// --- Mesures de l'affichage (tache LVGL seulement : aucun verrou necessaire) ---------------------------------------------
// Une ligne tous les STATS_PERIODE_US, et seulement s'il y a eu au moins un rendu : un ecran fixe ne produit aucune ligne.
#define STATS_PERIODE_US (10 * 1000 * 1000)

typedef struct {
    int64_t fenetre_debut_us;
    int64_t rendu_debut_us;
    int64_t flush_debut_us;
    uint32_t images;   // cycles de rendu (RENDER_START..RENDER_READY) : plusieurs zones par image
    uint32_t zones;    // appels du flush
    uint64_t rendu_us; // somme des durees de rendu (dessin + attente + envoi)
    uint32_t rendu_max_us;
    uint64_t flush_us; // somme des durees du flush (rotation PPA + lancement de la copie DMA2D)
    uint32_t flush_max_us;
    uint64_t pixels; // somme des surfaces envoyees (pixels logiques)
} stats_t;

static stats_t s_stats;

static void stats_remettre(int64_t maintenant_us) {
    s_stats = (stats_t){0};
    s_stats.fenetre_debut_us = maintenant_us;
}

static void sur_rendu_debut(lv_event_t *e) {
    (void)e;
    s_stats.rendu_debut_us = esp_timer_get_time();
}

static void sur_rendu_fin(lv_event_t *e) {
    (void)e;
    const int64_t maintenant = esp_timer_get_time();
    const uint32_t duree = (uint32_t)(maintenant - s_stats.rendu_debut_us);
    s_stats.images++;
    s_stats.rendu_us += duree;
    if (duree > s_stats.rendu_max_us) {
        s_stats.rendu_max_us = duree;
    }
    const int64_t fenetre = maintenant - s_stats.fenetre_debut_us;
    if (fenetre >= STATS_PERIODE_US) {
        const uint32_t z = s_stats.zones ? s_stats.zones : 1;
        const uint32_t i = s_stats.images ? s_stats.images : 1;
        ESP_LOGI(TAG,
                 "affichage sur %d ms : %u images, %u zones ; rendu par image moyenne %u µs, max %u µs ; envoi "
                 "(rotation PPA + "
                 "copie) par zone moyenne %u µs, max %u µs ; zone moyenne %u px",
                 (int)(fenetre / 1000), (unsigned)s_stats.images, (unsigned)s_stats.zones,
                 (unsigned)(s_stats.rendu_us / i), (unsigned)s_stats.rendu_max_us, (unsigned)(s_stats.flush_us / z),
                 (unsigned)s_stats.flush_max_us, (unsigned)(s_stats.pixels / z));
        stats_remettre(maintenant);
    }
}

static void sur_flush_debut(lv_event_t *e) {
    const lv_area_t *zone = (const lv_area_t *)lv_event_get_param(e);
    if (zone != NULL) {
        s_stats.pixels += (uint64_t)lv_area_get_size(zone);
    }
    s_stats.flush_debut_us = esp_timer_get_time();
}

static void sur_flush_fin(lv_event_t *e) {
    (void)e;
    const uint32_t duree = (uint32_t)(esp_timer_get_time() - s_stats.flush_debut_us);
    s_stats.zones++;
    s_stats.flush_us += duree;
    if (duree > s_stats.flush_max_us) {
        s_stats.flush_max_us = duree;
    }
}

// --- Banc de la rotation : PPA contre processeur, au demarrage, une seule fois --------------------------------------------
// Mesure ce qui decide du choix : le temps de rotation d'une bande de dessin (1280 x 28 px = 35 840 px, la taille d'un tampon
// partiel de 36 000 px en largeur logique) par l'accelerateur PPA et par le processeur (lv_draw_sw_rotate, le chemin de
// secours d'esp_lvgl_port sans PPA). A retirer quand le choix est consigne dans docs/mesures/orientation.md.
#define BANC_LARGEUR 1280
#define BANC_HAUTEUR_MAX 28
#define BANC_HAUTEUR_MIN 7
#define BANC_ESSAIS 20
#define ALIGNE(n, a) (((n) + (a) - 1) & ~((a) - 1))

static void banc_rotation(void) {
    // Le tampon de dessin (2 x 72 000 octets) et celui de la rotation occupent deja la memoire interne accessible au DMA au
    // moment ou ce banc tourne : une bande de 28 lignes (2 x 71 680 octets) n'y tient pas toujours (essai du 2026-11-06 :
    // « memoire insuffisante »). On essaie donc des bandes de plus en plus etroites (28, 14, 7 lignes) et on journalise ce qui
    // reste. Le temps affiche est celui de la bande effectivement mesuree.
    int hauteur = BANC_HAUTEUR_MAX;
    uint8_t *src = NULL;
    uint8_t *dst = NULL;
    size_t octets = 0;
    for (; hauteur >= BANC_HAUTEUR_MIN; hauteur /= 2) {
        octets = ALIGNE((size_t)BANC_LARGEUR * (size_t)hauteur * 2, 64);
        src = heap_caps_aligned_calloc(64, 1, octets, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        dst = heap_caps_aligned_calloc(64, 1, octets, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (src != NULL && dst != NULL) {
            break;
        }
        heap_caps_free(src);
        heap_caps_free(dst);
        src = NULL;
        dst = NULL;
    }
    if (src == NULL || dst == NULL) {
        ESP_LOGW(TAG,
                 "banc de rotation : mémoire insuffisante, non fait (interne libre %u o, plus grand bloc DMA %u o)",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
        return;
    }
    for (size_t i = 0; i < (size_t)BANC_LARGEUR * (size_t)hauteur * 2; i++) {
        src[i] = (uint8_t)(i * 31u);
    }

    ppa_client_handle_t client = NULL;
    const ppa_client_config_t cfg = {.oper_type = PPA_OPERATION_SRM};
    uint32_t ppa_us = 0;
    esp_err_t ppa_err = ppa_register_client(&cfg, &client);
    if (ppa_err == ESP_OK) {
        ppa_srm_oper_config_t op = {0};
        op.in.buffer = src;
        op.in.pic_w = BANC_LARGEUR;
        op.in.pic_h = hauteur;
        op.in.block_w = BANC_LARGEUR;
        op.in.block_h = hauteur;
        op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
        op.out.buffer = dst;
        op.out.buffer_size = octets;
        op.out.pic_w = hauteur;
        op.out.pic_h = BANC_LARGEUR;
        op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
        op.rotation_angle = PPA_SRM_ROTATION_ANGLE_90;
        op.scale_x = 1.0f;
        op.scale_y = 1.0f;
        op.mode = PPA_TRANS_MODE_BLOCKING;
        ppa_err = ppa_do_scale_rotate_mirror(client, &op); // un essai a blanc (caches, premier appel)
        const int64_t t0 = esp_timer_get_time();
        for (int i = 0; i < BANC_ESSAIS && ppa_err == ESP_OK; i++) {
            ppa_err = ppa_do_scale_rotate_mirror(client, &op);
        }
        ppa_us = (uint32_t)((esp_timer_get_time() - t0) / BANC_ESSAIS);
        ppa_unregister_client(client);
    }

    const int64_t t1 = esp_timer_get_time();
    for (int i = 0; i < BANC_ESSAIS; i++) {
        lv_draw_sw_rotate(src, dst, BANC_LARGEUR, hauteur, BANC_LARGEUR * 2, hauteur * 2, LV_DISPLAY_ROTATION_90,
                          LV_COLOR_FORMAT_RGB565);
    }
    const uint32_t cpu_us = (uint32_t)((esp_timer_get_time() - t1) / BANC_ESSAIS);

    if (ppa_err == ESP_OK) {
        ESP_LOGI(TAG, "banc de rotation d'une bande %d x %d px (moyenne de %d) : PPA %u µs, processeur %u µs",
                 BANC_LARGEUR, hauteur, BANC_ESSAIS, (unsigned)ppa_us, (unsigned)cpu_us);
    } else {
        ESP_LOGW(TAG, "banc de rotation : le PPA a refusé (%s) ; processeur seul : %u µs la bande",
                 esp_err_to_name(ppa_err), (unsigned)cpu_us);
    }
    heap_caps_free(src);
    heap_caps_free(dst);
}

// --- Trace du tactile : une ligne par appui, avec le point BRUT (tel que rendu par esp_lcd_touch, repere natif de la dalle) et
// le point FINAL (apres la rotation de LVGL, repere de l'interface). Donne la preuve de la transformation sans dependre de ce
// que l'on voit : on touche un coin connu et on lit les deux points dans le journal serie.
static lv_indev_read_cb_t s_lecture_origine = NULL;
static bool s_appuye = false;
static lv_point_t s_brut = {0, 0};

static void lecture_tracee(lv_indev_t *indev, lv_indev_data_t *data) {
    s_lecture_origine(indev, data);
    const bool appuye = data->state == LV_INDEV_STATE_PRESSED;
    if (appuye && !s_appuye) {
        s_brut = data->point; // premier point de l'appui, avant que LVGL ne le tourne
    }
    s_appuye = appuye;
}

static void sur_appui(lv_event_t *e) {
    // Sur la liste d'evenements d'un indev, la CIBLE de l'evenement est l'indev (lv_indev_send_event le passe en target) ;
    // son PARAMETRE est l'objet touche (indev_obj_act) : lv_event_get_indev() lisait ce parametre et rendait un objet pris pour
    // un indev -> plantage « Load access fault » a chaque appui (binaire 557eb44, constate le 06/11/2026 a 22:25).
    lv_indev_t *indev = (lv_indev_t *)lv_event_get_target(e);
    lv_point_t logique = {0, 0};
    lv_indev_get_point(indev, &logique);
    lv_display_t *disp = lv_indev_get_display(indev);
    ESP_LOGI(TAG, "appui : brut (%d, %d) sur la dalle %d x %d -> logique (%d, %d) sur l'interface %d x %d",
             (int)s_brut.x, (int)s_brut.y, TAB5_LCD_H_RES, TAB5_LCD_V_RES, (int)logique.x, (int)logique.y,
             (int)lv_display_get_horizontal_resolution(disp), (int)lv_display_get_vertical_resolution(disp));
}

// Appelee par board_init() apres lvgl_port_add_touch(). Le verrou de LVGL doit etre pris par l'appelant.
void tab5_orientation_trace_tactile(lv_indev_t *indev) {
    s_lecture_origine = lv_indev_get_read_cb(indev);
    if (s_lecture_origine == NULL) {
        return;
    }
    lv_indev_set_read_cb(indev, lecture_tracee);
    lv_indev_add_event_cb(indev, sur_appui, LV_EVENT_PRESSED, NULL);
}

// Appelee par board_init() une fois l'affichage LVGL cree. Le verrou de LVGL doit etre pris par l'appelant.
void tab5_orientation_apply(lv_display_t *disp) {
    lv_display_set_rotation(disp, lvgl_rotation());
    ESP_LOGI(TAG,
             "rotation LVGL %d° (équivalent du `rotation: 270` d'ESPHome) : interface %d x %d sur une dalle %d x %d",
             TAB5_UI_ROTATION_DEGREES, (int)lv_display_get_horizontal_resolution(disp),
             (int)lv_display_get_vertical_resolution(disp), TAB5_LCD_H_RES, TAB5_LCD_V_RES);

    stats_remettre(esp_timer_get_time());
    lv_display_add_event_cb(disp, sur_rendu_debut, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(disp, sur_rendu_fin, LV_EVENT_RENDER_READY, NULL);
    lv_display_add_event_cb(disp, sur_flush_debut, LV_EVENT_FLUSH_START, NULL);
    lv_display_add_event_cb(disp, sur_flush_fin, LV_EVENT_FLUSH_FINISH, NULL);
}

void tab5_orientation_bench(void) { banc_rotation(); }
