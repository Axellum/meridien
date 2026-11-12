// Écran principal, accueil : les cinq jours.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- zone : les cinq jours ----

namespace {

// Une flèche de vent à deux triangles (l'esquisse : pointe, deux ailes, un creux) qui pointe VERS où le vent souffle.
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
} // namespace

void dessiner_jours(const Ctx &c) {
    const ThemePrincipal &t = theme_meteo();
    const VueMeteo &m = g.e.meteo;
    carte_de_verre(c, px(28));
    const int debut = premier_jour();
    const int n = std::min(COLONNES_JOURS, m.nb_jours - debut);
    if (n <= 0) {
        return;
    }
    const JourPrevu *jours = m.jours + debut;
    const std::int64_t numero_aujourdhui = ecran::heure::numero_jour_local(g.e.maintenant_s, fuseau());

    // Les rangées : leurs ordonnées de référence (sous le haut de la carte) croissent avec la hauteur gagnée sur 720 (la 7B : 30 px),
    // et le surplus au-delà de 90 px (portrait) est réparti en haut et en bas.
    const int gagne = std::min(g.extra_h, 90);
    const int marge_haut = g.extra_h > 90 ? (g.extra_h - 90) / 2 : 0;
    const auto rang = [&](int base, int par_30) { return c.y + px(base + par_30 * gagne / 30 + marge_haut); };
    const int y_icone = rang(36, 2);       // centre de la rangée « nom + icône »
    const int y_temps = rang(97, 6);       // ligne de base de « 14° / 8° »
    const int y_ruban_haut = rang(113, 8); // le ruban : les maximales...
    const int y_ruban_bas = rang(170, 20); // ... et les minimales
    const int y_vent = rang(200, 24);      // centre de la rangée du vent
    const int y_pluie = rang(236, 30);     // centre de la rangée de la pluie

    const int x0 = c.x + px(24);
    const int largeur = c.w - 2 * px(24);
    const int pas = largeur / n;
    const auto x_col = [&](int i) { return x0 + largeur * (2 * i + 1) / (2 * n); };

    // Aujourd'hui : la colonne teintée d'accent, comme l'heure en cours de la courbe d'heures.
    if (numero_du_jour_prevu(jours[0]) == numero_aujourdhui) {
        dz::rect(c.pc, x_col(0) - pas / 2 + px(4), c.y + px(8), pas - px(8), c.h - px(16), px(20), t.accent, 26);
    }

    // Le ruban entre les maximales (courbe du haut) et les minimales (courbe du bas), sur une échelle commune.
    accueil::Pt p_haut[COLONNES_JOURS], p_bas[COLONNES_JOURS];
    std::uint32_t t_haut[COLONNES_JOURS], t_bas[COLONNES_JOURS];
    int nb = 0;
    int plus_bas_dix = 0, plus_haut_dix = 0;
    int somme_haut = 0, somme_bas = 0;
    for (int i = 0; i < n; ++i) {
        if (!connu(jours[i].max_dix) || !connu(jours[i].min_dix)) {
            continue;
        }
        if (nb == 0 || jours[i].min_dix < plus_bas_dix) {
            plus_bas_dix = jours[i].min_dix;
        }
        if (nb == 0 || jours[i].max_dix > plus_haut_dix) {
            plus_haut_dix = jours[i].max_dix;
        }
        somme_haut += jours[i].max_dix;
        somme_bas += jours[i].min_dix;
        p_haut[nb] = {x_col(i), 0};
        p_bas[nb] = {x_col(i), 0};
        t_haut[nb] = couleur_temperature(jours[i].max_dix, t.rampe);
        t_bas[nb] = couleur_temperature(jours[i].min_dix, t.rampe);
        ++nb;
    }
    for (int i = 0, j = 0; i < n; ++i) {
        if (connu(jours[i].max_dix) && connu(jours[i].min_dix)) {
            p_haut[j].y =
                accueil::ordonnee_de(jours[i].max_dix, plus_bas_dix, plus_haut_dix, y_ruban_haut, y_ruban_bas);
            p_bas[j].y = accueil::ordonnee_de(jours[i].min_dix, plus_bas_dix, plus_haut_dix, y_ruban_haut, y_ruban_bas);
            ++j;
        }
    }
    if (nb >= 2) {
        const accueil::CourbeMonotone c_haut = accueil::courbe_monotone(p_haut, nb);
        const accueil::CourbeMonotone c_bas = accueil::courbe_monotone(p_bas, nb);
        const DegradeGlobal d = {couleur_temperature(somme_haut / nb, t.rampe),
                                 couleur_temperature(somme_bas / nb, t.rampe),
                                 87,
                                 31,
                                 y_ruban_haut,
                                 y_ruban_bas};
        voile_entre(c.pc, c_haut, c_bas, p_haut[0].x, p_haut[nb - 1].x, std::max(3, px(8)), d, g.voile[VOILE_JOURS]);
        tracer_courbe(c.pc, c_haut, t_haut, std::max(2, px(5)), std::max(3, px(8)));
        tracer_courbe(c.pc, c_bas, t_bas, std::max(2, px(5)), std::max(3, px(8)));
    }
    for (int j = 0; j < nb; ++j) {
        point_de_courbe(c.pc, t, p_haut[j].x, p_haut[j].y, px(9), px(5));
        point_de_courbe(c.pc, t, p_bas[j].x, p_bas[j].y, px(9), px(5));
    }

    int pluie_max_dix = 0;
    for (int i = 0; i < n; ++i) {
        if (connu(jours[i].pluie_dix_mm) && jours[i].pluie_dix_mm > pluie_max_dix) {
            pluie_max_dix = jours[i].pluie_dix_mm;
        }
    }

    for (int i = 0; i < n; ++i) {
        const JourPrevu &j = jours[i];
        const int cx = x_col(i);
        const bool aujourdhui = numero_du_jour_prevu(j) == numero_aujourdhui;

        // Le nom à gauche, l'icône à droite, le tout centré sur la colonne.
        char nom[24];
        jour_court(i18n::langue_courante(), ecran::heure::jour_semaine_du_jour(j.date_s, fuseau()), nom, sizeof nom);
        if (nom[0] >= 'a' && nom[0] <= 'z') {
            nom[0] = static_cast<char>(nom[0] - 'a' +
                                       'A'); // « Dim », comme « Fre » et « Cum » (la table donne « dim » en français)
        }
        const int cote = px(52);
        const int w_nom = std::min(dz::largeur_texte(g.petit, nom), std::max(px(20), pas - cote - px(22)));
        const int xg = cx - (w_nom + px(14) + cote) / 2;
        texte_base(c.pc, g.petit, nom, xg, y_icone + px(10), w_nom + px(4), LV_TEXT_ALIGN_LEFT,
                   aujourdhui ? t.accent : t.texte_2);
        icones::meteo(c.pc.couche, icone_pour_code(j.code, true), cote, xg + w_nom + px(14), y_icone - cote / 2, t);

        // « 14° / 8° » : la maximale grande, la minimale petite, chacune de la couleur de sa température.
        char haut[24], bas[24];
        fmt_temp(haut, sizeof haut, j.max_dix);
        fmt_temp(bas, sizeof bas, j.min_dix);
        const Police *p_grand = &g.corps;
        const Police *p_petit = &g.petit;
        const int sep = px(10);
        int w_total = dz::largeur_texte(*p_grand, haut) + 2 * sep + dz::largeur_texte(*p_petit, "/") +
                      dz::largeur_texte(*p_petit, bas);
        if (w_total > pas - px(8)) { // colonne étroite : une taille de moins
            p_grand = &g.petit;
            p_petit = &g.mini;
            w_total = dz::largeur_texte(*p_grand, haut) + 2 * sep + dz::largeur_texte(*p_petit, "/") +
                      dz::largeur_texte(*p_petit, bas);
        }
        int x = cx - w_total / 2;
        texte_base(c.pc, *p_grand, haut, x, y_temps, dz::largeur_texte(*p_grand, haut) + px(4), LV_TEXT_ALIGN_LEFT,
                   connu(j.max_dix) ? couleur_temperature(j.max_dix, t.rampe) : t.texte_3);
        x += dz::largeur_texte(*p_grand, haut) + sep;
        texte_base(c.pc, *p_petit, "/", x, y_temps, dz::largeur_texte(*p_petit, "/") + px(4), LV_TEXT_ALIGN_LEFT,
                   t.filet);
        x += dz::largeur_texte(*p_petit, "/") + sep;
        texte_base(c.pc, *p_petit, bas, x, y_temps, dz::largeur_texte(*p_petit, bas) + px(4), LV_TEXT_ALIGN_LEFT,
                   connu(j.min_dix) ? couleur_temperature(j.min_dix, t.rampe) : t.texte_3);

        // Le vent : la flèche (vers où il souffle) et sa force.
        if (connu(j.vent_dix_kmh)) {
            char vitesse[24];
            std::snprintf(vitesse, sizeof vitesse, "%d km/h", arrondi_dix(j.vent_dix_kmh));
            const int fleche = px(30);
            const bool avec_fleche = j.direction_deg >= 0;
            const int w_fleche = avec_fleche ? fleche + px(10) : 0;
            if (w_fleche + dz::largeur_texte(g.mini, vitesse) > pas - px(8)) {
                std::snprintf(vitesse, sizeof vitesse, "%d",
                              arrondi_dix(j.vent_dix_kmh)); // colonne étroite (portrait) : sans l'unité
            }
            const int w_vitesse = dz::largeur_texte(g.mini, vitesse);
            const int w_tout = w_fleche + w_vitesse;
            int xv = cx - w_tout / 2;
            if (avec_fleche) {
                fleche_vent(c.pc, xv + fleche / 2, y_vent, fleche, angle_fleche_vent(j.direction_deg),
                            couleur_metrique(t, Metrique::VENT));
                xv += fleche + px(10);
            }
            texte_base(c.pc, g.mini, vitesse, xv, y_vent + px(8), w_vitesse + px(4), LV_TEXT_ALIGN_LEFT, t.texte_2);
        }

        // La pluie du jour : une gélule (largeur en racine carrée de la pluie) ; le chiffre dedans seulement s'il y tient ; un petit
        // trait pour un jour sec ; rien quand la pluie est inconnue.
        if (connu(j.pluie_dix_mm) && j.pluie_dix_mm > 0) {
            char mm[24];
            fmt_dixieme(mm, sizeof mm, j.pluie_dix_mm);
            std::snprintf(mm + std::strlen(mm), sizeof mm - std::strlen(mm), " mm");
            const accueil::Gelule gl = accueil::gelule_du_jour(
                j.pluie_dix_mm, accueil::echelle_pluie(pluie_max_dix, accueil::PLANCHER_ECHELLE_JOUR_DIX), px(26),
                std::min(px(138), pas - px(8)), dz::largeur_texte(g.mini, mm), px(16), pas - px(8));
            const int h_gelule = px(30);
            dz::rect(c.pc, cx - gl.largeur / 2, y_pluie - h_gelule / 2, gl.largeur, h_gelule, h_gelule / 2,
                     accueil::couleur_niveau_pluie(t, gl.niveau));
            if (gl.avec_texte) {
                texte_base(c.pc, g.mini, mm, cx - gl.largeur / 2, y_pluie + px(9), gl.largeur, LV_TEXT_ALIGN_CENTER,
                           t.texte_sur_couleur);
            }
        } else if (connu(j.pluie_dix_mm)) {
            dz::rect(c.pc, cx - px(13), y_pluie - px(4), px(26), px(8), px(4), t.texte, 36);
        }
    }
}

} // namespace principal
} // namespace ecran
