// Tests du noyau pur : la mise en page de la page mer (mer_geometrie.hpp). Pour chaque taille d'écran qu'on vise, chaque zone est dans son
// parent, deux zones sœurs ne se chevauchent pas, les zones tactiles sont assez grandes pour un doigt. Ce qu'ils ne disent PAS : qu'un
// texte tienne dans sa zone (cela dépend de la police et de la langue : planches de tools/rendu, docs/captures/ecran/).
#include <cstddef>
#include <cstdint>

#include "ecran/mer_geometrie.hpp"
#include "mini.hpp"

using namespace ecran::mer;
using namespace ecran::mer::geo;

namespace {

struct Taille {
    int w;
    int h;
};

// Les trois écrans visés : Tab5 en paysage, Waveshare 7B, Tab5 en portrait.
constexpr Taille VISEES[] = {{1280, 720}, {1024, 600}, {720, 1280}};
// D'autres tailles qu'une carte pourrait avoir un jour : la page ne doit pas se mettre à se chevaucher.
constexpr Taille AUTRES[] = {{800, 480}, {1366, 768}, {1920, 1080}, {600, 1024}, {1080, 1920}};

constexpr int NB_VISEES = static_cast<int>(sizeof VISEES / sizeof VISEES[0]);
constexpr int NB_AUTRES = static_cast<int>(sizeof AUTRES / sizeof AUTRES[0]);

constexpr Taille taille(int i) { return i < NB_VISEES ? VISEES[i] : AUTRES[i - NB_VISEES]; }
constexpr int NB_TAILLES = NB_VISEES + NB_AUTRES;

// Deux à deux, aucune de ces zones n'en chevauche une autre.
template <std::size_t N> constexpr bool toutes_separees(const Rect (&z)[N]) {
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = i + 1; j < N; ++j)
            if (!separes(z[i], z[j]))
                return false;
    return true;
}

template <std::size_t N> constexpr bool toutes_dans(const Rect (&z)[N], const Rect &parent) {
    for (const Rect &r : z)
        if (!dans(r, parent))
            return false;
    return true;
}

// ---- les outils de base --------------------------------------------------------------------------------------------------------

constexpr int les_rectangles_se_comparent_comme_on_s_y_attend() {
    constexpr Rect grand{10, 20, 100, 50};
    VERIFIER(grand.droite() == 110 && grand.bas() == 70);
    VERIFIER(dans(Rect{10, 20, 100, 50}, grand));  // le même
    VERIFIER(dans(Rect{11, 21, 98, 48}, grand));   // à l'intérieur
    VERIFIER(!dans(Rect{9, 20, 100, 50}, grand));  // dépasse à gauche
    VERIFIER(!dans(Rect{10, 19, 100, 50}, grand)); // dépasse en haut
    VERIFIER(!dans(Rect{10, 20, 101, 50}, grand)); // dépasse à droite
    VERIFIER(!dans(Rect{10, 20, 100, 51}, grand)); // dépasse en bas
    VERIFIER(!dans(Rect{20, 30, 0, 10}, grand));   // vide : ne compte pas comme « dedans »
    VERIFIER(!dans(Rect{20, 30, 10, 0}, grand));
    // Se toucher par un bord n'est pas se chevaucher ; un pixel de recouvrement, si.
    VERIFIER(separes(Rect{0, 0, 10, 10}, Rect{10, 0, 10, 10}));
    VERIFIER(separes(Rect{0, 0, 10, 10}, Rect{0, 10, 10, 10}));
    VERIFIER(!separes(Rect{0, 0, 10, 10}, Rect{9, 0, 10, 10}));
    VERIFIER(!separes(Rect{0, 0, 10, 10}, Rect{0, 9, 10, 10}));
    VERIFIER(!separes(Rect{0, 0, 10, 10}, Rect{2, 2, 3, 3})); // l'un dans l'autre
    // Un point : le bord haut-gauche est dedans, le bord bas-droit est dehors.
    VERIFIER(contient(grand, 10, 20) && contient(grand, 109, 69));
    VERIFIER(!contient(grand, 110, 69) && !contient(grand, 109, 70) && !contient(grand, 9, 20) &&
             !contient(grand, 10, 19));
    return 0;
}

