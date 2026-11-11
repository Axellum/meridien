#include "ui_alertes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string_view>

#include "ecran/i18n.hpp"
#include "ecran/vigie_textes.hpp"
#include "ui_ressources.h"

// Page des alertes officielles. Une carte par alerte en vigueur (la plus grave d'abord), puis une carte « Sources » qui est TOUJOURS là :
// Vigie, les sources qu'elle a lues, la mention qu'elles demandent, ce qui peut manquer à la liste, et la phrase fixe qui dit que ce
// n'est pas un service d'alerte officiel. Tout se lit, rien ne se règle ici : un seul bouton, « Fermer ».
//
// Les mots de l'ÉMETTEUR (titre, description, consigne, niveau et phénomène d'origine, zone, nom) sont posés tels quels ; seuls les mots de
// vigie_textes.hpp (étiquettes, mots de Vigie) passent par la table de traduction. Aucune couleur écrite ici : le thème de l'écran
// principal (visuel_themes.hpp), dont `officielle[]` donne la couleur de chaque niveau.

namespace ecran {

namespace {

using vigie::mot;

struct Page {
    ContexteAlertes ctx;
    float echelle = 1.0F;
    lv_font_t *titre = nullptr;
    lv_font_t *texte = nullptr;
    lv_font_t *petite = nullptr; // les trois : celles du registre partagé (ui_ressources.h)
    i18n::Langue langue = i18n::Langue::Fr;
};

Page p;

int32_t S(int32_t px) {
    return std::max<int32_t>(1, static_cast<int32_t>(std::lround(static_cast<float>(px) * p.echelle)));
}

lv_color_t couleur(std::uint32_t rgb) { return lv_color_hex(rgb); }

// Un mot de la table de Vigie dans la langue de l'écran, terminé par un zéro (les entrées de la table sont des string_view).
const char *M(std::string_view fr) {
    static char tampon[8][256];
    static int suivant = 0;
    char *t = tampon[suivant];
    suivant = (suivant + 1) % 8;
    const std::string_view s = mot(fr, p.langue);
    const std::size_t n = std::min(s.size(), sizeof tampon[0] - 1);
    std::memcpy(t, s.data(), n);
    t[n] = '\0';
    return t;
}

lv_obj_t *etiquette(lv_obj_t *parent, const char *texte, const lv_font_t *police, std::uint32_t rgb) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_font(l, police, 0);
    lv_obj_set_style_text_color(l, couleur(rgb), 0);
    lv_label_set_text(l, texte);
    return l;
}

lv_obj_t *conteneur(lv_obj_t *parent, lv_flex_flow_t sens) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, LV_PCT(100));
    lv_obj_set_height(c, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(c, sens);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

lv_obj_t *carte(lv_obj_t *parent) {
    const visuel::ThemePrincipal &t = *p.ctx.theme;
    lv_obj_t *c = conteneur(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(c, couleur(visuel::carte_sur(t, t.fond)), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, S(20), 0);
    lv_obj_set_style_pad_all(c, S(24), 0);
    lv_obj_set_style_pad_row(c, S(10), 0);
    return c;
}

// « Étiquette : valeur » sur une ligne ; la valeur passe à la ligne sous elle-même si elle est longue.
void rangee(lv_obj_t *parent, const char *libelle, const char *valeur) {
    const visuel::ThemePrincipal &t = *p.ctx.theme;
    lv_obj_t *r = conteneur(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(r, S(16), 0);
    lv_obj_t *l = lv_label_create(r);
    lv_obj_set_width(l, S(180));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l, p.petite, 0);
    lv_obj_set_style_text_color(l, couleur(t.texte_3), 0);
    lv_label_set_text(l, libelle);
    lv_obj_t *v = lv_label_create(r);
    lv_obj_set_flex_grow(v, 1);
    lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(v, p.texte, 0);
    lv_obj_set_style_text_color(v, couleur(t.texte), 0);
    lv_label_set_text(v, valeur);
}

// Une date et une heure locales, ou « Non communiquée » (l'émetteur ne l'a pas donnée, ou l'heure locale est inconnue).
void instant(char *sortie, std::size_t n, const std::optional<std::int64_t> &s) {
    sortie[0] = '\0';
    heure::HeureLocale h;
    if (s && *s > 0 && p.ctx.heure_locale != nullptr && p.ctx.heure_locale(*s, h) &&
        vigie::formater_instant(p.langue, h, sortie, n) > 0) {
        return;
    }
    std::snprintf(sortie, n, "%s", M("Non communiquée"));
}

// Le texte d'un champ de l'émetteur, ou rien. Les retours à la ligne de l'émetteur sont gardés (une description en a souvent).
bool plein(const char *s) { return s != nullptr && s[0] != '\0'; }

void carte_alerte(lv_obj_t *corps, const vigie::Alerte &a) {
    const visuel::ThemePrincipal &t = *p.ctx.theme;
    lv_obj_t *c = carte(corps);

    // Tête : la pastille du niveau (couleur du thème) et le titre de l'émetteur.
    lv_obj_t *tete = conteneur(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tete, S(16), 0);
    lv_obj_set_flex_align(tete, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    char niveau_mot[64] = {};
    const std::string_view cle = vigie::cle_niveau(a.niveau);
    std::snprintf(niveau_mot, sizeof niveau_mot, "%s", M(cle.empty() ? std::string_view("Alerte") : cle));
    lv_obj_t *pastille = lv_obj_create(tete);
    lv_obj_remove_style_all(pastille);
    lv_obj_remove_flag(pastille, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pastille, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pastille, couleur(visuel::couleur_officielle(t, static_cast<int>(a.niveau))), 0);
    lv_obj_set_style_bg_opa(pastille, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pastille, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(pastille, S(22), 0);
    lv_obj_set_style_pad_ver(pastille, S(8), 0);
    lv_obj_t *mot_niveau = lv_label_create(pastille);
    lv_obj_set_style_text_font(mot_niveau, p.texte, 0);
    lv_obj_set_style_text_color(mot_niveau, couleur(t.texte_sur_couleur), 0);
    lv_label_set_text(mot_niveau, niveau_mot);

    const vigie::Titre titre = vigie::titre_de(a, i18n::code(p.langue));
    char titre_z[vigie::TITRE_CAP] = {};
    if (titre.origine != vigie::OrigineTitre::Aucune) {
        std::snprintf(titre_z, sizeof titre_z, "%.*s", static_cast<int>(titre.texte.size()), titre.texte.data());
    } else {
        vigie::libelle_derive(a.phenomene, a.niveau, p.langue, titre_z, sizeof titre_z);
    }
    lv_obj_t *titre_l = lv_label_create(tete);
    lv_obj_set_flex_grow(titre_l, 1);
    lv_label_set_long_mode(titre_l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(titre_l, p.texte, 0);
    lv_obj_set_style_text_color(titre_l, couleur(t.texte), 0);
    lv_label_set_text(titre_l, titre_z);

    // Les faits : phénomène et niveau tels que l'émetteur les écrit, la période, la zone, l'émetteur.
    if (plein(a.phenomene_original)) {
        rangee(c, M("Phénomène"), a.phenomene_original);
    }
    if (plein(a.niveau_original)) {
        rangee(c, M("Niveau"), a.niveau_original);
    }
    char debut[96] = {};
    char fin[96] = {};
    instant(debut, sizeof debut, a.debut ? a.debut : a.effectif);
    instant(fin, sizeof fin, a.fin ? a.fin : a.expire);
    rangee(c, M("Début"), debut);
    rangee(c, M("Fin"), fin);
    if (plein(a.zone)) {
        rangee(c, M("Zone"), a.zone);
    }
    if (plein(a.emetteur)) {
        rangee(c, M("Émetteur"), a.emetteur);
    }

    // Les mots de l'émetteur : description, puis ce qu'il demande de faire.
    const vigie::Texte *x = vigie::choisir_texte(a, i18n::code(p.langue));
    if (x != nullptr && plein(x->description)) {
        etiquette(c, x->description, p.petite, t.texte_2);
    }
    if (x != nullptr && plein(x->consigne)) {
        rangee(c, M("Que faire"), x->consigne);
    }
    if (plein(a.attribution)) {
        etiquette(c, a.attribution, p.petite, t.texte_3);
    }
}

void carte_sources(lv_obj_t *corps, const vigie::Alertes &a, std::int64_t recu_s) {
    const visuel::ThemePrincipal &t = *p.ctx.theme;
    lv_obj_t *c = carte(corps);
    etiquette(c, M("Sources"), p.texte, t.texte);
    etiquette(c, M("Alertes relayées par Vigie"), p.petite, t.texte_2);
    if (plein(a.attribution)) {
        etiquette(c, a.attribution, p.petite, t.texte_2);
    }
    if (plein(a.avertissement)) {
        etiquette(c, a.avertissement, p.petite, t.texte_3);
    }
    // Ce qui peut manquer à la liste : dit en une phrase, jamais passé sous silence.
    if (a.couverture.non_rattachables) {
        etiquette(c,
                  M("Certains avertissements de ce lieu ne peuvent pas être rattachés à un point : la liste peut être "
                    "incomplète."),
                  p.petite, t.avertissement);
    }
    if (a.couverture.source_perimee || a.couverture.source_absente) {
        etiquette(c, M("Des données de cette source ne sont pas à jour."), p.petite, t.avertissement);
    }
    if (recu_s > 0) {
        char quand[96] = {};
        instant(quand, sizeof quand, recu_s);
        char ligne[200];
        std::snprintf(ligne, sizeof ligne, "%s : %s", M("Reçu à"), quand);
        etiquette(c, ligne, p.petite, t.texte_3);
    }
    // La phrase fixe : ce n'est pas un service d'alerte officiel.
    etiquette(c, M("Ceci n'est pas un service d'alerte officiel. Suivez les avis de vos autorités nationales."),
              p.petite, t.texte);
}

void sur_fermer(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && p.ctx.fermer != nullptr) {
        p.ctx.fermer();
    }
}

lv_font_t *creer_police(const ContexteAlertes &ctx, int32_t reference) {
    return ressources::police(ctx.ttf, ctx.ttf_taille, S(reference));
}

} // namespace

bool ui_alertes_creer(const ContexteAlertes &ctx, const DonneesAlertes &donnees) {
    if (ctx.racine == nullptr || ctx.ttf == nullptr || ctx.theme == nullptr || ctx.largeur <= 0 || ctx.hauteur <= 0 ||
        donnees.alertes == nullptr || donnees.vue == nullptr) {
        return false;
    }
    p.ctx = ctx;
    p.echelle = static_cast<float>(std::min(ctx.largeur, ctx.hauteur)) / 720.0F;
    p.langue = i18n::langue_courante();
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
    lv_label_set_text(titre, M("Alertes officielles"));
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
    lv_label_set_text(fermer_l, M("Fermer"));
    lv_obj_center(fermer_l);

    // Corps : les cartes, en colonne, qui défilent si elles dépassent.
    lv_obj_t *corps = lv_obj_create(racine);
    lv_obj_remove_style_all(corps);
    lv_obj_set_size(corps, ctx.largeur, ctx.hauteur - barre_h);
    lv_obj_set_pos(corps, 0, barre_h);
    lv_obj_set_flex_flow(corps, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(corps, marge, 0);
    lv_obj_set_style_pad_bottom(corps, marge, 0);
    lv_obj_set_style_pad_row(corps, S(20), 0);
    lv_obj_add_flag(corps, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(corps, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(corps, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(corps, S(8), LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(corps, S(4), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(corps, couleur(t.texte_3), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(corps, LV_OPA_COVER, LV_PART_SCROLLBAR);

    const vigie::Alertes &a = *donnees.alertes;
    const vigie::Vue &v = *donnees.vue;
    if (v.nombre == 0) {
        lv_obj_t *c = carte(corps);
        etiquette(c, M("Aucune alerte en vigueur"), p.texte, t.texte);
    }
    for (std::size_t i = 0; i < v.nombre && v.indices[i] < a.nombre; ++i) {
        carte_alerte(corps, a.alertes[v.indices[i]]);
    }
    carte_sources(corps, a, donnees.recu_s);
    return true;
}

} // namespace ecran
