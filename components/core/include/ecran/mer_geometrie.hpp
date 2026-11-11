// Noyau pur : la MISE EN PAGE de la page mer, en pixels de l'écran réel. Où est chaque zone, chaque carte et ce qu'elle contient, pour un
// écran paysage (1280 x 720 du Tab5, 1024 x 600 de la 7B) ou portrait (720 x 1280). C++17 standard, aucun include ESP-IDF ni LVGL, aucun
// flottant, évaluable à la compilation (test/host/test_mer_geometrie.cpp).
//
// Pourquoi ici : la page dessine tout dans un seul événement de dessin (comme l'écran principal), donc rien ne vérifie « ça rentre » sauf
// ce fichier. Les tests tiennent trois choses, sur les trois tailles d'écran : chaque zone est DANS son parent, deux zones sœurs ne se
// chevauchent pas, et chaque zone tactile a une taille qu'un doigt atteint. Ce qu'ils ne tiennent PAS : qu'un texte tienne dans sa zone
// (il dépend de la police et de la langue : on le regarde sur le rendu PC, tools/rendu, planches de docs/captures/ecran/).
//
// Tout est dessiné pour une référence de 720 pixels sur le petit côté, puis mis à l'échelle (`mis`) : 1280 x 720 -> x1, 1024 x 600 ->
// x0,83, 720 x 1280 -> x1. Les largeurs se partagent la largeur réelle de l'écran.
#pragma once

#include <cstdint>

#include "ecran/mer_page.hpp"
#include "ecran/mer_trame.hpp"

namespace ecran::mer::geo {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    constexpr int droite() const noexcept { return x + w; }
    constexpr int bas() const noexcept { return y + h; }
};

// `a` est entièrement dans `b` (et n'est pas vide).
constexpr bool dans(const Rect &a, const Rect &b) noexcept {
    return a.w > 0 && a.h > 0 && a.x >= b.x && a.y >= b.y && a.droite() <= b.droite() && a.bas() <= b.bas();
}

// Deux rectangles qui ne se chevauchent pas (se toucher par un bord n'est pas se chevaucher).
constexpr bool separes(const Rect &a, const Rect &b) noexcept {
    return a.droite() <= b.x || b.droite() <= a.x || a.bas() <= b.y || b.bas() <= a.y;
}

constexpr bool contient(const Rect &r, int px, int py) noexcept {
    return px >= r.x && px < r.droite() && py >= r.y && py < r.bas();
}

// Un nombre de pixels de la référence (720 sur le petit côté) mis à l'échelle de l'écran, arrondi, jamais moins de 1 (0 reste 0).
constexpr int mis(int px, int petit_cote) noexcept {
    if (px <= 0)
        return 0;
    const int v = (px * petit_cote + 360) / 720;
    return v < 1 ? 1 : v;
}

// Tailles nominales des polices (pixels), à créer une fois par la page.
struct Polices {
    int verdict = 0; // « Bonne », « Déconseillée »
    int titre = 0;   // le nom du spot, le nom d'une activité, les heures
    int texte = 0;   // les lignes de la marée et du soleil, les cellules du détail
    int petit = 0;   // raison, critères d'une carte, axe, légende, avis
};

inline constexpr int PETIT_COTE_MIN = 320; // en dessous : pas de page (aucun de nos écrans)

// ---- l'intérieur d'une carte d'activité --------------------------------------------------------------------------------------------
struct InterieurCarte {
    Rect icone;
    Rect nom;
    Rect verdict;
    Rect raison; // deux lignes
    Rect etiquette_creneau;
    Rect creneau;
    Rect bandes;       // les 24 barres : `pas_barre` x 24 de large
    int pas_barre = 0; // pixels entre le début d'une barre et celui de la suivante (la barre fait un pixel de moins)
    Rect axe;          // même largeur que les bandes : trois étiquettes (début, milieu, fin)
    Rect rangees[RANGEES_CARTE]; // « Houle 1,8 m » : les premiers critères connus
};

// Les mesures d'une carte à l'échelle de l'écran : une seule source pour le dessin de l'intérieur et pour la hauteur minimale que la
// disposition lui garantit.
struct MesuresCarte {
    int pad = 0;
    int g = 0;
    int h_nom = 0;
    int h_verdict = 0;
    int h_raison = 0;
    int h_etiquette = 0;
    int h_creneau = 0;
    int h_bandes = 0;
    int h_axe = 0;
    int h_rangee = 0;
    int sous_bandes = 0; // l'écart entre les barres et leur axe
    int h_milieu = 0;    // l'étiquette du créneau, le créneau, les barres et l'axe
    int hauteur_min = 0; // en dessous, deux zones se recouvrent
};