constexpr int l_echelle_suit_le_petit_cote_et_ne_descend_jamais_sous_un_pixel() {
    VERIFIER(mis(0, 720) == 0 && mis(-5, 720) == 0); // rien reste rien
    VERIFIER(mis(44, 720) == 44 && mis(1, 720) == 1);
    VERIFIER(mis(44, 600) == 37);    // 44 x 600 / 720 = 36,7
    VERIFIER(mis(18, 600) == 15);    // 15,0
    VERIFIER(mis(1, 320) == 1);      // 0,44 arrondi à 0 : relevé à 1
    VERIFIER(mis(2, 320) == 1);      // 0,89
    VERIFIER(mis(100, 1440) == 200); // l'échelle monte aussi
    return 0;
}

// ---- la page -------------------------------------------------------------------------------------------------------------------

constexpr int un_ecran_trop_petit_n_a_pas_de_page() {
    VERIFIER(!disposer(0, 0).valide && !disposer(319, 1000).valide && !disposer(1000, 319).valide);
    VERIFIER(disposer(320, 320).valide);
    // Une page invalide ne touche rien : aucune carte.
    const Disposition d = disposer(100, 100);
    VERIFIER(carte_en(d, 50, 50) == -1);
    VERIFIER(!disposer_detail(100, 100).valide && disposer_detail(1024, 600).valide);
    return 0;
}

constexpr int le_sens_vient_de_la_forme_de_l_ecran() {
    VERIFIER(!disposer(1280, 720).portrait && !disposer(1024, 600).portrait);
    VERIFIER(disposer(720, 1280).portrait && disposer(600, 1024).portrait);
    VERIFIER(disposer(720, 720).valide &&
             !disposer(720, 720).portrait); // carré : comme le paysage (et une page, pas rien)
    VERIFIER(disposer(1280, 720).petit_cote == 720 && disposer(1024, 600).petit_cote == 600 &&
             disposer(720, 1280).petit_cote == 720);
    return 0;
}

constexpr int les_polices_suivent_l_echelle_et_restent_lisibles() {
    const Disposition tab5 = disposer(1280, 720);
    VERIFIER(tab5.polices.verdict == 44 && tab5.polices.titre == 30 && tab5.polices.texte == 22 &&
             tab5.polices.petit == 18);
    for (int i = 0; i < NB_VISEES; ++i) {
        const Disposition d = disposer(VISEES[i].w, VISEES[i].h);
        VERIFIER(d.polices.verdict > d.polices.titre && d.polices.titre > d.polices.texte &&
                 d.polices.texte > d.polices.petit);
        VERIFIER(d.polices.petit >= 15); // la plus petite police reste lisible sur la 7B (1024 x 600)
    }
    return 0;
}

constexpr int chaque_zone_est_dans_l_ecran_et_aucune_n_en_chevauche_une_autre() {
    for (int i = 0; i < NB_TAILLES; ++i) {
        const Taille t = taille(i);
        const Disposition d = disposer(t.w, t.h);
        VERIFIER(d.valide);
        const Rect zones[] = {d.entete,    d.bandeau, d.cartes[0], d.cartes[1], d.cartes[2],
                              d.cartes[3], d.legende, d.maree,     d.soleil};
        VERIFIER(toutes_dans(zones, d.ecran));
        VERIFIER(toutes_separees(zones));
        // Dans l'en-tête : le titre et le bouton Fermer.
        const Rect tete[] = {d.titre, d.fermer};
        VERIFIER(toutes_dans(tete, d.entete));
        VERIFIER(toutes_separees(tete));
        // Les marges : rien ne touche le bord de l'écran.
        for (const Rect &z : zones)
            VERIFIER(z.x >= d.marge && z.y >= d.marge && z.droite() <= t.w - d.marge && z.bas() <= t.h - d.marge);
    }
    return 0;
}

