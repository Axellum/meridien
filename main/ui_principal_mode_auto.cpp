// Écran principal, accueil : le mode automatique (pastille, carte pluie, tuile, bande des jours ; ADR-0023).
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- le mode automatique : la pastille, la carte pluie, la tuile, la bande des jours (ADR-0023) ----------------------------------

namespace {

// La teinte de la pastille du mode : un RÔLE du thème (le noyau dit le ton, jamais une couleur).
std::uint32_t teinte_du_mode(const ThemePrincipal &t) {
    switch (accueil::ton_de(g.mode.raison)) {
    case accueil::Ton::Pluie:
        return t.pluie;
    case accueil::Ton::Alerte:
        return g.e.bandeau.present ? couleur_officielle(t, static_cast<int>(g.e.bandeau.niveau)) : t.avertissement;
    case accueil::Ton::Epingle:
        return t.accent;
    case accueil::Ton::Neutre:
        break;
    }
    return t.texte_3;
}
} // namespace

// La pastille « Auto · pluie proche » à côté du lieu, suivie de « Auto en pause » quand un profil est épinglé. Rend le bord droit de ce
// qui a été dessiné (0 : rien, faute de place). Le texte est coupé par « … » plutôt que de déborder.
int dessiner_pastille_mode(const Ctx &c, int x, int largeur_max) {
    const int h = px(36);
    const int pad = px(14);
    if (largeur_max < px(90)) {
        return 0;
    }
    const ThemePrincipal &t = *g.theme;
    const int centre = c.y + c.h / 2;
    const char *phrase = texte_principal::t(accueil::phrase_pastille(g.mode.raison, g.mode.profil));
    const Ajuste a = ajuster({&g.petit, &g.mini}, phrase, largeur_max - 2 * pad);
    const int w_texte = std::min(largeur_max - 2 * pad, dz::largeur_texte(*a.police, a.texte));
    const int w = w_texte + 2 * pad;
    fond_de_puce(c.pc, x, centre - h / 2, w, h, teinte_du_mode(t), 51, 166);
    dz::texte(c.pc, *a.police, a.texte, x + pad, centre - dz::hauteur_ligne(*a.police) / 2, w_texte + px(4),
              LV_TEXT_ALIGN_LEFT, t.texte);
    int fin = x + w;
    if (g.mode.epingle) {
        const char *pause = texte_principal::t(accueil::PHRASE_AUTO_EN_PAUSE);
        const int w_pause = dz::largeur_texte(g.mini, pause);
        const int x_pause = fin + px(12);
        if (x_pause + w_pause <= x + largeur_max) {
            dz::texte(c.pc, g.mini, pause, x_pause, centre - dz::hauteur_ligne(g.mini) / 2, w_pause + px(4),
                      LV_TEXT_ALIGN_LEFT, t.texte_3);
            fin = x_pause + w_pause;
        }
    }
    return fin;
}

const char *texte_force_pluie(accueil::NiveauPluie n) {
    switch (n) {
    case accueil::NiveauPluie::FAIBLE:
        return texte_principal::t("Pluie faible");
    case accueil::NiveauPluie::MOYEN:
        return texte_principal::t("Pluie modérée");
    case accueil::NiveauPluie::FORT:
        break;
    }
    return texte_principal::t("Pluie forte");
}

