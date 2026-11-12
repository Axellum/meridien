// Écran principal, accueil : le bandeau (puces, bande de pluie, alerte déduite) et la place de l'alerte officielle.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- zone : le bandeau (la pluie, l'alerte) ----

namespace {

// Le bandeau est fait de deux moitiés qui gardent chacune leur place : la pluie (ou la mention des données périmées) à gauche,
// l'alerte à droite.
Rect moitie_du_bandeau(bool droite) {
    const Rect &r = g.rect[Z_BANDEAU];
    const int ecart = px(16);
    const int w = (r.w - ecart) / 2;
    return droite ? Rect{r.x + w + ecart, r.y, r.w - w - ecart, r.h} : Rect{r.x, r.y, w, r.h};
}
} // namespace

// Verre teinté d'une puce : le fond à 20 %, le contour à 65 % de sa couleur (`opa_*` en 0 à 255).
void fond_de_puce(const dz::Pinceau &pc, int x, int y, int w, int h, std::uint32_t rgb, std::uint8_t opa_fond,
                  std::uint8_t opa_bord) {
    dz::rect_bord(pc, x, y, w, h, h / 2, rgb, opa_fond, rgb, opa_bord, std::max(1, px(2)));
}

// L'icône d'une puce : 30 px de référence, centrée dans le carré de gauche de la puce.
void icone_de_puce(const dz::Pinceau &pc, Piece piece, int x, int y, int h, std::uint32_t rgb) {
    const int cote = px(30);
    icones::piece(pc.couche, piece, cote, x + h / 2 - cote / 2, y + h / 2 - cote / 2, rgb);
}

namespace {

// Une coche dans un rond (« rien à signaler ») : la police n'a pas ce signe, on le trace.
void coche_dans_rond(const dz::Pinceau &pc, int cx, int cy, int rayon, std::uint32_t rgb) {
    const int e = std::max(2, px(2));
    dz::anneau(pc, cx, cy, rayon, e, rgb);
    dz::trait(pc, cx - rayon * 45 / 100, cy + rayon * 5 / 100, cx - rayon * 10 / 100, cy + rayon * 40 / 100, e, rgb);
    dz::trait(pc, cx - rayon * 10 / 100, cy + rayon * 40 / 100, cx + rayon * 50 / 100, cy - rayon * 35 / 100, e, rgb);
}
} // namespace

// Une puce complète : verre teinté, icône, texte (coupé par « … » s'il le faut).
void puce(const dz::Pinceau &pc, const ThemePrincipal &t, const Rect &r, std::uint32_t rgb, std::uint8_t opa_fond,
          std::uint8_t opa_bord, Piece icone, const char *texte) {
    fond_de_puce(pc, r.x, r.y, r.w, r.h, rgb, opa_fond, opa_bord);
    icone_de_puce(pc, icone, r.x, r.y, r.h, rgb);
    const int x_texte = r.x + r.h + px(4);
    texte_ajuste(pc, {&g.petit, &g.mini}, texte, x_texte, r.y + r.h / 2, r.x + r.w - px(24) - x_texte,
                 LV_TEXT_ALIGN_LEFT, t.texte);
}

// « Pluie dès 13 h · 6,5 mm », « Pluie dans 25 min · 2,1 mm », « Pluie en cours · 3,2 mm » (le cumul s'il est connu et demandé).
void texte_de_pluie(char *sortie, std::size_t n, bool avec_cumul) {
    const accueil::EpisodePluie &e = g.pluie;
    char base[64] = {};
    switch (e.genre) {
    case accueil::GenrePluie::EN_COURS:
        std::snprintf(base, sizeof base, "%s", texte_principal::t("Pluie en cours"));
        break;
    case accueil::GenrePluie::BIENTOT:
        std::snprintf(base, sizeof base, texte_principal::t("Pluie dans %d min"), e.dans_min);
        break;
    case accueil::GenrePluie::PLUS_TARD: {
        char heure[16] = {};
        accueil::formater_heure_phrase(heure_du(e.debut_s), en_francais(), heure, sizeof heure);
        std::snprintf(base, sizeof base, texte_principal::t("Pluie dès %s"), heure);
        break;
    }
    default:
        break;
    }
    if (avec_cumul && connu(e.cumul_dix_mm) && e.cumul_dix_mm > 0) {
        char mm[16];
        fmt_dixieme(mm, sizeof mm, e.cumul_dix_mm);
        std::snprintf(sortie, n, "%s%s%s mm", base, POINT_MEDIAN, mm);
    } else {
        std::snprintf(sortie, n, "%s", base);
    }
}
// ---- la bande de pluie (ADR-0027) : la ligne centrale, une barre par quart d'heure ----

