#include "ui_details.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

#include "ecran/accueil.hpp"
#include "ecran/visuel_astres.hpp"
#include "ecran/visuel_echelles.hpp"
#include "ui_meteo_dessin.h"
#include "ui_meteo_icones.h"

// Page « Détails » : sept tuiles (vent, pluie, humidité, pression, UV, soleil, lune) sur le verre et le ciel de l'accueil. Tout ce qui se
// décide (valeurs, unités, « — » d'une grandeur inconnue, grisé d'une météo périmée, tailles, places) est dans le noyau pur
// ecran/grandeurs.hpp, testé sur le PC : ce fichier ne fait que poser des textes et des formes. Aucune couleur écrite ici (le thème de
// l'accueil, visuel_themes.hpp), aucun texte écrit ici (la page composée les porte, en sept langues), aucune animation.
//
// Les polices sont créées à la première ouverture et gardées (TinyTTF se libère mal : même choix que les réglages et les alertes). L'état
// de la page (la page composée, le thème grisé, la tuile du ciel tramé) est alloué par LVGL, donc en PSRAM, pour la durée de
// l'ouverture : il est rendu quand l'écran de la page est supprimé (LV_EVENT_DELETE).

namespace ecran {

namespace {

using namespace visuel;
namespace dz = dessin;
namespace gr = grandeurs;
using dz::Police;

struct Cadre {
    int x = 0, y = 0, w = 0, h = 0;
};

// Gardées entre deux ouvertures. Seul état qui reste en RAM interne quand la page est fermée.
struct Polices {
    const std::uint8_t *ttf = nullptr;
    int echelle_pm = 0; // l'échelle (en millièmes) pour laquelle elles ont été créées
    Police valeur, titre, texte;
};
Polices polices;

struct Etat {
    ContexteDetails ctx;
    float echelle = 1.0F;
    bool portrait = false;
    gr::Page page;
    gr::Zones zones;
    ThemePrincipal gris = kThemeCiel; // le thème des valeurs d'une météo périmée (accueil::theme_grise)
    lv_obj_t *barre = nullptr;
    lv_obj_t *grille = nullptr;
    lv_obj_t *fermer = nullptr;
    lv_draw_buf_t *ciel_image = nullptr; // le ciel tramé (voir appliquer_ciel)
    std::uint32_t empreinte = 0;

    // Pixels réels d'une mesure en pixels de référence (1280 x 720, ou 720 x 1280 en portrait).
    int px(int reference) const {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(reference) * echelle)));
    }
    // Un rectangle de référence en pixels réels : les bords sont arrondis, pas la largeur, pour que deux tuiles voisines se touchent
    // exactement où elles le doivent.
    Cadre cadre(const gr::Rect &r) const {
        const int x0 = static_cast<int>(std::lround(static_cast<float>(r.x) * echelle));
        const int y0 = static_cast<int>(std::lround(static_cast<float>(r.y) * echelle));
        const int x1 = static_cast<int>(std::lround(static_cast<float>(r.x + r.w) * echelle));
        const int y1 = static_cast<int>(std::lround(static_cast<float>(r.y + r.h) * echelle));
        return {x0, y0, x1 - x0, y1 - y0};
    }
    const ThemePrincipal &theme() const { return *ctx.theme; }
};

Etat *courant = nullptr; // la page montrée la plus récente (ui_details_mettre_a_jour)

Etat *etat_de(lv_event_t *evenement) { return static_cast<Etat *>(lv_event_get_user_data(evenement)); }

// ---- texte ------------------------------------------------------------------------------------------------------------

constexpr const char *POINTS = "\xE2\x80\xA6"; // …

// Copie `s` dans `sortie` ; si elle dépasse `largeur`, la coupe à la limite d'un caractère UTF-8 complet et pose « … ». Un texte
// n'est jamais réduit sous la taille de sa police : il est coupé (24 pixels réels au moins sur toute la page).
void ajuster(const Police &p, const char *s, int largeur, char *sortie, std::size_t n) {
    std::snprintf(sortie, n, "%s", s != nullptr ? s : "");
    if (dz::largeur_texte(p, sortie) <= largeur) {
        return;
    }
    std::size_t k = std::strlen(sortie);
    while (k > 0 && dz::largeur_texte(p, sortie) + dz::largeur_texte(p, POINTS) > largeur) {
        --k;
        while (k > 0 && (static_cast<unsigned char>(sortie[k]) & 0xC0) == 0x80) {
            --k;
        }
        sortie[k] = '\0';
    }
    if (k + 4 < n) {
        std::memcpy(sortie + k, POINTS, 4);
    }
}

