// Écran principal, accueil : le lieu, l'heure et la date, la météo du moment.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- zone : lieu ----

void dessiner_barre_accueil(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const int centre = c.y + c.h / 2;
    // « Exemple » se pose après le lieu et la pastille du mode ; la zone entière est au lieu (les réglages s'ouvrent par la roue d'appui
    // long).
    int bord = c.x + c.w - px(16);
    const int w_exemple = m.exemple ? largeur_etat_exemple() : 0;
    if (m.exemple) {
        bord -= w_exemple + px(16);
    }
    // Le lieu garde au plus 55 % de la barre : la pastille du mode a toujours de quoi se dire à côté d'un nom long.
    const int bord_lieu = std::min(bord, c.x + c.w * 55 / 100);
    int x_fin = c.x + px(6);
    if (m.ville[0] != '\0') {
        const int icone = px(28);
        icones::piece(c.pc.couche, Piece::epingle, icone, c.x + px(6), centre - icone / 2, t.texte_2);
        const int x_texte = c.x + px(44);
        const Ajuste a = ajuster({&g.petit, &g.mini}, m.ville, std::max(px(40), bord_lieu - x_texte));
        const int w = std::min(bord_lieu - x_texte, dz::largeur_texte(*a.police, a.texte));
        dz::texte(c.pc, *a.police, a.texte, x_texte, centre - dz::hauteur_ligne(*a.police) / 2,
                  std::max(px(40), w) + px(4), LV_TEXT_ALIGN_LEFT, t.texte_2);
        x_fin = x_texte + w;
    }
    const int x_pastille = x_fin + px(16);
    x_fin =
        dessiner_pastille_mode(c, x_pastille, c.x + c.w - px(6) - (m.exemple ? w_exemple + px(16) : 0) - x_pastille);
    if (m.exemple) {
        const int x = (x_fin > 0 ? x_fin : x_pastille - px(16)) + px(16);
        dz::rect(c.pc, x, centre - px(18), w_exemple, px(36), px(18), t.avertissement);
        dz::texte(c.pc, g.petit, texte_principal::t(texte_principal::kExemple), x,
                  centre - dz::hauteur_ligne(g.petit) / 2, w_exemple, LV_TEXT_ALIGN_CENTER, t.texte_sur_couleur);
    }
}

// ---- zone : l'heure et la date ----

void dessiner_horloge_accueil(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const char *heure = g.e.heure_connue ? g.e.heure : texte_principal::kHeureAbsente;
    const lv_text_align_t align = g.portrait ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT;
    const int retrait = g.portrait ? 0 : px(4);
    dz::texte_par_les_chiffres(c.pc, g.heure, heure, c.x + retrait, c.y + px(11), c.w - retrait, align, t.texte);
    const char *date = g.e.heure_connue && g.e.date != nullptr && g.e.date[0] != '\0'
                           ? g.e.date
                           : (g.e.date_attente != nullptr && g.e.date_attente[0] != '\0'
                                  ? g.e.date_attente
                                  : texte_principal::t(texte_principal::kDateAttente));
    const int retrait_date = g.portrait ? 0 : px(10);
    const Ajuste a = ajuster({&g.titre, &g.corps, &g.petit}, date, c.w - retrait_date);
    // La ligne de base de la date est à 178 px de référence sous le haut de la zone.
    dz::texte_par_les_chiffres(c.pc, *a.police, a.texte, c.x + retrait_date,
                               c.y + px(178) - dz::hauteur_chiffres(*a.police), c.w - retrait_date, align, t.texte_2);
}

// ---- zone : la météo du moment ----

void dessiner_maintenant_accueil(const Ctx &c) {
    const ThemePrincipal &t = theme_meteo();
    const VueMeteo &m = g.e.meteo;
    const int icone = px(56);
    const int x_icone = c.x + px(12);
    const int y_icone = c.y + px(2);
    if (!m.connue) {
        icones::meteo(c.pc.couche, IdIcone::inconnue, icone, x_icone, y_icone, t, 110);
        const int x = x_icone + icone + px(24);
        texte_ajuste(c.pc, {&g.corps, &g.petit, &g.mini}, texte_principal::t(texte_principal::kMeteoIndisponible), x,
                     c.y + c.h / 2, c.x + c.w - x, LV_TEXT_ALIGN_LEFT, t.texte_2);
        return;
    }
    icones::meteo(c.pc.couche, icone_pour_code(m.code, m.jour), icone, x_icone, y_icone, t);
    char temp[24];
    fmt_temp(temp, sizeof temp, m.temp_dix);
    const int x_temp = c.x + px(80);
    dz::texte_par_les_chiffres(c.pc, g.temperature, temp, x_temp, c.y + px(8), c.x + c.w - x_temp, LV_TEXT_ALIGN_LEFT,
                               connu(m.temp_dix) ? couleur_temperature(m.temp_dix, t.rampe) : t.texte);
    // « Éclaircies · ressenti 9° » : le ressenti saute quand la place manque, le libellé est alors coupé s'il le faut.
    const int x_texte = x_temp + dz::largeur_texte(g.temperature, temp) + px(20);
    const int w = c.x + c.w - x_texte;
    if (w < px(40)) {
        return;
    }
    char libelle[64];
    libelle_meteo(libelle, sizeof libelle, m.code, m.jour);
    char phrase[128];
    std::snprintf(phrase, sizeof phrase, "%s", libelle);
    if (connu(m.ressenti_dix)) {
        char ressenti[24], mention[48], complet[128];
        fmt_temp(ressenti, sizeof ressenti, m.ressenti_dix);
        std::snprintf(mention, sizeof mention, texte_principal::t("ressenti %s"), ressenti);
        std::snprintf(complet, sizeof complet, "%s%s%s", libelle, POINT_MEDIAN, mention);
        if (dz::largeur_texte(g.mini, complet) <= w) {
            std::snprintf(phrase, sizeof phrase, "%s", complet);
        }
    }
    const Ajuste a = ajuster({&g.mini}, phrase, w);
    // La ligne de base du texte est à 43 px de référence sous le haut de la zone.
    dz::texte_par_les_chiffres(c.pc, *a.police, a.texte, x_texte, c.y + px(43) - dz::hauteur_chiffres(*a.police), w,
                               LV_TEXT_ALIGN_LEFT, t.texte_2);
}

} // namespace principal
} // namespace ecran
