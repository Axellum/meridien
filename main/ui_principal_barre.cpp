// Écran principal : la barre du haut et la place de la pastille de l'alerte officielle.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- zone : barre du haut ------------------------------------------------------------------------------------------------

// Place que prennent à droite de la barre les deux indications d'état (exemple, périmée) : le dessin de la barre et le calcul de la
// place de la pastille de l'alerte officielle s'appuient sur les mêmes valeurs, pour ne jamais se chevaucher.
int largeur_etat_exemple() {
    return dz::largeur_texte(g.petit, texte_principal::t(texte_principal::kExemple)) + px(28);
}

namespace {

int largeur_etat_perimee() { return dz::largeur_texte(g.petit, texte_principal::t(texte_principal::kPerimee)); }

int bord_droit_libre(int x_barre, int w_barre) {
    int droite = x_barre + w_barre;
    if (g.e.meteo.exemple) {
        droite -= largeur_etat_exemple() + px(12);
    }
    if (g.e.meteo.perimee) {
        droite -= largeur_etat_perimee() + px(30) + px(22);
    }
    return droite;
}

// Où finit la ville dans la barre (épingle, texte ajusté au tiers de la barre au plus, marge) : la pastille de l'alerte commence après,
// jamais dessus. Sans ville, la barre est libre dès son bord. Mêmes calculs que dessiner_barre.
int bord_gauche_libre(int x_barre, int w_barre) {
    const VueMeteo &m = g.e.meteo;
    if (m.ville[0] == '\0') {
        return x_barre;
    }
    const Ajuste a = ajuster({&g.corps, &g.petit}, m.ville, w_barre / 3);
    return x_barre + px(30) + px(8) + std::min(w_barre / 3, dz::largeur_texte(*a.police, a.texte)) + px(24);
}

// Calcule la pastille de l'alerte officielle (ADR-0009) : au centre de la barre, jamais sur la ville ni sur les
// indications d'état ; son texte est celui de l'émetteur, coupé par « … » s'il est trop long (la page des alertes le donne en entier) ;
// « +N » quand d'autres alertes sont en vigueur. Si la place manque pour du texte, il reste l'icône et le « +N » : une alerte en
// vigueur ne disparaît jamais de l'écran faute de place.
PlaceOfficielle placer_pastille_officielle() {
    PlaceOfficielle p;
    const vigie::Bandeau &b = g.e.bandeau;
    const Rect &r = g.rect[Z_BARRE];
    if (!b.present || r.w <= 0 || r.h <= 0) {
        return p;
    }
    const int icone = px(30);
    const int pad_g = px(12);
    const int ecart = px(10);
    const int droite = bord_droit_libre(r.x, r.w);
    const int gauche = bord_gauche_libre(r.x, r.w);
    const int dispo = droite - gauche;
    int d_autres = 0;
    if (b.autres > 0) {
        std::snprintf(p.autres, sizeof p.autres, "+%u", static_cast<unsigned>(b.autres));
        d_autres = std::max(px(30), dz::largeur_texte(g.mini, p.autres) + px(14));
    }
    const int pad_d = d_autres > 0 ? px(5) : px(20);
    const int fixe = pad_g + icone + pad_d + (d_autres > 0 ? ecart + d_autres : 0);
    const int texte_max = dispo - fixe - ecart;
    if (texte_max >= px(60) && b.texte[0] != '\0') {
        const Ajuste a = ajuster({&g.corps, &g.petit}, b.texte, texte_max);
        std::snprintf(p.texte, sizeof p.texte, "%s", a.texte);
        p.police = a.police;
        p.w_texte = std::min(texte_max, dz::largeur_texte(*a.police, a.texte));
    }
    int x = pad_g + icone;
    if (p.w_texte > 0) {
        x += ecart;
        p.x_texte = x;
        x += p.w_texte;
    }
    if (d_autres > 0) {
        x += ecart;
        p.x_autres = x;
        p.d_autres = d_autres;
        x += d_autres;
    }
    p.w = x + pad_d;
    p.h = px(40);
    p.y = r.y + r.h / 2 - p.h / 2;
    p.x = std::max(r.x + (r.w - p.w) / 2, gauche);
    if (p.x + p.w > droite) {
        p.x = droite - p.w;
    }
    p.ok = p.x >= r.x;
    return p;
}
} // namespace