constexpr MesuresCarte mesures_carte(int petit_cote) noexcept {
    MesuresCarte r;
    const int m = petit_cote;
    r.pad = mis(12, m);
    r.g = mis(4, m);
    r.h_nom = mis(30, m);
    r.h_verdict = mis(52, m);
    r.h_raison = mis(44, m);
    r.h_etiquette = mis(18, m);
    r.h_creneau = mis(32, m);
    r.h_bandes = mis(30, m);
    r.h_axe = mis(18, m);
    r.h_rangee = mis(24, m);
    r.sous_bandes = mis(2, m);
    r.h_milieu = r.h_etiquette + r.h_creneau + r.g + r.h_bandes + r.sous_bandes + r.h_axe;
    r.hauteur_min =
        2 * r.pad + r.h_nom + r.g + r.h_verdict + r.g + r.h_raison + r.h_milieu + RANGEES_CARTE * r.h_rangee;
    return r;
}

constexpr InterieurCarte interieur_carte(const Rect &carte, int petit_cote) noexcept {
    InterieurCarte c;
    const int m = petit_cote;
    const MesuresCarte k = mesures_carte(m);
    const int pad = k.pad;
    const int g = k.g;
    const int h_nom = k.h_nom;
    const int h_verdict = k.h_verdict;
    const int h_raison = k.h_raison;
    const int h_etiquette = k.h_etiquette;
    const int h_creneau = k.h_creneau;
    const int h_bandes = k.h_bandes;
    const int h_axe = k.h_axe;
    const int h_rangee = k.h_rangee;
    const Rect in{carte.x + pad, carte.y + pad, carte.w - 2 * pad, carte.h - 2 * pad};

    // En haut : le nom (avec son icône), le verdict, la raison.
    int y = in.y;
    c.icone = Rect{in.x, y, h_nom, h_nom};
    c.nom = Rect{in.x + h_nom + mis(8, m), y, in.w - h_nom - mis(8, m), h_nom};
    y += h_nom + g;
    c.verdict = Rect{in.x, y, in.w, h_verdict};
    y += h_verdict + g;
    c.raison = Rect{in.x, y, in.w, h_raison};
    const int haut_fin = y + h_raison;

    // En bas : les critères, collés au bas de la carte.
    const int bas_debut = in.bas() - RANGEES_CARTE * h_rangee;
    for (int k = 0; k < RANGEES_CARTE; ++k)
        c.rangees[k] = Rect{in.x, bas_debut + k * h_rangee, in.w, h_rangee};

    // Au milieu : le meilleur créneau et les barres, centrés dans ce qui reste.
    const int libre = bas_debut - haut_fin - k.h_milieu;
    int ym = haut_fin + (libre > 0 ? libre / 2 : 0);
    c.etiquette_creneau = Rect{in.x, ym, in.w, h_etiquette};
    ym += h_etiquette;
    c.creneau = Rect{in.x, ym, in.w, h_creneau};
    ym += h_creneau + g;
    c.pas_barre = in.w / HEURES;
    const int largeur_bandes = c.pas_barre * HEURES;
    c.bandes = Rect{in.x + (in.w - largeur_bandes) / 2, ym, largeur_bandes, h_bandes};
    ym += h_bandes + k.sous_bandes;
    c.axe = Rect{c.bandes.x, ym, largeur_bandes, h_axe};
    return c;
}

// ---- l'intérieur du panneau de marée -----------------------------------------------------------------------------------------------
struct InterieurMaree {
    Rect titre;
    Rect badge;        // « Coefficient 92 (estimé) », à droite du titre
    Rect prochaine[2]; // les deux prochaines marées, côte à côte
    Rect courbe;       // la courbe des 24 heures, `courbe_maree` y est mise à l'échelle
};