namespace {

// Largeur de référence en dessous de laquelle la moitié gauche du bandeau ne tient ni la phrase ni huit barres (au rendu : environ 600
// en paysage, environ 320 en portrait 720 x 1280).
constexpr int LARGEUR_MIN_MOITIE_BANDE = 450;

// La phrase de la bande, sur UNE ligne (la zone du bandeau n'a la place que d'une ligne). Il pleut : « Pluie modérée · fin dans 42 min »
// (la fin seulement si les quarts la donnent et si `avec_fin`) ; sinon « Pluie dans 25 min » ou « Pluie dès 15 h ».
void texte_de_bande(char *sortie, std::size_t n, bool avec_fin) {
    const accueil::BandePluie &b = g.bande;
    if (b.en_cours) {
        std::snprintf(sortie, n, "%s", texte_force_pluie(b.force));
        if (avec_fin && b.fin_dans_min >= 0) {
            const std::size_t deja = std::strlen(sortie);
            std::snprintf(sortie + deja, n - deja, "%s", POINT_MEDIAN);
            const std::size_t apres = std::strlen(sortie);
            std::snprintf(sortie + apres, n - apres, texte_principal::t("fin dans %d min"), b.fin_dans_min);
        }
        return;
    }
    const std::int64_t debut_s = b.barre[b.premier].debut_s;
    const int dans_min = static_cast<int>((debut_s - g.e.maintenant_s + 59) / 60);
    if (dans_min < accueil::PLUIE_BIENTOT_MIN) {
        std::snprintf(sortie, n, texte_principal::t("Pluie dans %d min"), dans_min > 1 ? dans_min : 1);
    } else {
        char heure[16] = {};
        accueil::formater_heure_phrase(heure_du(debut_s), en_francais(), heure, sizeof heure);
        std::snprintf(sortie, n, texte_principal::t("Pluie dès %s"), heure);
    }
}

// La bande : l'icône et la phrase à gauche, puis une barre par quart d'heure (la couleur dit la force, la hauteur la quantité), l'heure
// sous les barres de chaque demi-heure (de chaque heure si la place manque, aucune sinon), un repère « maintenant » en haut et un
// trait pointillé à « dans une heure » (le seuil du mode automatique). `r` : la moitié gauche du bandeau (l'alerte a la droite).
void dessiner_bande_pluie(const Ctx &c, const Rect &r) {
    const accueil::BandePluie &b = g.bande;
    if (!b.presente || b.n <= 0 || r.w <= 0 || r.h <= 0) {
        return;
    }
    const ThemePrincipal &t = theme_meteo();
    const ThemePrincipal &normal = *g.theme;
    dz::rect_bord(c.pc, r.x, r.y, r.w, r.h, px(24), normal.carte, normal.carte_opa, normal.texte, 20, px(2));

    const int pad = px(18);
    const int cote = px(30);
    icones::piece(c.pc.couche, Piece::parapluie, cote, r.x + pad, r.y + r.h / 2 - cote / 2, t.pluie);
    const int x_texte = r.x + pad + cote + px(12);
    const int w_max = (r.w - 2 * pad - cote - px(12)) * 45 / 100;
    char s[96];
    texte_de_bande(s, sizeof s, true);
    if (dz::largeur_texte(g.mini, s) > w_max) {
        texte_de_bande(s, sizeof s, false); // la fin part avant que la phrase soit coupée par « … »
    }
    const Ajuste a = ajuster({&g.petit, &g.mini}, s, w_max);
    const int w_texte = std::min(w_max, dz::largeur_texte(*a.police, a.texte));
    dz::texte(c.pc, *a.police, a.texte, x_texte, r.y + (r.h - dz::hauteur_ligne(*a.police)) / 2, w_texte + px(4),
              LV_TEXT_ALIGN_LEFT, t.texte);

    const int gx0 = x_texte + w_texte + px(32);
    const int gx1 = r.x + r.w - pad;
    const int largeur = gx1 - gx0;
    if (largeur < b.n * px(14)) {
        return; // plus de place pour les barres : la phrase reste
    }
    const int pas = largeur / b.n;
    // La ligne ne fait que 62 px de référence : les barres prennent tout ce que les heures et le repère « maintenant » leur laissent.
    const int base_etiquettes = r.y + r.h - px(4);
    const int y_bas = base_etiquettes - dz::hauteur_chiffres(g.mini) - px(3);
    const int y_haut = r.y + px(9);
    const int h_barres = y_bas - y_haut;
    if (h_barres < px(10)) {
        return;
    }
    // Les étiquettes : toutes les demi-heures si elles tiennent, sinon toutes les heures, sinon aucune.
    char etiquette[16];
    int plus_large = 0;
    for (int i = 0; i < b.n; ++i) {
        ecran::heure::formater_etiquette_heure(heure_du(b.barre[i].debut_s), en_francais(), etiquette,
                                               sizeof etiquette);
        plus_large = std::max(plus_large, dz::largeur_texte(g.mini, etiquette));
    }
    const int modulo = 2 * pas >= plus_large + px(8) ? 30 : (4 * pas >= plus_large + px(8) ? 60 : 0);

    dz::rect(c.pc, gx0, y_bas, largeur, std::max(1, px(2)), 0, t.texte, 60);
    const int largeur_barre =
        std::clamp(pas * 45 / 100, px(8), px(26)); // des barres, pas des galets : comme les pastilles de la courbe
    for (int i = 0; i < b.n; ++i) {
        const accueil::BarreQuart &q = b.barre[i];
        const int cx = gx0 + pas * i + pas / 2;
        const int hb = accueil::hauteur_de_barre_quart(q.dix_mm_h, b.echelle_dix_h, h_barres, px(5));
        if (hb > 0) {
            dz::rect(c.pc, cx - largeur_barre / 2, y_bas - hb, largeur_barre, hb, std::min(px(8), largeur_barre / 2),
                     accueil::couleur_niveau_pluie(t, q.niveau));
        } else if (q.dix_mm >= 0) {
            dz::rect(c.pc, cx - px(10), y_bas - px(4), px(20), px(4), px(2), t.texte, 36); // sec : un trait
        } // sans valeur : rien, jamais un trait sec
        const ecran::heure::HeureLocale local = heure_du(q.debut_s);
        if (modulo > 0 && local.minute % modulo == 0) {
            ecran::heure::formater_etiquette_heure(local, en_francais(), etiquette, sizeof etiquette);
            texte_base(c.pc, g.mini, etiquette, cx - pas, base_etiquettes, 2 * pas, LV_TEXT_ALIGN_CENTER, t.texte_2);
        }
    }
    if (b.plus_une_heure_pm >= 0) {
        const int x = gx0 + static_cast<int>(static_cast<std::int64_t>(largeur) * b.plus_une_heure_pm / 1000);
        for (int y = y_haut; y < y_bas; y += px(7)) {
            dz::rect(c.pc, x, y, std::max(1, px(1)), px(3), 0, t.texte, 110);
        }
    }
    const int x_maintenant = gx0 + static_cast<int>(static_cast<std::int64_t>(largeur) * b.maintenant_pm / 1000);
    dz::triangle(c.pc, x_maintenant - px(5), r.y + px(2), x_maintenant + px(5), r.y + px(2), x_maintenant, r.y + px(8),
                 t.accent);
}
} // namespace

