// Petits outils de dessin de l'écran principal : texte, rectangles, traits, triangles, arcs, dégradés. Ils posent des tâches de
// dessin LVGL sur la couche reçue par l'événement LV_EVENT_DRAW_MAIN d'une zone ; aucune ne crée d'objet LVGL (un objet coûte de
// la RAM interne, une tâche de dessin non). LVGL pur : aucun en-tête ESP-IDF.
//
// Tout se compte en pixels de l'écran réel. Le texte est posé par le haut de sa ligne (hauteur de ligne de la police) ; les grands
// chiffres (heure, température) se placent par le haut des chiffres avec `y_chiffres()` pour que la marge visible soit celle
// qu'on a voulue, pas celle du blanc au-dessus des capitales.
#pragma once

#include <cstdint>
#include <cstring>

#include "ecran/visuel_themes.hpp"
#include "lvgl.h"

namespace ecran::dessin {

struct Pinceau {
    lv_layer_t *couche;
    const visuel::ThemePrincipal *theme;
};

// Une police et sa taille nominale (en pixels), pour retrouver la hauteur des capitales (Roboto : 0,711 em).
struct Police {
    const lv_font_t *f = nullptr;
    int px = 0;
};

inline lv_color_t couleur(std::uint32_t rgb) { return lv_color_hex(rgb); }

inline int hauteur_ligne(const Police &p) { return lv_font_get_line_height(p.f); }

// Distance du haut de la ligne au haut des chiffres (capitales de Roboto : 0,711 em au-dessus de la ligne de base).
inline int vide_au_dessus_des_chiffres(const Police &p) {
    const int ascent = lv_font_get_line_height(p.f) - p.f->base_line;
    return ascent - p.px * 711 / 1000;
}

// Hauteur visible des chiffres.
inline int hauteur_chiffres(const Police &p) { return p.px * 711 / 1000; }

inline int largeur_texte(const Police &p, const char *s) {
    if (s == nullptr || *s == '\0') {
        return 0;
    }
    lv_point_t taille;
    lv_text_get_size(&taille, s, p.f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return taille.x;
}

// Pose le texte `s` dans la boîte (x, y, largeur w), alignée comme demandé ; `y` est le haut de la LIGNE. LVGL passe à la ligne
// quand le texte dépasse `w` : `lignes` (1 par défaut) est le nombre de lignes que la boîte laisse voir, le reste est coupé.
inline void texte(const Pinceau &pc, const Police &p, const char *s, int x, int y, int w, lv_text_align_t align,
                  std::uint32_t rgb, std::uint8_t opa = LV_OPA_COVER, int lignes = 1) {
    if (s == nullptr || *s == '\0' || opa == LV_OPA_TRANSP) {
        return;
    }
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text = s;
    d.text_local = 1; // LVGL garde une copie : le tampon de l'appelant peut être réutilisé dès le retour
    d.font = p.f;
    d.color = couleur(rgb);
    d.opa = opa;
    d.align = align;
    const lv_area_t zone = {x, y, x + w - 1, y + hauteur_ligne(p) * (lignes < 1 ? 1 : lignes) - 1};
    lv_draw_label(pc.couche, &d, &zone);
}

// Pose le texte de façon que le HAUT DES CHIFFRES soit à `y_chiffres`.
inline void texte_par_les_chiffres(const Pinceau &pc, const Police &p, const char *s, int x, int y_chiffres, int w,
                                   lv_text_align_t align, std::uint32_t rgb, std::uint8_t opa = LV_OPA_COVER) {
    texte(pc, p, s, x, y_chiffres - vide_au_dessus_des_chiffres(p), w, align, rgb, opa);
}

inline void rect(const Pinceau &pc, int x, int y, int w, int h, int rayon, std::uint32_t rgb,
                 std::uint8_t opa = LV_OPA_COVER) {
    if (w <= 0 || h <= 0 || opa == LV_OPA_TRANSP) {
        return;
    }
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = couleur(rgb);
    d.bg_opa = opa;
    d.radius = rayon;
    const lv_area_t zone = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(pc.couche, &d, &zone);
}

// Rectangle à dégradé horizontal (de gauche à droite) : barres de température.
inline void rect_degrade_h(const Pinceau &pc, int x, int y, int w, int h, int rayon, std::uint32_t gauche,
                           std::uint32_t droite) {
    if (w <= 0 || h <= 0) {
        return;
    }
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = rayon;
    d.bg_opa = LV_OPA_COVER;
    d.bg_grad.dir = LV_GRAD_DIR_HOR;
    d.bg_grad.stops_count = 2;
    d.bg_grad.stops[0] = {couleur(gauche), LV_OPA_COVER, 0};
    d.bg_grad.stops[1] = {couleur(droite), LV_OPA_COVER, 255};
    const lv_area_t zone = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(pc.couche, &d, &zone);
}

// Rectangle translucide avec un filet : le verre des cartes de l'accueil et ses puces (fond `fond` à `fond_opa`, contour `bord` à
// `bord_opa`, épaisseur `epaisseur` en pixels ; le contour est dessiné DANS le rectangle).
inline void rect_bord(const Pinceau &pc, int x, int y, int w, int h, int rayon, std::uint32_t fond,
                      std::uint8_t fond_opa, std::uint32_t bord, std::uint8_t bord_opa, int epaisseur) {
    if (w <= 0 || h <= 0) {
        return;
    }
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = rayon;
    d.bg_color = couleur(fond);
    d.bg_opa = fond_opa;
    d.border_color = couleur(bord);
    d.border_opa = bord_opa;
    d.border_width = epaisseur;
    const lv_area_t zone = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(pc.couche, &d, &zone);
}

// Rectangle à dégradé vertical, la couleur ET l'opacité variant de `haut` à `bas` : les voiles sous les courbes. Une bande étroite
// dont les deux bouts reprennent un dégradé GLOBAL (calculé par l'appelant à ses deux ordonnées) donne le même résultat qu'un
// dégradé unique sous toute la courbe, sans polygone.
inline void rect_degrade_v(const Pinceau &pc, int x, int y, int w, int h, std::uint32_t haut, std::uint8_t haut_opa,
                           std::uint32_t bas, std::uint8_t bas_opa) {
    if (w <= 0 || h <= 0) {
        return;
    }
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_COVER;
    d.bg_grad.dir = LV_GRAD_DIR_VER;
    d.bg_grad.stops_count = 2;
    d.bg_grad.stops[0] = {couleur(haut), haut_opa, 0};
    d.bg_grad.stops[1] = {couleur(bas), bas_opa, 255};
    const lv_area_t zone = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(pc.couche, &d, &zone);
}

inline void disque(const Pinceau &pc, int cx, int cy, int rayon, std::uint32_t rgb, std::uint8_t opa = LV_OPA_COVER) {
    rect(pc, cx - rayon, cy - rayon, 2 * rayon, 2 * rayon, LV_RADIUS_CIRCLE, rgb, opa);
}

inline void anneau(const Pinceau &pc, int cx, int cy, int rayon, int epaisseur, std::uint32_t rgb,
                   std::uint8_t opa = LV_OPA_COVER) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.radius = LV_RADIUS_CIRCLE;
    d.border_color = couleur(rgb);
    d.border_opa = opa;
    d.border_width = epaisseur;
    const lv_area_t zone = {cx - rayon, cy - rayon, cx + rayon - 1, cy + rayon - 1};
    lv_draw_rect(pc.couche, &d, &zone);
}

inline void trait(const Pinceau &pc, int x1, int y1, int x2, int y2, int epaisseur, std::uint32_t rgb,
                  std::uint8_t opa = LV_OPA_COVER, bool arrondi = true) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1 = {static_cast<lv_value_precise_t>(x1), static_cast<lv_value_precise_t>(y1)};
    d.p2 = {static_cast<lv_value_precise_t>(x2), static_cast<lv_value_precise_t>(y2)};
    d.width = epaisseur;
    d.color = couleur(rgb);
    d.opa = opa;
    d.round_start = arrondi;
    d.round_end = arrondi;
    lv_draw_line(pc.couche, &d);
}