constexpr InterieurMaree interieur_maree(const Rect &panneau, int petit_cote) noexcept {
    InterieurMaree r;
    const int m = petit_cote;
    const int pad = mis(14, m);
    const int g = mis(6, m);
    const int h_titre = mis(28, m);
    const int h_ligne = mis(22, m);
    const Rect in{panneau.x + pad, panneau.y + pad, panneau.w - 2 * pad, panneau.h - 2 * pad};
    const int largeur_titre = in.w * 35 / 100;
    r.titre = Rect{in.x, in.y, largeur_titre, h_titre};
    r.badge = Rect{in.x + largeur_titre + g, in.y, in.w - largeur_titre - g, h_titre};
    const int y2 = in.y + h_titre + g;
    const int moitie = (in.w - g) / 2;
    r.prochaine[0] = Rect{in.x, y2, moitie, h_ligne};
    r.prochaine[1] = Rect{in.x + moitie + g, y2, in.w - moitie - g, h_ligne};
    const int y3 = y2 + h_ligne + g;
    r.courbe = Rect{in.x, y3, in.w, in.bas() - y3};
    return r;
}

// ---- l'intérieur du panneau du soleil -------------------------------------------------------------------------------------------------
struct InterieurSoleil {
    Rect titre;
    Rect etiquette[2]; // « Lever », « Coucher »
    Rect valeur[2];    // « 07:58 », « 17:46 »
    Rect avis;         // « Règles de Vigie, pas pour la sécurité »
};

constexpr InterieurSoleil interieur_soleil(const Rect &panneau, int petit_cote) noexcept {
    InterieurSoleil r;
    const int m = petit_cote;
    const int pad = mis(14, m);
    const int g = mis(6, m);
    const int h_titre = mis(28, m);
    const int h_etiquette = mis(20, m);
    const int h_valeur = mis(36, m);
    const Rect in{panneau.x + pad, panneau.y + pad, panneau.w - 2 * pad, panneau.h - 2 * pad};
    r.titre = Rect{in.x, in.y, in.w, h_titre};
    const int y1 = in.y + h_titre + g;
    const int moitie = (in.w - g) / 2;
    for (int k = 0; k < 2; ++k) {
        const int x = k == 0 ? in.x : in.x + moitie + g;
        const int w = k == 0 ? moitie : in.w - moitie - g;
        r.etiquette[k] = Rect{x, y1, w, h_etiquette};
        r.valeur[k] = Rect{x, y1 + h_etiquette, w, h_valeur};
    }
    const int y2 = y1 + h_etiquette + h_valeur + g;
    r.avis = Rect{in.x, y2, in.w, in.bas() - y2};
    return r;
}

// ---- la page -----------------------------------------------------------------------------------------------------------------------
struct Disposition {
    bool valide = false;
    bool portrait = false;
    int petit_cote = 0; // le petit côté de l'écran : l'échelle de tout le reste
    int marge = 0;
    int ecart = 0;
    int rayon = 0; // arrondi des cartes et des panneaux
    Polices polices;
    Rect ecran;
    Rect entete;
    Rect titre;
    Rect fermer;
    Rect bandeau;
    Rect cartes[NB_ACTIVITES]; // dans l'ordre des activités : surf, kite, bateau, pêche à pied
    Rect legende;
    Rect maree;
    Rect soleil;
    // Le pied : le crédit des données de mer (CC BY 4.0) et « pas pour la navigation » (ADR-0018). Une ligne en paysage (crédit à gauche,
    // avertissement à droite), deux en portrait. Il descend dans la marge du bas (`marge_bas`, plus petite que `marge`) : c'est un texte
    // sans cadre, il n'a pas besoin de la marge d'un panneau, et les cartes ne perdent que ce qui ne tenait pas dans cette marge.
    Rect pied;
    int marge_bas = 0;
};

