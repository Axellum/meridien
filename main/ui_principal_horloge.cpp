// Écran principal, composition « horloge en vedette » : l'heure géante, la météo du moment, les sept jours.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- zone : horloge et date ----------------------------------------------------------------------------------------------

void dessiner_horloge(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const char *heure = g.e.heure_connue ? g.e.heure : texte_principal::kHeureAbsente;
    const lv_text_align_t align =
        (g.portrait || t.disposition == Disposition::HORLOGE) ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT;
    // Le bloc « chiffres + date » est centré dans la zone (la hauteur de la zone varie selon que les prévisions sont là ou non).
    const int h_bloc = dz::hauteur_chiffres(g.heure) + px(18) + dz::hauteur_chiffres(g.titre);
    const int haut_chiffres = c.y + std::max(px(2), (c.h - h_bloc) / 2);
    dz::texte_par_les_chiffres(c.pc, g.heure, heure, c.x, haut_chiffres, c.w, align, t.texte);
    const int bas_chiffres = haut_chiffres + dz::hauteur_chiffres(g.heure);
    const char *date = g.e.heure_connue && g.e.date != nullptr && g.e.date[0] != '\0'
                           ? g.e.date
                           : (g.e.date_attente != nullptr && g.e.date_attente[0] != '\0'
                                  ? g.e.date_attente
                                  : texte_principal::t(texte_principal::kDateAttente));
    const int y_date = bas_chiffres + px(18);
    const Ajuste a = ajuster({&g.titre, &g.corps, &g.petit}, date, c.w);
    dz::texte_par_les_chiffres(c.pc, *a.police, a.texte, c.x, y_date, c.w, align, t.texte_2);
}

// ---- zone : la météo du moment ------------------------------------------------------------------------------------------

namespace {

void triangle_haut_bas(const dz::Pinceau &pc, int cx, int cy, int r, bool haut, std::uint32_t rgb) {
    if (haut) {
        dz::triangle(pc, cx, cy - r, cx - r, cy + r * 3 / 4, cx + r, cy + r * 3 / 4, rgb);
    } else {
        dz::triangle(pc, cx, cy + r, cx - r, cy - r * 3 / 4, cx + r, cy - r * 3 / 4, rgb);
    }
}

// « ▲ 16° » ou « ▼ 6° » centré verticalement sur `y_centre`, dans la couleur de la rampe de températures.
void ligne_extreme(const Ctx &c, int x, int y_centre, bool haut, int dix) {
    const ThemePrincipal &t = *g.theme;
    char s[24];
    fmt_temp(s, sizeof s, dix);
    const int r = px(9);
    triangle_haut_bas(c.pc, x + r, y_centre, r, haut, couleur_temperature(dix, t.rampe));
    dz::texte(c.pc, g.corps, s, x + 2 * r + px(8), y_centre - dz::hauteur_ligne(g.corps) / 2,
              dz::largeur_texte(g.corps, s) + px(4), LV_TEXT_ALIGN_LEFT, t.texte);
}
} // namespace

