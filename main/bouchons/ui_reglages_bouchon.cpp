// BOUCHON — corps provisoire de ui_reglages_ouvrir() : un bandeau « Réglages : bientôt » posé deux secondes sur la couche
// supérieure, puis retiré. Compilé tant que `main/ui_reglages_ouvrir.cpp` (la colle de `reglages-ecran`) n'existe pas ; à
// supprimer avec ce dossier ensuite (voir bouchons/ui_reglages.h). LVGL pur, comme les autres fichiers d'interface.
#include "ui_reglages.h"

#include <cstdint>

#include "lvgl.h"
#include "palette.h"

// Police Roboto Bold embarquée (main/assets/roboto_700.ttf, EMBED_FILES de main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

constexpr const char *kTexte = "Réglages : bientôt";
constexpr uint32_t kDureeMs = 2000;

lv_obj_t *g_bandeau = nullptr;

void retirer(lv_timer_t *) {
    if (g_bandeau != nullptr) {
        lv_obj_delete(g_bandeau);
        g_bandeau = nullptr;
    }
}

} // namespace

void ui_reglages_ouvrir() {
    if (g_bandeau != nullptr) {
        return; // déjà affiché : un second appui ne le prolonge pas
    }
    static lv_font_t *police = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 40);

    const ecran::Palette &couleurs = ecran::kPaletteSombre;
    lv_obj_t *couche = lv_display_get_layer_top(lv_display_get_default());
    g_bandeau = lv_obj_create(couche);
    lv_obj_remove_style_all(g_bandeau);
    lv_obj_remove_flag(g_bandeau, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(g_bandeau, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g_bandeau, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(g_bandeau, lv_color_hex(couleurs.surface), 0);
    lv_obj_set_style_bg_opa(g_bandeau, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_bandeau, 24, 0);
    lv_obj_set_style_pad_all(g_bandeau, 28, 0);
    lv_obj_align(g_bandeau, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *etiquette = lv_label_create(g_bandeau);
    lv_label_set_text(etiquette, kTexte);
    if (police != nullptr) {
        lv_obj_set_style_text_font(etiquette, police, 0);
    }
    lv_obj_set_style_text_color(etiquette, lv_color_hex(couleurs.texte), 0);

    lv_timer_t *minuteur = lv_timer_create(retirer, kDureeMs, nullptr);
    lv_timer_set_repeat_count(minuteur, 1);
}

// Sans la colle de `reglages-ecran` il n'y a pas d'installation guidée à reprendre : rien à faire.
void ui_installation_ouvrir_si_necessaire() {}
