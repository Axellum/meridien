#include "ui_sources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

#include "ecran/i18n.hpp"
#include "ecran/mer_textes.hpp"
#include "ecran/texte_modele.hpp"
#include "ui_ressources.h"

// Page « Sources » : une carte, une phrase par mention (credits::composer a décidé lesquelles et dans quel ordre), puis « Fermer ». Rien
// ne se règle ici. Aucune couleur écrite : les rôles du thème de l'écran principal.

namespace ecran {

namespace {

struct Page {
    ContexteSources ctx;
    float echelle = 1.0F;
    lv_font_t *titre = nullptr;
    lv_font_t *texte = nullptr;
    lv_font_t *petite = nullptr;
};

Page p;

int32_t S(int32_t px) {
    return std::max<int32_t>(1, static_cast<int32_t>(std::lround(static_cast<float>(px) * p.echelle)));
}

lv_color_t couleur(std::uint32_t rgb) { return lv_color_hex(rgb); }

lv_obj_t *etiquette(lv_obj_t *parent, const char *texte, const lv_font_t *police, std::uint32_t rgb) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_font(l, police, 0);
    lv_obj_set_style_text_color(l, couleur(rgb), 0);
    lv_label_set_text(l, texte);
    return l;
}

// La phrase d'une ligne dans la langue de l'écran, marqueurs remplis (le crédit de la mer vient de sa propre table).
void phrase(const credits::Ligne &ligne, char *sortie, std::size_t capacite) {
    sortie[0] = '\0';
    if (ligne.rubrique == credits::Rubrique::Mer) {
        const std::string_view s = mer::texte(mer::Texte::Credit, i18n::langue_courante());
        const std::size_t n = std::min(s.size(), capacite - 1);
        std::memcpy(sortie, s.data(), n);
        sortie[n] = '\0';
        return;
    }
    if (ligne.cle == nullptr) {
        return;
    }
    const texte::Parametre params[] = {{"lien", ligne.lien != nullptr ? ligne.lien : ""},
                                       {"date", ligne.date != nullptr ? ligne.date : ""}};
    texte::remplir(i18n::tr(ligne.cle), params, 2, sortie, capacite);
}

void sur_fermer(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && p.ctx.fermer != nullptr) {
        p.ctx.fermer();
    }
}

lv_font_t *creer_police(const ContexteSources &ctx, int32_t reference) {
    return ressources::police(ctx.ttf, ctx.ttf_taille, S(reference));
}

} // namespace

bool ui_sources_creer(const ContexteSources &ctx, const credits::Liste &liste) {
    if (ctx.racine == nullptr || ctx.ttf == nullptr || ctx.theme == nullptr || ctx.largeur <= 0 || ctx.hauteur <= 0) {
        return false;
    }
    p.ctx = ctx;
    p.echelle = static_cast<float>(std::min(ctx.largeur, ctx.hauteur)) / 720.0F;
    // Les polices viennent du registre partagé (ui_ressources.h) : créées une seule fois pour tout l'appareil, à la taille de l'écran.
    p.titre = creer_police(ctx, 44);
    p.texte = creer_police(ctx, 32);
    p.petite = creer_police(ctx, 26);
    if (p.titre == nullptr || p.texte == nullptr || p.petite == nullptr) {
        return false;
    }
    const visuel::ThemePrincipal &t = *ctx.theme;
    lv_obj_t *racine = ctx.racine;
    lv_obj_set_style_bg_color(racine, couleur(t.fond), 0);
    lv_obj_set_style_bg_opa(racine, LV_OPA_COVER, 0);
    lv_obj_remove_flag(racine, LV_OBJ_FLAG_SCROLLABLE);

    // Barre du haut : le titre à gauche, « Fermer » à droite (zone tactile de 72 px au moins, à l'échelle de 720 px).
    const int32_t marge = S(32);
    const int32_t barre_h = S(112);
    lv_obj_t *barre = lv_obj_create(racine);
    lv_obj_remove_style_all(barre);
    lv_obj_remove_flag(barre, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(barre, ctx.largeur, barre_h);
    lv_obj_set_pos(barre, 0, 0);
    lv_obj_t *titre = lv_label_create(barre);
    lv_label_set_long_mode(titre, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(titre, ctx.largeur - S(220) - 2 * marge);
    lv_obj_set_style_text_font(titre, p.titre, 0);
    lv_obj_set_style_text_color(titre, couleur(t.texte), 0);
    lv_label_set_text(titre, i18n::tr_c(credits::TITRE));
    lv_obj_align(titre, LV_ALIGN_LEFT_MID, marge, 0);

    lv_obj_t *fermer = lv_button_create(barre);
    lv_obj_set_size(fermer, S(180), S(80));
    lv_obj_set_style_radius(fermer, S(20), 0);
    lv_obj_set_style_shadow_width(fermer, 0, 0);
    lv_obj_set_style_border_width(fermer, 0, 0);
    lv_obj_set_style_bg_color(fermer, couleur(t.accent), 0);
    lv_obj_set_style_bg_opa(fermer, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(fermer, couleur(t.filet), LV_STATE_PRESSED);
    lv_obj_align(fermer, LV_ALIGN_RIGHT_MID, -marge, 0);
    lv_obj_add_event_cb(fermer, sur_fermer, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *fermer_l = lv_label_create(fermer);
    lv_obj_set_style_text_font(fermer_l, p.texte, 0);
    lv_obj_set_style_text_color(fermer_l, couleur(t.fond), 0);
    lv_label_set_text(fermer_l, i18n::tr_c("Fermer"));
    lv_obj_center(fermer_l);

    // Corps : une carte, qui défile si elle dépasse.
    lv_obj_t *corps = lv_obj_create(racine);
    lv_obj_remove_style_all(corps);
    lv_obj_set_size(corps, ctx.largeur, ctx.hauteur - barre_h);
    lv_obj_set_pos(corps, 0, barre_h);
    lv_obj_set_flex_flow(corps, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(corps, marge, 0);
    lv_obj_set_style_pad_bottom(corps, marge, 0);
    lv_obj_add_flag(corps, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(corps, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(corps, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(corps, S(8), LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(corps, S(4), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(corps, couleur(t.texte_3), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(corps, LV_OPA_COVER, LV_PART_SCROLLBAR);

    lv_obj_t *carte = lv_obj_create(corps);
    lv_obj_remove_style_all(carte);
    lv_obj_set_width(carte, LV_PCT(100));
    lv_obj_set_height(carte, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(carte, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(carte, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carte, couleur(visuel::carte_sur(t, t.fond)), 0);
    lv_obj_set_style_bg_opa(carte, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(carte, S(20), 0);
    lv_obj_set_style_pad_all(carte, S(24), 0);
    lv_obj_set_style_pad_row(carte, S(16), 0);
    for (std::size_t i = 0; i < liste.n; ++i) {
        char texte_ligne[320];
        phrase(liste.ligne[i], texte_ligne, sizeof texte_ligne);
        if (texte_ligne[0] != '\0') {
            // L'avertissement du NWS (« ni approuvé ni parrainé ») est une précision de la ligne météo : discret ; les autres se lisent.
            const bool precision = liste.ligne[i].rubrique == credits::Rubrique::MeteoNws;
            etiquette(carte, texte_ligne, precision ? p.petite : p.texte, precision ? t.texte_3 : t.texte_2);
        }
    }
    return true;
}

} // namespace ecran
