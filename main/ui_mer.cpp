#include "ui_mer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "ecran/i18n.hpp"
#include "ecran/mer_geometrie.hpp"
#include "ecran/mer_page.hpp"
#include "ecran/mer_textes.hpp"
#include "ecran/mer_vue.hpp"
#include "ui_meteo_dessin.h"
#include "ui_meteo_icones.h"

// Page « mer » : un seul objet LVGL qui se dessine tout entier (comme les zones de l'écran principal), pour ne coûter ni objet ni RAM
// interne par carte, barre ou étiquette. Ce fichier ne DÉCIDE rien : quoi montrer vient de ecran/mer_page.hpp (verdict montré ou non,
// quelles barres, quel critère, quelle courbe), les mots de ecran/mer_textes.hpp (sept langues), les positions de ecran/mer_geometrie.hpp ;
// tous testés sur le PC (test/host/test_mer_*.cpp). Ici : les pinceaux, les couleurs du thème, l'appui.
//
// Aucune couleur écrite ici : les rôles du thème de l'écran principal (visuel_themes.hpp). Le verdict « bon » prend le vert de
// `officielle[]`, « possible » son jaune, « déconseillé » son rouge ; la nuit et l'inconnu prennent le trait fin du thème (jamais une
// couleur de verdict : on ne devine pas).