void dessiner_bandeau(const Ctx &c) {
    const ThemePrincipal &t = theme_meteo();
    const ThemePrincipal &normal = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const Alerte al = alerte_du_jour();
    if (g.composition == accueil::Composition::AlerteAgrandie) {
        // La grande carte d'alerte est l'objet g.pastille (elle reçoit l'appui) ; ici, la tuile à côté d'elle.
        dessiner_tuile(c);
        return;
    }
    const accueil::ContenuBandeau cb =
        accueil::composer_bandeau(m.connue, m.perimee, g.pluie.genre, g.e.bandeau.present, al != Alerte::AUCUNE);
    const Rect gauche = moitie_du_bandeau(false);
    // Il pleut ou il va pleuvoir dans les deux heures : la bande de pluie prend la place de la puce, TOUJOURS sur la moitié gauche
    // (les deux moitiés du bandeau gardent leur place : la pluie à gauche, l'alerte à droite ; Axel, 09/11/2026). Seule exception :
    // une moitié trop étroite pour les barres (le portrait : environ 320 px de référence). Elle prend alors toute la largeur si
    // l'alerte ne la réclame pas, sinon la puce d'avant reste (la phrase seule).
    const bool moitie_trop_etroite = gauche.w < px(LARGEUR_MIN_MOITIE_BANDE);
    const bool bande = g.bande.presente && cb.gauche != accueil::Puce::PERIMEE &&
                       (!moitie_trop_etroite || cb.droite == accueil::Puce::AUCUNE);
    if (bande) {
        dessiner_bande_pluie(c, moitie_trop_etroite ? g.rect[Z_BANDEAU] : gauche);
    } else {
        switch (g.composition == accueil::Composition::CartePluie ? accueil::Puce::AUCUNE : cb.gauche) {
        case accueil::Puce::PLUIE: {
            char s[96];
            texte_de_pluie(s, sizeof s, true);
            if (dz::largeur_texte(g.mini, s) > gauche.w - gauche.h - px(4) - px(24)) {
                texte_de_pluie(s, sizeof s,
                               false); // le cumul est le détail : il part avant que la phrase soit coupée par « … »
            }
            puce(c.pc, t, gauche, t.pluie, 51, 166, Piece::parapluie, s);
            break;
        }
        case accueil::Puce::PERIMEE: {
            char hhmm[16], s[96];
            fmt_hhmm(hhmm, sizeof hhmm, m.instant_s);
            std::snprintf(s, sizeof s, texte_principal::t("Données de %s · périmées"), hhmm);
            puce(c.pc, normal, gauche, normal.avertissement, 51, 166, Piece::hors_ligne, s);
            break;
        }
        case accueil::Puce::CALME: {
            // Centrée sur toute la largeur : il n'y a rien d'autre à dire.
            const Rect r = {c.x + (c.w - gauche.w) / 2, gauche.y, gauche.w, gauche.h};
            fond_de_puce(c.pc, r.x, r.y, r.w, r.h, normal.texte_3, 26, 64);
            coche_dans_rond(c.pc, r.x + r.h / 2, r.y + r.h / 2, px(13), normal.texte_3);
            const int x_texte = r.x + r.h + px(4);
            texte_ajuste(c.pc, {&g.petit, &g.mini}, texte_principal::t("Rien à signaler dans les prochaines heures"),
                         x_texte, r.y + r.h / 2, r.x + r.w - px(24) - x_texte, LV_TEXT_ALIGN_LEFT, normal.texte);
            break;
        }
        default:
            break;
        }
    }
    if (cb.droite == accueil::Puce::ALERTE_METEO) {
        // L'alerte déduite de la météo du jour : pas d'appui (seule l'alerte officielle ouvre la page des alertes).
        const Rect r = g.composition == accueil::Composition::CartePluie ? g.rect[Z_BANDEAU] : moitie_du_bandeau(true);
        puce(c.pc, t, r, couleur_alerte(t, al), 51, 166, piece_alerte(al), texte_alerte(al));
    }
    // L'alerte officielle est dessinée par l'objet g.pastille, posé sur cette moitié (il reçoit l'appui).
}

