// Noyau pur : géométrie de la roue d'appui long (portage de celle du Tab5, ADR-0036 et ADR-0042 du dépôt Tab5). Pour un point
// d'appui, un écran et un anneau de n boutons, donne où est le moyeu, où est chaque bouton, où est son mot, comment s'ouvre le
// second anneau d'une famille, et quel bouton se trouve sous un doigt. Aucun LVGL, aucune police, aucune couleur, aucun texte : les
// boutons sont des numéros, la largeur d'un mot est un nombre de pixels que l'appelant a mesuré (la police est son affaire).
//
// D'où viennent les valeurs (Tab5/ecran/tab5_roue.cpp du dépôt Tab5, commit e330d609 du 06/11/2026 ; relues dans le code, pas dans
// la description de l'ADR, qui dit 12 px de marge là où une première lecture supposait 32) :
//   premier anneau : boutons de 72 px, centres à 180 px du moyeu, 30° entre deux, 6 boutons au plus (tab5_roue.cpp:54-55,59 ;
//   kRoueBoutons, tab5_roue.h:25) ; second anneau : boutons de 72 px à 290 px, 20° entre deux, centré sur l'angle de sa famille,
//   8 choix au plus (tab5_roue.cpp:56-57,60 ; kRoueChoix, tab5_roue.h:31) ; marge de 12 px aux bords (:58) ; pivotement par pas de
//   5° (:61, disposer :129-157) ; sinus par une table de 19 valeurs (:93-106) ; mots : boîte de 150 x 28 px au plus, à 4 px du
//   bouton, dans son axe, serrée entre voisins de 8 px, 40 px au moins (:81-86, mot_recul :437-446, largeurs_libres :419-429) ;
//   « dessous » : l'anneau passe sous le moyeu quand le second anneau et ses mots ne tiennent pas au-dessus (:629).
//
// Règles (chacune est un test de test/host/test_roue_geometrie.cpp ou de test_roue_balayage.cpp) :
//   1. `disposer` est celle du Tab5, ligne à ligne : un éventail qui sort à gauche ou à droite pivote de 5° en 5° du côté opposé,
//      un éventail qui sort en haut ou en bas pivote vers la verticale ; au pire un bouton est ramené dans l'écran.
//      Valeurs de référence : le code du Tab5 lui-même, compilé à part (voir le test).
//   2. Écart voulu avec le Tab5 : le Tab5 ouvre sa roue sur une tuile, jamais dans un coin ; Meridien l'ouvre là où le doigt
//      s'appuie. Dans un coin, ou au bord avec six boutons, le pivotement du Tab5 ne peut rien (150° d'éventail pour 149° de place) et
//      empile les boutons au même endroit. `ouvrir` garde le moyeu sous le doigt tant que le Tab5 donnerait une roue qui tient
//      (boutons, mots, second anneau de chaque famille), sinon avance le moyeu vers le centre de l'écran, par pas de 1/64 du chemin,
//      jusqu'à la première position où tout tient ; si le chemin entier est impossible, le même chemin décalé de 4 px vers le haut ou le bas,
//      puis de 8 px, etc. jusqu'à 80 px. Si rien ne tient, `Roue::tient` est faux et la roue est celle du centre de l'écran.
//      Le prix est un moyeu écarté du doigt, MESURÉ (ADR-0017) : sur 1280 x 720, avec le second anneau de deux familles dans la
//      demande, 92 % des appuis ont un moyeu déplacé (264 px en moyenne, 676 au pire, dans un coin) ; avec le premier anneau
//      seul, 60 % (76 px en moyenne, 368 au pire). L'appelant règle ce prix : `Demande::choix[f] = 0` retire le second anneau de
//      la famille f de ce qu'`ouvrir` exige, et `anneau_complet` dit ensuite si cette famille tient quand même.
//   3. Une roue qui tient : tous les boutons sont à 12 px au moins des bords, deux boutons ne se recouvrent pas, tous les mots sont
//      dans l'écran (même marge), ne recouvrent ni un bouton ni un autre mot (sauf si deux mots voisins sont plus proches que
//      leur largeur minimale), et rien n'a été « ramené dans l'écran » par la dernière ressource de disposer.
//   4. Échelle : toutes les mesures sont en pixels de l'écran réel ; `Ecran::echelle_pm` (millièmes) les ramène depuis l'espace du
//      Tab5 (1280 x 720). 1000 : tel quel (le Tab5 ; le Tab5 sur une carte plus petite garde aussi 1000, seul l'écran change) ;
//      800 : Waveshare 7B 1024 x 600. Arrondi à l'entier le plus proche, moitié vers le haut.
//   5. Toucher : la zone d'un bouton est le disque du bouton élargi de TOUCHER_MARGE (donc jamais plus petite que le bouton) ; si
//      deux zones se recouvrent, le centre le plus proche gagne. Un pixel à la distance exacte du rayon est dedans.
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation, aucune allocation.
#pragma once