constexpr int les_quatre_cartes_sont_egales_et_gardent_l_ordre_des_activites() {
    for (int i = 0; i < NB_TAILLES; ++i) {
        const Taille t = taille(i);
        const Disposition d = disposer(t.w, t.h);
        for (int k = 1; k < NB_ACTIVITES; ++k)
            VERIFIER(d.cartes[k].w == d.cartes[0].w && d.cartes[k].h == d.cartes[0].h);
        if (!d.portrait) {
            // Une rangée : de gauche à droite, même ligne.
            for (int k = 1; k < NB_ACTIVITES; ++k)
                VERIFIER(d.cartes[k].y == d.cartes[0].y && d.cartes[k].x >= d.cartes[k - 1].droite() + d.ecart);
        } else {
            // Deux rangées de deux : surf, kite / bateau, pêche à pied.
            VERIFIER(d.cartes[0].y == d.cartes[1].y && d.cartes[2].y == d.cartes[3].y);
            VERIFIER(d.cartes[2].y >= d.cartes[0].bas() + d.ecart);
            VERIFIER(d.cartes[0].x == d.cartes[2].x && d.cartes[1].x == d.cartes[3].x &&
                     d.cartes[1].x > d.cartes[0].droite());
        }
    }
    return 0;
}

constexpr int la_page_est_dans_l_ordre_de_haut_en_bas() {
    for (int i = 0; i < NB_TAILLES; ++i) {
        const Taille t = taille(i);
        const Disposition d = disposer(t.w, t.h);
        VERIFIER(d.entete.bas() <= d.bandeau.y && d.bandeau.bas() <= d.cartes[0].y);
        const int bas_cartes = d.portrait ? d.cartes[3].bas() : d.cartes[0].bas();
        VERIFIER(bas_cartes <= d.legende.y && d.legende.bas() <= d.maree.y);
        // Paysage : marée et soleil côte à côte, marée la plus large. Portrait : l'un sous l'autre, sur toute la largeur.
        if (!d.portrait) {
            VERIFIER(d.maree.y == d.soleil.y && d.maree.droite() <= d.soleil.x && d.maree.w > d.soleil.w);
            VERIFIER(d.soleil.droite() == t.w - d.marge); // le soleil va jusqu'à la marge droite
        } else {
            VERIFIER(d.maree.bas() <= d.soleil.y && d.maree.x == d.soleil.x && d.maree.w == d.soleil.w);
        }
    }
    return 0;
}

constexpr int les_zones_tactiles_se_touchent_au_doigt() {
    for (int i = 0; i < NB_VISEES; ++i) {
        const Disposition d = disposer(VISEES[i].w, VISEES[i].h);
        // Le bouton Fermer : au moins 44 px de haut et 110 de large sur nos écrans (la 7B, 1024 x 600, est la plus serrée).
        VERIFIER(d.fermer.h >= 44 && d.fermer.w >= 110);
        // Une carte : bien plus qu'un doigt dans les deux sens.
        for (const Rect &c : d.cartes)
            VERIFIER(c.w >= 200 && c.h >= 250);
        const Detail t = disposer_detail(VISEES[i].w, VISEES[i].h);
        VERIFIER(t.fermer.w >= 44 && t.fermer.h >= 44);
    }
    return 0;
}

constexpr int la_carte_touchee_est_trouvee_et_rien_n_est_touche_entre_les_cartes() {
    for (int i = 0; i < NB_TAILLES; ++i) {
        const Taille t = taille(i);
        const Disposition d = disposer(t.w, t.h);
        for (int k = 0; k < NB_ACTIVITES; ++k) {
            const Rect &c = d.cartes[k];
            VERIFIER(carte_en(d, c.x + c.w / 2, c.y + c.h / 2) == k);
            VERIFIER(carte_en(d, c.x, c.y) == k);                    // le coin haut-gauche est dedans
            VERIFIER(carte_en(d, c.droite() - 1, c.bas() - 1) == k); // le dernier pixel aussi
            VERIFIER(carte_en(d, c.droite(), c.bas()) != k);         // un pixel plus loin, non
        }
        // L'espace entre deux cartes, l'en-tête, le bandeau, la marée : aucune carte.
        VERIFIER(carte_en(d, d.cartes[0].droite() + d.ecart / 2, d.cartes[0].y + 5) == -1 || d.ecart < 2);
        VERIFIER(carte_en(d, d.entete.x + 3, d.entete.y + 3) == -1);
        VERIFIER(carte_en(d, d.bandeau.x + 3, d.bandeau.y + 3) == -1);
        VERIFIER(carte_en(d, d.maree.x + 3, d.maree.y + 3) == -1);
        VERIFIER(carte_en(d, -1, -1) == -1 && carte_en(d, t.w, t.h) == -1);
    }
    return 0;
}