// ---- l'alerte officielle (objet g.pastille, posé sur la moitié droite du bandeau) ----

namespace {

// Le texte de la puce : le titre de l'émetteur, suivi de la fenêtre « 14–22 h » quand l'alerte tient dans un seul jour. Trop long, le
// titre est coupé par « … » et la fenêtre reste (c'est elle qui dit QUAND) ; sans place pour les deux, le titre seul.
void texte_officielle(const vigie::Bandeau &b, int largeur, char *sortie, std::size_t n, const Police **police) {
    char fenetre[24] = {};
    if (b.debut_s > 0 && b.fin_s > 0) {
        accueil::formater_fenetre(heure_du(b.debut_s), heure_du(b.fin_s), en_francais(), fenetre, sizeof fenetre);
    }
    const char *liaison = fenetre[0] != '\0' ? POINT_MEDIAN : "";
    for (const Police *p : {&g.petit, &g.mini}) {
        std::snprintf(sortie, n, "%s%s%s", b.texte, liaison, fenetre);
        *police = p;
        if (dz::largeur_texte(*p, sortie) <= largeur) {
            return;
        }
    }
    const Police &p = g.mini;
    const int w_fenetre = fenetre[0] != '\0' ? dz::largeur_texte(p, liaison) + dz::largeur_texte(p, fenetre) : 0;
    if (largeur - w_fenetre < px(80)) {
        const Ajuste a = ajuster({&p}, b.texte, largeur);
        std::snprintf(sortie, n, "%s", a.texte);
        return;
    }
    const Ajuste a = ajuster({&p}, b.texte, largeur - w_fenetre);
    // Un titre plus long que le tampon serait coupé à n - 1 octets par snprintf (ajuster() le borne à la largeur de la puce : cela
    // n'arrive pas) ; on lit le retour pour ne jamais laisser un texte à moitié écrit si snprintf échoue.
    if (std::snprintf(sortie, n, "%s%s%s", a.texte, liaison, fenetre) < 0) {
        sortie[0] = '\0';
    }
}

// La grande carte d'alerte (composition d'alerte, ADR-0015 règle 1) : le titre de l'émetteur sur deux lignes au plus, la fenêtre sur la
// sienne. Les positions sont calculées au dessin (sur_dessin_pastille) ; ici seulement le texte, coupé par « … » s'il est trop long.
PlaceOfficielle placer_carte_alerte(const Rect &r) {
    PlaceOfficielle p;
    const vigie::Bandeau &b = g.e.bandeau;
    p.x = r.x;
    p.y = r.y;
    p.w = r.w;
    p.h = r.h;
    p.grande = true;
    if (b.autres > 0) {
        std::snprintf(p.autres, sizeof p.autres, "+%u", static_cast<unsigned>(b.autres));
        p.d_autres = std::max(px(36), dz::largeur_texte(g.petit, p.autres) + px(18));
        p.x_autres = p.w - px(28) - p.d_autres;
    }
    if (b.debut_s > 0 && b.fin_s > 0) {
        accueil::formater_fenetre(heure_du(b.debut_s), heure_du(b.fin_s), en_francais(), p.fenetre, sizeof p.fenetre);
    }
    const int largeur = p.w - 2 * px(28);
    const Ajuste a = ajuster({&g.corps}, b.texte, 2 * largeur - largeur / 6);
    std::snprintf(p.texte, sizeof p.texte, "%s", a.texte);
    p.police = &g.corps;
    p.w_texte = largeur;
    p.x_texte = px(28);
    p.ok = p.w > 0 && p.h > 0;
    return p;
}
} // namespace