namespace {

// « Pluie modérée · puis plus forte vers 15 h » (la force de l'heure du début, puis un mot si une heure plus forte suit).
void texte_de_force(char *sortie, std::size_t n) {
    const accueil::CartePluie &cp = g.carte_pluie;
    sortie[0] = '\0';
    if (!cp.force_connue) {
        return;
    }
    std::snprintf(sortie, n, "%s", texte_force_pluie(cp.force));
    if (cp.plus_forte_plus_tard && cp.pic_s > 0) {
        char heure[16] = {};
        accueil::formater_heure_phrase(heure_du(cp.pic_s), en_francais(), heure, sizeof heure);
        const std::size_t deja = std::strlen(sortie);
        std::snprintf(sortie + deja, n - deja, "%s", POINT_MEDIAN);
        const std::size_t apres = std::strlen(sortie);
        std::snprintf(sortie + apres, n - apres, texte_principal::t("puis plus forte vers %s"), heure);
    }
}

// « environ 5 h · 6,5 mm en tout » : la durée seulement si elle est connue, le cumul seulement s'il est connu et non nul.
void texte_de_duree(char *sortie, std::size_t n) {
    const accueil::CartePluie &cp = g.carte_pluie;
    sortie[0] = '\0';
    if (cp.duree_h > 0) {
        std::snprintf(sortie, n, texte_principal::t("environ %d h"), cp.duree_h);
    }
    // Le cumul vient de la carte quand elle est là, sinon de l'épisode annoncé (une pluie plus tardive que la carte, dite par la tuile).
    const int cumul = cp.presente ? cp.cumul_dix_mm : g.pluie.cumul_dix_mm;
    if (connu(cumul) && cumul > 0) {
        char mm[16];
        fmt_dixieme(mm, sizeof mm, cumul);
        const std::size_t deja = std::strlen(sortie);
        if (deja > 0) {
            std::snprintf(sortie + deja, n - deja, "%s", POINT_MEDIAN);
        }
        const std::size_t apres = std::strlen(sortie);
        std::snprintf(sortie + apres, n - apres, texte_principal::t("%s mm en tout"), mm);
    }
}

// La durée et le cumul : sur une ligne (la police la plus grande qui tient), sinon, pour une langue longue ou un cumul à trois chiffres,
// sur deux lignes coupées à leur point médian. Jamais le cumul rogné par « … » tant qu'il y a un point où couper.
struct BlocDuree {
    const Police *police = nullptr;
    int lignes = 0; // 0 : rien à dire
};

// La plus large des lignes de `s` (séparées par « \n »), dans la police `p`.
int largeur_de_la_plus_large_ligne(const Police &p, const char *s) {
    int maxi = 0;
    while (*s != '\0') {
        const char *fin = std::strchr(s, '\n');
        char ligne[96];
        const std::size_t n =
            std::min(fin != nullptr ? static_cast<std::size_t>(fin - s) : std::strlen(s), sizeof ligne - 1);
        std::memcpy(ligne, s, n);
        ligne[n] = '\0';
        maxi = std::max(maxi, dz::largeur_texte(p, ligne));
        if (fin == nullptr) {
            break;
        }
        s = fin + 1;
    }
    return maxi;
}

// Prépare `duree` (elle peut y gagner un « \n ») pour une boîte de `largeur` pixels.
BlocDuree composer_duree(char *duree, int largeur) {
    BlocDuree b;
    if (duree[0] == '\0') {
        return b;
    }
    b.lignes = 1;
    b.police = &g.petit;
    if (dz::largeur_texte(g.petit, duree) <= largeur) {
        return b;
    }
    b.police = &g.mini;
    if (dz::largeur_texte(g.mini, duree) <= largeur) {
        return b;
    }
    char *point = std::strstr(duree, POINT_MEDIAN);
    if (point == nullptr) {
        return b; // rien où couper : la ligne sera tronquée par « … » (jamais vu : une durée et un cumul se séparent toujours)
    }
    const std::size_t n_point = std::strlen(POINT_MEDIAN);
    std::memmove(point + 1, point + n_point, std::strlen(point + n_point) + 1);
    *point = '\n';
    b.lignes = 2;
    b.police = largeur_de_la_plus_large_ligne(g.petit, duree) <= largeur ? &g.petit : &g.mini;
    return b;
}

int hauteur_de_la_duree(const BlocDuree &b) {
    return b.lignes > 0 ? b.lignes * dz::hauteur_ligne(*b.police) + px(8) : 0;
}

// `y` : le haut du bloc.
void poser_la_duree(const Ctx &c, const BlocDuree &b, const char *duree, int x, int y, int w, std::uint32_t rgb) {
    if (b.lignes >= 2) {
        dz::texte(c.pc, *b.police, duree, x, y, w, LV_TEXT_ALIGN_LEFT, rgb, LV_OPA_COVER, b.lignes);
    } else if (b.lignes == 1) {
        texte_ajuste(c.pc, {&g.petit, &g.mini}, duree, x, y + dz::hauteur_ligne(g.petit) / 2, w, LV_TEXT_ALIGN_LEFT,
                     rgb);
    }
}

// Les barres de la pluie heure par heure (la carte pluie et, en plus petit, la tuile). Chaque barre a la couleur de son niveau et une
// hauteur proportionnelle à la pluie de l'heure sur l'échelle de la carte ; une heure sèche est un petit trait, une heure inconnue
// n'a rien. `reperes` : deux lignes fines à 1 et 3 mm/h, avec leur valeur à gauche. `etiquettes` : l'heure sous chaque barre (une sur deux
// quand la place manque).
void dessiner_graphique_pluie(const dz::Pinceau &pc, const ThemePrincipal &t, int x, int y, int w, int h,
                              bool etiquettes, bool reperes) {
    const accueil::CartePluie &cp = g.carte_pluie;
    if (cp.n <= 0 || w <= 0 || h <= 0) {
        return;
    }
    const int h_etiq = etiquettes ? dz::hauteur_ligne(g.mini) + px(6) : 0;
    const int h_barres = h - h_etiq;
    if (h_barres < px(24)) {
        return;
    }
    const char *const REPERES[2] = {"1 mm/h", "3 mm/h"};
    const int gouttiere = reperes ? dz::largeur_texte(g.mini, REPERES[1]) + px(12) : 0;
    const int x0 = x + gouttiere;
    const int pas = (w - gouttiere) / cp.n;
    const int base = y + h_barres;
    if (reperes) {
        const int seuils[2] = {accueil::SEUIL_MODEREE_DIX, accueil::SEUIL_FORTE_DIX};
        for (int i = 0; i < 2; ++i) {
            if (!accueil::repere_visible(seuils[i], cp.echelle_dix)) {
                continue;
            }
            const int yy = base - accueil::hauteur_du_repere(seuils[i], cp.echelle_dix, h_barres);
            dz::rect(pc, x0, yy, w - gouttiere, std::max(1, px(1)), 0, t.texte, 40);
            dz::texte(pc, g.mini, REPERES[i], x, yy - dz::hauteur_ligne(g.mini) / 2, gouttiere - px(8),
                      LV_TEXT_ALIGN_RIGHT, t.texte_3);
        }
    }
    dz::rect(pc, x0, base, w - gouttiere, std::max(1, px(2)), 0, t.texte, 60);
    const int largeur_barre = std::max(px(8), pas - px(16));
    char etiquette[16];
    // Toutes les étiquettes, ou une sur deux : jamais un mélange selon la largeur de chaque heure (« 9 h » est plus courte que « 10 h »).
    int plus_large = 0;
    for (int i = 0; etiquettes && i < cp.n; ++i) {
        accueil::formater_heure_phrase(heure_du(cp.barre[i].instant_s), en_francais(), etiquette, sizeof etiquette);
        plus_large = std::max(plus_large, dz::largeur_texte(g.mini, etiquette));
    }
    const bool toutes = plus_large + px(4) <= pas;
    for (int i = 0; i < cp.n; ++i) {
        const accueil::BarrePluie &b = cp.barre[i];
        const int cx = x0 + pas * i + pas / 2;
        const int hb = accueil::hauteur_de_barre(b.dix_mm, cp.echelle_dix, h_barres, px(6));
        if (hb > 0) {
            dz::rect(pc, cx - largeur_barre / 2, base - hb, largeur_barre, hb, std::min(px(8), largeur_barre / 2),
                     accueil::couleur_niveau_pluie(t, b.niveau));
        } else if (connu(b.dix_mm)) {
            dz::rect(pc, cx - px(10), base - px(6), px(20), px(4), px(2), t.texte, 36); // sec : un trait
        }
        if (!etiquettes) {
            continue;
        }
        accueil::formater_heure_phrase(heure_du(b.instant_s), en_francais(), etiquette, sizeof etiquette);
        if (toutes || i % 2 == 0) {
            dz::texte(pc, g.mini, etiquette, cx - pas, base + px(6), 2 * pas, LV_TEXT_ALIGN_CENTER, t.texte_2);
        }
    }
}
} // namespace