// ---- l'intérieur d'une carte ---------------------------------------------------------------------------------------------------

constexpr int l_interieur_d_une_carte_tient_dans_la_carte_sans_chevauchement() {
    for (int i = 0; i < NB_TAILLES; ++i) {
        const Taille t = taille(i);
        const Disposition d = disposer(t.w, t.h);
        for (int k = 0; k < NB_ACTIVITES; ++k) {
            const InterieurCarte c = interieur_carte(d.cartes[k], d.petit_cote);
            const Rect zones[] = {c.icone,  c.nom, c.verdict,    c.raison,     c.etiquette_creneau, c.creneau,
                                  c.bandes, c.axe, c.rangees[0], c.rangees[1], c.rangees[2]};
            VERIFIER(toutes_dans(zones, d.cartes[k]));
            // L'icône est à gauche du nom, sur la même ligne ; tout le reste est empilé sans recouvrement.
            VERIFIER(c.icone.droite() <= c.nom.x && c.icone.y == c.nom.y);
            const Rect pile[] = {c.nom,    c.verdict, c.raison,     c.etiquette_creneau, c.creneau,
                                 c.bandes, c.axe,     c.rangees[0], c.rangees[1],        c.rangees[2]};
            VERIFIER(toutes_separees(pile));
            // L'ordre de haut en bas ne change pas.
            VERIFIER(c.nom.bas() <= c.verdict.y && c.verdict.bas() <= c.raison.y &&
                     c.raison.bas() <= c.etiquette_creneau.y);
            VERIFIER(c.etiquette_creneau.bas() <= c.creneau.y && c.creneau.bas() <= c.bandes.y &&
                     c.bandes.bas() <= c.axe.y);
            VERIFIER(c.axe.bas() <= c.rangees[0].y && c.rangees[0].bas() <= c.rangees[1].y &&
                     c.rangees[1].bas() <= c.rangees[2].y);
        }
    }
    return 0;
}

constexpr int les_24_barres_tiennent_dans_la_carte_et_restent_assez_larges() {
    for (int i = 0; i < NB_TAILLES; ++i) {
        const Taille t = taille(i);
        const Disposition d = disposer(t.w, t.h);
        for (int k = 0; k < NB_ACTIVITES; ++k) {
            const InterieurCarte c = interieur_carte(d.cartes[k], d.petit_cote);
            VERIFIER(c.pas_barre >= 1);
            VERIFIER(c.bandes.w == c.pas_barre * HEURES);             // 24 pas entiers, pas de barre coupée
            VERIFIER(c.axe.x == c.bandes.x && c.axe.w == c.bandes.w); // l'axe est sous les barres
            VERIFIER(c.bandes.x >= c.rangees[0].x &&
                     c.bandes.droite() <= c.rangees[0].droite()); // pas plus large que les lignes du dessous
            // Centrées dans la largeur utile (à un pixel près).
            const int gauche = c.bandes.x - c.rangees[0].x;
            const int droite = c.rangees[0].droite() - c.bandes.droite();
            VERIFIER(gauche >= 0 && droite >= 0 && (gauche - droite <= 1) && (droite - gauche <= 1));
        }
    }
    // Sur nos trois écrans, une barre fait au moins 7 pixels de pas : on y voit chaque heure.
    for (int i = 0; i < NB_VISEES; ++i) {
        const Disposition d = disposer(VISEES[i].w, VISEES[i].h);
        VERIFIER(interieur_carte(d.cartes[0], d.petit_cote).pas_barre >= 8);
    }
    return 0;
}

