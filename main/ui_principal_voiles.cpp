// Écran principal, accueil : briques de dessin (texte par ligne de base, verre, courbes) et voiles cuits sous les courbes.
#include "ui_principal_interne.h"

#include "src/draw/sw/lv_draw_sw_grad.h" // lv_draw_sw_grad_color_calculate : les couleurs mêmes du remplissage (voile_entre)

namespace ecran {
namespace principal {

// ---- l'accueil (thèmes « ciel », « tableau sombre », « clair ») -------------------------------------------------------------
//
// La composition de l'esquisse retenue : à gauche le lieu, l'heure, la date et la météo du moment ; à droite la courbe des 12 prochaines
// heures ; au milieu le bandeau (la pluie à gauche, l'alerte à droite) ; en bas les cinq jours avec leur vent et leur pluie. Les
// positions sont celles de l'esquisse, en pixels de RÉFÉRENCE (1280 x 720) : `px()` les met à l'échelle de l'écran. Rien n'est animé.
// Une valeur de la météo périmée est dessinée avec le thème grisé (theme_meteo) ; l'heure, la date et le lieu ne le sont pas.

// Le thème des valeurs de la météo : grisé quand elle est périmée (une valeur périmée n'est plus une mesure).
const ThemePrincipal &theme_meteo() { return g.e.meteo.perimee ? g.theme_gris : *g.theme; }

bool en_francais() { return i18n::langue_courante() == i18n::Langue::Fr; }

// Texte posé par sa LIGNE DE BASE, comme dans l'esquisse (les polices n'ont pas toutes le même vide sous la ligne de base).
void texte_base(const dz::Pinceau &pc, const Police &p, const char *s, int x, int base, int w, lv_text_align_t align,
                std::uint32_t rgb) {
    dz::texte(pc, p, s, x, base - (dz::hauteur_ligne(p) - p.f->base_line), w, align, rgb);
}

// Le verre d'une carte : la teinte du thème et un filet clair à peine visible.
void carte_de_verre(const Ctx &c, int rayon) {
    const ThemePrincipal &t = *g.theme;
    for (int i = 0; i < g.nb_verre; ++i) { // déjà dans l'image du ciel, au pixel près
        const Rect &v = g.verre[i];
        if (v.x == c.x && v.y == c.y && v.w == c.w && v.h == c.h && rayon == px(28)) {
            return;
        }
    }
    if (t.carte_opa > 0) {
        dz::rect_bord(c.pc, c.x, c.y, c.w, c.h, rayon, t.carte, t.carte_opa, t.texte, 20, px(2));
    }
}

// Un point de la courbe : un halo de la teinte de la carte, puis un point de la couleur du texte.
void point_de_courbe(const dz::Pinceau &pc, const ThemePrincipal &t, int x, int y, int rayon_halo, int rayon) {
    dz::disque(pc, x, y, rayon_halo, t.carte);
    dz::disque(pc, x, y, rayon, t.texte);
}

// Le tracé d'une courbe monotone en petits traits arrondis, chacun de la couleur de la température qu'il traverse (mélange entre les
// teintes de ses deux points).
void tracer_courbe(const dz::Pinceau &pc, const accueil::CourbeMonotone &courbe, const std::uint32_t *teintes,
                   int epaisseur, int pas) {
    for (int i = 0; i + 1 < courbe.n; ++i) {
        const int x0 = courbe.p[i].x;
        const int x1 = courbe.p[i + 1].x;
        const Rgb a = depuis_hex(teintes[i]);
        const Rgb b = depuis_hex(teintes[i + 1]);
        for (int xa = x0; xa < x1;) {
            const int xb = std::min(x1, xa + pas);
            const int milieu_pm = x1 > x0 ? ((xa + xb) / 2 - x0) * 1000 / (x1 - x0) : 0;
            dz::trait(pc, xa, courbe.y_en(xa), xb, courbe.y_en(xb), epaisseur, en_hex(melanger(a, b, milieu_pm)));
            xa = xb;
        }
    }
}

namespace {

// Une bande verticale d'un voile : sa place et son dégradé (couleur et opacité en haut et en bas).
struct BandeVoile {
    int x = 0, y = 0, w = 0, h = 0;
    std::uint32_t c_haut = 0, c_bas = 0;
    std::uint8_t a_haut = 0, a_bas = 0;
};

// Les bandes du voile entre deux courbes (`haut` au-dessus de `bas`), de `x_debut` à `x_fin`, de `pas` pixels chacune ; chaque bout
// reprend le dégradé global : le même résultat qu'un seul dégradé sous toute la courbe, sans polygone.
template <typename F>
void bandes_du_voile(const accueil::CourbeMonotone &haut, const accueil::CourbeMonotone &bas, int x_debut, int x_fin,
                     int pas, const DegradeGlobal &d, F &&f) {
    for (int x = x_debut; x < x_fin; x += pas) {
        BandeVoile b;
        b.x = x;
        b.w = std::min(pas, x_fin - x);
        const int xm = x + b.w / 2;
        const int y_haut = haut.y_en(xm);
        const int y_bas = bas.y_en(xm);
        if (y_bas <= y_haut) {
            continue;
        }
        b.y = y_haut;
        b.h = y_bas - y_haut;
        d.en(y_haut, &b.c_haut, &b.a_haut);
        d.en(y_bas, &b.c_bas, &b.a_bas);
        f(b);
    }
}

// Cuit les bandes du voile dans l'image de `v` (boîte x1, y1, w, h ; A8 si `une_teinte`, sinon RGB565A8), réutilisée si sa taille et
// son format conviennent. Faux si l'image ne peut pas être allouée.
template <typename Parcours>
bool cuire_voile(VoileCuit &v, int x1, int y1, int w, int h, bool une_teinte, Parcours &&parcourir) {
    const lv_color_format_t cf = une_teinte ? LV_COLOR_FORMAT_A8 : LV_COLOR_FORMAT_RGB565A8;
    if (v.image != nullptr && (static_cast<int>(v.image->header.w) != w || static_cast<int>(v.image->header.h) != h ||
                               v.image->header.cf != cf)) {
        lv_draw_buf_destroy(v.image);
        --images_voile;
        v.image = nullptr;
    }
    if (v.image == nullptr) {
        v.image = lv_draw_buf_create(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), cf, LV_STRIDE_AUTO);
        if (v.image == nullptr) {
            return false;
        }
        ++images_voile;
    }
    std::memset(v.image->data, 0, v.image->data_size); // masque nul : hors des bandes, l'image ne touche à rien
    const std::uint32_t stride = v.image->header.stride;
    std::uint8_t *couleurs = v.image->data;
    // RGB565A8 : les pixels RGB565, puis le masque A8 dont chaque ligne fait la moitié du pas des pixels (lv_draw_sw_img.c).
    std::uint8_t *masque = une_teinte ? couleurs : couleurs + static_cast<std::size_t>(stride) * h;
    const std::uint32_t pas_masque = une_teinte ? stride : stride / 2;
    parcourir([&](const BandeVoile &b) {
        const lv_grad_dsc_t grad = dz::degrade_v(b.c_haut, b.a_haut, b.c_bas, b.a_bas);
        for (int r = 0; r < b.h; ++r) {
            lv_color_t c;
            lv_opa_t a;
            lv_draw_sw_grad_color_calculate(&grad, b.h, r, &c, &a); // la ligne r d'un rectangle de b.h lignes
            if (a <= LV_OPA_MIN) {
                continue; // le remplissage saute une ligne aussi transparente (lv_draw_sw_blend)
            }
            const int y = b.y - y1 + r;
            std::memset(masque + static_cast<std::size_t>(y) * pas_masque + (b.x - x1), a,
                        static_cast<std::size_t>(b.w));
            if (!une_teinte) {
                const std::uint16_t c16 = lv_color_to_u16(c);
                auto *ligne =
                    reinterpret_cast<std::uint16_t *>(couleurs + static_cast<std::size_t>(y) * stride) + (b.x - x1);
                for (int i = 0; i < b.w; ++i) {
                    ligne[i] = c16;
                }
            }
        }
    });
    return true;
}
} // namespace

// Le voile entre deux courbes, sous la courbe des heures ou entre les températures maximales et minimales des cinq jours.
//
// Dessinée telle quelle, chaque bande est un rectangle à dégradé vertical que LVGL remplit LIGNE PAR LIGNE : ≈ 144 bandes × 28 lignes
// ≈ 4 000 mélanges par bande d'écran repeinte, ≈ 13 900 des ≈ 15 900 mélanges d'un repeint de l'accueil (profil du 08/11/2026 sur le
// Tab5). Le voile est donc CUIT une fois dans une image (PSRAM), refaite seulement quand ce qu'il montre change (son empreinte :
// données, disposition, thème, taille), puis posé d'un seul `lv_draw_image` par bande d'écran. Au pixel près le même dessin :
// chaque pixel de l'image porte la couleur et l'opacité que le remplissage aurait données à sa ligne (lv_draw_sw_grad_color_calculate,
// la fonction même du remplissage, sur la même hauteur) ; LVGL fond une image RGB565 + masque, ou A8 + couleur, avec le même
// `lv_color_16_16_mix` qu'un remplissage translucide (lv_draw_sw_blend_to_rgb565.c), et une ligne qu'il aurait sautée (opacité
// ≤ LV_OPA_MIN) a un masque nul. A8 quand le voile n'a qu'une couleur (les heures : un tiers de la mémoire), RGB565A8 sinon (les
// jours : deux couleurs). Si l'image ne peut pas être allouée, le voile est dessiné en bandes comme avant.
void voile_entre(const dz::Pinceau &pc, const accueil::CourbeMonotone &haut, const accueil::CourbeMonotone &bas,
                 int x_debut, int x_fin, int pas, const DegradeGlobal &d, VoileCuit &v) {
    const auto parcourir = [&](auto &&f) { bandes_du_voile(haut, bas, x_debut, x_fin, pas, d, f); };
    Empreinte e;
    int x1 = 0, y1 = 0, x2 = -1, y2 = -1;
    bool une_teinte = true;
    std::uint32_t teinte = 0;
    parcourir([&](const BandeVoile &b) {
        if (x2 < x1) { // la première bande
            x1 = b.x;
            y1 = b.y;
            x2 = b.x + b.w - 1;
            y2 = b.y + b.h - 1;
            teinte = b.c_haut;
        }
        x1 = std::min(x1, b.x);
        y1 = std::min(y1, b.y);
        x2 = std::max(x2, b.x + b.w - 1);
        y2 = std::max(y2, b.y + b.h - 1);
        une_teinte = une_teinte && b.c_haut == teinte && b.c_bas == teinte;
        e.u(static_cast<std::uint32_t>(b.x));
        e.u(static_cast<std::uint32_t>(b.y));
        e.u(static_cast<std::uint32_t>(b.w));
        e.u(static_cast<std::uint32_t>(b.h));
        e.u(b.c_haut);
        e.u(b.c_bas);
        e.u(static_cast<std::uint32_t>(b.a_haut) | (static_cast<std::uint32_t>(b.a_bas) << 8));
    });
    if (x2 < x1) {
        return; // aucune bande
    }
    const int w = x2 - x1 + 1;
    const int h = y2 - y1 + 1;
    if (v.image == nullptr || e.h != v.cle) {
        v.cle = 0;
        if (!cuire_voile(v, x1, y1, w, h, une_teinte, parcourir)) {
            parcourir([&](const BandeVoile &b) {
                dz::rect_degrade_v(pc, b.x, b.y, b.w, b.h, b.c_haut, b.a_haut, b.c_bas, b.a_bas);
            });
            return;
        }
        v.cle = e.h;
        v.x = x1;
        v.y = y1;
        v.teinte = teinte;
    }
    if (dz::hors_zone(pc, x1, y1, x2, y2)) {
        return;
    }
    lv_draw_image_dsc_t dsc;
    lv_draw_image_dsc_init(&dsc);
    dsc.src = v.image;
    if (v.image->header.cf == LV_COLOR_FORMAT_A8) {
        dsc.recolor = dz::couleur(v.teinte);
        dsc.recolor_opa = LV_OPA_COVER;
    }
    const lv_area_t zone = {x1, y1, x2, y2};
    lv_draw_image(pc.couche, &dsc, &zone);
}

} // namespace principal
} // namespace ecran