// La carte pluie (ADR-0015 règle 2) : elle recouvre la zone des cinq jours. À gauche (en haut en portrait) le compte à rebours, la
// force de la pluie, sa durée et son cumul ; à droite (en bas) les barres des prochaines heures.
void dessiner_carte_pluie(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    carte_de_verre(c, px(28));
    const accueil::CartePluie &cp = g.carte_pluie;
    if (!cp.presente) {
        return;
    }
    const int pad = px(32);
    const int x0 = c.x + pad;
    const int w_tout = c.w - 2 * pad;
    const int w_texte = g.portrait ? w_tout : w_tout * 46 / 100;

    char tete[96], force[128], duree[96];
    texte_de_pluie(tete, sizeof tete, false);
    texte_de_force(force, sizeof force);
    texte_de_duree(duree, sizeof duree);
    const BlocDuree bloc_duree = composer_duree(duree, w_texte);

    const int icone = px(56);
    const int x_tete = x0 + icone + px(16);
    const Ajuste a_tete = ajuster({&g.temperature, &g.titre, &g.corps}, tete, w_texte - icone - px(16));
    const int h_tete = std::max(icone, dz::hauteur_ligne(*a_tete.police));
    const int h_force = force[0] != '\0' ? dz::hauteur_ligne(g.corps) + px(10) : 0;
    const int h_duree = hauteur_de_la_duree(bloc_duree);
    const int h_bloc = h_tete + h_force + h_duree + (h_force > 0 || h_duree > 0 ? px(10) : 0);
    int y = c.y + std::max(px(24), g.portrait ? px(32) : (c.h - h_bloc) / 2);

    icones::piece(c.pc.couche, Piece::parapluie, icone, x0, y + (h_tete - icone) / 2, t.pluie);
    dz::texte(c.pc, *a_tete.police, a_tete.texte, x_tete, y + (h_tete - dz::hauteur_ligne(*a_tete.police)) / 2,
              w_texte - icone - px(16) + px(4), LV_TEXT_ALIGN_LEFT, t.texte);
    y += h_tete + px(10);
    if (force[0] != '\0') {
        const int rayon = px(9);
        const std::uint32_t teinte = accueil::couleur_niveau_pluie(t, cp.force);
        dz::disque(c.pc, x0 + rayon, y + dz::hauteur_ligne(g.corps) / 2, rayon, teinte);
        texte_ajuste(c.pc, {&g.corps, &g.petit, &g.mini}, force, x0 + 2 * rayon + px(12),
                     y + dz::hauteur_ligne(g.corps) / 2, w_texte - 2 * rayon - px(12), LV_TEXT_ALIGN_LEFT, t.texte);
        y += h_force;
    }
    if (bloc_duree.lignes > 0) {
        poser_la_duree(c, bloc_duree, duree, x0, y, w_texte, t.texte_2);
        y += h_duree;
    }
    if (g.portrait) {
        const int y_graph = y + px(20);
        dessiner_graphique_pluie(c.pc, t, x0, y_graph, w_tout, c.y + c.h - pad - y_graph, true, true);
    } else {
        const int x_graph = x0 + w_texte + px(40);
        dessiner_graphique_pluie(c.pc, t, x_graph, c.y + px(28), c.x + c.w - pad - x_graph, c.h - px(28) - px(24), true,
                                 true);
    }
}