PlaceOfficielle placer_puce_officielle() {
    PlaceOfficielle p;
    const vigie::Bandeau &b = g.e.bandeau;
    const Rect &zone = g.rect[Z_BANDEAU];
    if (!b.present || zone.w <= 0 || zone.h <= 0) {
        return p;
    }
    if (g.composition == accueil::Composition::AlerteAgrandie && g.rect_alerte.w > 0 && g.rect_alerte.h > 0) {
        return placer_carte_alerte(g.rect_alerte);
    }
    // Seule, la puce prend toute la largeur du bandeau (la carte pluie dit la pluie) ; sinon la moitié droite.
    const Rect r = g.composition == accueil::Composition::CartePluie ? zone : moitie_du_bandeau(true);
    p.x = r.x;
    p.y = r.y;
    p.w = r.w;
    p.h = r.h;
    int d_autres = 0;
    if (b.autres > 0) {
        std::snprintf(p.autres, sizeof p.autres, "+%u", static_cast<unsigned>(b.autres));
        d_autres = std::max(px(30), dz::largeur_texte(g.mini, p.autres) + px(14));
        p.d_autres = d_autres;
        p.x_autres = p.w - px(18) - d_autres;
    }
    const int x_texte = p.h + px(4);
    const int bord = d_autres > 0 ? p.x_autres - px(10) : p.w - px(24);
    const int w_dispo = bord - x_texte;
    if (w_dispo >= px(60) && b.texte[0] != '\0') {
        const Police *police = nullptr;
        texte_officielle(b, w_dispo, p.texte, sizeof p.texte, &police);
        p.police = police;
        p.w_texte = std::min(w_dispo, dz::largeur_texte(*police, p.texte));
        p.x_texte = x_texte;
    }
    p.ok = true;
    return p;
}

} // namespace principal
} // namespace ecran