constexpr int les_zones_de_texte_d_une_carte_ont_la_hauteur_de_leur_police() {
    for (int i = 0; i < NB_VISEES; ++i) {
        const Disposition d = disposer(VISEES[i].w, VISEES[i].h);
        const InterieurCarte c = interieur_carte(d.cartes[0], d.petit_cote);
        // La hauteur d'une ligne est au moins la hauteur du corps de sa police (une approximation de la hauteur de ligne réelle : le rendu
        // PC regarde le reste).
        VERIFIER(c.nom.h >= d.polices.titre);
        VERIFIER(c.verdict.h >= d.polices.verdict);
        VERIFIER(c.creneau.h >= d.polices.texte);
        VERIFIER(c.rangees[0].h >= d.polices.petit);
        VERIFIER(c.etiquette_creneau.h >= d.polices.petit - 1); // l'étiquette d'une ligne, un pixel de tolérance
        // La raison tient sur deux lignes de la petite police.
        VERIFIER(c.raison.h >= 2 * d.polices.petit);
    }
    return 0;
}

// ---- la hauteur minimale d'une carte -------------------------------------------------------------------------------------------

// Vrai quand toutes les zones de l'intérieur de `carte` sont dans la carte et ne se recouvrent pas.
constexpr bool l_interieur_tient(const Rect &carte, int petit_cote) {
    const InterieurCarte c = interieur_carte(carte, petit_cote);
    const Rect zones[] = {c.icone,  c.nom, c.verdict,    c.raison,     c.etiquette_creneau, c.creneau,
                          c.bandes, c.axe, c.rangees[0], c.rangees[1], c.rangees[2]};
    const Rect pile[] = {c.nom,    c.verdict, c.raison,     c.etiquette_creneau, c.creneau,
                         c.bandes, c.axe,     c.rangees[0], c.rangees[1],        c.rangees[2]};
    return toutes_dans(zones, carte) && toutes_separees(pile);
}

constexpr int la_hauteur_minimale_d_une_carte_est_exactement_celle_qui_suffit() {
    const int ECHELLES[] = {320, 480, 600, 720, 1080, 1440};
    for (const int m : ECHELLES) {
        const int h = mesures_carte(m).hauteur_min;
        VERIFIER(h > 0);
        VERIFIER(l_interieur_tient(Rect{0, 0, 300, h}, m));      // juste assez : tout tient
        VERIFIER(l_interieur_tient(Rect{0, 0, 300, h + 40}, m)); // plus haut : tient aussi
        VERIFIER(!l_interieur_tient(Rect{0, 0, 300, h - 1}, m)); // un pixel de moins : deux zones se recouvrent
    }
    // À l'échelle de référence, la somme est celle qu'on lit dans le dessin : 24 + 30 + 4 + 52 + 4 + 44 + 104 + 72.
    VERIFIER(mesures_carte(720).hauteur_min == 334);
    return 0;
}

constexpr int une_page_valide_garde_toujours_des_cartes_assez_hautes() {
    // En paysage, comme en portrait sur plusieurs largeurs et hauteurs : si la page existe, l'intérieur des cartes tient et les panneaux
    // restent entre 150 et 170 (à l'échelle de 720).
    const int LARGEURS[] = {600, 720, 1080};
    for (const int w : LARGEURS)
        for (int h = w + 1; h <= 2 * w; h += 37) {
            const Disposition d = disposer(w, h);
            if (!d.valide)
                continue;
            VERIFIER(d.portrait);
            for (const Rect &c : d.cartes)
                VERIFIER(c.h >= mesures_carte(d.petit_cote).hauteur_min && l_interieur_tient(c, d.petit_cote));
            VERIFIER(d.maree.h == d.soleil.h);
            VERIFIER(d.maree.h <= mis(170, d.petit_cote) && d.maree.h >= mis(150, d.petit_cote));
        }
    // Paysage : la hauteur de l'écran fixe tout, la largeur ne change que celle des cartes ; de 320 à 1440 de haut, 4:3 comme 21:9.
    const int LARGEURS_PAYSAGE_POUR_CENT[] = {100, 133, 178, 233};
    for (int h = 320; h <= 1440; h += 40)
        for (const int pour_cent : LARGEURS_PAYSAGE_POUR_CENT) {
            const Disposition d = disposer(h * pour_cent / 100, h);
            VERIFIER(d.valide && !d.portrait);
            for (const Rect &c : d.cartes)
                VERIFIER(c.h >= mesures_carte(d.petit_cote).hauteur_min && l_interieur_tient(c, d.petit_cote));
        }
    return 0;
}