namespace ecran {

namespace {

namespace dz = dessin;
namespace geo = mer::geo;
using visuel::ThemePrincipal;

// Ce que l'appui désigne (hors des cartes, qui valent 0 à 3).
constexpr int CIBLE_AUCUNE = -1;
constexpr int CIBLE_FERMER = 4; // le bouton « Fermer » de la page
constexpr int CIBLE_CROIX = 5;  // la croix du détail
constexpr int CIBLE_VOILE = 6;  // hors du panneau du détail : ferme le détail aussi

// Les polices sont créées une fois par taille d'écran et gardées (TinyTTF se libère mal : même choix que ui_alertes.cpp). Un changement
// de taille d'écran (rotation) en crée de nouvelles et abandonne les anciennes : quelques dizaines de Ko, une fois par changement.
struct Polices {
    const std::uint8_t *ttf = nullptr;
    int petit_cote = 0;
    dz::Police verdict;
    dz::Police titre;
    dz::Police texte;
    dz::Police petit;
};

Polices gp;

struct Page {
    ContexteMer ctx;
    mer::Mer m; // la mer au moment de l'ouverture : la page garde sa propre image
    std::int64_t maintenant_s = 0;
    std::int32_t decalage_s = 0;
    i18n::Langue langue = i18n::Langue::Fr;
    mer::Fraicheur fraicheur = mer::Fraicheur::Rien;
    geo::Disposition d;
    geo::Detail det;
    lv_obj_t *zone = nullptr;
    int detail = -1; // l'activité dont le détail est ouvert, ou -1
    int appuye = CIBLE_AUCUNE;
};

Page g;

// Ce qu'une fonction de dessin reçoit : la couche, le thème, et le coin haut-gauche de l'objet (les rectangles de la mise en page sont
// relatifs à l'objet).
struct Dessin {
    dz::Pinceau pc;
    const ThemePrincipal &t;
    int ox;
    int oy;
};

int M(int px) { return geo::mis(px, g.d.petit_cote); }

// ---- textes -------------------------------------------------------------------------------------------------------------------------

// Un texte de la table, terminé par un zéro (les entrées de la table sont des string_view).
const char *copie(std::string_view s) {
    static char tampon[8][200];
    static int suivant = 0;
    char *t = tampon[suivant];
    suivant = (suivant + 1) % 8;
    const std::size_t n = std::min(s.size(), sizeof tampon[0] - 1);
    std::memcpy(t, s.data(), n);
    t[n] = '\0';
    return t;
}

const char *T(mer::Texte x) { return copie(mer::texte(x, g.langue)); }

// Raccourcit `s` (UTF-8) jusqu'à tenir dans `w` pixels, trois points au bout ; ne coupe jamais un caractère.
void ajuster(char *s, std::size_t capacite, const dz::Police &p, int w) {
    if (dz::largeur_texte(p, s) <= w) {
        return;
    }
    std::size_t n = std::strlen(s);
    while (n > 0) {
        --n;
        while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) {
            --n; // reculer jusqu'au début du caractère
        }
        if (n + 4 > capacite) {
            continue;
        }
        std::memcpy(s + n, "...", 4);
        if (dz::largeur_texte(p, s) <= w) {
            return;
        }
        s[n] = '\0';
    }
    s[0] = '\0';
}

// Une ligne de texte dans le rectangle `r` (relatif à l'objet), centrée en hauteur, raccourcie si elle ne tient pas.
void ligne(const Dessin &c, const dz::Police &p, const char *s, const geo::Rect &r, lv_text_align_t alignement,
           std::uint32_t rgb, std::uint8_t opa = LV_OPA_COVER) {
    if (s == nullptr || s[0] == '\0' || r.w <= 0) {
        return;
    }
    char b[200];
    std::snprintf(b, sizeof b, "%s", s);
    ajuster(b, sizeof b, p, r.w);
    const int y = r.y + (r.h - dz::hauteur_ligne(p)) / 2;
    dz::texte(c.pc, p, b, c.ox + r.x, c.oy + y, r.w, alignement, rgb, opa);
}

// Un texte qui passe à la ligne dans `r`, au plus `lignes` lignes (le reste est coupé par la boîte).
void paragraphe(const Dessin &c, const dz::Police &p, const char *s, const geo::Rect &r, int lignes, std::uint32_t rgb,
                std::uint8_t opa = LV_OPA_COVER) {
    if (s == nullptr || s[0] == '\0' || r.w <= 0) {
        return;
    }
    dz::texte(c.pc, p, s, c.ox + r.x, c.oy + r.y, r.w, LV_TEXT_ALIGN_LEFT, rgb, opa, lignes);
}

// Un tiret dessiné (pas un caractère : une valeur absente ne dépend d'aucun glyphe), à gauche, au centre ou à droite de `r`.
void tiret(const Dessin &c, const geo::Rect &r, lv_text_align_t alignement, std::uint32_t rgb,
           std::uint8_t opa = LV_OPA_COVER) {
    const int w = M(18);
    const int h = std::max(2, M(3));
    int x = r.x;
    if (alignement == LV_TEXT_ALIGN_CENTER) {
        x = r.x + (r.w - w) / 2;
    } else if (alignement == LV_TEXT_ALIGN_RIGHT) {
        x = r.x + r.w - w;
    }
    dz::rect(c.pc, c.ox + x, c.oy + r.y + (r.h - h) / 2, w, h, h / 2, rgb, opa);
}

// ---- couleurs -----------------------------------------------------------------------------------------------------------------------

std::uint32_t fond_carte(const ThemePrincipal &t) { return visuel::carte_sur(t, t.fond); }

std::uint32_t couleur_verdict(const ThemePrincipal &t, mer::Verdict v) {
    switch (v) {
    case mer::Verdict::Bon:
        return visuel::couleur_officielle(t, 1); // vert
    case mer::Verdict::Possible:
        return visuel::couleur_officielle(t, 2); // jaune
    case mer::Verdict::Deconseille:
        return visuel::couleur_officielle(t, 4); // rouge
    default:
        return t.texte_3; // inconnu : un neutre, jamais la couleur d'un verdict
    }
}

// ---- icônes des activités ---------------------------------------------------------------------------------------------------------------
// Dessinées avec les mêmes pinceaux que le reste (la police d'icônes MDI n'existe pas sur Meridien) : des formes simples, reconnaissables à
// 30 px. Les coordonnées sont des centièmes du carré.

void icone_activite(const Dessin &c, int activite, const geo::Rect &r, std::uint32_t rgb, std::uint8_t opa) {
    const int s = std::min(r.w, r.h);
    if (s <= 0) {
        return;
    }
    const int x0 = c.ox + r.x + (r.w - s) / 2;
    const int y0 = c.oy + r.y + (r.h - s) / 2;
    auto X = [&](int f) { return x0 + f * s / 100; };
    auto Y = [&](int f) { return y0 + f * s / 100; };
    auto E = [&](int f) { return std::max(2, f * s / 100); };
    const dz::Pinceau &pc = c.pc;
    switch (static_cast<mer::Activite>(activite)) {
    case mer::Activite::Surf: // une planche à bout pointu, penchée, au-dessus de trois vagues
        dz::trait(pc, X(37), Y(64), X(47), Y(38), E(30), rgb, opa);          // le dos arrondi
        dz::triangle(pc, X(62), Y(4), X(32), Y(34), X(60), Y(46), rgb, opa); // le nez
        for (int i = 0; i < 3; ++i) {
            dz::arc(pc, X(20 + 30 * i), Y(90), E(15), E(8), 180, 360, rgb, opa, false);
        }
        break;
    case mer::Activite::Kite: // un cerf-volant à deux facettes et sa ligne
        dz::triangle(pc, X(50), Y(6), X(18), Y(38), X(50), Y(70), rgb, opa);
        dz::triangle(pc, X(50), Y(6), X(82), Y(38), X(50), Y(70), rgb, static_cast<std::uint8_t>(opa * 62 / 100));
        dz::trait(pc, X(50), Y(70), X(34), Y(94), E(5), rgb, opa);
        break;
    case mer::Activite::Bateau: // une coque, un mât et une voile
        dz::triangle(pc, X(8), Y(66), X(92), Y(66), X(24), Y(90), rgb, opa);
        dz::triangle(pc, X(92), Y(66), X(76), Y(90), X(24), Y(90), rgb, opa);
        dz::trait(pc, X(48), Y(8), X(48), Y(66), E(6), rgb, opa, false);
        dz::triangle(pc, X(54), Y(12), X(54), Y(60), X(88), Y(60), rgb, opa);
        dz::triangle(pc, X(42), Y(24), X(42), Y(60), X(14), Y(60), rgb, static_cast<std::uint8_t>(opa * 62 / 100));
        break;
    case mer::Activite::PecheAPied: // le seau de la pêche à pied, avec son anse
        dz::arc(pc, X(50), Y(42), E(30), E(7), 195, 345, rgb, opa, true);
        dz::rect(pc, X(16), Y(38), E(68), E(9), E(3), rgb, opa);
        dz::triangle(pc, X(22), Y(46), X(78), Y(46), X(32), Y(92), rgb, opa);
        dz::triangle(pc, X(78), Y(46), X(68), Y(92), X(32), Y(92), rgb, opa);
        break;
    default:
        break;
    }
}

// ---- verre --------------------------------------------------------------------------------------------------------------------------

void verre(const Dessin &c, const geo::Rect &r, std::uint32_t bord, std::uint8_t bord_opa) {
    dz::rect_bord(c.pc, c.ox + r.x, c.oy + r.y, r.w, r.h, g.d.rayon, fond_carte(c.t), LV_OPA_COVER, bord, bord_opa,
                  M(2));
}

// ---- en-tête et bandeau -----------------------------------------------------------------------------------------------------------------

void entete(const Dessin &c) {
    const ThemePrincipal &t = c.t;
    const char *nom = g.m.nom[0] != '\0' ? g.m.nom : T(mer::Texte::TitreMer);
    ligne(c, gp.titre, nom, g.d.titre, LV_TEXT_ALIGN_LEFT, t.texte);
    // Le bouton « Fermer » : la zone tactile prend toute la hauteur de l'en-tête, le bouton dessiné est plus bas.
    const geo::Rect &z = g.d.fermer;
    const int h = std::min(z.h, M(52));
    const bool appuye = g.appuye == CIBLE_FERMER;
    dz::rect(c.pc, c.ox + z.x, c.oy + z.y + (z.h - h) / 2, z.w, h, M(18), appuye ? t.filet : t.accent);
    ligne(c, gp.texte, T(mer::Texte::Fermer), z, LV_TEXT_ALIGN_CENTER, t.fond);
}

void bandeau(const Dessin &c) {
    const ThemePrincipal &t = c.t;
    const geo::Rect &r = g.d.bandeau;
    char texte[200] = {};
    const mer::Bandeau genre =
        mer::composer_bandeau(g.m, g.fraicheur, g.maintenant_s, g.decalage_s, g.langue, texte, sizeof texte);
    const int pad = M(12);
    const int rayon = r.h / 2;
    if (genre == mer::Bandeau::Rien) {
        // Rien à dire : la commune la plus proche, discrète, sans cadre. Vide si la trame n'en donne pas.
        ligne(c, gp.petit, g.m.lieu, geo::Rect{r.x, r.y, r.w, r.h}, LV_TEXT_ALIGN_LEFT, t.texte_3);
        return;
    }
    const mer::StyleBandeau style = mer::style_bandeau(genre, g.m.vigilance_niveau);
    std::uint32_t couleur = t.avertissement; // un âge ou une absence : la couleur d'avertissement du thème
    if (genre != mer::Bandeau::Etat) {
        couleur = visuel::couleur_officielle(t, style.niveau);
    }
    if (style.teinte) {
        dz::rect_bord(c.pc, c.ox + r.x, c.oy + r.y, r.w, r.h, rayon, couleur, 56, couleur, 230, M(2));
    } else {
        dz::rect_bord(c.pc, c.ox + r.x, c.oy + r.y, r.w, r.h, rayon, fond_carte(t), LV_OPA_COVER, couleur, 230, M(2));
    }
    int x = r.x + pad + M(4);
    if (genre != mer::Bandeau::Etat) {
        const int cote = r.h - 2 * M(8);
        icones::piece(c.pc.couche, visuel::Piece::alerte, cote, c.ox + x, c.oy + r.y + (r.h - cote) / 2, couleur);
        x += cote + M(10);
    }
    const geo::Rect in{x, r.y, r.x + r.w - pad - M(4) - x, r.h};
    ligne(c, gp.texte, texte, in, LV_TEXT_ALIGN_LEFT, t.texte);
}

// ---- une carte d'activité -----------------------------------------------------------------------------------------------------------------

void barres(const Dessin &c, const geo::InterieurCarte &in, const mer::Prevision &a, bool montrees) {
    const ThemePrincipal &t = c.t;
    const geo::Rect &b = in.bandes;
    if (!montrees) {
        // Pas de verdict à montrer : un rail vide à la place des barres, la carte garde sa forme.
        dz::rect(c.pc, c.ox + b.x, c.oy + b.y, b.w, b.h, M(4), t.filet, 70);
        return;
    }
    const int largeur = std::max(1, in.pas_barre - 1);
    for (int h = 0; h < mer::HEURES; ++h) {
        const mer::Teinte teinte = mer::teinte_barre(a, h);
        const int x = c.ox + b.x + h * in.pas_barre;
        if (teinte == mer::Teinte::Neutre) {
            dz::rect(c.pc, x, c.oy + b.y, largeur, b.h, std::min(3, largeur / 2), t.filet, 110);
        } else {
            const mer::Verdict v = teinte == mer::Teinte::Bon        ? mer::Verdict::Bon
                                   : teinte == mer::Teinte::Possible ? mer::Verdict::Possible
                                                                     : mer::Verdict::Deconseille;
            dz::rect(c.pc, x, c.oy + b.y, largeur, b.h, std::min(3, largeur / 2), couleur_verdict(t, v));
        }
    }
    // Le trait blanc « maintenant ».
    const int courante = mer::heure_courante(g.m, g.fraicheur, g.maintenant_s);
    if (courante >= 0) {
        const int epaisseur = std::max(2, M(3));
        const int x = c.ox + b.x + courante * in.pas_barre + (largeur - epaisseur) / 2;
        dz::rect(c.pc, x, c.oy + b.y - M(3), epaisseur, b.h + 2 * M(3), epaisseur / 2, t.texte);
    }
}

// Les trois repères de l'axe : le début de la bande, douze heures plus tard, vingt-quatre heures plus tard.
void axe(const Dessin &c, const geo::Rect &a, std::uint32_t rgb, std::uint8_t opa) {
    if (!mer::axe_montre(g.m, g.fraicheur)) {
        return;
    }
    const int tiers = a.w / 3;
    const geo::Rect zones[3] = {
        {a.x, a.y, tiers, a.h}, {a.x + tiers, a.y, a.w - 2 * tiers, a.h}, {a.x + a.w - tiers, a.y, tiers, a.h}};
    const lv_text_align_t alignements[3] = {LV_TEXT_ALIGN_LEFT, LV_TEXT_ALIGN_CENTER, LV_TEXT_ALIGN_RIGHT};
    for (int i = 0; i < 3; ++i) {
        char h[16] = {};
        if (mer::formater_heure(g.m.premiere_heure_s + i * 12 * 3600, g.decalage_s, h, sizeof h) != 0) {
            ligne(c, gp.petit, h, zones[i], alignements[i], rgb, opa);
        }
    }
}

void carte(const Dessin &c, int k) {
    const ThemePrincipal &t = c.t;
    const geo::Rect &r = g.d.cartes[k];
    const mer::VueCarte v = mer::vue_carte(g.m, k, g.fraicheur);
    const mer::Prevision &a = g.m.activites[k];
    const bool jugee = v.etat == mer::EtatCarte::Jugee;
    const std::uint8_t opa = v.terne ? 160 : LV_OPA_COVER; // quelques minutes : la carte s'atténue
    const std::uint32_t couleur = couleur_verdict(t, v.verdict);

    verre(c, r, jugee ? couleur : t.filet, jugee ? (v.terne ? 120 : 200) : LV_OPA_COVER);
    if (g.appuye == k) {
        dz::rect(c.pc, c.ox + r.x, c.oy + r.y, r.w, r.h, g.d.rayon, t.texte, 30);
    }
    const geo::InterieurCarte in = geo::interieur_carte(r, g.d.petit_cote);

    // Le nom de l'activité, avec son icône.
    icone_activite(c, k, in.icone, t.texte_2, opa);
    ligne(c, gp.titre, copie(mer::nom_activite(static_cast<mer::Activite>(k), g.langue)), in.nom, LV_TEXT_ALIGN_LEFT,
          t.texte, opa);

    // Le verdict, ou un tiret quand il n'y en a pas à montrer ; dessous, pourquoi.
    if (jugee) {
        ligne(c, gp.verdict, copie(mer::nom_verdict(v.verdict, g.langue)), in.verdict, LV_TEXT_ALIGN_LEFT, couleur,
              opa);
    } else {
        tiret(c, in.verdict, LV_TEXT_ALIGN_LEFT, t.texte_3);
    }
    char raison[160] = {};
    if (mer::formater_texte_carte(g.m, k, g.fraicheur, g.langue, raison, sizeof raison) != 0) {
        paragraphe(c, gp.petit, raison, in.raison, 2, t.texte_2, opa);
    }

    // Le meilleur créneau.
    if (jugee) {
        ligne(c, gp.petit, T(mer::Texte::MeilleurCreneau), in.etiquette_creneau, LV_TEXT_ALIGN_LEFT, t.texte_3, opa);
        char creneau[40] = {};
        if (mer::formater_creneau(a.creneau_debut_s, a.creneau_fin_s, g.decalage_s, creneau, sizeof creneau) != 0) {
            ligne(c, gp.texte, creneau, in.creneau, LV_TEXT_ALIGN_LEFT, t.texte, opa);
        } else {
            ligne(c, gp.texte, T(mer::Texte::Aucun), in.creneau, LV_TEXT_ALIGN_LEFT, t.texte_3, opa);
        }
    }

    // Les 24 heures en barres, et leur axe.
    barres(c, in, a, jugee);
    axe(c, in.axe, t.texte_3, opa);

    // Les premiers critères connus : le nom à gauche, la valeur à droite.
    if (jugee) {
        int indices[mer::RANGEES_CARTE] = {};
        const int n = mer::criteres_de_carte(a, indices, mer::RANGEES_CARTE);
        for (int i = 0; i < n; ++i) {
            const mer::Critere_ligne &cl = a.criteres[indices[i]];
            char valeur[48] = {};
            if (mer::formater_valeur(cl, g.langue, valeur, sizeof valeur) == 0) {
                continue;
            }
            const geo::Rect &rg = in.rangees[i];
            const int largeur_valeur = dz::largeur_texte(gp.petit, valeur);
            ligne(c, gp.petit, copie(mer::nom_critere(cl.id, g.langue)),
                  geo::Rect{rg.x, rg.y, rg.w - largeur_valeur - M(8), rg.h}, LV_TEXT_ALIGN_LEFT, t.texte_3, opa);
            ligne(c, gp.petit, valeur, rg, LV_TEXT_ALIGN_RIGHT, t.texte, opa);
        }
    }
}

// ---- la légende ---------------------------------------------------------------------------------------------------------------------

void legende(const Dessin &c) {
    const ThemePrincipal &t = c.t;
    // En paysage la légende tient sur une ligne (les pastilles à gauche, la phrase à droite) ; en portrait elle en a deux.
    const geo::Rect tout = g.d.legende;
    const geo::Rect r = g.d.portrait ? geo::Rect{tout.x, tout.y, tout.w, tout.h / 2} : tout;
    const int rayon = std::max(3, r.h / 4);
    int x = r.x + M(6);
    const mer::Verdict verdicts[3] = {mer::Verdict::Bon, mer::Verdict::Possible, mer::Verdict::Deconseille};
    for (int i = 0; i < 4; ++i) {
        const bool nuit = i == 3;
        const char *mot = nuit ? T(mer::Texte::Nuit) : copie(mer::nom_verdict(verdicts[i], g.langue));
        const int w = dz::largeur_texte(gp.petit, mot);
        if (nuit) {
            dz::disque(c.pc, c.ox + x + rayon, c.oy + r.y + r.h / 2, rayon, t.filet, 110);
        } else {
            dz::disque(c.pc, c.ox + x + rayon, c.oy + r.y + r.h / 2, rayon, couleur_verdict(t, verdicts[i]));
        }
        x += 2 * rayon + M(6);
        ligne(c, gp.petit, mot, geo::Rect{x, r.y, w + M(4), r.h}, LV_TEXT_ALIGN_LEFT, t.texte_2);
        x += w + M(20);
    }
    // La phrase qui dit lire une barre : à droite dans ce qui reste, ou sur la seconde ligne en portrait.
    if (g.d.portrait) {
        ligne(c, gp.petit, T(mer::Texte::Legende), geo::Rect{tout.x + M(6), tout.y + r.h, tout.w - 2 * M(6), r.h},
              LV_TEXT_ALIGN_LEFT, t.texte_3);
    } else {
        const geo::Rect reste{x, r.y, r.x + r.w - M(6) - x, r.h};
        ligne(c, gp.petit, T(mer::Texte::Legende), reste, LV_TEXT_ALIGN_RIGHT, t.texte_3);
    }
}

// ---- la marée -------------------------------------------------------------------------------------------------------------------------

void courbe(const Dessin &c, const geo::Rect &zone) {
    const ThemePrincipal &t = c.t;
    const mer::CourbeMaree cm = mer::courbe_maree(g.m, g.maintenant_s);
    if (!cm.presente) {
        return;
    }
    const std::uint32_t eau = t.metrique[static_cast<int>(visuel::Metrique::PLUIE)];
    const int h_etiquette = dz::hauteur_ligne(gp.petit);
    const geo::Rect pl{zone.x, zone.y + M(6), zone.w, zone.h - h_etiquette - M(6) - M(4)};
    if (pl.w < 48 || pl.h < 8) {
        return;
    }
    auto X = [&](int k) { return pl.x + k * pl.w / (mer::ECHANTILLONS_MAREE - 1); };
    auto Y = [&](int k) { return pl.y + cm.y_pm[k] * pl.h / 1000; };
    const int pied = pl.y + pl.h;
    // Le pied de la courbe.
    dz::rect(c.pc, c.ox + pl.x, c.oy + pied, pl.w, std::max(1, M(2)), 0, t.filet);
    // Le voile sous la courbe, par colonnes de 3 px : la hauteur suit la courbe entre deux points.
    for (int k = cm.premier; k < cm.dernier; ++k) {
        const int xa = X(k);
        const int xb = X(k + 1);
        for (int x = xa; x < xb; x += 3) {
            const int y = Y(k) + (Y(k + 1) - Y(k)) * (x - xa) / std::max(1, xb - xa);
            dz::rect(c.pc, c.ox + x, c.oy + y, std::min(3, xb - x), pied - y, 0, eau, 52);
        }
    }
    // La courbe.
    for (int k = cm.premier; k < cm.dernier; ++k) {
        dz::trait(c.pc, c.ox + X(k), c.oy + Y(k), c.ox + X(k + 1), c.oy + Y(k + 1), std::max(2, M(3)), eau);
    }
    // « Maintenant » : un trait et un point sur la courbe.
    if (cm.repere) {
        const int xr = pl.x + cm.x_pm * pl.w / 1000;
        const int yr = pl.y + cm.y_repere_pm * pl.h / 1000;
        dz::rect(c.pc, c.ox + xr - 1, c.oy + pl.y, 2, pl.h, 1, t.texte, 150);
        dz::disque(c.pc, c.ox + xr, c.oy + yr, M(7), t.texte);
        dz::disque(c.pc, c.ox + xr, c.oy + yr, M(4), eau);
    }
    // Les trois repères d'heure, sous le pied.
    axe(c, geo::Rect{pl.x, pied + M(4), pl.w, h_etiquette}, t.texte_3, LV_OPA_COVER);
}

void panneau_maree(const Dessin &c) {
    const ThemePrincipal &t = c.t;
    const geo::Rect &r = g.d.maree;
    verre(c, r, t.filet, LV_OPA_COVER);
    const geo::InterieurMaree in = geo::interieur_maree(r, g.d.petit_cote);
    ligne(c, gp.texte, T(mer::Texte::Maree), in.titre, LV_TEXT_ALIGN_LEFT, t.texte);
    if (!g.m.valide) {
        return;
    }
    if (g.m.nb_marees == 0) {
        // Aucun modèle de marée près de ce spot : on le dit, sans courbe.
        paragraphe(c, gp.petit, T(mer::Texte::PasDeMaree),
                   geo::Rect{in.prochaine[0].x, in.prochaine[0].y, in.courbe.w, in.courbe.h}, 3, t.texte_3);
        return;
    }
    // Le coefficient estimé de la prochaine pleine mer : une pastille à droite du titre.
    const int ic = mer::coefficient_a_montrer(g.m, g.maintenant_s);
    char coefficient[64] = {};
    if (ic >= 0 &&
        mer::formater_coefficient(g.m.coefficients[ic].valeur, g.langue, coefficient, sizeof coefficient) != 0) {
        const int h = std::min(in.badge.h, M(26));
        const int w = std::min(in.badge.w, dz::largeur_texte(gp.petit, coefficient) + 2 * M(12));
        const int x = in.badge.x + in.badge.w - w;
        const int y = in.badge.y + (in.badge.h - h) / 2;
        dz::rect_bord(c.pc, c.ox + x, c.oy + y, w, h, h / 2, t.accent, 48, t.accent, 200, std::max(1, M(2)));
        ligne(c, gp.petit, coefficient, geo::Rect{x, y, w, h}, LV_TEXT_ALIGN_CENTER, t.texte);
    }
    // Les deux prochaines marées.
    int indices[2] = {};
    const int n = mer::prochaines_marees(g.m, g.maintenant_s, indices);
    for (int j = 0; j < n; ++j) {
        char b[96] = {};
        if (mer::formater_maree(g.m.marees[indices[j]], g.decalage_s, g.langue, b, sizeof b) != 0) {
            ligne(c, gp.texte, b, in.prochaine[j], LV_TEXT_ALIGN_LEFT, t.texte_2);
        }
    }
    courbe(c, in.courbe);
}

// ---- le soleil ------------------------------------------------------------------------------------------------------------------------

void panneau_soleil(const Dessin &c) {
    const ThemePrincipal &t = c.t;
    const geo::Rect &r = g.d.soleil;
    verre(c, r, t.filet, LV_OPA_COVER);
    const geo::InterieurSoleil in = geo::interieur_soleil(r, g.d.petit_cote);
    ligne(c, gp.texte, T(mer::Texte::Soleil), in.titre, LV_TEXT_ALIGN_LEFT, t.texte);
    const std::int64_t instants[2] = {g.m.lever_s, g.m.coucher_s};
    const mer::Texte noms[2] = {mer::Texte::Lever, mer::Texte::Coucher};
    for (int k = 0; k < 2; ++k) {
        ligne(c, gp.petit, T(noms[k]), in.etiquette[k], LV_TEXT_ALIGN_LEFT, t.texte_3);
        char h[16] = {};
        if (g.m.valide && mer::formater_heure(instants[k], g.decalage_s, h, sizeof h) != 0) {
            ligne(c, gp.titre, h, in.valeur[k], LV_TEXT_ALIGN_LEFT, t.accent);
        } else {
            tiret(c, in.valeur[k], LV_TEXT_ALIGN_LEFT, t.texte_3);
        }
    }
    paragraphe(c, gp.petit, T(mer::Texte::Regles), in.avis, 2, t.texte_3);
}

// ---- le pied ----------------------------------------------------------------------------------------------------------------------------

// Le crédit des données de mer (CC BY 4.0 : Open-Meteo, Copernicus Marine, marée EOT20 ; formes longues dans THIRD_PARTY.md) et
// l'avertissement de l'ADR-0018. En paysage, une ligne : le crédit à gauche, l'avertissement à droite ; en portrait, l'un sous l'autre.
void pied(const Dessin &c) {
    const ThemePrincipal &t = c.t;
    const geo::Rect &r = g.d.pied;
    const char *avertissement = T(mer::Texte::PasNavigation);
    if (g.d.portrait) {
        const int h = r.h / 2;
        ligne(c, gp.petit, T(mer::Texte::Credit), geo::Rect{r.x, r.y, r.w, h}, LV_TEXT_ALIGN_LEFT, t.texte_3);
        ligne(c, gp.petit, avertissement, geo::Rect{r.x, r.y + h, r.w, r.h - h}, LV_TEXT_ALIGN_LEFT, t.texte_3);
        return;
    }
    const int w_avertissement = dz::largeur_texte(gp.petit, avertissement);
    ligne(c, gp.petit, avertissement, r, LV_TEXT_ALIGN_RIGHT, t.texte_3);
    ligne(c, gp.petit, T(mer::Texte::Credit), geo::Rect{r.x, r.y, r.w - w_avertissement - M(24), r.h},
          LV_TEXT_ALIGN_LEFT, t.texte_3);
}

// ---- le détail --------------------------------------------------------------------------------------------------------------------------

// Ce qu'on dessine à côté d'un critère : des blocs de points, une pastille de verdict ou un tiret.
void marque(const Dessin &c, const mer::Critere_ligne &cl, const geo::Rect &cellule) {
    const ThemePrincipal &t = c.t;
    const mer::MarqueCritere m = mer::marque_critere(cl);
    const int q = M(15);
    const int ecart = M(4);
    const int y = cellule.y + (cellule.h - q) / 2;
    if (m.marque == mer::Marque::Blocs) {
        for (int j = 0; j < m.total; ++j) {
            const int x = cellule.x + j * (q + ecart);
            if (j < m.plein) {
                dz::rect(c.pc, c.ox + x, c.oy + y, q, q, M(3), couleur_verdict(t, m.verdict));
            } else {
                dz::rect(c.pc, c.ox + x, c.oy + y, q, q, M(3), t.filet, 140);
            }
        }
        char pts[16] = {};
        if (mer::formater_points(cl, pts, sizeof pts) != 0) {
            const int x = cellule.x + m.total * (q + ecart) + M(4);
            ligne(c, gp.petit, pts, geo::Rect{x, cellule.y, cellule.x + cellule.w - x, cellule.h}, LV_TEXT_ALIGN_LEFT,
                  t.texte_2);
        }
    } else if (m.marque == mer::Marque::Pastille) {
        dz::disque(c.pc, c.ox + cellule.x + q / 2, c.oy + y + q / 2, q / 2, couleur_verdict(t, m.verdict));
    } else {
        tiret(c, cellule, LV_TEXT_ALIGN_LEFT, t.texte_3);
    }
}

void detail(const Dessin &c) {
    const ThemePrincipal &t = c.t;
    const int k = g.detail;
    const mer::Prevision &a = g.m.activites[k];
    const geo::Detail &D = g.det;
    const dz::Police &p_cellule = g.d.portrait ? gp.petit : gp.texte;

    // Le voile sur la page, puis le panneau.
    dz::rect(c.pc, c.ox, c.oy, g.ctx.largeur, g.ctx.hauteur, 0, t.fond, 215);
    dz::rect_bord(c.pc, c.ox + D.panneau.x, c.oy + D.panneau.y, D.panneau.w, D.panneau.h, g.d.rayon, fond_carte(t),
                  LV_OPA_COVER, t.filet, LV_OPA_COVER, M(2));

    // En-tête : l'icône, « Surf · détail », « pour 11:00 », la croix.
    icone_activite(c, k, D.icone, t.texte_2, LV_OPA_COVER);
    char titre[120] = {};
    if (mer::formater_titre_detail(static_cast<mer::Activite>(k), g.langue, titre, sizeof titre) != 0) {
        ligne(c, gp.titre, titre, D.titre, LV_TEXT_ALIGN_LEFT, t.texte);
    }
    char heure[64] = {};
    if (mer::formater_heure_detail(a.detail_s, g.decalage_s, g.langue, heure, sizeof heure) != 0) {
        ligne(c, gp.petit, heure, D.sous_titre, LV_TEXT_ALIGN_LEFT, t.texte_3);
    }
    {
        const int cx = c.ox + D.fermer.x + D.fermer.w / 2;
        const int cy = c.oy + D.titre.y + D.titre.h / 2;
        const int bras = std::max(4, D.fermer.w * 17 / 100);
        if (g.appuye == CIBLE_CROIX) {
            dz::disque(c.pc, cx, cy, D.fermer.w / 2 - M(4), t.texte, 36);
        }
        dz::trait(c.pc, cx - bras, cy - bras, cx + bras, cy + bras, std::max(2, M(4)), t.texte);
        dz::trait(c.pc, cx - bras, cy + bras, cx + bras, cy - bras, std::max(2, M(4)), t.texte);
    }

    // Le verdict et la note.
    if (a.verdict != mer::Verdict::Inconnu) {
        ligne(c, gp.verdict, copie(mer::nom_verdict(a.verdict, g.langue)), D.verdict, LV_TEXT_ALIGN_LEFT,
              couleur_verdict(t, a.verdict));
    } else {
        tiret(c, D.verdict, LV_TEXT_ALIGN_LEFT, t.texte_3);
    }
    char note[64] = {};
    if (mer::formater_note(a.points, a.sur, g.langue, note, sizeof note) != 0) {
        ligne(c, gp.texte, note, D.note, LV_TEXT_ALIGN_RIGHT, t.texte_2);
    }

    // Les colonnes : critère, valeur, note, lecture.
    const mer::Texte colonnes[geo::COLONNES_DETAIL] = {mer::Texte::ColCritere, mer::Texte::ColValeur,
                                                       mer::Texte::ColNote, mer::Texte::ColLecture};
    for (int j = 0; j < geo::COLONNES_DETAIL; ++j) {
        ligne(c, gp.petit, T(colonnes[j]), D.entetes[j], LV_TEXT_ALIGN_LEFT, t.texte_3);
    }
    dz::rect(c.pc, c.ox + D.entetes[0].x, c.oy + D.entetes[0].bas(),
             D.entetes[geo::COLONNES_DETAIL - 1].droite() - D.entetes[0].x, 1, 0, t.filet);
    const int n = std::min<int>(a.nb_criteres, mer::CRITERES_MAX);
    for (int i = 0; i < n; ++i) {
        const mer::Critere_ligne &cl = a.criteres[i];
        ligne(c, p_cellule, copie(mer::nom_critere(cl.id, g.langue)), D.cellules[i][0], LV_TEXT_ALIGN_LEFT, t.texte);
        char valeur[48] = {};
        if (mer::valeur_montrable(cl) && mer::formater_valeur(cl, g.langue, valeur, sizeof valeur) != 0) {
            ligne(c, p_cellule, valeur, D.cellules[i][1], LV_TEXT_ALIGN_LEFT, t.texte);
        } else {
            tiret(c, D.cellules[i][1], LV_TEXT_ALIGN_LEFT, t.texte_3);
        }
        marque(c, cl, D.cellules[i][2]);
        const char *lecture = copie(mer::texte_etat(cl.id, cl.etat, g.langue));
        if (lecture[0] != '\0') {
            ligne(c, p_cellule, lecture, D.cellules[i][3], LV_TEXT_ALIGN_LEFT,
                  cl.verdict != mer::Verdict::Inconnu ? couleur_verdict(t, cl.verdict) : t.texte);
        } else {
            tiret(c, D.cellules[i][3], LV_TEXT_ALIGN_LEFT, t.texte_3);
        }
        if (i + 1 < n) {
            const geo::Rect &fin = D.cellules[i][0];
            dz::rect(c.pc, c.ox + fin.x, c.oy + fin.bas() - 1, D.cellules[i][geo::COLONNES_DETAIL - 1].droite() - fin.x,
                     1, 0, t.filet, 90);
        }
    }
    paragraphe(c, gp.petit, T(mer::Texte::Regles), D.avis, 2, t.texte_3);
}

// ---- dessin et appui --------------------------------------------------------------------------------------------------------------------

void sur_dessin(lv_event_t *evenement) {
    lv_layer_t *couche = lv_event_get_layer(evenement);
    lv_obj_t *objet = lv_event_get_target_obj(evenement);
    lv_area_t a;
    lv_obj_get_coords(objet, &a);
    const Dessin c = {{couche, g.ctx.theme}, *g.ctx.theme, a.x1, a.y1};
    entete(c);
    bandeau(c);
    for (int k = 0; k < mer::NB_ACTIVITES; ++k) {
        carte(c, k);
    }
    legende(c);
    panneau_maree(c);
    panneau_soleil(c);
    pied(c);
    if (g.detail >= 0) {
        detail(c);
    }
}

// Ce que désigne le doigt posé en (x, y), relatifs à l'objet.
int cible(int x, int y) {
    if (g.detail >= 0) {
        if (geo::contient(g.det.fermer, x, y)) {
            return CIBLE_CROIX;
        }
        return geo::contient(g.det.panneau, x, y) ? CIBLE_AUCUNE : CIBLE_VOILE;
    }
    if (geo::contient(g.d.fermer, x, y)) {
        return CIBLE_FERMER;
    }
    const int k = geo::carte_en(g.d, x, y);
    return k >= 0 && mer::vue_carte(g.m, k, g.fraicheur).ouvrable ? k : CIBLE_AUCUNE;
}

int cible_du_doigt() {
    lv_indev_t *doigt = lv_indev_active();
    if (doigt == nullptr || g.zone == nullptr) {
        return CIBLE_AUCUNE;
    }
    lv_point_t p;
    lv_indev_get_point(doigt, &p);
    lv_area_t a;
    lv_obj_get_coords(g.zone, &a);
    return cible(p.x - a.x1, p.y - a.y1);
}

void redessiner() {
    if (g.zone != nullptr) {
        lv_obj_invalidate(g.zone);
    }
}

void ouvrir_detail(int activite) {
    // Le panneau a la hauteur de ses critères : pas de grand vide sous un détail court.
    g.det = geo::disposer_detail(g.ctx.largeur, g.ctx.hauteur, g.m.activites[activite].nb_criteres);
    g.detail = activite;
    g.appuye = CIBLE_AUCUNE;
    redessiner();
}

void sur_toucher(lv_event_t *evenement) {
    switch (lv_event_get_code(evenement)) {
    case LV_EVENT_PRESSED:
        g.appuye = cible_du_doigt();
        redessiner();
        break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        if (g.appuye != CIBLE_AUCUNE) {
            g.appuye = CIBLE_AUCUNE;
            redessiner();
        }
        break;
    case LV_EVENT_CLICKED: {
        const int c = cible_du_doigt();
        if (c == CIBLE_FERMER) {
            if (g.ctx.fermer != nullptr) {
                g.ctx.fermer();
            }
        } else if (c == CIBLE_CROIX || c == CIBLE_VOILE) {
            g.detail = -1;
            redessiner();
        } else if (c >= 0 && c < mer::NB_ACTIVITES) {
            ouvrir_detail(c);
        }
        break;
    }
    case LV_EVENT_DELETE:
        // L'écran est supprimé : plus rien à redessiner. Seulement si c'est LA zone de la page courante : la suppression d'une page
        // fermée (lv_obj_delete_async) peut arriver après l'ouverture de la suivante, qu'elle ne doit pas éteindre.
        if (lv_event_get_target_obj(evenement) == g.zone) {
            g.zone = nullptr;
            g.detail = -1;
        }
        break;
    default:
        break;
    }
}

dz::Police creer_police(const ContexteMer &ctx, int px) {
    dz::Police p;
    p.px = px;
    p.f = lv_tiny_ttf_create_data(ctx.ttf, ctx.ttf_taille, px);
    return p;
}

bool preparer_polices(const ContexteMer &ctx, const geo::Disposition &d) {
    if (gp.ttf == ctx.ttf && gp.petit_cote == d.petit_cote && gp.petit.f != nullptr) {
        return true;
    }
    Polices neuves;
    neuves.ttf = ctx.ttf;
    neuves.petit_cote = d.petit_cote;
    neuves.verdict = creer_police(ctx, d.polices.verdict);
    neuves.titre = creer_police(ctx, d.polices.titre);
    neuves.texte = creer_police(ctx, d.polices.texte);
    neuves.petit = creer_police(ctx, d.polices.petit);
    if (neuves.verdict.f == nullptr || neuves.titre.f == nullptr || neuves.texte.f == nullptr ||
        neuves.petit.f == nullptr) {
        return false;
    }
    gp = neuves;
    return true;
}

} // namespace

