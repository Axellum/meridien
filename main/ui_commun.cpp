// Aides de dessin communes aux écrans de réglages et d'installation (ui_reglages_interne.h). LVGL pur, aucun include ESP-IDF.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "ecran/texte_modele.hpp"
#include "ui_reglages_interne.h"

namespace ecran::ui {

namespace {
Env g_env;
}

Env &env() { return g_env; }

int32_t S(int32_t px) {
    return std::max<int32_t>(1, static_cast<int32_t>(std::lround(static_cast<float>(px) * g_env.echelle)));
}

lv_color_t couleur(uint32_t rgb) { return lv_color_hex(rgb); }

const char *T(const char *francais) { return i18n::tr_c(francais); }

void env_choisir_theme() {
    const int theme = g_env.hote != nullptr ? g_env.hote->nombre("affichage.theme") : 0;
    g_env.p = &palette_du_theme(theme);
}

void env_initialiser(lv_obj_t *racine, const uint8_t *ttf, std::size_t ttf_taille, int32_t largeur, int32_t hauteur,
                     HoteReglages *hote, void (*fermer)(), void (*ouvrir_diagnostic)()) {
    g_env.hote = hote;
    g_env.racine = racine;
    g_env.fermer = fermer;
    g_env.ouvrir_diagnostic = ouvrir_diagnostic;
    g_env.largeur = largeur;
    g_env.hauteur = hauteur;
    g_env.echelle = static_cast<float>(std::min(largeur, hauteur)) / 720.0F;
    // Les polices sont créées une fois et gardées (TinyTTF ne les libère pas à bas prix) ; la taille suit l'écran à la création.
    if (g_env.f.texte == nullptr || g_env.ttf != ttf) {
        g_env.ttf = ttf;
        g_env.ttf_taille = ttf_taille;
        g_env.f.titre = lv_tiny_ttf_create_data(ttf, ttf_taille, S(44));
        g_env.f.texte = lv_tiny_ttf_create_data(ttf, ttf_taille, S(34));
        g_env.f.petite = lv_tiny_ttf_create_data(ttf, ttf_taille, S(27));
        g_env.f.touche = lv_tiny_ttf_create_data(ttf, ttf_taille, S(36));
    }
    env_choisir_theme();
}

lv_obj_t *creer_texte(lv_obj_t *parent, const char *texte, const lv_font_t *police, uint32_t couleur_rgb) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_font(l, police, 0);
    lv_obj_set_style_text_color(l, couleur(couleur_rgb), 0);
    lv_label_set_text(l, texte);
    return l;
}

lv_obj_t *creer_bouton(lv_obj_t *parent, const char *texte, StyleBouton style, lv_event_cb_t cb, void *donnee) {
    const PaletteReglages &p = *g_env.p;
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_height(b, S(kToucheMinPx));
    lv_obj_set_style_min_height(b, S(kToucheMinPx),
                                0); // un appelant ne peut pas le rendre plus petit que la zone tactile minimale
    lv_obj_set_style_radius(b, S(16), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_hor(b, S(22), 0);
    uint32_t fond = p.surface;
    uint32_t encre = p.texte;
    switch (style) {
    case StyleBouton::Principal:
        fond = p.accent;
        encre = p.texte_sur_accent;
        break;
    case StyleBouton::Danger:
        fond = p.danger;
        encre = p.texte_sur_danger;
        break;
    case StyleBouton::Discret:
        fond = p.surface_haute;
        break;
    case StyleBouton::Normal:
        break;
    }
    lv_obj_set_style_bg_color(b, couleur(fond), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, couleur(p.surface_haute), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b, couleur(p.surface), LV_STATE_DISABLED);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, texte);
    lv_obj_set_style_text_font(l, g_env.f.texte, 0);
    lv_obj_set_style_text_color(l, couleur(encre), 0);
    lv_obj_set_style_text_color(l, couleur(p.desactive), LV_STATE_DISABLED);
    lv_obj_center(l);
    if (cb != nullptr) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, donnee);
    }
    return b;
}

// À appeler juste après la création du bouton : un bouton inactif ne répond plus au toucher et prend la couleur « désactivé »
// (couleurs posées directement, l'état LV_STATE_DISABLED de LVGL laisse au thème par défaut le soin de choisir un gris).
void bouton_activer(lv_obj_t *bouton, bool actif) {
    if (actif) {
        return;
    }
    const PaletteReglages &p = *g_env.p;
    lv_obj_remove_flag(bouton, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(bouton, couleur(p.surface), 0);
    lv_obj_t *l = lv_obj_get_child(bouton, 0);
    if (l != nullptr) {
        lv_obj_set_style_text_color(l, couleur(p.desactive), 0);
    }
}

void bouton_changer_texte(lv_obj_t *bouton, const char *texte) {
    lv_obj_t *l = lv_obj_get_child(bouton, 0);
    if (l != nullptr) {
        lv_label_set_text(l, texte);
        lv_obj_center(l);
    }
}

namespace {
lv_obj_t *creer_conteneur_plat(lv_obj_t *parent, lv_flex_flow_t sens) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, LV_PCT(100));
    lv_obj_set_height(c, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(c, sens);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}
} // namespace

void style_defilement(lv_obj_t *conteneur) {
    const PaletteReglages &p = *g_env.p;
    lv_obj_set_scrollbar_mode(conteneur, LV_SCROLLBAR_MODE_AUTO); // visible seulement quand le contenu dépasse
    lv_obj_set_style_width(conteneur, S(8), LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(conteneur, S(4),
                               LV_PART_SCROLLBAR); // dans la marge du conteneur : ne recouvre aucune carte
    lv_obj_set_style_radius(conteneur, S(4), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(conteneur, couleur(p.texte_discret), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(conteneur, LV_OPA_COVER, LV_PART_SCROLLBAR);
}

lv_obj_t *creer_colonne(lv_obj_t *parent) {
    lv_obj_t *c = creer_conteneur_plat(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, S(8), 0);
    return c;
}

lv_obj_t *creer_ligne(lv_obj_t *parent) {
    lv_obj_t *c = creer_conteneur_plat(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c, S(12), 0);
    return c;
}

const char *remplir(const char *modele_fr, const ecran::texte::Parametre *p, std::size_t n, char *tampon,
                    std::size_t capacite) {
    ecran::texte::remplir(T(modele_fr), p, n, tampon, capacite);
    return tampon;
}

} // namespace ecran::ui