constexpr int un_ecran_portrait_trop_court_n_a_pas_de_page_et_un_assez_haut_en_a_une() {
    // 720 x 900 : les deux cartes d'une rangée, la légende et deux panneaux de 150 ne tiennent pas dans 900 px.
    VERIFIER(!disposer(720, 900).valide);
    VERIFIER(disposer(720, 1280).valide && disposer(1080, 1920).valide && disposer(600, 1024).valide);
    // La limite est nette : au-dessus d'une hauteur, la page existe, et plus haut elle existe toujours.
    int premiere = 0;
    for (int h = 721; h <= 1400 && premiere == 0; ++h)
        if (disposer(720, h).valide)
            premiere = h;
    // 1204 = marge du bas 16 + haut de page 136 (marge, en-tête, bandeau, intervalles) + deux cartes de 334 + quatre intervalles 32
    //        + légende sur deux lignes 52 + deux panneaux de 150 : si la mise en page change, ce nombre change, et c'est voulu.
    VERIFIER(premiere == 1204);
    VERIFIER(!disposer(720, premiere - 1).valide);
    for (int h = premiere; h <= 1400; h += 11)
        VERIFIER(disposer(720, h).valide);
    // Sur nos écrans visés, les panneaux gardent leur taille pleine (170) : rien n'a été serré.
    VERIFIER(disposer(720, 1280).maree.h == 170 && disposer(720, 1280).soleil.h == 170);
    return 0;
}

// ---- la marée et le soleil -------------------------------------------------------------------------------------------------------

constexpr int l_interieur_du_panneau_de_maree_tient_et_laisse_de_la_place_a_la_courbe() {
    for (int i = 0; i < NB_TAILLES; ++i) {
        const Taille t = taille(i);
        const Disposition d = disposer(t.w, t.h);
        const InterieurMaree r = interieur_maree(d.maree, d.petit_cote);
        const Rect zones[] = {r.titre, r.badge, r.prochaine[0], r.prochaine[1], r.courbe};
        VERIFIER(toutes_dans(zones, d.maree));
        VERIFIER(toutes_separees(zones));
        VERIFIER(r.titre.y == r.badge.y && r.prochaine[0].y == r.prochaine[1].y);
        VERIFIER(r.titre.bas() <= r.prochaine[0].y && r.prochaine[0].bas() <= r.courbe.y);
        VERIFIER(r.courbe.bas() <= d.maree.bas());
    }
    // La courbe est assez haute pour qu'on lise la montée et la descente, sur nos trois écrans.
    for (int i = 0; i < NB_VISEES; ++i) {
        const Disposition d = disposer(VISEES[i].w, VISEES[i].h);
        const InterieurMaree r = interieur_maree(d.maree, d.petit_cote);
        VERIFIER(r.courbe.h >= 60 && r.courbe.w >= 300);
    }
    return 0;
}

constexpr int l_interieur_du_panneau_du_soleil_tient() {
    for (int i = 0; i < NB_TAILLES; ++i) {
        const Taille t = taille(i);
        const Disposition d = disposer(t.w, t.h);
        const InterieurSoleil r = interieur_soleil(d.soleil, d.petit_cote);
        const Rect zones[] = {r.titre, r.etiquette[0], r.etiquette[1], r.valeur[0], r.valeur[1], r.avis};
        VERIFIER(toutes_dans(zones, d.soleil));
        VERIFIER(toutes_separees(zones));
        VERIFIER(r.etiquette[0].x < r.etiquette[1].x && r.valeur[0].x == r.etiquette[0].x &&
                 r.valeur[1].x == r.etiquette[1].x);
        // La valeur est sous son étiquette.
        VERIFIER(r.etiquette[0].bas() <= r.valeur[0].y && r.titre.bas() <= r.etiquette[0].y &&
                 r.valeur[0].bas() <= r.avis.y);
    }
    // L'avis (« Règles de Vigie, pas pour la sécurité ») a la place d'une ligne de petite police au moins.
    for (int i = 0; i < NB_VISEES; ++i) {
        const Disposition d = disposer(VISEES[i].w, VISEES[i].h);
        VERIFIER(interieur_soleil(d.soleil, d.petit_cote).avis.h >= d.polices.petit);
    }
    return 0;
}