// Met l'objet de la pastille à sa place (ou le cache). À appeler quand l'alerte, la langue, les indications d'état ou la disposition
// changent : la zone de la barre est redessinée au même moment (empreinte).
void positionner_pastille_officielle() {
    g.officielle = g.est_accueil ? placer_puce_officielle() : placer_pastille_officielle();
    if (g.pastille == nullptr) {
        return;
    }
    const PlaceOfficielle &p = g.officielle;
    if (!p.ok) {
        if (!lv_obj_has_flag(g.pastille, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(g.pastille, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    lv_obj_set_pos(g.pastille, p.x, p.y);
    lv_obj_set_size(g.pastille, p.w, p.h);
    lv_obj_remove_flag(g.pastille, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(g.pastille);
}

void dessiner_barre(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const int centre = c.y + c.h / 2;
    const int pastille = px(30);
    int x = c.x;
    // La ville, à gauche, avec une épingle.
    if (m.ville[0] != '\0') {
        icones::piece(c.pc.couche, Piece::epingle, pastille, x, centre - pastille / 2, t.texte_2);
        x += pastille + px(8);
        const Ajuste a = ajuster({&g.corps, &g.petit}, m.ville, c.w / 3);
        dz::texte(c.pc, *a.police, a.texte, x, centre - dz::hauteur_ligne(*a.police) / 2, c.w / 3, LV_TEXT_ALIGN_LEFT,
                  t.texte_2);
    }
    // À droite : l'état des données (périmées, exemple).
    int droite = c.x + c.w;
    if (m.exemple) {
        const char *s = texte_principal::t(texte_principal::kExemple);
        const int w = largeur_etat_exemple();
        dz::rect(c.pc, droite - w, centre - px(18), w, px(36), px(18), t.avertissement);
        dz::texte(c.pc, g.petit, s, droite - w, centre - dz::hauteur_ligne(g.petit) / 2, w, LV_TEXT_ALIGN_CENTER,
                  t.texte_sur_couleur);
        droite -= w + px(12);
    }
    if (m.perimee) {
        const char *s = texte_principal::t(texte_principal::kPerimee);
        const int w = largeur_etat_perimee();
        dz::texte(c.pc, g.petit, s, droite - w, centre - dz::hauteur_ligne(g.petit) / 2, w, LV_TEXT_ALIGN_RIGHT,
                  t.avertissement);
        icones::piece(c.pc.couche, Piece::hors_ligne, pastille, droite - w - pastille - px(6), centre - pastille / 2,
                      t.avertissement);
        droite -= w + pastille + px(22);
    }
    // Au centre : l'alerte officielle de Vigie quand il y en a une en vigueur (objet g.pastille, dessiné par-dessus cette zone) ;
    // sinon l'alerte déduite de la météo du jour, une pastille de la couleur de l'alerte.
    const Alerte al = g.e.bandeau.present ? Alerte::AUCUNE : alerte_du_jour();
    if (al != Alerte::AUCUNE) {
        const char *s = texte_alerte(al);
        const int w_texte = dz::largeur_texte(g.corps, s);
        const int w = pastille + px(10) + w_texte + px(34);
        const int h = px(40);
        int x_pastille = c.x + (c.w - w) / 2;
        x_pastille = std::max(x_pastille, c.x + c.w / 3 + px(20));
        if (x_pastille + w <= droite) {
            dz::rect(c.pc, x_pastille, centre - h / 2, w, h, h / 2, couleur_alerte(t, al));
            icones::piece(c.pc.couche, piece_alerte(al), pastille, x_pastille + px(12), centre - pastille / 2,
                          t.texte_sur_couleur);
            dz::texte(c.pc, g.corps, s, x_pastille + px(12) + pastille + px(10),
                      centre - dz::hauteur_ligne(g.corps) / 2, w_texte + px(4), LV_TEXT_ALIGN_LEFT,
                      t.texte_sur_couleur);
        }
    }
}

} // namespace principal
} // namespace ecran