#include <cstdint>

namespace ecran::roue {

// ---- Mesures du Tab5, en pixels de son écran (1280 x 720, Tab5/socle/tab5_geometrie.h:46-47) ----
inline constexpr int TAB5_LARGEUR = 1280;
inline constexpr int TAB5_HAUTEUR = 720;
inline constexpr int BOUTONS_MAX = 6; // premier anneau
inline constexpr int CHOIX_MAX = 8;   // second anneau
inline constexpr int RAYON_1 = 180;
inline constexpr int DIAMETRE_1 = 72;
inline constexpr int PAS_1 = 30;
inline constexpr int RAYON_2 = 290;
inline constexpr int DIAMETRE_2 = 72;
inline constexpr int PAS_2 = 20;
inline constexpr int MARGE = 12;
inline constexpr int PIVOT = 5;
inline constexpr int MOYEU = 120;     // kMoyeu (:62)
inline constexpr int JAUGE = 148;     // kJauge (:63) : le cercle autour du moyeu
inline constexpr int LEGENDE_L = 150; // largeur d'un mot au plus (kLegendeL)
inline constexpr int LEGENDE_H = 28;  // hauteur d'un mot (kLegendeH)
inline constexpr int LEGENDE_2 = 60;  // place d'un mot au-dessus d'un choix (kLegende2)
inline constexpr int MOT_ECART = 4;   // entre un bouton et son mot (kMotEcart)
inline constexpr int ECART_MOTS = 8;  // entre deux mots voisins (kEcartMots)
inline constexpr int MOT_MIN = 40;    // largeur minimale d'un mot serré (kMotMin)
// Ajout de Meridien (le Tab5 touche le rectangle de 72 x 72 de LVGL, sans marge) : le disque du bouton, élargi de 8 px.
// Moitié de l'écart entre deux boutons voisins du premier anneau (21 px) : les zones ne se recouvrent pas dans un éventail régulier.
inline constexpr int TOUCHER_MARGE = 8;
// Nombre de pas du chemin du moyeu vers le centre de l'écran (règle 2).
inline constexpr int ETAPES_MOYEU = 64;
// Décalage vertical du chemin quand rien ne tient dessus (règle 2) : DECALAGES_VERTICAUX pas de PAS_VERTICAL pixels, des deux côtés.
inline constexpr int PAS_VERTICAL = 4;
inline constexpr int DECALAGES_VERTICAUX = 20;

struct Ecran {
    int largeur;
    int hauteur;
    int echelle_pm; // 1000 = les mesures du Tab5
};
inline constexpr Ecran ECRAN_TAB5{TAB5_LARGEUR, TAB5_HAUTEUR, 1000};
inline constexpr Ecran ECRAN_7B{1024, 600, 800};

constexpr int mettre_a_l_echelle(int valeur, int echelle_pm) noexcept { return (valeur * echelle_pm + 500) / 1000; }

// Toutes les mesures, en pixels de l'écran réel.
struct Mesures {
    int largeur;
    int hauteur;
    int rayon_1;
    int diametre_1;
    int pas_1;
    int rayon_2;
    int diametre_2;
    int pas_2;
    int marge;
    int moyeu;
    int jauge;
    int legende_l;
    int legende_h;
    int legende_2;
    int mot_ecart;
    int ecart_mots;
    int mot_min;
    int toucher_marge;
};

constexpr Mesures mesures(const Ecran &e) noexcept {
    const int k = e.echelle_pm;
    return Mesures{e.largeur,
                   e.hauteur,
                   mettre_a_l_echelle(RAYON_1, k),
                   mettre_a_l_echelle(DIAMETRE_1, k),
                   PAS_1,
                   mettre_a_l_echelle(RAYON_2, k),
                   mettre_a_l_echelle(DIAMETRE_2, k),
                   PAS_2,
                   mettre_a_l_echelle(MARGE, k),
                   mettre_a_l_echelle(MOYEU, k),
                   mettre_a_l_echelle(JAUGE, k),
                   mettre_a_l_echelle(LEGENDE_L, k),
                   mettre_a_l_echelle(LEGENDE_H, k),
                   mettre_a_l_echelle(LEGENDE_2, k),
                   mettre_a_l_echelle(MOT_ECART, k),
                   mettre_a_l_echelle(ECART_MOTS, k),
                   mettre_a_l_echelle(MOT_MIN, k),
                   mettre_a_l_echelle(TOUCHER_MARGE, k)};
}

struct Point {
    int x = 0;
    int y = 0;
};

constexpr bool operator==(const Point &a, const Point &b) noexcept { return a.x == b.x && a.y == b.y; }

// Où est le mot par rapport à son bouton, d'après l'axe du bouton (cos 60° = 0,5 : jusqu'à 60° de l'horizontale, côté).
enum class Cote : std::uint8_t { Aucun, Gauche, Droite, Haut, Bas };

// Boîte du mot : centre (x, y), largeur, et la hauteur `legende_h` des mesures. Largeur 0 : pas de mot.
struct Mot {
    int x = 0;
    int y = 0;
    int largeur = 0;
    Cote cote = Cote::Aucun;
};

struct Bouton {
    int x = 0; // centre
    int y = 0;
    int angle = 0; // degrés, sens trigonométrique, 90 : à la verticale du moyeu ; pivotement compris
    Mot mot;
};

struct Anneau {
    int n = 0;
    int pivot = 0;      // degrés dont l'éventail a tourné (0 : régulier, centré)
    bool rogne = false; // un centre a été ramené dans l'écran (les boutons ne sont plus à leur place exacte)
    Bouton bouton[CHOIX_MAX] = {};
};

// Ce que l'appelant sait de la roue qu'il ouvre : des nombres, jamais de texte ni d'icône.
struct Demande {
    int n = 0;                   // boutons du premier anneau, 1 à BOUTONS_MAX
    int mot[BOUTONS_MAX] = {};   // largeur du mot de chaque bouton, en pixels ; 0 : pas de mot
    int choix[BOUTONS_MAX] = {}; // choix de la famille que ce bouton déplie, 0 à CHOIX_MAX ; 0 : pas une famille.
                                 // `Roue::tient` ne promet le second anneau que pour EXACTEMENT ce nombre de choix (un
                                 // éventail plus court pivote autrement et peut dépasser).
    int mot_choix = 0;           // largeur du plus large mot d'un choix, en pixels ; 0 : pas de mot au second anneau
};

struct Roue {
    Point appui;                // le point reçu
    Point moyeu;                // le centre de la roue
    bool moyeu_deplace = false; // le moyeu n'est pas sous le doigt (règle 2)
    bool dessous = false;       // les anneaux sont sous le moyeu (l'appui est trop haut pour le second anneau)
    bool tient = false; // la roue tient (règle 3) pour ce moyeu, premier anneau et second anneau de chaque famille ;
                        // faux : aucune position n'a convenu, la roue est celle du centre de l'écran
    Anneau premier;
};

namespace detail {

// sin(k × 5°) × 32767, arrondis, de 0 à 90° (tab5_roue.cpp:93-94) : tout angle de la roue est un multiple de 5°.
inline constexpr int SIN5[19] = {0,     2856,  5690,  8481,  11207, 13848, 16383, 18794, 21062, 23170,
                                 25101, 26841, 28377, 29697, 30791, 31650, 32269, 32642, 32767};

constexpr int abs_int(int v) noexcept { return v < 0 ? -v : v; }
constexpr int min_int(int a, int b) noexcept { return a < b ? a : b; }
constexpr int max_int(int a, int b) noexcept { return a > b ? a : b; }
constexpr int limiter(int v, int bas, int haut) noexcept { return v < bas ? bas : (v > haut ? haut : v); }

} // namespace detail

// sinus et cosinus d'un angle en degrés, multipliés par 32767 (tab5_roue.cpp:96-108).
constexpr int sinus5(int deg) noexcept {
    deg %= 360;
    if (deg < 0) {
        deg += 360;
    }
    int signe = 1;
    if (deg >= 180) {
        deg -= 180;
        signe = -1;
    }
    if (deg > 90) {
        deg = 180 - deg;
    }
    return signe * detail::SIN5[deg / PIVOT];
}

constexpr int cosinus5(int deg) noexcept { return sinus5(deg + 90); }

// r × v / 32767 arrondi au plus proche, la moitié loin de zéro (tab5_roue.cpp:111-114).
constexpr int projeter(int r, int v) noexcept {
    const int p = r * v;
    return p >= 0 ? (p + 16383) / 32767 : -((-p + 16383) / 32767);
}

// Les anneaux passent sous le moyeu quand le second anneau et ses mots ne tiennent pas au-dessus (tab5_roue.cpp:629).
constexpr bool passe_dessous(const Mesures &m, int y_moyeu) noexcept {
    return y_moyeu - (m.rayon_2 + m.legende_2 + m.legende_h / 2) < 0;
}

// Centres de n boutons de rayon `rayon_bouton` sur un arc de rayon `rayon` autour du moyeu, `pas` degrés entre deux, l'éventail
// centré sur l'angle `centre` (90 : la verticale), de gauche à droite sans pivotement. Au-dessus du moyeu, en dessous si `sous`.
// tab5_roue.cpp:129-157, ligne à ligne.
constexpr Anneau disposer(const Mesures &m, Point moyeu, int n, int rayon, int pas, int centre, int rayon_bouton,
                          bool sous) noexcept {
    n = detail::limiter(n, 0, CHOIX_MAX);
    Anneau a;
    a.n = n;
    int decalage = 0;
    for (int essai = 0; essai <= 180 / PIVOT; essai++) {
        a.pivot = decalage; // celui de cet essai : le dernier calculé reste, comme dans le Tab5
        int gauche = m.largeur;
        int droite = 0;
        for (int i = 0; i < n; i++) {
            const int ang = centre + (n - 1) * pas / 2 - i * pas + decalage;
            a.bouton[i].angle = ang;
            a.bouton[i].x = moyeu.x + projeter(rayon, cosinus5(ang));
            const int dy = projeter(rayon, sinus5(ang));
            a.bouton[i].y = sous ? moyeu.y + dy : moyeu.y - dy;
            gauche = detail::min_int(gauche, a.bouton[i].x);
            droite = detail::max_int(droite, a.bouton[i].x);
        }
        int sens = 0;
        if (gauche - rayon_bouton < m.marge) {
            sens = -PIVOT;
        } else if (droite + rayon_bouton > m.largeur - m.marge) {
            sens = PIVOT;
        }
        // Un bouton qui sortirait en haut ou en bas : vers la verticale du moyeu.
        for (int i = 0; i < n && sens == 0; i++) {
            if (a.bouton[i].y - rayon_bouton < m.marge || a.bouton[i].y + rayon_bouton > m.hauteur - m.marge) {
                sens = a.bouton[i].angle < 90 ? PIVOT : -PIVOT;
            }
        }
        if (sens == 0) {
            break;
        }
        decalage += sens;
    }
    for (int i = 0; i < n; i++) {
        const int x = detail::limiter(a.bouton[i].x, m.marge + rayon_bouton, m.largeur - m.marge - rayon_bouton);
        const int y = detail::limiter(a.bouton[i].y, m.marge + rayon_bouton, m.hauteur - m.marge - rayon_bouton);
        a.rogne = a.rogne || x != a.bouton[i].x || y != a.bouton[i].y;
        a.bouton[i].x = x;
        a.bouton[i].y = y;
    }
    return a;
}

// Distance du centre d'un bouton, d'angle `angle`, au centre de son mot : le mot est une boîte de sa largeur sur `legende_h`,
// posée dans l'axe du bouton à `mot_ecart` de son bord ; l'étendue de la boîte sur cet axe l'en éloigne d'autant
// (tab5_roue.cpp:445).
constexpr int recul_du_mot(const Mesures &m, int diametre, int largeur, int angle) noexcept {
    const int l = detail::min_int(largeur, m.legende_l);
    return diametre / 2 + m.mot_ecart + projeter(l / 2, detail::abs_int(cosinus5(angle))) +
           projeter(m.legende_h / 2, detail::abs_int(sinus5(angle)));
}

constexpr Cote cote_du_mot(int angle, bool sous) noexcept {
    if (detail::abs_int(cosinus5(angle)) >= 16383) {
        return cosinus5(angle) > 0 ? Cote::Droite : Cote::Gauche;
    }
    return (sinus5(angle) > 0) != sous ? Cote::Haut : Cote::Bas;
}

// Place le mot de chaque bouton de l'anneau (`largeur[i]` en pixels, 0 : pas de mot), dans l'axe du bouton, au-delà de lui ;
// puis serre ceux d'une même rangée (moins de `legende_h` d'écart en hauteur) à leur écart moins `ecart_mots`, `mot_min` au moins
// (tab5_roue.cpp:419-429 et 495-507).
constexpr void placer_mots(const Mesures &m, Anneau &a, Point moyeu, bool sous, int rayon, int diametre,
                           const int *largeur) noexcept {
    for (int i = 0; i < a.n; i++) {
        Mot &mot = a.bouton[i].mot;
        mot = Mot{};
        if (largeur == nullptr || largeur[i] <= 0) {
            continue;
        }
        const int angle = a.bouton[i].angle;
        const int r = rayon + recul_du_mot(m, diametre, largeur[i], angle);
        const int dy = projeter(r, sinus5(angle));
        mot.x = moyeu.x + projeter(r, cosinus5(angle));
        mot.y = sous ? moyeu.y + dy : moyeu.y - dy;
        mot.largeur = detail::min_int(largeur[i], m.legende_l);
        mot.cote = cote_du_mot(angle, sous);
    }
    for (int i = 0; i < a.n; i++) {
        if (a.bouton[i].mot.largeur == 0) {
            continue;
        }
        int libre = m.legende_l;
        for (int j = 0; j < a.n; j++) {
            if (j == i || a.bouton[j].mot.largeur == 0 ||
                detail::abs_int(a.bouton[i].mot.y - a.bouton[j].mot.y) >= m.legende_h) {
                continue;
            }
            libre = detail::min_int(libre, detail::abs_int(a.bouton[i].mot.x - a.bouton[j].mot.x) - m.ecart_mots);
        }
        libre = detail::max_int(libre, m.mot_min);
        a.bouton[i].mot.largeur = detail::min_int(a.bouton[i].mot.largeur, libre);
    }
}

namespace detail {

// Deux boîtes de centre et de demi-étendue donnés se recouvrent-elles ? (se toucher n'est pas se recouvrir)
constexpr bool boites_se_recouvrent(int x1, int y1, int l1, int h1, int x2, int y2, int l2, int h2) noexcept {
    const int g1 = x1 - l1 / 2;
    const int g2 = x2 - l2 / 2;
    return g1 < g2 + l2 && g2 < g1 + l1 && y1 - h1 / 2 < y2 - h2 / 2 + h2 && y2 - h2 / 2 < y1 - h1 / 2 + h1;
}

// Le disque (cx, cy, rayon) touche-t-il la boîte (centre bx, by, largeur l, hauteur h) ? (se toucher n'est pas se recouvrir)
constexpr bool disque_sur_boite(int cx, int cy, int rayon, int bx, int by, int l, int h) noexcept {
    const int gauche = bx - l / 2;
    const int haut = by - h / 2;
    const int px = limiter(cx, gauche, gauche + l);
    const int py = limiter(cy, haut, haut + h);
    const int dx = cx - px;
    const int dy = cy - py;
    return dx * dx + dy * dy < rayon * rayon;
}

// Un anneau « tient » : rien n'a été ramené dans l'écran, deux boutons ne se recouvrent pas, les mots sont dans l'écran à `marge`
// de ses bords et ne recouvrent ni un bouton ni un autre mot.
constexpr bool anneau_tient(const Mesures &m, const Anneau &a, int diametre) noexcept {
    if (a.rogne) {
        return false;
    }
    const int rb = diametre / 2;
    for (int i = 0; i < a.n; i++) {
        const Bouton &b = a.bouton[i];
        for (int j = i + 1; j < a.n; j++) {
            const int dx = b.x - a.bouton[j].x;
            const int dy = b.y - a.bouton[j].y;
            if (dx * dx + dy * dy < diametre * diametre) {
                return false;
            }
        }
        if (b.mot.largeur == 0) {
            continue;
        }
        const Mot &mot = b.mot;
        const int gauche = mot.x - mot.largeur / 2;
        const int haut = mot.y - m.legende_h / 2;
        if (gauche < m.marge || gauche + mot.largeur > m.largeur - m.marge || haut < m.marge ||
            haut + m.legende_h > m.hauteur - m.marge) {
            return false;
        }
        for (int j = 0; j < a.n; j++) {
            const Mot &autre = a.bouton[j].mot;
            if (disque_sur_boite(a.bouton[j].x, a.bouton[j].y, rb, mot.x, mot.y, mot.largeur, m.legende_h)) {
                return false;
            }
            if (j > i && autre.largeur != 0 &&
                boites_se_recouvrent(mot.x, mot.y, mot.largeur, m.legende_h, autre.x, autre.y, autre.largeur,
                                     m.legende_h)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace detail

// Le second anneau de la famille `famille` du premier : `n` choix, centrés sur l'angle de ce bouton, du même côté du moyeu que
// le premier anneau (tab5_roue.cpp:526-527 et 532-537). `largeur[j]` : largeur du mot du choix j (nullptr : pas de mot).
constexpr Anneau deplier(const Mesures &m, const Roue &roue, int famille, int n, const int *largeur) noexcept {
    if (famille < 0 || famille >= roue.premier.n) {
        return Anneau{};
    }
    Anneau a = disposer(m, roue.moyeu, n, m.rayon_2, m.pas_2, roue.premier.bouton[famille].angle, m.diametre_2 / 2,
                        roue.dessous);
    placer_mots(m, a, roue.moyeu, roue.dessous, m.rayon_2, m.diametre_2, largeur);
    return a;
}

// Un anneau tient dans l'écran : aucun bouton n'a été ramené, deux boutons ne se recouvrent pas, les mots sont dans l'écran à
// `marge` de ses bords et ne recouvrent ni un bouton ni un autre mot. Pour savoir, après `ouvrir`, si le second anneau d'une
// famille que la demande n'exigeait pas (`choix[f] = 0`) peut tout de même s'ouvrir.
constexpr bool anneau_complet(const Mesures &m, const Anneau &a, int diametre) noexcept {
    return detail::anneau_tient(m, a, diametre);
}

namespace detail {

// Le premier anneau tient, et le second de chaque famille aussi, avec des mots aussi larges que `d.mot_choix`.
constexpr bool roue_tient(const Mesures &m, const Roue &r, const Demande &d) noexcept {
    if (!anneau_tient(m, r.premier, m.diametre_1)) {
        return false;
    }
    int largeur[CHOIX_MAX] = {};
    for (int j = 0; j < CHOIX_MAX; j++) {
        largeur[j] = d.mot_choix;
    }
    for (int f = 0; f < r.premier.n; f++) {
        if (d.choix[f] <= 0) {
            continue;
        }
        if (!anneau_tient(m, deplier(m, r, f, d.choix[f], largeur), m.diametre_2)) {
            return false;
        }
    }
    return true;
}

// La roue autour de `moyeu` : premier anneau, mots compris, et si elle tient.
constexpr Roue composer(const Mesures &m, Point appui, Point moyeu, const Demande &d) noexcept {
    Roue r;
    r.appui = appui;
    r.moyeu = moyeu;
    r.moyeu_deplace = !(appui == moyeu);
    r.dessous = passe_dessous(m, moyeu.y);
    r.premier =
        disposer(m, moyeu, detail::limiter(d.n, 0, BOUTONS_MAX), m.rayon_1, m.pas_1, 90, m.diametre_1 / 2, r.dessous);
    placer_mots(m, r.premier, moyeu, r.dessous, m.rayon_1, m.diametre_1, d.mot);
    r.tient = roue_tient(m, r, d);
    return r;
}

} // namespace detail

// Ouvre la roue sur un point d'appui (règle 2) : le moyeu reste sous le doigt tant que la roue tient. Le moyeu entier (le cercle de
// `jauge`) reste dans l'écran, à `marge` des bords, quoi qu'il arrive.
constexpr Roue ouvrir(const Mesures &m, Point appui, const Demande &d) noexcept {
    const int bord = m.marge + m.jauge / 2;
    const Point depart{detail::limiter(appui.x, bord, m.largeur - bord),
                       detail::limiter(appui.y, bord, m.hauteur - bord)};
    const Point centre{m.largeur / 2, m.hauteur / 2};
    // Le chemin du doigt au centre de l'écran, en ETAPES_MOYEU pas ; puis, si rien n'y tient (une bande de 20 px à mi-hauteur où
    // ni « au-dessus » ni « en dessous » ne laissent la place au second anneau et à ses mots, ou une demande trop lourde pour
    // le bord), le même chemin décalé de 4 px vers le haut, vers le bas, de 8 px, etc. jusqu'à DECALAGES_VERTICAUX pas.
    for (int j = 0; j <= DECALAGES_VERTICAUX; j++) {
        for (int sens = (j == 0 ? 0 : -1); sens <= (j == 0 ? 0 : 1); sens += 2) {
            const int decalage = sens * j * PAS_VERTICAL;
            for (int k = 0; k <= ETAPES_MOYEU; k++) {
                const Point p{depart.x + (centre.x - depart.x) * k / ETAPES_MOYEU,
                              detail::limiter(depart.y + decalage + (centre.y - depart.y) * k / ETAPES_MOYEU, bord,
                                              m.hauteur - bord)};
                const Roue r = detail::composer(m, appui, p, d);
                if (r.tient) {
                    return r;
                }
            }
        }
    }
    return detail::composer(m, appui, centre, d); // rien ne tient : le centre, `tient` faux
}

// Rayon de la zone tactile d'un bouton de diamètre `diametre` (règle 5).
constexpr int rayon_tactile(const Mesures &m, int diametre) noexcept { return diametre / 2 + m.toucher_marge; }

// Le bouton de l'anneau dont la zone contient le pixel `p` : son rang, ou -1. Le centre le plus proche gagne.
constexpr int bouton_sous(const Mesures &m, const Anneau &a, int diametre, Point p) noexcept {
    const int rayon = rayon_tactile(m, diametre);
    int trouve = -1;
    int meilleur = rayon * rayon + 1;
    for (int i = 0; i < a.n; i++) {
        const int dx = p.x - a.bouton[i].x;
        const int dy = p.y - a.bouton[i].y;
        const int d2 = dx * dx + dy * dy;
        if (d2 <= rayon * rayon && d2 < meilleur) {
            meilleur = d2;
            trouve = i;
        }
    }
    return trouve;
}

} // namespace ecran::roue