// ---- le détail -----------------------------------------------------------------------------------------------------------------

constexpr int le_detail_tient_dans_l_ecran_et_ses_zones_ne_se_chevauchent_pas() {
    const int RANGEES[] = {1, 3, CRITERES_MAX};
    for (int i = 0; i < NB_TAILLES; ++i)
        for (const int rangees : RANGEES) {
            const Taille t = taille(i);
            const Detail d = disposer_detail(t.w, t.h, rangees);
            VERIFIER(d.valide);
            const Rect ecran{0, 0, t.w, t.h};
            VERIFIER(dans(d.panneau, ecran));
            const Rect entete[] = {d.icone, d.titre, d.sous_titre, d.fermer, d.verdict, d.note, d.avis};
            VERIFIER(toutes_dans(entete, d.panneau));
            // L'icône, le titre et la croix sont sur une ligne ; le sous-titre sous le titre ; le verdict et la note côte à côte.
            VERIFIER(d.icone.droite() <= d.titre.x && d.titre.droite() <= d.fermer.x);
            VERIFIER(d.titre.bas() <= d.sous_titre.y);
            VERIFIER(d.verdict.droite() <= d.note.x && d.verdict.y == d.note.y);
            VERIFIER(d.sous_titre.bas() <= d.verdict.y && d.verdict.bas() <= d.entetes[0].y);
            // Les en-têtes de colonnes sont dans le panneau, côte à côte, dans l'ordre.
            VERIFIER(toutes_dans(d.entetes, d.panneau));
            VERIFIER(toutes_separees(d.entetes));
            for (int c = 1; c < COLONNES_DETAIL; ++c)
                VERIFIER(d.entetes[c].x >= d.entetes[c - 1].droite() && d.entetes[c].y == d.entetes[0].y);
            // Les cellules : chaque colonne sous son en-tête (même x, même largeur), rangée sous rangée, sans recouvrement.
            for (int k = 0; k < rangees; ++k) {
                for (int c = 0; c < COLONNES_DETAIL; ++c) {
                    const Rect &cel = d.cellules[k][c];
                    VERIFIER(dans(cel, d.panneau));
                    VERIFIER(cel.x == d.entetes[c].x && cel.w == d.entetes[c].w);
                    VERIFIER(k == 0 ? cel.y >= d.entetes[c].bas() : cel.y == d.cellules[k - 1][c].bas());
                }
                VERIFIER(toutes_separees(d.cellules[k]));
            }
            VERIFIER(d.cellules[rangees - 1][0].bas() <= d.avis.y);
            // Les rangées qu'on ne montre pas n'ont aucune cellule : rien à dessiner, rien à toucher.
            for (int k = rangees; k < CRITERES_MAX; ++k)
                VERIFIER(d.cellules[k][0].w == 0 && d.cellules[k][0].h == 0);
        }
    return 0;
}

constexpr int le_panneau_du_detail_epouse_le_nombre_de_criteres() {
    for (int i = 0; i < NB_TAILLES; ++i) {
        const Taille t = taille(i);
        // Plus de critères, panneau plus haut (jamais plus bas) ; toujours centré à un pixel près ; la valeur par défaut est le maximum.
        int precedente = 0;
        for (int rangees = 1; rangees <= CRITERES_MAX; ++rangees) {
            const Detail d = disposer_detail(t.w, t.h, rangees);
            VERIFIER(d.panneau.h >= precedente);
            precedente = d.panneau.h;
            const int haut = d.panneau.y;
            const int bas = t.h - d.panneau.bas();
            VERIFIER(haut - bas <= 1 && bas - haut <= 1);
        }
        VERIFIER(disposer_detail(t.w, t.h).panneau.h == disposer_detail(t.w, t.h, CRITERES_MAX).panneau.h);
        // Une valeur absurde est ramenée dans les bornes.
        VERIFIER(disposer_detail(t.w, t.h, 0).panneau.h == disposer_detail(t.w, t.h, 1).panneau.h);
        VERIFIER(disposer_detail(t.w, t.h, 99).panneau.h == disposer_detail(t.w, t.h, CRITERES_MAX).panneau.h);
    }
    // Avec un seul critère, le panneau est bien plus bas qu'avec le maximum : c'est ce qui évite le grand vide du bas.
    VERIFIER(disposer_detail(1280, 720, 1).panneau.h + 100 < disposer_detail(1280, 720, CRITERES_MAX).panneau.h);
    return 0;
}

