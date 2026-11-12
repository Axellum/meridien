// Écran principal, accueil : la courbe des 12 prochaines heures.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- zone : la courbe des heures ----

void dessiner_heures_accueil(const Ctx &c) {
    const ThemePrincipal &t = theme_meteo();
    const VueMeteo &m = g.e.meteo;
    carte_de_verre(c, px(28));
    const int debut = premiere_heure();
    const int n = std::min(COLONNES_HEURES, m.nb_heures - debut);
    if (!m.connue || n <= 0) { // pas de prévision heure par heure : la carte le dit par un tiret
        texte_ajuste(c.pc, {&g.titre}, TIRET, c.x, c.y + c.h / 2, c.w, LV_TEXT_ALIGN_CENTER, t.texte_3);
        return;
    }
    const HeurePrevue *h = m.heures + debut;
    const int pad_x = px(20);
    const int largeur = c.w - 2 * pad_x;
    const int pas = largeur / n;
    const auto x_col = [&](int i) { return c.x + pad_x + largeur * (2 * i + 1) / (2 * n); };

    int temps[COLONNES_HEURES];
    int pluie_max_dix = 0;
    for (int i = 0; i < n; ++i) {
        temps[i] = h[i].temp_dix;
        if (connu(h[i].pluie_dix_mm) && h[i].pluie_dix_mm > pluie_max_dix) {
            pluie_max_dix = h[i].pluie_dix_mm;
        }
    }
    const bool a_pluie = pluie_max_dix > 0;
    const int y_icones = c.y + px(33); // centre des icônes
    const int y_haut = c.y + px(96);   // ordonnée du maximum
    const int y_pied = c.y + px(256);  // pied des barres de pluie et du voile
    // Ordonnée du minimum : sous le point, son chiffre (36 px) doit rester au-dessus des étiquettes d'heures (286), pluie ou non.
    const int y_bas = c.y + (a_pluie ? px(202) : px(208));
    const int y_etiquette = c.y + px(286); // ligne de base des heures

    // Les colonnes, une couleur chacune pour suivre une heure du regard (de son icône à son point et à son étiquette) : la première,
    // l'heure en cours, en accent ; les suivantes en trois couleurs du thème qui reviennent dans l'ordre, toutes aussi marquées.
    for (int i = 0; i < n; ++i) {
        dz::rect(c.pc, x_col(i) - pas / 2 + px(1), c.y + px(14), pas - px(2), px(276), px(14),
                 accueil::couleur_de_colonne(t, i), 31);
    }

    // Les points de la courbe : les heures dont la température est connue.
    accueil::Pt pts[COLONNES_HEURES];
    std::uint32_t teintes[COLONNES_HEURES];
    int k = 0;
    int bas_dix = 0, haut_dix = 0;
    for (int i = 0; i < n; ++i) {
        if (!connu(temps[i])) {
            continue;
        }
        if (k == 0 || temps[i] < bas_dix) {
            bas_dix = temps[i];
        }
        if (k == 0 || temps[i] > haut_dix) {
            haut_dix = temps[i];
        }
        pts[k] = {x_col(i), 0};
        teintes[k] = couleur_temperature(temps[i], t.rampe);
        ++k;
    }
    for (int i = 0, j = 0; i < n; ++i) {
        if (connu(temps[i])) {
            pts[j++].y = accueil::ordonnee_de(temps[i], bas_dix, haut_dix, y_haut, y_bas);
        }
    }
    const accueil::CourbeMonotone courbe = accueil::courbe_monotone(pts, k);

    // Le voile sous la courbe : de la teinte moyenne, opaque à 34 % en haut, nul au pied.
    if (k >= 2) {
        int somme = 0;
        for (int i = 0; i < n; ++i) {
            if (connu(temps[i])) {
                somme += temps[i];
            }
        }
        const std::uint32_t moyenne = couleur_temperature(somme / k, t.rampe);
        int y_min = y_pied;
        for (int j = 0; j < k; ++j) {
            y_min = std::min(y_min, pts[j].y);
        }
        const DegradeGlobal d = {moyenne, moyenne, 87, 0, y_min, y_pied};
        // Le « bas » du voile est une droite au pied : une courbe à deux points à la même ordonnée.
        const accueil::Pt droite[2] = {{pts[0].x, y_pied}, {pts[k - 1].x, y_pied}};
        voile_entre(c.pc, courbe, accueil::courbe_monotone(droite, 2), pts[0].x, pts[k - 1].x, std::max(3, px(8)), d,
                    g.voile[VOILE_HEURES]);
    }

    // Les barres de pluie : arrondies, de hauteur en racine carrée de la pluie, teintées selon l'intensité.
    int hauteur_barre[COLONNES_HEURES] = {};
    if (a_pluie) {
        const int largeur_barre = std::min(px(20), pas / 2);
        for (int i = 0; i < n; ++i) {
            if (!connu(h[i].pluie_dix_mm) || h[i].pluie_dix_mm <= 0) {
                continue;
            }
            // `gelule_du_jour` donne ici la HAUTEUR (de 8 à 44 px de référence) : la même racine carrée que la largeur des gélules des jours.
            const accueil::Gelule b = accueil::gelule_du_jour(
                h[i].pluie_dix_mm, accueil::echelle_pluie(pluie_max_dix, accueil::PLANCHER_ECHELLE_HEURE_DIX), px(8),
                px(44), 0, 0, 0);
            hauteur_barre[i] = b.largeur;
            dz::rect(c.pc, x_col(i) - largeur_barre / 2, y_pied - b.largeur, largeur_barre, b.largeur,
                     largeur_barre / 2, accueil::couleur_niveau_pluie(t, b.niveau));
        }
    }

    if (k >= 2) {
        tracer_courbe(c.pc, courbe, teintes, std::max(2, px(5)), std::max(3, px(6)));
    }

    // Les icônes du temps : une à la première heure, puis une nouvelle chaque fois que le temps change d'une heure à l'autre (douze
    // icônes côte à côte chargeaient la carte ; un temps qui dure ne la répète pas).
    IdIcone icone_de[COLONNES_HEURES];
    for (int i = 0; i < n; ++i) {
        icone_de[i] = icone_pour_code(h[i].code, h[i].jour);
    }
    const int cote_icone = px(38);
    for (int i = 0; i < n; ++i) {
        if (accueil::a_une_icone_heure(icone_de, n, i)) {
            icones::meteo(c.pc.couche, icone_de[i], cote_icone, x_col(i) - cote_icone / 2, y_icones - cote_icone / 2,
                          t);
        }
    }

    // Le maximum (au-dessus de son point) et le minimum (en dessous, sauf si la barre de pluie de sa colonne y monte : au-dessus).
    const accueil::Extremes ex = accueil::extremes_de(temps, n);
    const auto etiquette = [&](int i, bool en_haut) {
        const int x = x_col(i);
        const int y = accueil::ordonnee_de(temps[i], bas_dix, haut_dix, y_haut, y_bas);
        point_de_courbe(c.pc, t, x, y, px(10), px(6));
        char s[24];
        fmt_temp(s, sizeof s, temps[i]);
        if (!en_haut && hauteur_barre[i] > 0 && y_pied - hauteur_barre[i] < y + px(36) + px(4)) {
            en_haut = true; // sinon le chiffre s'écrirait sur la barre
        }
        const int base = en_haut ? y - px(18) : y + px(36);
        texte_base(c.pc, g.mini, s, x - px(40), base, px(80), LV_TEXT_ALIGN_CENTER,
                   couleur_temperature(temps[i], t.rampe));
    };
    if (ex.i_max >= 0) {
        etiquette(ex.i_max, true);
    }
    if (ex.i_min >= 0) {
        etiquette(ex.i_min, false);
    }

    // Une heure sur quatre porte son étiquette, la première en accent.
    const bool francais = en_francais();
    for (int i = 0; i < n; ++i) {
        if (!accueil::est_repere_heure(i, n)) {
            continue;
        }
        char libelle[16];
        ecran::heure::formater_etiquette_heure(heure_du(h[i].instant_s), francais, libelle, sizeof libelle);
        texte_base(c.pc, g.mini, libelle, x_col(i) - px(40), y_etiquette, px(80), LV_TEXT_ALIGN_CENTER,
                   i == 0 ? t.accent : t.texte_3);
    }
}

} // namespace principal
} // namespace ecran