// Un texte d'une ligne, centré verticalement sur `y_centre`, coupé par « … » s'il dépasse `w`.
void texte_centre(const dz::Pinceau &pc, const Police &p, const char *s, int x, int y_centre, int w,
                  lv_text_align_t align, std::uint32_t rgb) {
    if (s == nullptr || s[0] == '\0' || w <= 0) {
        return;
    }
    char tampon[96];
    ajuster(p, s, w, tampon, sizeof tampon);
    dz::texte(pc, p, tampon, x, y_centre - dz::hauteur_ligne(p) / 2, w, align, rgb);
}

// ---- fond : le ciel de l'accueil ---------------------------------------------------------------------------------------------

// Même tramage que l'accueil (main/ui_principal.cpp) : un dégradé vertical sombre calculé tel quel par LVGL montre des bandes en RGB565 ;
// une matrice de Bayer 8 x 8 mélange finement les deux teintes voisines de chaque marche. Copié et non partagé pour ne pas toucher
// l'accueil (une autre session le modifie) ; à factoriser quand elle aura fini (voir l'ADR de la page).
//
// La trame se répète toutes les 8 colonnes : l'image n'est qu'une TUILE de kLargeurTuileCiel colonnes sur toute la hauteur (92 Ko à
// 1280 x 720 au lieu de 1,8 Mo), que LVGL répète (`bg_image_tiled`). Les pixels sont les mêmes que ceux d'une image de la taille de
// l'écran. Pourquoi : relue de la PSRAM à chaque bande repeinte, l'image pleine taille a rendu la repeinte de l'accueil nettement plus
// lente (239 -> 183 ms mesurés sur la 7B avec la tuile). Mesuré sur cette page (7B) : ouverture 187 -> 102 ms, docs/mesures/details-ciel-en-mosaique.md.
constexpr int kLargeurTuileCiel = 64; // multiple de 8 : la trame de Bayer ne se raccorde qu'à ce prix
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
        const std::int32_t r = a[0] + static_cast<std::int32_t>(static_cast<std::int64_t>(d[0]) * y / dernier);
        const std::int32_t gv = a[1] + static_cast<std::int32_t>(static_cast<std::int64_t>(d[1]) * y / dernier);
        const std::int32_t b = a[2] + static_cast<std::int32_t>(static_cast<std::int64_t>(d[2]) * y / dernier);
        auto *ligne = reinterpret_cast<std::uint16_t *>(static_cast<std::uint8_t *>(buf->data) +
                                                        static_cast<std::size_t>(y) * buf->header.stride);
        const std::uint8_t *seuil = BAYER_8[y & 7];
        for (int x = 0; x < w; ++x) {
            const std::int32_t s = seuil[x & 7];
            std::int32_t qr = (r + s * 32) >> 11;
            std::int32_t qg = (gv + s * 16) >> 10;
            std::int32_t qb = (b + s * 32) >> 11;
            qr = qr > 31 ? 31 : qr;
            qg = qg > 63 ? 63 : qg;
            qb = qb > 31 ? 31 : qb;
            ligne[x] = static_cast<std::uint16_t>((qr << 11) | (qg << 5) | qb);
        }
    }
}

