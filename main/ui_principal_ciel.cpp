// Écran principal : le fond (ciel tramé) et le verre cuit dans son image.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- fond : dégradé du ciel ---------------------------------------------------------------------------------------------------

namespace {

// Le dégradé du ciel est vertical et sombre : en RGB565 (5 bits de rouge et de bleu, 6 de vert) un dégradé que LVGL calcule tel
// quel montre des bandes horizontales, une marche de 4 à 8 niveaux sur 8 bits tous les quelques dizaines de lignes. On le dessine
// donc une fois dans une image RGB565 TRAMÉE (matrice de Bayer 8x8 : chaque marche devient un mélange fin des deux teintes voisines,
// que l'oeil prend pour la teinte intermédiaire). L'image fait la taille de l'écran (1,8 Mo à 1280 x 720 : la PSRAM, jamais la RAM
// interne) et n'est refaite que quand le ciel change (matin, soir, nuit, temps) : quelques dizaines de millisecondes. Si
// l'allocation échoue, on retombe sur le dégradé de LVGL (avec ses bandes) plutôt que sur un écran sans fond.
constexpr std::uint8_t BAYER_8[8][8] = {{0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
                                        {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
                                        {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
                                        {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21}};

void remplir_ciel_trame(lv_draw_buf_t *buf, std::uint32_t haut, std::uint32_t bas) {
    const int h = static_cast<int>(buf->header.h);
    const int w = static_cast<int>(buf->header.w);
    const std::int32_t a[3] = {static_cast<std::int32_t>((haut >> 16) & 0xFF) << 8,
                               static_cast<std::int32_t>((haut >> 8) & 0xFF) << 8,
                               static_cast<std::int32_t>(haut & 0xFF) << 8};
    const std::int32_t d[3] = {(static_cast<std::int32_t>((bas >> 16) & 0xFF) << 8) - a[0],
                               (static_cast<std::int32_t>((bas >> 8) & 0xFF) << 8) - a[1],
                               (static_cast<std::int32_t>(bas & 0xFF) << 8) - a[2]};
    const int dernier = h > 1 ? h - 1 : 1;
    for (int y = 0; y < h; ++y) {
        // composantes de la ligne en 8 bits + 8 bits de fraction
        const std::int32_t r = a[0] + static_cast<std::int32_t>(static_cast<std::int64_t>(d[0]) * y / dernier);
        const std::int32_t gv = a[1] + static_cast<std::int32_t>(static_cast<std::int64_t>(d[1]) * y / dernier);
        const std::int32_t b = a[2] + static_cast<std::int32_t>(static_cast<std::int64_t>(d[2]) * y / dernier);
        auto *ligne = reinterpret_cast<std::uint16_t *>(static_cast<std::uint8_t *>(buf->data) +
                                                        static_cast<std::size_t>(y) * buf->header.stride);
        const std::uint8_t *seuil = BAYER_8[y & 7];
        for (int x = 0; x < w; ++x) {
            const std::int32_t s = seuil[x & 7];   // 0 à 63 : fraction d'une marche de quantification
            std::int32_t qr = (r + s * 32) >> 11;  // marche de 8 niveaux = 2048 en 8.8 (rouge et bleu : 5 bits)
            std::int32_t qg = (gv + s * 16) >> 10; // marche de 4 niveaux = 1024 (vert : 6 bits)
            std::int32_t qb = (b + s * 32) >> 11;
            qr = qr > 31 ? 31 : qr;
            qg = qg > 63 ? 63 : qg;
            qb = qb > 31 ? 31 : qb;
            ligne[x] = static_cast<std::uint16_t>((qr << 11) | (qg << 5) | qb);
        }
    }
}
} // namespace

// Cuit le verre des cartes des heures et des cinq jours (ou de la pluie) DANS l'image du ciel : chaque bande repeinte n'a plus qu'à
// recopier des pixels au lieu de fondre un rectangle arrondi translucide (un tiers de l'écran). On les dessine avec le MÊME code que
// le dessin direct (carte_de_verre) et le MÊME LVGL, sur l'image en RGB565 comme la dalle : le résultat est le même pixel pour pixel.
// Refait quand une carte change de place, de taille ou de teinte (disposition, thème) ; l'image est alors remplie à neuf d'abord, le
// verre ne s'empile jamais. Si l'image manque, rien n'est cuit et le dessin direct reste en service.
void cuire_verre() {
    if (s_verre_differe) {
        return; // première image : verre dessiné en direct ; ui_principal_cuire_verre_differe() le cuira juste après
    }
    Rect voulu[2];
    int n = 0;
    const ThemePrincipal &t = *g.theme;
    if (g.ciel_image != nullptr && g.est_accueil && t.ciel_dynamique && t.carte_opa > 0) {
        for (const int z : {static_cast<int>(Z_HEURES), static_cast<int>(Z_SEMAINE)}) {
            if (g.rect[z].w > 0 && g.rect[z].h > 0) {
                voulu[n++] = g.rect[z];
            }
        }
    }
    Empreinte e;
    e.u(static_cast<std::uint32_t>(n));
    for (int i = 0; i < n; ++i) {
        e.u(static_cast<std::uint32_t>(voulu[i].x));
        e.u(static_cast<std::uint32_t>(voulu[i].y));
        e.u(static_cast<std::uint32_t>(voulu[i].w));
        e.u(static_cast<std::uint32_t>(voulu[i].h));
    }
    e.u(t.carte);
    e.u(t.carte_opa);
    e.u(t.texte);
    e.u(g.ciel_haut);
    e.u(g.ciel_bas);
    e.u(reinterpret_cast<std::uintptr_t>(g.ciel_image));
    e.u(static_cast<std::uint32_t>(px(28)));
    e.u(static_cast<std::uint32_t>(px(2)));
    if (e.h == g.cle_verre) {
        return;
    }
    g.cle_verre = e.h;
    if (g.nb_verre > 0 && g.ciel_image != nullptr) { // du verre déjà cuit : repartir du ciel nu
        remplir_ciel_trame(g.ciel_image, g.ciel_haut, g.ciel_bas);
    }
    g.nb_verre = 0;
    if (n > 0) {
        lv_layer_t couche;
        lv_layer_init(&couche);
        const lv_area_t tout = {0, 0, static_cast<int32_t>(g.ciel_image->header.w) - 1,
                                static_cast<int32_t>(g.ciel_image->header.h) - 1};
        couche.draw_buf = g.ciel_image;
        couche.color_format = static_cast<lv_color_format_t>(g.ciel_image->header.cf);
        couche.buf_area = tout;
        couche._clip_area = tout;
        couche.phy_clip_area = tout;
        lv_draw_unit_send_event(nullptr, LV_EVENT_CHILD_CREATED, &couche);
        for (int i = 0; i < n; ++i) {
            const Ctx c = {{&couche, g.theme}, voulu[i].x, voulu[i].y, voulu[i].w, voulu[i].h};
            carte_de_verre(c, px(28)); // g.nb_verre est 0 : dessine vraiment
        }
        couche.all_tasks_added = true;
        while (couche.draw_task_head != nullptr) {
            lv_draw_dispatch_wait_for_request();
            if (!lv_draw_dispatch_layer(lv_obj_get_display(g.ecran), &couche)) {
                lv_draw_wait_for_finish();
                lv_draw_dispatch_request();
            }
        }
        lv_draw_unit_send_event(nullptr, LV_EVENT_SCREEN_LOAD_START, &couche);
        lv_draw_unit_send_event(nullptr, LV_EVENT_CHILD_DELETED, &couche);
        for (int i = 0; i < n; ++i) {
            g.verre[i] = voulu[i];
        }
        g.nb_verre = n;
    }
    if (g.ecran != nullptr) {
        lv_obj_invalidate(g.ecran);
    }
}

void appliquer_ciel() {
    const ThemePrincipal &t = *g.theme;
    if (!t.ciel_dynamique) {
        lv_obj_set_style_bg_image_src(g.ecran, nullptr, 0);
        lv_obj_set_style_bg_color(g.ecran, dz::couleur(t.fond), 0);
        lv_obj_set_style_bg_grad_dir(g.ecran, LV_GRAD_DIR_NONE, 0);
        if (g.ciel_image !=
            nullptr) { // venu d'un thème à ciel : le style ne le référence plus, on rend ses 1,8 Mo de PSRAM
            lv_draw_buf_destroy(g.ciel_image);
            g.ciel_image = nullptr;
        }
        g.nb_verre = 0;
        cuire_verre();
        return;
    }
    const Degrade d = ciel(moment_du_jour(), ambiance_du_moment());
    // Le dégradé de LVGL reste réglé : c'est le repli si l'image tramée ne peut pas être créée.
    lv_obj_set_style_bg_color(g.ecran, dz::couleur(d.haut), 0);
    lv_obj_set_style_bg_grad_color(g.ecran, dz::couleur(d.bas), 0);
    lv_obj_set_style_bg_grad_dir(g.ecran, LV_GRAD_DIR_VER, 0);
    lv_draw_buf_t *neuf =
        lv_draw_buf_create(static_cast<std::uint32_t>(g.largeur), static_cast<std::uint32_t>(g.hauteur),
                           LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    if (neuf == nullptr) {
        return;
    }
    remplir_ciel_trame(neuf, d.haut, d.bas);
    g.ciel_haut = d.haut;
    g.ciel_bas = d.bas;
    lv_obj_set_style_bg_image_src(g.ecran, neuf, 0);
    lv_obj_set_style_bg_image_opa(g.ecran, LV_OPA_COVER, 0);
    // L'image, opaque et de la taille de l'écran, couvre tout le fond : le dégradé de repli dessous n'est plus qu'une couleur unie
    // (un remplissage simple par bande d'écran au lieu d'un mélange par ligne, 720 lignes de 1 280 pixels écrites pour rien).
    lv_obj_set_style_bg_grad_dir(g.ecran, LV_GRAD_DIR_NONE, 0);
    if (g.ciel_image != nullptr) { // l'ancienne image n'est plus référencée par le style : on peut la rendre
        lv_draw_buf_destroy(g.ciel_image);
    }
    g.ciel_image = neuf;
    g.nb_verre = 0; // l'image est neuve : sans verre
    g.cle_verre = 0;
    cuire_verre();
}

} // namespace principal
} // namespace ecran
