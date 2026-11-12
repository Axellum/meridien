// Écran principal : la mise en page (les deux compositions) et les briques communes des zones.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- mise en page -------------------------------------------------------------------------------------------------------
//
// Deux compositions. L'ACCUEIL (thèmes « ciel », « tableau sombre », « clair ») : les zones sont calculées par le noyau pur
// (accueil::disposer, testé sur le PC) en pixels de référence, puis mises à l'échelle de l'écran ; la hauteur gagnée sur 720 (la 7B)
// va à la carte des jours. L'HORLOGE en vedette garde sa composition d'avant : l'heure géante au centre, la météo du moment dessous,
// les sept jours en bas, ni courbe d'heures ni bandeau.

namespace {

struct Cotes {
    int marge, ecart, barre_h;
};

Cotes cotes() { return {px(g.portrait ? 28 : 36), px(10), px(48)}; }

// Un rectangle en pixels de référence -> pixels de l'écran, par ses BORDS (deux zones qui se touchent en référence se touchent encore).
Rect en_pixels(const accueil::Rect &r) {
    const int x0 = px(r.x);
    const int y0 = px(r.y);
    return {x0, y0, px(r.x + r.w) - x0, px(r.y + r.h) - y0};
}

void calculer_disposition() {
    if (g.est_accueil) {
        const int ref_l = std::max(1, static_cast<int>(std::lround(static_cast<float>(g.largeur) / g.echelle)));
        const int ref_h = std::max(1, static_cast<int>(std::lround(static_cast<float>(g.hauteur) / g.echelle)));
        const accueil::Zones z = accueil::disposer(ref_l, ref_h, g.portrait);
        g.extra_h = z.extra_h;
        g.rect[Z_BARRE] = en_pixels(z.barre);
        g.rect[Z_HORLOGE] = en_pixels(z.horloge);
        g.rect[Z_MAINTENANT] = en_pixels(z.maintenant);
        g.rect[Z_HEURES] = en_pixels(z.heures);
        g.rect_alerte = {};
        g.rect_tuile = {};
        g.bas_en_bande = false;
        if (g.composition == accueil::Composition::Normale) {
            g.rect[Z_BANDEAU] = en_pixels(z.bandeau);
            g.rect[Z_SEMAINE] = en_pixels(z.jours);
            return;
        }
        // Le mode automatique a ajouté quelque chose à la zone du bas (ADR-0023) : le noyau pur calcule les rectangles.
        const accueil::ZonesAuto a = accueil::disposer_auto(ref_l, ref_h, g.portrait, g.composition,
                                                            g.alerte_dans_bandeau, g.tuile_dit_quelque_chose);
        g.rect[Z_BANDEAU] = en_pixels(a.bandeau);
        g.rect[Z_SEMAINE] = en_pixels(a.bas);
        g.rect_alerte = en_pixels(a.alerte);
        g.rect_tuile = en_pixels(a.tuile);
        g.bas_en_bande = a.bas_en_bande;
        return;
    }
    // Composition calme : l'heure géante au centre, la météo du moment dessous, les sept jours en bas ; ni courbe d'heures ni bandeau
    // (leurs zones restent vides, donc cachées).
    // En paysage : 262 px pour l'heure + la date, 196 pour la carte du moment (sans quoi « Ressenti » est coupé), et les
    // 168 qui restent suffisent à la carte des sept jours.
    const Cotes c = cotes();
    const int utile = g.largeur - 2 * c.marge;
    const int y0 = px(10);
    g.rect[Z_BARRE] = {c.marge, y0, utile, c.barre_h};
    int y = y0 + c.barre_h + px(4);
    const int bas = g.hauteur - px(12);
    const int h_horloge = g.portrait ? px(330) : px(262);
    g.rect[Z_HORLOGE] = {c.marge, y, utile, h_horloge};
    y += h_horloge + c.ecart;
    const int h_maintenant = g.portrait ? px(230) : px(196);
    const int w_maintenant = g.portrait ? utile : std::min(utile, px(600));
    g.rect[Z_MAINTENANT] = {(g.largeur - w_maintenant) / 2, y, w_maintenant, h_maintenant};
    y += h_maintenant + c.ecart;
    g.rect[Z_HEURES] = {};
    g.rect[Z_BANDEAU] = {};
    g.rect[Z_SEMAINE] = {c.marge, y, utile, bas - y};
}

} // namespace

void appliquer_disposition() {
    calculer_disposition();
    for (int z = 0; z < Z_NOMBRE; ++z) {
        if (g.zone[z] == nullptr) {
            continue;
        }
        const Rect &r = g.rect[z];
        const bool vide = r.w <= 0 || r.h <= 0;
        if (vide) {
            lv_obj_add_flag(g.zone[z], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(g.zone[z], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(g.zone[z], r.x, r.y);
            lv_obj_set_size(g.zone[z], r.w, r.h);
        }
    }
    positionner_pastille_officielle(); // la barre ou le bandeau a pu changer de taille
    cuire_verre();                     // les cartes de verre ont pu changer de place
}

// ---- zones : briques communes --------------------------------------------------------------------------------------------

void fond_de_carte(const Ctx &c, int x, int y, int w, int h, int rayon) {
    const ThemePrincipal &t = *g.theme;
    if (t.carte_opa > 0) {
        dz::rect(c.pc, x, y, w, h, rayon, t.carte, t.carte_opa);
    }
}

} // namespace principal
} // namespace ecran
