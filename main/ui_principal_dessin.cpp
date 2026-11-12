// Écran principal : l'aiguillage du dessin (une zone, la pastille de l'alerte officielle) et l'appui sur la pastille.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- dispatcher de dessin -----------------------------------------------------------------------------------------------------

void sur_dessin(lv_event_t *evenement) {
    lv_layer_t *couche = lv_event_get_layer(evenement);
    lv_obj_t *objet = lv_event_get_target_obj(evenement);
    const int zone = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(evenement)));
    lv_area_t a;
    lv_obj_get_coords(objet, &a);
    const Ctx c = {{couche, g.theme}, a.x1, a.y1, lv_area_get_width(&a), lv_area_get_height(&a)};
    switch (zone) {
    case Z_BARRE:
        if (g.est_accueil) {
            dessiner_barre_accueil(c);
        } else {
            dessiner_barre(c);
        }
        break;
    case Z_HORLOGE:
        if (g.est_accueil) {
            dessiner_horloge_accueil(c);
        } else {
            dessiner_horloge(c);
        }
        break;
    case Z_MAINTENANT:
        if (g.est_accueil) {
            dessiner_maintenant_accueil(c);
        } else {
            dessiner_maintenant(c);
        }
        break;
    case Z_HEURES:
        dessiner_heures_accueil(c);
        break;
    case Z_BANDEAU:
        dessiner_bandeau(c);
        break;
    case Z_SEMAINE:
        if (g.est_accueil) {
            if (g.composition == accueil::Composition::CartePluie) {
                dessiner_carte_pluie(c);
            } else if (g.bas_en_bande) {
                dessiner_bande_jours(c);
            } else {
                dessiner_jours(c);
            }
        } else {
            dessiner_semaine(c);
        }
        break;
    default:
        break;
    }
}