// La tuile de la composition d'alerte (à droite de la grande carte d'alerte, sous elle en portrait) : la pluie qui arrive, en petit, ou
// la mention des données périmées. En portrait elle tient sur une ligne (la même puce que le bandeau d'avant), en paysage elle est une
// carte avec sa phrase et ses barres.
void dessiner_tuile(const Ctx &c) {
    const ThemePrincipal &t = theme_meteo();
    const ThemePrincipal &normal = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const Rect &r = g.rect_tuile;
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    const bool perimee = m.perimee;
    if (g.portrait) {
        if (perimee) {
            char hhmm[16], s[96];
            fmt_hhmm(hhmm, sizeof hhmm, m.instant_s);
            std::snprintf(s, sizeof s, texte_principal::t("Données de %s · périmées"), hhmm);
            puce(c.pc, normal, r, normal.avertissement, 51, 166, Piece::hors_ligne, s);
        } else {
            char s[96];
            texte_de_pluie(s, sizeof s, true);
            puce(c.pc, t, r, t.pluie, 51, 166, Piece::parapluie, s);
        }
        return;
    }
    dz::rect_bord(c.pc, r.x, r.y, r.w, r.h, px(28), normal.carte, normal.carte_opa, normal.texte, 20, px(2));
    const int pad = px(24);
    const int icone = px(44);
    if (perimee) {
        // « Données de 07:52 » puis « périmées » : la phrase est coupée à son point médian, pas au milieu d'un mot.
        char hhmm[16], s[96];
        fmt_hhmm(hhmm, sizeof hhmm, m.instant_s);
        std::snprintf(s, sizeof s, texte_principal::t("Données de %s · périmées"), hhmm);
        if (char *point = std::strstr(s, POINT_MEDIAN); point != nullptr) {
            const std::size_t n_point = std::strlen(POINT_MEDIAN);
            std::memmove(point + 1, point + n_point, std::strlen(point + n_point) + 1);
            *point = '\n';
        }
        const int h_bloc = icone + px(12) + 2 * dz::hauteur_ligne(g.petit);
        const int y0 = r.y + std::max(pad, (r.h - h_bloc) / 2);
        icones::piece(c.pc.couche, Piece::hors_ligne, icone, r.x + pad, y0, normal.avertissement);
        dz::texte(c.pc, g.petit, s, r.x + pad, y0 + icone + px(12), r.w - 2 * pad, LV_TEXT_ALIGN_LEFT, normal.texte,
                  LV_OPA_COVER, 2);
        return;
    }
    char tete[96], duree[96];
    texte_de_pluie(tete, sizeof tete, false);
    texte_de_duree(duree, sizeof duree);
    const BlocDuree bloc_duree = composer_duree(duree, r.w - 2 * pad);
    // Sans barres (une pluie plus tardive que la carte) le contenu est court : on le centre dans la tuile.
    const int h_duree = hauteur_de_la_duree(bloc_duree);
    const int h_texte = icone + px(8) + h_duree;
    const int y0 = g.carte_pluie.n > 0 ? r.y + pad : r.y + std::max(pad, (r.h - h_texte) / 2);
    icones::piece(c.pc.couche, Piece::parapluie, icone, r.x + pad, y0, t.pluie);
    const int x_texte = r.x + pad + icone + px(12);
    texte_ajuste(c.pc, {&g.corps, &g.petit, &g.mini}, tete, x_texte, y0 + icone / 2, r.x + r.w - pad - x_texte,
                 LV_TEXT_ALIGN_LEFT, t.texte);
    int y = y0 + icone + px(8);
    if (bloc_duree.lignes > 0) {
        poser_la_duree(c, bloc_duree, duree, r.x + pad, y, r.w - 2 * pad, t.texte_2);
        y += h_duree;
    }
    dessiner_graphique_pluie(c.pc, t, r.x + pad, y + px(4), r.w - 2 * pad, r.y + r.h - pad - y - px(4), false, false);
}