void dessiner_maintenant(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    fond_de_carte(c, c.x, c.y, c.w, c.h, px(28));
    const int pad = px(12);
    if (!m.connue) {
        const int cote = std::min(c.h - 2 * pad - dz::hauteur_ligne(g.corps), px(120));
        icones::meteo(c.pc.couche, IdIcone::inconnue, cote, c.x + (c.w - cote) / 2, c.y + pad, t, 110);
        texte_ajuste(c.pc, {&g.corps, &g.petit}, texte_principal::t(texte_principal::kMeteoIndisponible), c.x + pad,
                     c.y + c.h - pad - dz::hauteur_ligne(g.corps) / 2, c.w - 2 * pad, LV_TEXT_ALIGN_CENTER, t.texte_2);
        return;
    }
    const int cote = std::min(c.h - 2 * pad, c.w * 40 / 100);
    const int icone_x = c.x + pad;
    icones::meteo(c.pc.couche, icone_pour_code(m.code, m.jour), cote, icone_x, c.y + (c.h - cote) / 2, t);
    const int x0 = icone_x + cote + px(14);
    const int w = c.x + c.w - pad - x0;

    const int h_chiffres = dz::hauteur_chiffres(g.temperature);
    const int h_libelle = dz::hauteur_ligne(g.titre);
    const int h_detail = dz::hauteur_ligne(g.petit);
    const int bloc = h_chiffres + px(8) + h_libelle + px(2) + h_detail;
    int y = c.y + std::max(px(6), (c.h - bloc) / 2);

    // La température, très grande, dans la couleur de la rampe ; le maximum et le minimum du jour à sa droite.
    char temp[24];
    fmt_temp(temp, sizeof temp, m.temp_dix);
    const std::uint32_t couleur_temp = connu(m.temp_dix) ? couleur_temperature(m.temp_dix, t.rampe) : t.texte;
    dz::texte_par_les_chiffres(c.pc, g.temperature, temp, x0, y, w, LV_TEXT_ALIGN_LEFT, couleur_temp);
    if (connu(m.min_dix) && connu(m.max_dix)) {
        const int x_extremes = x0 + dz::largeur_texte(g.temperature, temp) + px(18);
        if (x_extremes + px(104) <=
            x0 + w) { // sinon (nombre très large) on ne les montre pas plutôt que de les écraser
            ligne_extreme(c, x_extremes, y + h_chiffres * 27 / 100, true, m.max_dix);
            ligne_extreme(c, x_extremes, y + h_chiffres * 73 / 100, false, m.min_dix);
        }
    }
    y += h_chiffres + px(8);

    // Le libellé du temps, puis le ressenti.
    char libelle[64];
    libelle_meteo(libelle, sizeof libelle, m.code, m.jour);
    texte_ajuste(c.pc, {&g.titre, &g.corps, &g.petit}, libelle, x0, y + h_libelle / 2, w, LV_TEXT_ALIGN_LEFT, t.texte);
    y += h_libelle + px(2);
    if (connu(m.ressenti_dix)) {
        char ressenti[24], ligne[64];
        fmt_temp(ressenti, sizeof ressenti, m.ressenti_dix);
        std::snprintf(ligne, sizeof ligne, "%s %s", texte_principal::t("Ressenti"), ressenti);
        texte_ajuste(c.pc, {&g.petit, &g.mini}, ligne, x0, y + h_detail / 2, w, LV_TEXT_ALIGN_LEFT, t.texte_2);
    }
}
// ---- zone : sept jours -------------------------------------------------------------------------------------------------------

namespace {

void nom_du_jour(char *sortie, std::size_t n, const JourPrevu &j, bool aujourdhui) {
    if (aujourdhui) {
        std::snprintf(sortie, n, "%s", texte_principal::t("Aujourd'hui"));
        return;
    }
    // trois premiers points de code du nom du jour dans la langue de l'écran (« ven », « Fre », « Cum »)
    jour_court(i18n::langue_courante(), ecran::heure::jour_semaine_du_jour(j.date_s, fuseau()), sortie, n);
}
} // namespace