// La pastille de l'alerte officielle : un seul objet qui se dessine (couleur du niveau, icône, texte de l'émetteur, « +N ») et reçoit
// l'appui. Elle voit son propre état « appuyé » : un voile clair le montre, sans redessiner toute la barre.
void sur_dessin_pastille(lv_event_t *evenement) {
    const PlaceOfficielle &p = g.officielle;
    if (!p.ok) {
        return;
    }
    lv_layer_t *couche = lv_event_get_layer(evenement);
    lv_obj_t *objet = lv_event_get_target_obj(evenement);
    lv_area_t a;
    lv_obj_get_coords(objet, &a);
    const dz::Pinceau pc = {couche, g.theme};
    const ThemePrincipal &t = *g.theme;
    const int centre = a.y1 + p.h / 2;
    if (g.est_accueil && p.grande) {
        // Le bandeau AGRANDI (alerte orange ou rouge, ADR-0015 règle 1) : une grande carte de verre teinté du niveau, jamais grisée. En
        // haut l'icône, le niveau en mots et le « +N » ; dessous le titre de l'émetteur (deux lignes au plus) puis la fenêtre horaire.
        const std::uint32_t rgb = couleur_officielle(t, static_cast<int>(g.e.bandeau.niveau));
        const int rayon = px(28);
        dz::rect_bord(pc, a.x1, a.y1, p.w, p.h, rayon, rgb, 77, rgb, 166, std::max(1, px(2)));
        if (lv_obj_has_state(objet, LV_STATE_PRESSED)) {
            dz::rect(pc, a.x1, a.y1, p.w, p.h, rayon, t.texte, 40);
        }
        const int pad = px(28);
        const int icone = px(56);
        const int x = a.x1 + pad;
        const int largeur = p.w - 2 * pad;
        // Le titre de l'émetteur : en grand quand il tient sur une ligne, sinon en plus petit sur deux lignes au plus. Le bloc entier
        // (icône, titre, fenêtre) est centré dans la carte.
        const dz::Police &f_titre = dz::largeur_texte(g.titre, p.texte) > largeur ? g.corps : g.titre;
        const int lignes = dz::largeur_texte(f_titre, p.texte) > largeur ? 2 : 1;
        const int h_fenetre = p.fenetre[0] != '\0' ? dz::hauteur_ligne(g.corps) + px(6) : 0;
        const int h_bloc = icone + px(12) + lignes * dz::hauteur_ligne(f_titre) + px(6) + h_fenetre;
        int y = a.y1 + std::max(px(20), (p.h - h_bloc) / 2);
        icones::piece(couche, Piece::alerte, icone, x, y, rgb);
        const bool rouge = g.e.bandeau.niveau == vigie::Niveau::Rouge;
        const int x_libelle = x + icone + px(16);
        const int w_libelle = p.w - 2 * pad - icone - px(16) - (p.d_autres > 0 ? p.d_autres + px(16) : 0);
        texte_ajuste(pc, {&g.titre, &g.corps, &g.petit}, texte_principal::t(rouge ? "Alerte rouge" : "Alerte orange"),
                     x_libelle, y + icone / 2, w_libelle, LV_TEXT_ALIGN_LEFT, t.texte);
        if (p.d_autres > 0) {
            const int d = p.d_autres;
            dz::disque(pc, a.x1 + p.x_autres + d / 2, y + icone / 2, d / 2, rgb);
            dz::texte(pc, g.petit, p.autres, a.x1 + p.x_autres, y + icone / 2 - dz::hauteur_ligne(g.petit) / 2, d,
                      LV_TEXT_ALIGN_CENTER, t.texte_sur_couleur);
        }
        y += icone + px(12);
        dz::texte(pc, f_titre, p.texte, x, y, largeur, LV_TEXT_ALIGN_LEFT, t.texte, LV_OPA_COVER, lignes);
        y += lignes * dz::hauteur_ligne(f_titre) + px(6);
        if (p.fenetre[0] != '\0') {
            dz::texte(pc, g.corps, p.fenetre, x, y, largeur, LV_TEXT_ALIGN_LEFT, t.texte_2);
        }
        return;
    }
    if (g.est_accueil) {
        // Dans le bandeau : la même puce que les autres (verre teinté du niveau), jamais grisée : un avis en vigueur reste un avis.
        const std::uint32_t rgb = couleur_officielle(t, static_cast<int>(g.e.bandeau.niveau));
        fond_de_puce(pc, a.x1, a.y1, p.w, p.h, rgb, 51, 166);
        if (lv_obj_has_state(objet, LV_STATE_PRESSED)) {
            dz::rect(pc, a.x1, a.y1, p.w, p.h, p.h / 2, t.texte, 40);
        }
        icone_de_puce(pc, Piece::alerte, a.x1, a.y1, p.h, rgb);
        if (p.w_texte > 0 && p.police != nullptr) {
            dz::texte(pc, *p.police, p.texte, a.x1 + p.x_texte, centre - dz::hauteur_ligne(*p.police) / 2,
                      p.w_texte + px(4), LV_TEXT_ALIGN_LEFT, t.texte);
        }
        if (p.d_autres > 0) {
            dz::disque(pc, a.x1 + p.x_autres + p.d_autres / 2, centre, p.d_autres / 2, rgb);
            dz::texte(pc, g.mini, p.autres, a.x1 + p.x_autres, centre - dz::hauteur_ligne(g.mini) / 2, p.d_autres,
                      LV_TEXT_ALIGN_CENTER, t.texte_sur_couleur);
        }
        return;
    }
    const int icone = px(30);
    const std::uint32_t fond = couleur_officielle(t, static_cast<int>(g.e.bandeau.niveau));
    dz::rect(pc, a.x1, a.y1, p.w, p.h, p.h / 2, fond);
    if (lv_obj_has_state(objet, LV_STATE_PRESSED)) {
        dz::rect(pc, a.x1, a.y1, p.w, p.h, p.h / 2, t.texte_sur_couleur, 70);
    }
    icones::piece(couche, Piece::alerte, icone, a.x1 + px(12), centre - icone / 2, t.texte_sur_couleur);
    if (p.w_texte > 0 && p.police != nullptr) {
        dz::texte(pc, *p.police, p.texte, a.x1 + p.x_texte, centre - dz::hauteur_ligne(*p.police) / 2,
                  p.w_texte + px(4), LV_TEXT_ALIGN_LEFT, t.texte_sur_couleur);
    }
    if (p.d_autres > 0) {
        dz::disque(pc, a.x1 + p.x_autres + p.d_autres / 2, centre, p.d_autres / 2, t.texte_sur_couleur);
        dz::texte(pc, g.mini, p.autres, a.x1 + p.x_autres, centre - dz::hauteur_ligne(g.mini) / 2, p.d_autres,
                  LV_TEXT_ALIGN_CENTER, fond);
    }
}

void sur_pastille(lv_event_t *evenement) {
    if (lv_event_get_code(evenement) == LV_EVENT_CLICKED && g.ouvrir_alertes != nullptr) {
        g.ouvrir_alertes();
    }
}

} // namespace principal
} // namespace ecran