void appliquer_ciel(Etat &E) {
    lv_obj_t *racine = E.ctx.racine;
    const ThemePrincipal &t = E.theme();
    if (!t.ciel_dynamique) {
        lv_obj_set_style_bg_color(racine, dz::couleur(t.fond), 0);
        lv_obj_set_style_bg_opa(racine, LV_OPA_COVER, 0);
        return;
    }
    const Degrade d = ciel(E.page.ciel.moment, E.page.ciel.ambiance);
    // Le dégradé de LVGL reste réglé (couleurs) : c'est le repli si l'image tramée ne peut pas être créée (pas de PSRAM libre). Tant que
    // l'image couvre l'écran il est coupé (voir plus bas) : LVGL le dessinait sous l'image, à chaque bande, pour rien.
    lv_obj_set_style_bg_color(racine, dz::couleur(d.haut), 0);
    lv_obj_set_style_bg_grad_color(racine, dz::couleur(d.bas), 0);
    lv_obj_set_style_bg_opa(racine, LV_OPA_COVER, 0);
    lv_draw_buf_t *neuf =
        lv_draw_buf_create(static_cast<std::uint32_t>(kLargeurTuileCiel), static_cast<std::uint32_t>(E.ctx.hauteur),
                           LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    if (neuf == nullptr) {
        lv_obj_set_style_bg_grad_dir(racine, LV_GRAD_DIR_VER, 0);
        return;
    }
    remplir_ciel_trame(neuf, d.haut, d.bas);
    lv_obj_set_style_bg_image_src(racine, neuf, 0);
    lv_obj_set_style_bg_image_tiled(racine, true, 0);
    lv_obj_set_style_bg_image_opa(racine, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_dir(racine, LV_GRAD_DIR_NONE, 0);
    if (E.ciel_image != nullptr) { // l'ancienne image n'est plus référencée par le style : on peut la rendre
        lv_draw_buf_destroy(E.ciel_image);
    }
    E.ciel_image = neuf;
}

// ---- le verre et les pièces communes ------------------------------------------------------------------------------------------

// Le verre d'une carte : la teinte du thème et un filet clair à peine visible (comme `carte_de_verre` de l'accueil).
void verre(const Etat &E, const dz::Pinceau &pc, const Cadre &c) {
    const ThemePrincipal &t = E.theme();
    if (t.carte_opa > 0) {
        dz::rect_bord(pc, c.x, c.y, c.w, c.h, E.px(gr::RAYON_TUILE), t.carte, t.carte_opa, t.texte, 20, E.px(2));
    }
}

// Une flèche de vent à deux triangles (pointe, deux ailes, un creux) qui pointe VERS où le vent souffle (angle : 0 en haut, sens horaire).
void fleche_vent(const dz::Pinceau &pc, int cx, int cy, int taille, int angle_deg, std::uint32_t rgb) {
    const double a = angle_deg * 3.14159265358979 / 180.0;
    const double s = std::sin(a);
    const double co = std::cos(a);
    const double k = taille / 2.0;
    const auto pt = [&](double x, double y) {
        return lv_point_t{cx + static_cast<int>(std::lround((x * co - y * s) * k)),
                          cy + static_cast<int>(std::lround((x * s + y * co) * k))};
    };
    const lv_point_t pointe = pt(0.0, -1.0);
    const lv_point_t aile_d = pt(0.62, 0.85);
    const lv_point_t creux = pt(0.0, 0.42);
    const lv_point_t aile_g = pt(-0.62, 0.85);
    dz::triangle(pc, pointe.x, pointe.y, aile_d.x, aile_d.y, creux.x, creux.y, rgb);
    dz::triangle(pc, pointe.x, pointe.y, creux.x, creux.y, aile_g.x, aile_g.y, rgb);
}

void triangle_haut_bas(const dz::Pinceau &pc, int cx, int cy, int r, bool haut, std::uint32_t rgb) {
    const int pointe = haut ? cy - r : cy + r;
    const int base = haut ? cy + r * 7 / 10 : cy - r * 7 / 10;
    dz::triangle(pc, cx, pointe, cx - r, base, cx + r, base, rgb);
}

// Les mesures communes d'une tuile (pixels réels) : trois rangées, l'en-tête (icône et libellé), la valeur, les lignes de détail.
struct Mise {
    Cadre c;
    int pad = 0;      // marge intérieure
    int ico = 0;      // côté de l'icône
    int x_in = 0;     // bord gauche du contenu
    int w_in = 0;     // largeur du contenu
    int y_icone = 0;  // haut de la rangée de l'icône
    int y_valeur = 0; // haut de la ligne de la valeur
    int y_ligne1 = 0; // haut du premier détail
    int y_ligne2 = 0; // haut du second détail
};

Mise mise(const Etat &E, const Cadre &c) {
    Mise m;
    m.c = c;
    m.pad = E.px(gr::TUILE_MARGE);
    m.ico = E.px(gr::TUILE_ICONE);
    m.x_in = c.x + m.pad;
    m.w_in = c.w - 2 * m.pad;
    m.y_icone = c.y + m.pad;
    m.y_valeur = m.y_icone + m.ico + E.px(gr::TUILE_ESPACE);
    m.y_ligne1 = m.y_valeur + dz::hauteur_ligne(polices.valeur);
    m.y_ligne2 = m.y_ligne1 + dz::hauteur_ligne(polices.texte);
    return m;
}

// Le libellé à droite de l'icône, centré sur elle ; `reserve` : largeur gardée à droite (flèche du vent, tendance de la pression).
void libelle(const Etat &E, const dz::Pinceau &pc, const ThemePrincipal &t, const Mise &m, const gr::Tuile &tu,
             int reserve) {
    const int x = m.x_in + m.ico + E.px(12);
    texte_centre(pc, polices.texte, tu.libelle, x, m.y_icone + m.ico / 2, m.c.x + m.c.w - m.pad - reserve - x,
                 LV_TEXT_ALIGN_LEFT, t.texte_2);
}

// La valeur en gros, suivie de son unité en plus petit sur la même ligne de base. Inconnue : « — » en texte discret, sans unité.
void valeur(const Etat &E, const dz::Pinceau &pc, const ThemePrincipal &t, const Mise &m, const gr::Tuile &tu,
            std::uint32_t rgb) {
    const Police &fv = polices.valeur;
    const std::uint32_t couleur = tu.connue ? rgb : t.texte_3;
    dz::texte(pc, fv, tu.valeur, m.x_in, m.y_valeur, m.w_in, LV_TEXT_ALIGN_LEFT, couleur);
    if (tu.unite[0] == '\0') {
        return;
    }
    const Police &fu = polices.texte;
    const int w_valeur = dz::largeur_texte(fv, tu.valeur);
    const int ligne_de_base = m.y_valeur + dz::hauteur_ligne(fv) - fv.f->base_line;
    const int y_unite = ligne_de_base - (dz::hauteur_ligne(fu) - fu.f->base_line);
    dz::texte(pc, fu, tu.unite, m.x_in + w_valeur + E.px(8), y_unite, std::max(0, m.w_in - w_valeur - E.px(8)),
              LV_TEXT_ALIGN_LEFT, t.texte_3);
}

// Un détail d'une ligne (coupé par « … » s'il dépasse). Le premier détail d'une tuile qui n'a pas de second peut passer à la ligne.
void detail(const dz::Pinceau &pc, const Mise &m, const char *s, int y, std::uint32_t rgb, bool deux_lignes) {
    if (s == nullptr || s[0] == '\0') {
        return;
    }
    if (deux_lignes && dz::largeur_texte(polices.texte, s) > m.w_in) {
        dz::texte(pc, polices.texte, s, m.x_in, y, m.w_in, LV_TEXT_ALIGN_LEFT, rgb, LV_OPA_COVER, 2);
        return;
    }
    texte_centre(pc, polices.texte, s, m.x_in, y + dz::hauteur_ligne(polices.texte) / 2, m.w_in, LV_TEXT_ALIGN_LEFT,
                 rgb);
}

// ---- les sept tuiles ------------------------------------------------------------------------------------------------------------

void tuile_vent(const Etat &E, const dz::Pinceau &pc, const ThemePrincipal &t, const gr::Tuile &tu, const Cadre &c) {
    const Mise m = mise(E, c);
    const std::uint32_t rgb = couleur_metrique(t, Metrique::VENT);
    const int rayon = m.ico / 2;
    icones::piece(pc.couche, Piece::vent, m.ico, m.x_in, m.y_icone, rgb);
    libelle(E, pc, t, m, tu, 2 * rayon + E.px(12));
    // La flèche, dans un anneau, en haut à droite : sans direction connue, rien n'est dessiné (jamais une flèche vers le nord par défaut).
    if (tu.fleche_deg != INCONNU) {
        const int cx = c.x + c.w - m.pad - rayon;
        const int cy = m.y_icone + rayon;
        dz::anneau(pc, cx, cy, rayon, E.px(3), t.filet);
        fleche_vent(pc, cx, cy, rayon * 12 / 10, tu.fleche_deg, rgb);
    }
    valeur(E, pc, t, m, tu, t.texte);
    detail(pc, m, tu.ligne1, m.y_ligne1, t.texte_2, false);
    detail(pc, m, tu.ligne2, m.y_ligne2, t.texte_2, false);
}

void tuile_pluie(const Etat &E, const dz::Pinceau &pc, const ThemePrincipal &t, const gr::Tuile &tu, const Cadre &c) {
    const Mise m = mise(E, c);
    icones::piece(pc.couche, Piece::parapluie, m.ico, m.x_in, m.y_icone, couleur_metrique(t, Metrique::PLUIE));
    libelle(E, pc, t, m, tu, 0);
    valeur(E, pc, t, m, tu, t.texte);
    detail(pc, m, tu.ligne1, m.y_ligne1, t.texte_2, true);
}

void tuile_humidite(const Etat &E, const dz::Pinceau &pc, const ThemePrincipal &t, const gr::Tuile &tu,
                    const Cadre &c) {
    const Mise m = mise(E, c);
    const std::uint32_t rgb = couleur_metrique(t, Metrique::HUMIDITE);
    icones::piece(pc.couche, Piece::humidite, m.ico, m.x_in, m.y_icone, rgb);
    libelle(E, pc, t, m, tu, 0);
    valeur(E, pc, t, m, tu, t.texte);
    // La jauge : un rail et son remplissage, à la place du premier détail. Sans valeur, le rail reste vide.
    const int h = E.px(14);
    const int y = m.y_ligne1 + dz::hauteur_ligne(polices.texte) / 2 - h / 2;
    dz::rect(pc, m.x_in, y, m.w_in, h, h / 2, t.filet);
    if (tu.jauge_pm != INCONNU) {
        dz::rect(pc, m.x_in, y, std::max(h, m.w_in * tu.jauge_pm / 1000), h, h / 2, rgb);
    }
}

void tuile_pression(const Etat &E, const dz::Pinceau &pc, const ThemePrincipal &t, const gr::Tuile &tu,
                    const Cadre &c) {
    const Mise m = mise(E, c);
    const std::uint32_t rgb = couleur_metrique(t, Metrique::PRESSION);
    icones::piece(pc.couche, Piece::pression, m.ico, m.x_in, m.y_icone, rgb);
    libelle(E, pc, t, m, tu, E.px(44));
    // La tendance sur trois heures : un triangle qui monte ou descend, ou un trait pour « stable », en haut à droite.
    const int cx = c.x + c.w - m.pad - E.px(14);
    const int cy = m.y_icone + m.ico / 2;
    if (tu.tendance == TendancePression::HAUSSE) {
        triangle_haut_bas(pc, cx, cy, E.px(13), true, rgb);
    } else if (tu.tendance == TendancePression::BAISSE) {
        triangle_haut_bas(pc, cx, cy, E.px(13), false, rgb);
    } else if (tu.tendance == TendancePression::STABLE) {
        dz::trait(pc, cx - E.px(13), cy, cx + E.px(13), cy, E.px(5), rgb);
    }
    valeur(E, pc, t, m, tu, t.texte);
    detail(pc, m, tu.ligne1, m.y_ligne1, t.texte_2, false);
}

void tuile_uv(const Etat &E, const dz::Pinceau &pc, const ThemePrincipal &t, const gr::Tuile &tu, const Cadre &c) {
    const Mise m = mise(E, c);
    icones::piece(pc.couche, Piece::uv, m.ico, m.x_in, m.y_icone, couleur_metrique(t, Metrique::UV));
    libelle(E, pc, t, m, tu, 0);
    valeur(E, pc, t, m, tu, couleur_uv(t, tu.categorie_uv));
    detail(pc, m, tu.ligne1, m.y_ligne1, t.texte_2, false);
    // Cinq segments qui s'allument jusqu'au niveau (la couleur de chaque segment est celle de son niveau).
    const int h = E.px(14);
    const int ecart = E.px(6);
    const int seg = (m.w_in - 4 * ecart) / 5;
    const int y = m.y_ligne2 + dz::hauteur_ligne(polices.texte) / 2 - h / 2;
    for (int i = 0; i < 5; ++i) {
        dz::rect(pc, m.x_in + i * (seg + ecart), y, seg, h, h / 2, i < tu.segments ? t.niveau_uv[i] : t.filet);
    }
}

// Le soleil : les icônes du lever et du coucher aux deux coins, « Soleil » entre elles, les deux heures en gros, l'arc du jour entre les
// deux heures avec le soleil à sa place, les mots « Lever » et « Coucher » dessous.
void tuile_soleil(const Etat &E, const dz::Pinceau &pc, const ThemePrincipal &t, const gr::Tuile &tu, const Cadre &c) {
    const Mise m = mise(E, c);
    const std::uint32_t rgb = couleur_metrique(t, Metrique::SOLEIL);
    icones::piece(pc.couche, Piece::lever, m.ico, m.x_in, m.y_icone, rgb);
    icones::piece(pc.couche, Piece::coucher, m.ico, c.x + c.w - m.pad - m.ico, m.y_icone, rgb);
    const int bord = m.pad + m.ico + E.px(12);
    texte_centre(pc, polices.texte, tu.libelle, c.x + bord, m.y_icone + m.ico / 2, c.w - 2 * bord, LV_TEXT_ALIGN_CENTER,
                 t.texte_2);

    const std::uint32_t couleur = tu.connue ? t.texte : t.texte_3;
    const int moitie = m.w_in / 2;
    dz::texte(pc, polices.valeur, tu.valeur, m.x_in, m.y_valeur, moitie, LV_TEXT_ALIGN_LEFT, couleur);
    dz::texte(pc, polices.valeur, tu.valeur2, c.x + c.w - m.pad - moitie, m.y_valeur, moitie, LV_TEXT_ALIGN_RIGHT,
              couleur);

    // L'arc : un demi-cercle posé sur l'horizon, au centre de la tuile. Rempli du lever jusqu'au soleil, qui est un disque avec son halo.
    const int rayon = E.px(62);
    const int cx = c.x + c.w / 2;
    const int cy = m.y_valeur + dz::hauteur_ligne(polices.valeur) - E.px(12);
    dz::arc(pc, cx, cy, rayon, E.px(5), 180, 360, t.filet, LV_OPA_COVER, true);
    dz::trait(pc, cx - rayon - E.px(8), cy, cx + rayon + E.px(8), cy, E.px(3), t.filet, LV_OPA_COVER, false);
    if (tu.position_pm != INCONNU) {
        const int fin = 180 + tu.position_pm * 180 / 1000;
        if (fin > 182) {
            dz::arc(pc, cx, cy, rayon, E.px(5), 180, fin, rgb);
        }
        const double ang = fin * 3.14159265358979 / 180.0;
        const int sx = cx + static_cast<int>(std::lround(std::cos(ang) * rayon));
        const int sy = cy + static_cast<int>(std::lround(std::sin(ang) * rayon));
        dz::disque(pc, sx, sy, E.px(15), rgb, 90);
        dz::disque(pc, sx, sy, E.px(10), rgb);
    }
    const int y_mots = m.y_ligne1 + dz::hauteur_ligne(polices.texte) / 2;
    texte_centre(pc, polices.texte, tu.ligne1, m.x_in, y_mots, moitie, LV_TEXT_ALIGN_LEFT, t.texte_2);
    texte_centre(pc, polices.texte, tu.ligne2, c.x + c.w - m.pad - moitie, y_mots, moitie, LV_TEXT_ALIGN_RIGHT,
                 t.texte_2);
}

// La lune : l'image de la phase du jour (calculée sur l'appareil), l'éclairement en gros, le nom de la phase dessous (sur deux lignes s'il
// est long).
void tuile_lune(const Etat &E, const dz::Pinceau &pc, const ThemePrincipal &t, const gr::Tuile &tu, const Cadre &c) {
    const Mise m = mise(E, c);
    const std::uint32_t rgb = couleur_metrique(t, Metrique::LUNE);
    if (tu.phase != INCONNU) {
        icones::lune(pc.couche, tu.phase, m.ico, m.x_in, m.y_icone, rgb);
    } else {
        dz::anneau(pc, m.x_in + m.ico / 2, m.y_icone + m.ico / 2, m.ico / 2, E.px(3),
                   t.filet); // pas d'heure sûre : un disque vide
    }
    libelle(E, pc, t, m, tu, 0);
    valeur(E, pc, t, m, tu, t.texte);
    detail(pc, m, tu.ligne1, m.y_ligne1, t.texte_2, true);
}

void tuile(const Etat &E, const dz::Pinceau &pc, const gr::Tuile &tu, const Cadre &c) {
    verre(E, pc, c);
    const ThemePrincipal &t = tu.grisee ? E.gris : E.theme();
    switch (tu.grandeur) {
    case Metrique::VENT:
        tuile_vent(E, pc, t, tu, c);
        break;
    case Metrique::PLUIE:
        tuile_pluie(E, pc, t, tu, c);
        break;
    case Metrique::HUMIDITE:
        tuile_humidite(E, pc, t, tu, c);
        break;
    case Metrique::PRESSION:
        tuile_pression(E, pc, t, tu, c);
        break;
    case Metrique::UV:
        tuile_uv(E, pc, t, tu, c);
        break;
    case Metrique::SOLEIL:
        tuile_soleil(E, pc, t, tu, c);
        break;
    case Metrique::LUNE:
        tuile_lune(E, pc, t, tu, c);
        break;
    case Metrique::NOMBRE:
        break;
    }
}

// ---- les trois objets ---------------------------------------------------------------------------------------------------------

void sur_dessin_grille(lv_event_t *evenement) {
    const Etat &E = *etat_de(evenement);
    const dz::Pinceau pc = {lv_event_get_layer(evenement), E.ctx.theme};
    for (int i = 0; i < gr::NB_TUILES; ++i) {
        tuile(E, pc, E.page.tuile[i], E.cadre(E.zones.tuile[i]));
    }
}

// La barre : le titre et la ville à gauche ; à droite, l'état des données (« Exemple », « Données de 07:52 · périmées », « Météo
// indisponible »). En portrait la place manque pour tout sur une ligne : le titre (et « Exemple ») en haut, l'état à la place de la ville dessous.
void sur_dessin_barre(lv_event_t *evenement) {
    const Etat &E = *etat_de(evenement);
    const gr::Page &p = E.page;
    const dz::Pinceau pc = {lv_event_get_layer(evenement), E.ctx.theme};
    const ThemePrincipal &t = E.theme();
    const Cadre c = E.cadre(E.zones.barre);
    const Police &ft = polices.titre;
    const Police &fx = polices.texte;
    const int piece = E.px(32);
    const bool mention = p.mention[0] != '\0';
    const bool exemple = p.exemple[0] != '\0';
    const int w_exemple = exemple ? dz::largeur_texte(fx, p.exemple) + E.px(28) : 0;
    const int w_mention = mention ? dz::largeur_texte(fx, p.mention) + piece + E.px(8) : 0;
    const int hauteur_titre = dz::hauteur_ligne(ft);
    const int hauteur_texte = dz::hauteur_ligne(fx);

    const int y1 = E.portrait ? c.y + (c.h - hauteur_titre - hauteur_texte) / 2 + hauteur_titre / 2
                              : c.y + c.h / 2;                 // centre de la 1re ligne
    const int y2 = y1 + hauteur_titre / 2 + hauteur_texte / 2; // centre de la 2e (portrait)

    int droite = c.x + c.w;
    if (exemple) {
        const int h = E.px(40);
        dz::rect(pc, droite - w_exemple, y1 - h / 2, w_exemple, h, h / 2, t.avertissement);
        dz::texte(pc, fx, p.exemple, droite - w_exemple, y1 - hauteur_texte / 2, w_exemple, LV_TEXT_ALIGN_CENTER,
                  t.texte_sur_couleur);
        droite -= w_exemple + E.px(16);
    }
    int libre = droite; // bord droit de ce que la ville peut occuper
    if (mention) {
        const int yc = E.portrait ? y2 : y1;
        const int x = (E.portrait ? c.x + c.w : droite) - w_mention;
        icones::piece(pc.couche, Piece::hors_ligne, piece, E.portrait ? c.x : x, yc - piece / 2, t.avertissement);
        if (E.portrait) {
            texte_centre(pc, fx, p.mention, c.x + piece + E.px(8), yc, c.w - piece - E.px(8), LV_TEXT_ALIGN_LEFT,
                         t.avertissement);
        } else {
            dz::texte(pc, fx, p.mention, x + piece + E.px(8), yc - hauteur_texte / 2, w_mention - piece - E.px(8),
                      LV_TEXT_ALIGN_RIGHT, t.avertissement);
            libre = x - E.px(24);
        }
    }
    const int w_titre = dz::largeur_texte(ft, p.titre);
    texte_centre(pc, ft, p.titre, c.x, y1, std::min(w_titre + E.px(4), c.w), LV_TEXT_ALIGN_LEFT, t.texte);
    // La ville : après le titre en paysage ; en portrait sous lui, sauf quand l'état de la météo prend cette ligne.
    if (p.ville[0] != '\0' && !(E.portrait && mention)) {
        const int x0 = E.portrait ? c.x : c.x + w_titre + E.px(28);
        const int yc = E.portrait ? y2 : y1;
        icones::piece(pc.couche, Piece::epingle, piece, x0, yc - piece / 2, t.texte_2);
        texte_centre(pc, fx, p.ville, x0 + piece + E.px(8), yc, (E.portrait ? c.x + c.w : libre) - x0 - piece - E.px(8),
                     LV_TEXT_ALIGN_LEFT, t.texte_2);
    }
}

void sur_dessin_fermer(lv_event_t *evenement) {
    const Etat &E = *etat_de(evenement);
    lv_obj_t *objet = lv_event_get_target_obj(evenement);
    const dz::Pinceau pc = {lv_event_get_layer(evenement), E.ctx.theme};
    const ThemePrincipal &t = E.theme();
    lv_area_t a;
    lv_obj_get_coords(objet, &a);
    const int w = lv_area_get_width(&a);
    const int h = lv_area_get_height(&a);
    const bool appuye = lv_obj_has_state(objet, LV_STATE_PRESSED);
    dz::rect(pc, a.x1, a.y1, w, h, E.px(20), appuye ? t.filet : t.accent);
    texte_centre(pc, polices.titre, E.page.fermer, a.x1, a.y1 + h / 2, w, LV_TEXT_ALIGN_CENTER, t.texte_sur_couleur);
}

void sur_evenement_fermer(lv_event_t *evenement) {
    Etat *E = etat_de(evenement);
    switch (lv_event_get_code(evenement)) {
    case LV_EVENT_CLICKED:
        if (E->ctx.fermer != nullptr) {
            E->ctx.fermer();
        }
        break;
    case LV_EVENT_PRESSED:
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        lv_obj_invalidate(E->fermer); // la couleur du bouton suit le doigt
        break;
    default:
        break;
    }
}

// La suppression de l'écran de la page (LV_EVENT_DELETE, envoyé AVANT celle de ses enfants) rend l'état et le ciel. Les rappels des trois
// objets sont retirés d'abord : plus rien ne lira l'état pendant qu'ils partent.
void sur_suppression(lv_event_t *evenement) {
    Etat *E = etat_de(evenement);
    for (lv_obj_t *o : {E->barre, E->grille, E->fermer}) {
        if (o != nullptr) {
            lv_obj_remove_event_cb_with_user_data(o, nullptr, E);
        }
    }
    lv_obj_set_style_bg_image_src(E->ctx.racine, nullptr, 0);
    if (E->ciel_image != nullptr) {
        lv_draw_buf_destroy(E->ciel_image);
        E->ciel_image = nullptr;
    }
    if (courant == E) {
        courant = nullptr;
    }
    E->~Etat();
    lv_free(E);
}

lv_obj_t *creer_zone(lv_obj_t *parent, const Cadre &c) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, c.x, c.y);
    lv_obj_set_size(o, c.w, c.h);
    return o;
}