constexpr Disposition disposer(int largeur, int hauteur) noexcept {
    Disposition d;
    const int petit = largeur < hauteur ? largeur : hauteur;
    if (petit < PETIT_COTE_MIN)
        return d;
    const int m = petit;
    d.portrait = hauteur > largeur;
    d.petit_cote = m;
    d.marge = mis(16, m);
    d.ecart = mis(8, m);
    d.rayon = mis(18, m);
    d.polices = Polices{mis(44, m), mis(30, m), mis(22, m), mis(18, m)};
    d.ecran = Rect{0, 0, largeur, hauteur};

    const int h_entete = mis(60, m);
    const int h_bandeau = mis(44, m);
    const int h_legende = mis(26, m);
    const int h_ligne_pied = mis(22, m); // une ligne de la petite police (18 à l'échelle de 720)
    const int ecart_pied = mis(2, m);
    const int largeur_utile = largeur - 2 * d.marge;
    int y = d.marge;
    d.entete = Rect{d.marge, y, largeur_utile, h_entete};
    // La zone tactile de « Fermer » prend toute la hauteur de l'en-tête (un doigt la trouve sans viser) ; le bouton dessiné est plus bas.
    const int w_fermer = mis(170, m);
    d.fermer = Rect{d.entete.droite() - w_fermer, d.entete.y, w_fermer, h_entete};
    d.titre = Rect{d.entete.x, d.entete.y, d.fermer.x - d.ecart - d.entete.x, h_entete};
    y += h_entete + d.ecart;
    d.bandeau = Rect{d.marge, y, largeur_utile, h_bandeau};
    y += h_bandeau + d.ecart;
    const int y_cartes = y;
    d.marge_bas = mis(4, m);
    const int h_pied = d.portrait ? 2 * h_ligne_pied : h_ligne_pied;
    d.pied = Rect{d.marge, hauteur - d.marge_bas - h_pied, largeur_utile, h_pied};
    const int bas_panneaux = d.pied.y - ecart_pied; // le bas de la marée et du soleil

    if (!d.portrait) {
        // Paysage : quatre cartes côte à côte ; marée (deux tiers) et soleil (un tiers) en bas.
        const int h_bas = mis(170, m);
        d.maree = Rect{d.marge, bas_panneaux - h_bas, (largeur_utile - d.ecart) * 2 / 3, h_bas};
        d.soleil =
            Rect{d.maree.droite() + d.ecart, d.maree.y, largeur_utile - (d.maree.droite() + d.ecart - d.marge), h_bas};
        d.legende = Rect{d.marge, d.maree.y - d.ecart - h_legende, largeur_utile, h_legende};
        const int h_cartes = d.legende.y - d.ecart - y_cartes;
        if (h_cartes < mesures_carte(m).hauteur_min)
            return Disposition{}; // écran trop plat : l'intérieur d'une carte ne tiendrait pas, pas de page plutôt qu'une page qui se recouvre
        const int w_carte = (largeur_utile - 3 * d.ecart) / 4;
        for (int i = 0; i < NB_ACTIVITES; ++i)
            d.cartes[i] = Rect{d.marge + i * (w_carte + d.ecart), y_cartes, w_carte, h_cartes};
    } else {
        // Portrait : quatre cartes en deux rangées de deux ; marée puis soleil, chacun sur toute la largeur.
        // Les cartes gardent d'abord leur hauteur minimale ; la marée et le soleil prennent ce qui reste, sans dépasser 170 (à l'échelle de 720)
        // ni descendre sous 150 (la courbe de marée perdrait sa lisibilité).
        const int h_panneau_max = mis(170, m);
        const int h_panneau_min = mis(150, m);
        const int pour_les_panneaux =
            bas_panneaux - y_cartes - 2 * mesures_carte(m).hauteur_min - 4 * d.ecart - 2 * h_legende;
        int h_panneau = pour_les_panneaux / 2;
        if (h_panneau > h_panneau_max)
            h_panneau = h_panneau_max;
        if (h_panneau < h_panneau_min)
            return Disposition{}; // écran trop plat : pas de page plutôt qu'une page qui se recouvre
        d.soleil = Rect{d.marge, bas_panneaux - h_panneau, largeur_utile, h_panneau};
        d.maree = Rect{d.marge, d.soleil.y - d.ecart - h_panneau, largeur_utile, h_panneau};
        // Deux lignes : les pastilles de verdict, puis la phrase qui dit lire une barre (elle ne tient pas à côté sur 720 px).
        d.legende = Rect{d.marge, d.maree.y - d.ecart - 2 * h_legende, largeur_utile, 2 * h_legende};
        const int h_zone = d.legende.y - d.ecart - y_cartes;
        const int h_carte = (h_zone - d.ecart) / 2;
        const int w_carte = (largeur_utile - d.ecart) / 2;
        for (int i = 0; i < NB_ACTIVITES; ++i)
            d.cartes[i] = Rect{d.marge + (i % 2) * (w_carte + d.ecart), y_cartes + (i / 2) * (h_carte + d.ecart),
                               w_carte, h_carte};
    }
    d.valide = true;
    return d;
}

// La carte touchée en (px, py), ou -1.
constexpr int carte_en(const Disposition &d, int px, int py) noexcept {
    if (!d.valide)
        return -1;
    for (int i = 0; i < NB_ACTIVITES; ++i)
        if (contient(d.cartes[i], px, py))
            return i;
    return -1;
}

// ---- le détail d'une activité (par-dessus la page) -----------------------------------------------------------------------------------
inline constexpr int COLONNES_DETAIL = 4; // critère, valeur, points, lecture