constexpr int le_detail_a_la_place_de_son_avis_et_de_ses_rangees() {
    for (int i = 0; i < NB_VISEES; ++i) {
        const Taille t = VISEES[i];
        const Detail d = disposer_detail(t.w, t.h);
        const Disposition p = disposer(t.w, t.h);
        // Une rangée de critère est lisible (deux lignes au plus : valeur + unité, lecture) ; l'avis tient sur au moins une ligne.
        VERIFIER(d.cellules[0][0].h >= 2 * p.polices.petit);
        VERIFIER(d.avis.h >= p.polices.petit);
        // La croix du détail est dans l'angle haut-droit du panneau.
        VERIFIER(d.fermer.droite() <= d.panneau.droite() && d.fermer.y <= d.panneau.y + d.panneau.h / 4);
        // Le panneau est centré à un pixel près et ne dépasse pas 600 px de haut (au-delà, on ne lit plus rien d'un coup d'œil).
        VERIFIER(d.panneau.h <= 600);
        VERIFIER(d.panneau.x - 0 == t.w - d.panneau.droite());
        VERIFIER(d.panneau.y - 0 - (t.h - d.panneau.bas()) <= 1 && (t.h - d.panneau.bas()) - d.panneau.y <= 1);
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(les_rectangles_se_comparent_comme_on_s_y_attend)                                                                 \
    X(l_echelle_suit_le_petit_cote_et_ne_descend_jamais_sous_un_pixel)                                                 \
    X(un_ecran_trop_petit_n_a_pas_de_page)                                                                             \
    X(le_sens_vient_de_la_forme_de_l_ecran)                                                                            \
    X(les_polices_suivent_l_echelle_et_restent_lisibles)                                                               \
    X(chaque_zone_est_dans_l_ecran_et_aucune_n_en_chevauche_une_autre)                                                 \
    X(les_quatre_cartes_sont_egales_et_gardent_l_ordre_des_activites)                                                  \
    X(la_page_est_dans_l_ordre_de_haut_en_bas)                                                                         \
    X(les_zones_tactiles_se_touchent_au_doigt)                                                                         \
    X(la_carte_touchee_est_trouvee_et_rien_n_est_touche_entre_les_cartes)                                              \
    X(l_interieur_d_une_carte_tient_dans_la_carte_sans_chevauchement)                                                  \
    X(la_hauteur_minimale_d_une_carte_est_exactement_celle_qui_suffit)                                                 \
    X(une_page_valide_garde_toujours_des_cartes_assez_hautes)                                                          \
    X(un_ecran_portrait_trop_court_n_a_pas_de_page_et_un_assez_haut_en_a_une)                                          \
    X(les_24_barres_tiennent_dans_la_carte_et_restent_assez_larges)                                                    \
    X(les_zones_de_texte_d_une_carte_ont_la_hauteur_de_leur_police)                                                    \
    X(l_interieur_du_panneau_de_maree_tient_et_laisse_de_la_place_a_la_courbe)                                         \
    X(l_interieur_du_panneau_du_soleil_tient)                                                                          \
    X(le_detail_tient_dans_l_ecran_et_ses_zones_ne_se_chevauchent_pas)                                                 \
    X(le_panneau_du_detail_epouse_le_nombre_de_criteres)                                                               \
    X(le_detail_a_la_place_de_son_avis_et_de_ses_rangees)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