// Les trois polices, à la taille de la première ouverture (ou d'une autre échelle d'écran : les anciennes sont alors perdues, TinyTTF
// se libère mal). Rend faux si l'une n'a pas pu être créée.
bool preparer_polices(const ContexteDetails &ctx, const Etat &E) {
    const int echelle_pm = static_cast<int>(std::lround(E.echelle * 1000.0F));
    if (polices.valeur.f != nullptr && polices.titre.f != nullptr && polices.texte.f != nullptr &&
        polices.ttf == ctx.ttf && polices.echelle_pm == echelle_pm) {
        return true;
    }
    const auto creer = [&](int reference) {
        Police p;
        p.px = E.px(reference);
        p.f = lv_tiny_ttf_create_data(ctx.ttf, ctx.ttf_taille, p.px);
        return p;
    };
    Polices neuves;
    neuves.ttf = ctx.ttf;
    neuves.echelle_pm = echelle_pm;
    neuves.valeur = creer(gr::POLICE_VALEUR);
    neuves.titre = creer(gr::POLICE_TITRE);
    neuves.texte = creer(gr::POLICE_TEXTE);
    if (neuves.valeur.f == nullptr || neuves.titre.f == nullptr || neuves.texte.f == nullptr) {
        return false;
    }
    polices = neuves;
    return true;
}

} // namespace