void dessiner_semaine(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    fond_de_carte(c, c.x, c.y, c.w, c.h, px(24));
    const int debut = premier_jour();
    const int n = m.nb_jours - debut;
    if (n <= 0) {
        return;
    }
    int mins[VUE_JOURS_MAX], maxs[VUE_JOURS_MAX];
    for (int i = 0; i < n; ++i) {
        mins[i] = m.jours[debut + i].min_dix;
        maxs[i] = m.jours[debut + i].max_dix;
    }
    const Echelle ech = echelle_semaine(mins, maxs, static_cast<std::size_t>(n));
    const std::int64_t numero_aujourdhui = ecran::heure::numero_jour_local(g.e.maintenant_s, fuseau());
    const int pad = px(10);
    const bool lignes = g.portrait; // portrait : une ligne par jour ; paysage : une colonne par jour
    const int h_barre = px(8);
    // Contenu d'une colonne, pour le centrer dans la carte.
    const int h_icone = px(44);
    const int h_colonne =
        dz::hauteur_ligne(g.petit) + px(2) + h_icone + px(2) + dz::hauteur_ligne(g.corps) + px(6) + h_barre;
    for (int i = 0; i < n; ++i) {
        const JourPrevu &j = m.jours[debut + i];
        const bool aujourdhui = numero_du_jour_prevu(j) == numero_aujourdhui;
        char jour[24];
        nom_du_jour(jour, sizeof jour, j, aujourdhui);
        char bas[24], haut[24], proba[16];
        fmt_temp(bas, sizeof bas, j.min_dix);
        fmt_temp(haut, sizeof haut, j.max_dix);
        std::snprintf(proba, sizeof proba, "%d %%", j.proba_pct);
        const std::uint32_t c_bas = connu(j.min_dix) ? couleur_temperature(j.min_dix, t.rampe) : t.texte_3;
        const std::uint32_t c_haut = connu(j.max_dix) ? couleur_temperature(j.max_dix, t.rampe) : t.texte_3;
        const std::uint32_t c_jour = aujourdhui ? t.accent : t.texte_2;
        if (!lignes) {
            const int col_w = (c.w - 2 * pad) / n;
            const int x = c.x + pad + i * col_w;
            const int cx = x + col_w / 2;
            if (i > 0) {
                dz::rect(c.pc, x, c.y + pad + px(6), px(1), c.h - 2 * pad - px(12), 0, t.filet, 120);
            }
            int y = c.y + std::max(pad, (c.h - h_colonne) / 2);
            texte_ajuste(c.pc, {&g.petit, &g.mini}, jour, x + px(4), y + dz::hauteur_ligne(g.petit) / 2, col_w - px(8),
                         LV_TEXT_ALIGN_CENTER, c_jour);
            y += dz::hauteur_ligne(g.petit) + px(2);
            icones::meteo(c.pc.couche, icone_pour_code(j.code, true), h_icone, cx - h_icone / 2, y, t);
            if (j.proba_pct >= 20) { // probabilité de pluie : à droite de l'icône, seulement quand elle compte
                dz::texte(c.pc, g.mini, proba, cx + h_icone / 2 - px(2), y + h_icone - dz::hauteur_ligne(g.mini),
                          col_w / 2, LV_TEXT_ALIGN_LEFT, couleur_metrique(t, Metrique::PLUIE));
            }
            y += h_icone + px(2);
            // maximum (gros) et minimum (petit) de part et d'autre de l'axe de la colonne
            const int base = y + dz::hauteur_ligne(g.corps) - g.corps.f->base_line;
            dz::texte(c.pc, g.corps, haut, x, y, col_w / 2 - px(3), LV_TEXT_ALIGN_RIGHT, c_haut);
            dz::texte(c.pc, g.petit, bas, cx + px(6), base - (dz::hauteur_ligne(g.petit) - g.petit.f->base_line),
                      col_w / 2 - px(6), LV_TEXT_ALIGN_LEFT, t.texte_2);
            y += dz::hauteur_ligne(g.corps) + px(6);
            // barre min-max sur l'échelle commune
            const int bw = col_w * 7 / 10;
            const int bx = x + (col_w - bw) / 2;
            dz::rect(c.pc, bx, y, bw, h_barre, h_barre / 2, t.filet);
            if (connu(j.min_dix) && connu(j.max_dix)) {
                const int a = position_sur_echelle(j.min_dix, ech, bw);
                const int b = position_sur_echelle(j.max_dix, ech, bw);
                dz::rect_degrade_h(c.pc, bx + a, y, std::max(h_barre, b - a), h_barre, h_barre / 2, c_bas, c_haut);
            }
        } else {
            const int ligne_h = (c.h - 2 * pad) / n;
            const int centre = c.y + pad + i * ligne_h + ligne_h / 2;
            const int w_jour = px(172);
            texte_ajuste(c.pc, {&g.petit, &g.mini}, jour, c.x + pad + px(8), centre, w_jour, LV_TEXT_ALIGN_LEFT,
                         c_jour);
            const int icone = std::min(ligne_h - px(4), px(46));
            icones::meteo(c.pc.couche, icone_pour_code(j.code, true), icone, c.x + pad + w_jour + px(4),
                          centre - icone / 2, t);
            const int w_t = px(64);
            const int x_bas = c.x + pad + w_jour + icone + px(24);
            const int x_proba = c.x + c.w - pad - px(78);
            const int x_haut = x_proba - px(12) - w_t;
            const int bx = x_bas + w_t + px(12);
            const int bw = x_haut - px(12) - bx;
            dz::texte(c.pc, g.petit, bas, x_bas, centre - dz::hauteur_ligne(g.petit) / 2, w_t, LV_TEXT_ALIGN_RIGHT,
                      t.texte_2);
            if (bw > 0) {
                dz::rect(c.pc, bx, centre - h_barre / 2, bw, h_barre, h_barre / 2, t.filet);
                if (connu(j.min_dix) && connu(j.max_dix)) {
                    const int a = position_sur_echelle(j.min_dix, ech, bw);
                    const int b = position_sur_echelle(j.max_dix, ech, bw);
                    dz::rect_degrade_h(c.pc, bx + a, centre - h_barre / 2, std::max(h_barre, b - a), h_barre,
                                       h_barre / 2, c_bas, c_haut);
                }
            }
            dz::texte(c.pc, g.petit, haut, x_haut, centre - dz::hauteur_ligne(g.petit) / 2, w_t, LV_TEXT_ALIGN_LEFT,
                      c_haut);
            if (j.proba_pct >= 0) {
                dz::texte(c.pc, g.mini, proba, x_proba, centre - dz::hauteur_ligne(g.mini) / 2, px(78),
                          LV_TEXT_ALIGN_RIGHT, couleur_metrique(t, Metrique::PLUIE));
            }
        }
    }
}

} // namespace principal
} // namespace ecran