inline void triangle(const Pinceau &pc, int x1, int y1, int x2, int y2, int x3, int y3, std::uint32_t rgb,
                     std::uint8_t opa = LV_OPA_COVER) {
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.p[0] = {static_cast<lv_value_precise_t>(x1), static_cast<lv_value_precise_t>(y1)};
    d.p[1] = {static_cast<lv_value_precise_t>(x2), static_cast<lv_value_precise_t>(y2)};
    d.p[2] = {static_cast<lv_value_precise_t>(x3), static_cast<lv_value_precise_t>(y3)};
    d.color = couleur(rgb);
    d.opa = opa;
    lv_draw_triangle(pc.couche, &d);
}

// Arc de cercle de centre (cx, cy), de `debut` à `fin` degrés (0 = à droite, sens horaire, comme LVGL).
inline void arc(const Pinceau &pc, int cx, int cy, int rayon, int epaisseur, int debut, int fin, std::uint32_t rgb,
                std::uint8_t opa = LV_OPA_COVER, bool arrondi = true) {
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center = {cx, cy};
    d.radius = static_cast<std::uint16_t>(rayon);
    d.width = epaisseur;
    d.start_angle = static_cast<lv_value_precise_t>(debut);
    d.end_angle = static_cast<lv_value_precise_t>(fin);
    d.color = couleur(rgb);
    d.opa = opa;
    d.rounded = arrondi;
    lv_draw_arc(pc.couche, &d);
}

} // namespace ecran::dessin