bool ui_details_creer(const ContexteDetails &ctx, const gr::Page &page) {
    if (ctx.racine == nullptr || ctx.ttf == nullptr || ctx.theme == nullptr || ctx.largeur <= 0 || ctx.hauteur <= 0) {
        return false;
    }
    void *memoire = lv_malloc(sizeof(Etat));
    if (memoire == nullptr) {
        return false;
    }
    Etat *E = new (memoire) Etat();
    E->ctx = ctx;
    E->portrait = ctx.hauteur > ctx.largeur;
    const float ref_l = E->portrait ? 720.0F : 1280.0F;
    const float ref_h = E->portrait ? 1280.0F : 720.0F;
    E->echelle = std::min(static_cast<float>(ctx.largeur) / ref_l, static_cast<float>(ctx.hauteur) / ref_h);
    if (!preparer_polices(ctx, *E)) {
        E->~Etat();
        lv_free(E);
        return false;
    }
    E->page = page;
    E->gris = accueil::theme_grise(*ctx.theme);
    E->empreinte = gr::empreinte(page);
    // La grille se calcule dans l'écran de référence : 1280 x 720 (Tab5), 1280 x 750 (7B de 1024 x 600 à l'échelle 0,8), 720 x 1280.
    const int ref_largeur = std::max(1, static_cast<int>(std::lround(static_cast<float>(ctx.largeur) / E->echelle)));
    const int ref_hauteur = std::max(1, static_cast<int>(std::lround(static_cast<float>(ctx.hauteur) / E->echelle)));
    E->zones = gr::disposer(ref_largeur, ref_hauteur, E->portrait);

    lv_obj_t *racine = ctx.racine;
    lv_obj_remove_flag(racine, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(racine,
                       LV_OBJ_FLAG_CLICKABLE); // pas d'appui long ici : la roue n'est pas rouverte par cette page
    lv_obj_add_event_cb(racine, sur_suppression, LV_EVENT_DELETE, E);
    appliquer_ciel(*E);

    // La grille englobe les sept tuiles ; elle ne reçoit aucun toucher.
    gr::Rect englobant = E->zones.tuile[0];
    int droite = englobant.x + englobant.w;
    int bas = englobant.y + englobant.h;
    for (const gr::Rect &r : E->zones.tuile) {
        englobant.x = std::min(englobant.x, r.x);
        englobant.y = std::min(englobant.y, r.y);
        droite = std::max(droite, r.x + r.w);
        bas = std::max(bas, r.y + r.h);
    }
    englobant.w = droite - englobant.x;
    englobant.h = bas - englobant.y;
    E->grille = creer_zone(racine, E->cadre(englobant));
    lv_obj_add_event_cb(E->grille, sur_dessin_grille, LV_EVENT_DRAW_MAIN, E);

    E->barre = creer_zone(racine, E->cadre(E->zones.barre));
    lv_obj_add_event_cb(E->barre, sur_dessin_barre, LV_EVENT_DRAW_MAIN, E);

    // « Fermer » : 180 x 80 de référence, soit plus de 64 pixels réels de haut sur la 7B ; la zone tactile déborde de 12 pixels.
    E->fermer = creer_zone(racine, E->cadre(E->zones.fermer));
    lv_obj_add_flag(E->fermer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(E->fermer, E->px(12));
    lv_obj_add_event_cb(E->fermer, sur_dessin_fermer, LV_EVENT_DRAW_MAIN, E);
    lv_obj_add_event_cb(E->fermer, sur_evenement_fermer, LV_EVENT_ALL, E);

    courant = E;
    return true;
}

void ui_details_mettre_a_jour(const gr::Page &page) {
    Etat *E = courant;
    if (E == nullptr) {
        return;
    }
    const std::uint32_t empreinte = gr::empreinte(page);
    if (empreinte == E->empreinte) {
        return;
    }
    E->empreinte = empreinte;
    const bool ciel_change = page.ciel.moment != E->page.ciel.moment || page.ciel.ambiance != E->page.ciel.ambiance;
    E->page = page;
    if (ciel_change) {
        appliquer_ciel(*E);
    }
    lv_obj_invalidate(E->barre);
    lv_obj_invalidate(E->grille);
    lv_obj_invalidate(E->fermer);
}

} // namespace ecran