// Les cinq jours réduits à une bande (composition d'alerte) : le nom du jour, l'icône, la maximale et la minimale. Pas de ruban, de
// vent ni de pluie : la place est donnée à l'alerte.
void dessiner_bande_jours(const Ctx &c) {
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
    const int x0 = c.x + px(24);
    const int largeur = c.w - 2 * px(24);
    const int pas = largeur / n;
    const bool large = pas >= px(200);
    for (int i = 0; i < n; ++i) {
        const JourPrevu &j = jours[i];
        const int cx = x0 + largeur * (2 * i + 1) / (2 * n);
        const bool aujourdhui = numero_du_jour_prevu(j) == numero_aujourdhui;
        char nom[24];
        jour_court(i18n::langue_courante(), ecran::heure::jour_semaine_du_jour(j.date_s, fuseau()), nom, sizeof nom);
        if (nom[0] >= 'a' && nom[0] <= 'z') {
            nom[0] = static_cast<char>(nom[0] - 'a' + 'A');
        }
        char haut[24], bas[24];
        fmt_temp(haut, sizeof haut, j.max_dix);
        fmt_temp(bas, sizeof bas, j.min_dix);
        const std::uint32_t c_haut = connu(j.max_dix) ? couleur_temperature(j.max_dix, t.rampe) : t.texte_3;
        const std::uint32_t c_bas = connu(j.min_dix) ? couleur_temperature(j.min_dix, t.rampe) : t.texte_3;
        const std::uint32_t c_nom = aujourdhui ? t.accent : t.texte_2;
        const int cote = large ? px(48) : px(40);
        if (large) {
            // Une seule rangée : le nom, l'icône, « 14° / 8° ».
            const int w_nom = dz::largeur_texte(g.petit, nom);
            const int w_haut = dz::largeur_texte(g.corps, haut);
            const int w_bas = dz::largeur_texte(g.mini, bas);
            const int sep = px(8);
            const int w_tout = w_nom + px(12) + cote + px(12) + w_haut + sep + w_bas;
            int x = cx - w_tout / 2;
            const int milieu = c.y + c.h / 2;
            texte_base(c.pc, g.petit, nom, x, milieu + px(9), w_nom + px(4), LV_TEXT_ALIGN_LEFT, c_nom);
            x += w_nom + px(12);
            icones::meteo(c.pc.couche, icone_pour_code(j.code, true), cote, x, milieu - cote / 2, t);
            x += cote + px(12);
            texte_base(c.pc, g.corps, haut, x, milieu + px(11), w_haut + px(4), LV_TEXT_ALIGN_LEFT, c_haut);
            x += w_haut + sep;
            texte_base(c.pc, g.mini, bas, x, milieu + px(8), w_bas + px(4), LV_TEXT_ALIGN_LEFT, c_bas);
        } else {
            // Colonne étroite (portrait) : le nom au-dessus, l'icône et la maximale dessous.
            dz::texte(c.pc, g.mini, nom, cx - pas / 2, c.y + px(6), pas, LV_TEXT_ALIGN_CENTER, c_nom);
            const int w_haut = dz::largeur_texte(g.corps, haut);
            const int w_tout = cote + px(6) + w_haut;
            int x = cx - w_tout / 2;
            const int milieu = c.y + c.h - px(6) - cote / 2;
            icones::meteo(c.pc.couche, icone_pour_code(j.code, true), cote, x, milieu - cote / 2, t);
            x += cote + px(6);
            texte_base(c.pc, g.corps, haut, x, milieu + px(11), w_haut + px(4), LV_TEXT_ALIGN_LEFT, c_haut);
        }
    }
}

} // namespace principal
} // namespace ecran