struct Detail {
    bool valide = false;
    Rect panneau;
    Rect icone;
    Rect titre;
    Rect sous_titre;
    Rect fermer; // la croix
    Rect verdict;
    Rect note;
    Rect entetes[COLONNES_DETAIL];
    Rect cellules[CRITERES_MAX][COLONNES_DETAIL];
    Rect avis;
};

// `rangees` : combien de critères le détail montre (1 à CRITERES_MAX) ; le panneau est juste assez haut pour eux, centré sur l'écran. Les
// cellules des rangées qu'on ne montre pas restent vides (largeur et hauteur nulles).
constexpr Detail disposer_detail(int largeur, int hauteur, int rangees = CRITERES_MAX) noexcept {
    Detail t;
    const int petit = largeur < hauteur ? largeur : hauteur;
    if (petit < PETIT_COTE_MIN)
        return t;
    if (rangees < 1)
        rangees = 1;
    if (rangees > CRITERES_MAX)
        rangees = CRITERES_MAX;
    const int m = petit;
    const bool portrait = hauteur > largeur;
    const int mx = mis(portrait ? 16 : 40, m);
    const int my = mis(30, m);
    const int pad = mis(22, m);
    const int g = mis(6, m);
    const int h_rangee = mis(46, m);
    const int h_avis = mis(56, m);
    // De haut en bas : la ligne d'en-tête (44), le sous-titre (24), le verdict (56), les titres de colonnes (24), les rangées, l'avis.
    const int h_contenu =
        mis(44, m) + mis(24, m) + g + mis(56, m) + mis(10, m) + mis(24, m) + g + rangees * h_rangee + g + h_avis;
    int h = h_contenu + 2 * pad;
    if (h > hauteur - 2 * my)
        h = hauteur - 2 * my;
    t.panneau = Rect{mx, (hauteur - h) / 2, largeur - 2 * mx, h};
    const Rect in{t.panneau.x + pad, t.panneau.y + pad, t.panneau.w - 2 * pad, t.panneau.h - 2 * pad};
    const int h_tete = mis(44, m);
    t.icone = Rect{in.x, in.y, h_tete, h_tete};
    // La croix se dessine dans l'angle ; sa zone tactile est un carré plus grand, collé au coin du panneau (un doigt la trouve sans viser).
    const int cote_croix = mis(64, m);
    t.fermer = Rect{t.panneau.droite() - cote_croix, t.panneau.y, cote_croix, cote_croix};
    t.titre = Rect{t.icone.droite() + mis(10, m), in.y, t.fermer.x - g - (t.icone.droite() + mis(10, m)), h_tete};
    int y = in.y + h_tete;
    const int h_sous = mis(24, m);
    t.sous_titre = Rect{t.titre.x, y, t.titre.w, h_sous};
    y += h_sous + g;
    const int h_verdict = mis(56, m);
    const int w_verdict = in.w * 45 / 100;
    t.verdict = Rect{in.x, y, w_verdict, h_verdict};
    t.note = Rect{in.x + w_verdict, y, in.w - w_verdict, h_verdict};
    y += h_verdict + mis(10, m);

    // Quatre colonnes : critère, valeur, points, lecture.
    const int gc = mis(10, m);
    const int parts[COLONNES_DETAIL] = {30, 22, 20, 28};
    const int utile = in.w - (COLONNES_DETAIL - 1) * gc;
    int x = in.x;
    int cx[COLONNES_DETAIL] = {};
    int cw[COLONNES_DETAIL] = {};
    for (int c = 0; c < COLONNES_DETAIL; ++c) {
        cx[c] = x;
        cw[c] = c == COLONNES_DETAIL - 1 ? in.droite() - x : utile * parts[c] / 100;
        x += cw[c] + gc;
    }
    const int h_entetes = mis(24, m);
    for (int c = 0; c < COLONNES_DETAIL; ++c)
        t.entetes[c] = Rect{cx[c], y, cw[c], h_entetes};
    y += h_entetes + g;
    for (int k = 0; k < rangees; ++k) {
        for (int c = 0; c < COLONNES_DETAIL; ++c)
            t.cellules[k][c] = Rect{cx[c], y, cw[c], h_rangee};
        y += h_rangee;
    }
    y += g;
    t.avis = Rect{in.x, y, in.w, in.bas() - y};
    t.valide = true;
    return t;
}

} // namespace ecran::mer::geo
