#include "ui_appairage.h"

#include <cstdint>
#include <cstring>

#include "lvgl.h"

#include "palette.h"
#include "textes_reglages.h"

// Police Roboto Bold embarquée (main/assets/roboto_700.ttf), la même que ui_bonjour.cpp.
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace ecran {

namespace {

const Palette &kCouleurs = kPaletteSombre;

constexpr int32_t kLargeur = 680;
constexpr int32_t kHauteur = 156;
constexpr int32_t kMargeBas = 6;
constexpr int32_t kRayon = 18;

lv_obj_t *g_bandeau = nullptr;
lv_obj_t *g_titre = nullptr;
lv_obj_t *g_adresse = nullptr;
lv_obj_t *g_nom = nullptr;
lv_obj_t *g_code = nullptr;

lv_obj_t *creer_ligne(lv_obj_t *parent, const lv_font_t *police, uint32_t couleur_rgb) {
    lv_obj_t *libelle = lv_label_create(parent);
    lv_obj_set_width(libelle, kLargeur - 24);
    lv_label_set_long_mode(libelle, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(libelle, police, 0);
    lv_obj_set_style_text_color(libelle, lv_color_hex(couleur_rgb), 0);
    lv_obj_set_style_text_align(libelle, LV_TEXT_ALIGN_CENTER, 0);
    return libelle;
}

// Ne redessine que si le texte change.
void changer_texte(lv_obj_t *libelle, const char *texte) {
    const char *actuel = lv_label_get_text(libelle);
    if (actuel == nullptr || std::strcmp(actuel, texte) != 0) {
        lv_label_set_text(libelle, texte);
    }
}

void creer(lv_display_t *affichage) {
    lv_font_t *petite = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 24);
    lv_font_t *grande = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 40);
    if (petite == nullptr || grande == nullptr) {
        return;
    }
    lv_obj_t *couche = lv_display_get_layer_top(affichage);
    const int32_t hauteur_ecran = lv_display_get_vertical_resolution(affichage);
    const int32_t largeur_ecran = lv_display_get_horizontal_resolution(affichage);

    g_bandeau = lv_obj_create(couche);
    lv_obj_remove_style_all(g_bandeau);
    lv_obj_remove_flag(g_bandeau, LV_OBJ_FLAG_CLICKABLE); // les appuis passent à l'écran dessous
    lv_obj_remove_flag(g_bandeau, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g_bandeau, kLargeur, kHauteur);
    lv_obj_set_pos(g_bandeau, (largeur_ecran - kLargeur) / 2, hauteur_ecran - kHauteur - kMargeBas);
    lv_obj_set_style_bg_opa(g_bandeau, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_bandeau, lv_color_hex(kCouleurs.surface), 0);
    lv_obj_set_style_radius(g_bandeau, kRayon, 0);
    lv_obj_set_style_border_width(g_bandeau, 2, 0);
    lv_obj_set_style_border_color(g_bandeau, lv_color_hex(kCouleurs.accent), 0);
    lv_obj_set_flex_flow(g_bandeau, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_bandeau, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(g_bandeau, 6, 0);
    lv_obj_set_style_pad_row(g_bandeau, 0, 0);

    g_titre = creer_ligne(g_bandeau, petite, kCouleurs.texte_discret);
    lv_label_set_text(g_titre, textes_reglages::kBandeauTitre);
    g_adresse = creer_ligne(g_bandeau, grande, kCouleurs.texte);
    g_nom = creer_ligne(g_bandeau, petite, kCouleurs.texte_discret);
    g_code = creer_ligne(g_bandeau, grande, kCouleurs.accent);
}

} // namespace

void ui_appairage_afficher(lv_display_t *affichage, const char *adresse, const char *nom, const char *code) {
    if (g_bandeau == nullptr) {
        creer(affichage);
        if (g_bandeau == nullptr) {
            return;
        }
    }
    char tampon[64];
    if (adresse != nullptr && adresse[0] != '\0') {
        lv_snprintf(tampon, sizeof(tampon), textes_reglages::kBandeauAdresse, adresse);
        changer_texte(g_adresse, tampon);
        lv_obj_remove_flag(g_adresse, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_adresse, LV_OBJ_FLAG_HIDDEN);
    }
    if (nom != nullptr && nom[0] != '\0') {
        lv_snprintf(tampon, sizeof(tampon), textes_reglages::kBandeauNom, nom);
        changer_texte(g_nom, tampon);
        lv_obj_remove_flag(g_nom, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_nom, LV_OBJ_FLAG_HIDDEN);
    }
    if (code != nullptr && code[0] != '\0') {
        lv_snprintf(tampon, sizeof(tampon), textes_reglages::kBandeauCode, code);
        changer_texte(g_code, tampon);
        lv_obj_remove_flag(g_code, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_code, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(g_bandeau, LV_OBJ_FLAG_HIDDEN);
}

} // namespace ecran