bool ui_mer_creer(const ContexteMer &ctx, const DonneesMer &donnees) {
    if (ctx.racine == nullptr || ctx.ttf == nullptr || ctx.theme == nullptr || donnees.mer == nullptr) {
        return false;
    }
    const geo::Disposition d = geo::disposer(ctx.largeur, ctx.hauteur);
    if (!d.valide || !preparer_polices(ctx, d)) {
        return false;
    }
    g = Page{};
    g.ctx = ctx;
    g.m = *donnees.mer;
    g.maintenant_s = donnees.maintenant_s;
    g.decalage_s = donnees.decalage_s;
    g.langue = i18n::langue_courante();
    g.fraicheur = mer::fraicheur(g.m, g.maintenant_s);
    g.d = d;

    const ThemePrincipal &t = *ctx.theme;
    lv_obj_set_style_bg_color(ctx.racine, dz::couleur(t.fond), 0);
    lv_obj_set_style_bg_opa(ctx.racine, LV_OPA_COVER, 0);
    lv_obj_remove_flag(ctx.racine, LV_OBJ_FLAG_SCROLLABLE);

    g.zone = lv_obj_create(ctx.racine);
    lv_obj_remove_style_all(g.zone);
    lv_obj_set_size(g.zone, ctx.largeur, ctx.hauteur);
    lv_obj_set_pos(g.zone, 0, 0);
    lv_obj_add_flag(g.zone, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(g.zone, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g.zone, sur_dessin, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(g.zone, sur_toucher, LV_EVENT_ALL, nullptr);
    return true;
}

bool ui_mer_detail(int activite) {
    if (g.zone == nullptr) {
        return false;
    }
    if (activite < 0) {
        g.detail = -1;
        redessiner();
        return true;
    }
    if (activite >= mer::NB_ACTIVITES || !mer::vue_carte(g.m, activite, g.fraicheur).ouvrable) {
        return false;
    }
    ouvrir_detail(activite);
    return true;
}

} // namespace ecran
