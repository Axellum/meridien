// Tests du noyau pur : géométrie de la roue d'appui long (roue_geometrie.hpp).
//
// D'où viennent les valeurs de référence (aucune n'a été calculée par ce code) :
//   - les tableaux REF_* sont la sortie de la VRAIE disposer() du Tab5, compilée à part (Tab5/ecran/tab5_roue.cpp du dépôt Tab5,
//     commit e330d609 du 06/11/2026, lignes 54-157 et 419-429 extraites telles quelles, jamais retranscrites ; les mesures du texte par
//     LVGL, impossibles hors de l'appareil, sont remplacées par les largeurs de la colonne « demandée »). Seule la dimension des
//     constantes change pour la 7B à l'échelle 0,8 (valeur × 0,8 arrondie, écran 1024 x 600) : le code n'est pas modifié ;
//   - 138 centres des anneaux de ces tableaux ont aussi été comparés, à moins d'un pixel près, à la trigonométrie flottante
//     (math.cos et math.sin d'un angle en degrés, sans table ni entier) quand l'éventail n'a ni pivoté ni été ramené dans l'écran ;
//   - quelques valeurs sont refaites à la main plus bas (cos 60° = 0,5, 290 × cos 70° = 99,19, …).
// Les mots demandés (90, 70, 62, 94, 62, 96 px…) sont des nombres d'essai, pas des mesures de police.
// Le balayage de tout l'écran est dans test_roue_balayage.cpp (exécuté seulement, sous AddressSanitizer et UBSan).
#include "ecran/roue_geometrie.hpp"
#include "mini.hpp"

#include <initializer_list>

using namespace ecran::roue;

namespace {

struct RefPremier {
    int xa, ya, n;
    bool dessous;
    int x[6], y[6], angle[6];
};
struct RefSecond {
    int xa, ya, n, famille, m;
    bool dessous;
    int x[8], y[8], angle[8];
};
struct RefMots {
    int anneau, xa, ya, n;
    int demande[6], x[6], y[6], largeur[6];
};

// clang-format off
// ---- sortie de l'oracle du Tab5 (disposer() compilée à part) ; une ligne par essai, le nom de l'ancre en commentaire ----
// Premier anneau : {xa, ya, n, dessous, x[], y[], angle[]}
inline constexpr RefPremier REF_PREMIER_TAB5[] = {
    {640, 360, 1, true, {640, 0, 0, 0, 0, 0}, {540, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // centre
    {640, 360, 3, true, {550, 640, 730, 0, 0, 0}, {516, 540, 516, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // centre
    {640, 360, 6, true, {466, 513, 593, 687, 767, 814}, {407, 487, 534, 534, 487, 407}, {165, 135, 105, 75, 45, 15}}, // centre
    {640, 400, 1, false, {640, 0, 0, 0, 0, 0}, {220, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // centre_dessus
    {640, 400, 3, false, {550, 640, 730, 0, 0, 0}, {244, 220, 244, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // centre_dessus
    {640, 400, 6, false, {466, 513, 593, 687, 767, 814}, {353, 273, 226, 226, 273, 353}, {165, 135, 105, 75, 45, 15}}, // centre_dessus
    {0, 400, 1, false, {62, 0, 0, 0, 0, 0}, {231, 0, 0, 0, 0, 0}, {70, 0, 0, 0, 0, 0}}, // bord_gauche
    {0, 400, 3, false, {62, 138, 177, 0, 0, 0}, {231, 284, 369, 0, 0, 0}, {70, 40, 10, 0, 0, 0}}, // bord_gauche
    {0, 400, 6, false, {174, 127, 48, 48, 48, 48}, {447, 527, 574, 574, 527, 447}, {-15, -45, -75, -105, -135, -165}}, // bord_gauche
    {1279, 400, 1, false, {1232, 0, 0, 0, 0, 0}, {226, 0, 0, 0, 0, 0}, {105, 0, 0, 0, 0, 0}}, // bord_droit
    {1279, 400, 3, false, {1105, 1152, 1232, 0, 0, 0}, {353, 273, 226, 0, 0, 0}, {165, 135, 105, 0, 0, 0}}, // bord_droit
    {1279, 400, 6, false, {1232, 1152, 1105, 1105, 1152, 1232}, {574, 527, 447, 353, 273, 226}, {255, 225, 195, 165, 135, 105}}, // bord_droit
    {640, 0, 1, true, {640, 0, 0, 0, 0, 0}, {180, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // bord_haut
    {640, 0, 3, true, {550, 640, 730, 0, 0, 0}, {156, 180, 156, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // bord_haut
    {640, 0, 6, true, {466, 513, 593, 687, 767, 814}, {48, 127, 174, 174, 127, 48}, {165, 135, 105, 75, 45, 15}}, // bord_haut
    {640, 719, 1, false, {640, 0, 0, 0, 0, 0}, {539, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // bord_bas
    {640, 719, 3, false, {550, 640, 730, 0, 0, 0}, {563, 539, 563, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // bord_bas
    {640, 719, 6, false, {466, 513, 593, 687, 767, 814}, {672, 592, 545, 545, 592, 672}, {165, 135, 105, 75, 45, 15}}, // bord_bas
    {0, 0, 1, true, {62, 0, 0, 0, 0, 0}, {169, 0, 0, 0, 0, 0}, {70, 0, 0, 0, 0, 0}}, // coin_haut_gauche
    {0, 0, 3, true, {62, 138, 177, 0, 0, 0}, {169, 116, 48, 0, 0, 0}, {70, 40, 10, 0, 0, 0}}, // coin_haut_gauche
    {0, 0, 6, true, {174, 127, 48, 48, 48, 48}, {48, 48, 48, 48, 48, 48}, {-15, -45, -75, -105, -135, -165}}, // coin_haut_gauche
    {1279, 0, 1, true, {1232, 0, 0, 0, 0, 0}, {174, 0, 0, 0, 0, 0}, {105, 0, 0, 0, 0, 0}}, // coin_haut_droit
    {1279, 0, 3, true, {1110, 1163, 1232, 0, 0, 0}, {62, 138, 177, 0, 0, 0}, {160, 130, 100, 0, 0, 0}}, // coin_haut_droit
    {1279, 0, 6, true, {1232, 1152, 1105, 1105, 1152, 1232}, {48, 48, 48, 48, 127, 174}, {255, 225, 195, 165, 135, 105}}, // coin_haut_droit
    {0, 719, 1, false, {62, 0, 0, 0, 0, 0}, {550, 0, 0, 0, 0, 0}, {70, 0, 0, 0, 0, 0}}, // coin_bas_gauche
    {0, 719, 3, false, {62, 138, 177, 0, 0, 0}, {550, 603, 672, 0, 0, 0}, {70, 40, 10, 0, 0, 0}}, // coin_bas_gauche
    {0, 719, 6, false, {174, 127, 48, 48, 48, 48}, {672, 672, 672, 672, 672, 672}, {-15, -45, -75, -105, -135, -165}}, // coin_bas_gauche
    {1279, 719, 1, false, {1232, 0, 0, 0, 0, 0}, {545, 0, 0, 0, 0, 0}, {105, 0, 0, 0, 0, 0}}, // coin_bas_droit
    {1279, 719, 3, false, {1105, 1152, 1232, 0, 0, 0}, {672, 592, 545, 0, 0, 0}, {165, 135, 105, 0, 0, 0}}, // coin_bas_droit
    {1279, 719, 6, false, {1232, 1152, 1105, 1105, 1152, 1232}, {672, 672, 672, 672, 592, 545}, {255, 225, 195, 165, 135, 105}}, // coin_bas_droit
    {780, 470, 1, false, {780, 0, 0, 0, 0, 0}, {290, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // esquisse
    {780, 470, 3, false, {690, 780, 870, 0, 0, 0}, {314, 290, 314, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // esquisse
    {780, 470, 6, false, {606, 653, 733, 827, 907, 954}, {423, 343, 296, 296, 343, 423}, {165, 135, 105, 75, 45, 15}}, // esquisse
};
// Second anneau : {xa, ya, n du premier, famille, m, dessous, x[], y[], angle[]}
inline constexpr RefSecond REF_SECOND_TAB5[] = {
    {640, 400, 6, 0, 3, false, {351, 360, 402, 0, 0, 0, 0, 0}, {425, 325, 234, 0, 0, 0, 0, 0}, {185, 165, 145, 0, 0, 0, 0, 0}}, // centre_dessus
    {640, 400, 6, 2, 8, false, {351, 377, 435, 517, 615, 715, 806, 878}, {375, 277, 195, 137, 111, 120, 162, 234}, {175, 155, 135, 115, 95, 75, 55, 35}}, // centre_dessus
    {640, 400, 6, 5, 8, false, {665, 763, 845, 903, 929, 920, 878, 806}, {111, 137, 195, 277, 375, 475, 566, 638}, {85, 65, 45, 25, 5, -15, -35, -55}}, // centre_dessus
    {640, 400, 3, 1, 8, false, {367, 418, 495, 590, 690, 785, 862, 913}, {301, 214, 149, 114, 114, 149, 214, 301}, {160, 140, 120, 100, 80, 60, 40, 20}}, // centre_dessus
    {780, 470, 6, 3, 5, false, {657, 755, 855, 946, 1018, 0, 0, 0}, {207, 181, 190, 232, 304, 0, 0, 0}, {115, 95, 75, 55, 35, 0, 0, 0}}, // esquisse
    {200, 500, 6, 1, 6, false, {55, 150, 250, 345, 422, 473, 0, 0}, {249, 214, 214, 249, 314, 401, 0, 0}, {120, 100, 80, 60, 40, 20, 0, 0}}, // x200y500
    {1100, 300, 6, 4, 8, true, {837, 811, 820, 862, 934, 1025, 1125, 1223}, {177, 275, 375, 466, 538, 580, 589, 563}, {205, 185, 165, 145, 125, 105, 85, 65}}, // x1100y300
    {0, 400, 6, 2, 8, false, {48, 48, 48, 48, 48, 75, 166, 238}, {375, 277, 195, 137, 111, 120, 162, 234}, {-185, -205, -225, -245, -265, -285, -305, -325}}, // bord_gauche
    {640, 360, 6, 2, 8, true, {351, 377, 435, 517, 615, 715, 806, 878}, {385, 483, 565, 623, 649, 640, 598, 526}, {175, 155, 135, 115, 95, 75, 55, 35}}, // centre
    {780, 470, 6, 3, 1, false, {855, 0, 0, 0, 0, 0, 0, 0}, {190, 0, 0, 0, 0, 0, 0, 0}, {75, 0, 0, 0, 0, 0, 0, 0}}, // esquisse
};
// Mots : {anneau (1 ou 2), xa, ya, n, largeur demandée[], x du mot[], y du mot[], largeur obtenue[]}
inline constexpr RefMots REF_MOTS_TAB5[] = {
    {1, 780, 470, 6, {90, 70, 62, 94, 62, 96}, {522, 600, 717, 844, 958, 1041}, {401, 290, 236, 232, 292, 400}, {90, 70, 62, 94, 62, 96}}, // esquisse
    {1, 780, 470, 6, {150, 150, 150, 150, 150, 150}, {494, 580, 715, 845, 980, 1066}, {393, 270, 226, 226, 270, 393}, {150, 150, 122, 122, 150, 150}}, // esquisse larges
    {2, 780, 470, 5, {80, 72, 66, 88, 60, 0}, {628, 750, 871, 990, 1077, 0}, {144, 124, 129, 170, 262, 0}, {80, 72, 66, 88, 60, 0}}, // esquisse f3
    {2, 780, 470, 5, {150, 150, 150, 150, 150, 0}, {622, 749, 874, 1000, 1107, 0}, {130, 120, 119, 155, 241, 0}, {119, 117, 117, 150, 150, 0}}, // esquisse f3 larges
    {1, 640, 400, 6, {90, 70, 62, 94, 62, 96}, {382, 460, 577, 704, 818, 901}, {331, 220, 166, 162, 222, 330}, {90, 70, 62, 94, 62, 96}}, // centre_dessus
    {1, 640, 400, 6, {150, 150, 150, 150, 150, 150}, {354, 440, 575, 705, 840, 926}, {323, 200, 156, 156, 200, 323}, {150, 150, 122, 122, 150, 150}}, // centre_dessus larges
    {2, 640, 400, 5, {80, 72, 66, 88, 60, 0}, {488, 610, 731, 850, 937, 0}, {74, 54, 59, 100, 192, 0}, {80, 72, 66, 88, 60, 0}}, // centre_dessus f3
    {2, 640, 400, 5, {150, 150, 150, 150, 150, 0}, {482, 609, 734, 860, 967, 0}, {60, 50, 49, 85, 171, 0}, {119, 117, 117, 150, 150, 0}}, // centre_dessus f3 larges
    {1, 200, 500, 6, {90, 70, 62, 94, 62, 96}, {-17, 95, 221, 348, 430, 468}, {348, 275, 264, 289, 393, 523}, {90, 70, 62, 94, 62, 96}}, // x200y500
    {1, 200, 500, 6, {150, 150, 150, 150, 150, 150}, {-37, 88, 221, 357, 466, 495}, {334, 260, 260, 276, 376, 526}, {150, 125, 125, 128, 150, 150}}, // x200y500 larges
    {2, 200, 500, 5, {80, 72, 66, 88, 60, 0}, {170, 291, 406, 506, 551, 0}, {154, 159, 205, 285, 406, 0}, {80, 72, 66, 88, 60, 0}}, // x200y500 f3
    {2, 200, 500, 5, {150, 150, 150, 150, 150, 0}, {169, 294, 420, 527, 592, 0}, {150, 149, 185, 271, 395, 0}, {117, 117, 150, 150, 150, 0}}, // x200y500 f3 larges
    {1, 640, 360, 6, {90, 70, 62, 94, 62, 96}, {382, 460, 577, 704, 818, 901}, {429, 540, 594, 598, 538, 430}, {90, 70, 62, 94, 62, 96}}, // centre
    {1, 640, 360, 6, {150, 150, 150, 150, 150, 150}, {354, 440, 575, 705, 840, 926}, {437, 560, 604, 604, 560, 437}, {150, 150, 122, 122, 150, 150}}, // centre larges
    {2, 640, 360, 5, {80, 72, 66, 88, 60, 0}, {488, 610, 731, 850, 937, 0}, {686, 706, 701, 660, 568, 0}, {80, 72, 66, 88, 60, 0}}, // centre f3
    {2, 640, 360, 5, {150, 150, 150, 150, 150, 0}, {482, 609, 734, 860, 967, 0}, {700, 710, 711, 675, 589, 0}, {119, 117, 117, 150, 150, 0}}, // centre f3 larges
    {1, 0, 0, 6, {90, 70, 62, 94, 62, 96}, {258, 180, 63, -64, -178, -261}, {-69, -180, -234, -238, -178, -70}, {90, 70, 62, 94, 62, 96}}, // coin_haut_gauche
    {1, 0, 0, 6, {150, 150, 150, 150, 150, 150}, {286, 200, 65, -65, -200, -286}, {-77, -200, -244, -244, -200, -77}, {150, 150, 122, 122, 150, 150}}, // coin_haut_gauche larges
    {2, 0, 0, 5, {80, 72, 66, 88, 60, 0}, {-152, -30, 91, 210, 297, 0}, {326, 346, 341, 300, 208, 0}, {80, 72, 66, 88, 60, 0}}, // coin_haut_gauche f3
    {2, 0, 0, 5, {150, 150, 150, 150, 150, 0}, {-158, -31, 94, 220, 327, 0}, {340, 350, 351, 315, 229, 0}, {119, 117, 117, 150, 150, 0}}, // coin_haut_gauche f3 larges
    {1, 0, 400, 6, {90, 70, 62, 94, 62, 96}, {258, 180, 63, -64, -178, -261}, {469, 580, 634, 638, 578, 470}, {90, 70, 62, 94, 62, 96}}, // bord_gauche
    {1, 0, 400, 6, {150, 150, 150, 150, 150, 150}, {286, 200, 65, -65, -200, -286}, {477, 600, 644, 644, 600, 477}, {150, 150, 122, 122, 150, 150}}, // bord_gauche larges
    {2, 0, 400, 5, {80, 72, 66, 88, 60, 0}, {-152, -30, 91, 210, 297, 0}, {74, 54, 59, 100, 192, 0}, {80, 72, 66, 88, 60, 0}}, // bord_gauche f3
    {2, 0, 400, 5, {150, 150, 150, 150, 150, 0}, {-158, -31, 94, 220, 327, 0}, {60, 50, 49, 85, 171, 0}, {119, 117, 117, 150, 150, 0}}, // bord_gauche f3 larges
};

// Premier anneau : {xa, ya, n, dessous, x[], y[], angle[]}
inline constexpr RefPremier REF_PREMIER_7B_0_8[] = {
    {512, 300, 1, false, {512, 0, 0, 0, 0, 0}, {156, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // centre
    {512, 300, 3, false, {440, 512, 584, 0, 0, 0}, {175, 156, 175, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // centre
    {512, 300, 6, false, {373, 410, 475, 549, 614, 651}, {263, 198, 161, 161, 198, 263}, {165, 135, 105, 75, 45, 15}}, // centre
    {512, 320, 1, false, {512, 0, 0, 0, 0, 0}, {176, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // centre_dessus
    {512, 320, 3, false, {440, 512, 584, 0, 0, 0}, {195, 176, 195, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // centre_dessus
    {512, 320, 6, false, {373, 410, 475, 549, 614, 651}, {283, 218, 181, 181, 218, 283}, {165, 135, 105, 75, 45, 15}}, // centre_dessus
    {0, 320, 1, false, {49, 0, 0, 0, 0, 0}, {185, 0, 0, 0, 0, 0}, {70, 0, 0, 0, 0, 0}}, // bord_gauche
    {0, 320, 3, false, {49, 110, 142, 0, 0, 0}, {185, 227, 295, 0, 0, 0}, {70, 40, 10, 0, 0, 0}}, // bord_gauche
    {0, 320, 6, false, {139, 102, 39, 39, 39, 39}, {357, 422, 459, 459, 422, 357}, {-15, -45, -75, -105, -135, -165}}, // bord_gauche
    {1023, 320, 1, false, {974, 0, 0, 0, 0, 0}, {185, 0, 0, 0, 0, 0}, {110, 0, 0, 0, 0, 0}}, // bord_droit
    {1023, 320, 3, false, {881, 913, 974, 0, 0, 0}, {295, 227, 185, 0, 0, 0}, {170, 140, 110, 0, 0, 0}}, // bord_droit
    {1023, 320, 6, false, {985, 985, 985, 985, 921, 884}, {357, 422, 459, 459, 422, 357}, {345, 315, 285, 255, 225, 195}}, // bord_droit
    {512, 0, 1, true, {512, 0, 0, 0, 0, 0}, {144, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // bord_haut
    {512, 0, 3, true, {440, 512, 584, 0, 0, 0}, {125, 144, 125, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // bord_haut
    {512, 0, 6, true, {373, 410, 475, 549, 614, 651}, {39, 102, 139, 139, 102, 39}, {165, 135, 105, 75, 45, 15}}, // bord_haut
    {512, 599, 1, false, {512, 0, 0, 0, 0, 0}, {455, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // bord_bas
    {512, 599, 3, false, {440, 512, 584, 0, 0, 0}, {474, 455, 474, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // bord_bas
    {512, 599, 6, false, {373, 410, 475, 549, 614, 651}, {561, 497, 460, 460, 497, 561}, {165, 135, 105, 75, 45, 15}}, // bord_bas
    {0, 0, 1, true, {49, 0, 0, 0, 0, 0}, {135, 0, 0, 0, 0, 0}, {70, 0, 0, 0, 0, 0}}, // coin_haut_gauche
    {0, 0, 3, true, {49, 110, 142, 0, 0, 0}, {135, 93, 39, 0, 0, 0}, {70, 40, 10, 0, 0, 0}}, // coin_haut_gauche
    {0, 0, 6, true, {139, 102, 39, 39, 39, 39}, {39, 39, 39, 39, 39, 39}, {-15, -45, -75, -105, -135, -165}}, // coin_haut_gauche
    {1023, 0, 1, true, {974, 0, 0, 0, 0, 0}, {135, 0, 0, 0, 0, 0}, {110, 0, 0, 0, 0, 0}}, // coin_haut_droit
    {1023, 0, 3, true, {881, 913, 974, 0, 0, 0}, {39, 93, 135, 0, 0, 0}, {170, 140, 110, 0, 0, 0}}, // coin_haut_droit
    {1023, 0, 6, true, {985, 985, 985, 985, 921, 884}, {39, 39, 39, 39, 39, 39}, {345, 315, 285, 255, 225, 195}}, // coin_haut_droit
    {0, 599, 1, false, {49, 0, 0, 0, 0, 0}, {464, 0, 0, 0, 0, 0}, {70, 0, 0, 0, 0, 0}}, // coin_bas_gauche
    {0, 599, 3, false, {49, 110, 142, 0, 0, 0}, {464, 506, 561, 0, 0, 0}, {70, 40, 10, 0, 0, 0}}, // coin_bas_gauche
    {0, 599, 6, false, {139, 102, 39, 39, 39, 39}, {561, 561, 561, 561, 561, 561}, {-15, -45, -75, -105, -135, -165}}, // coin_bas_gauche
    {1023, 599, 1, false, {974, 0, 0, 0, 0, 0}, {464, 0, 0, 0, 0, 0}, {110, 0, 0, 0, 0, 0}}, // coin_bas_droit
    {1023, 599, 3, false, {881, 913, 974, 0, 0, 0}, {561, 506, 464, 0, 0, 0}, {170, 140, 110, 0, 0, 0}}, // coin_bas_droit
    {1023, 599, 6, false, {985, 985, 985, 985, 921, 884}, {561, 561, 561, 561, 561, 561}, {345, 315, 285, 255, 225, 195}}, // coin_bas_droit
    {624, 376, 1, false, {624, 0, 0, 0, 0, 0}, {232, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // esquisse
    {624, 376, 3, false, {552, 624, 696, 0, 0, 0}, {251, 232, 251, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // esquisse
    {624, 376, 6, false, {485, 522, 587, 661, 726, 763}, {339, 274, 237, 237, 274, 339}, {165, 135, 105, 75, 45, 15}}, // esquisse
};
// Second anneau : {xa, ya, n du premier, famille, m, dessous, x[], y[], angle[]}
inline constexpr RefSecond REF_SECOND_7B_0_8[] = {
    {512, 320, 6, 0, 3, false, {281, 288, 322, 0, 0, 0, 0, 0}, {340, 260, 187, 0, 0, 0, 0, 0}, {185, 165, 145, 0, 0, 0, 0, 0}}, // centre_dessus
    {512, 320, 6, 2, 8, false, {281, 302, 348, 414, 492, 572, 645, 702}, {300, 222, 156, 110, 89, 96, 130, 187}, {175, 155, 135, 115, 95, 75, 55, 35}}, // centre_dessus
    {512, 320, 6, 5, 8, false, {532, 610, 676, 722, 743, 736, 702, 645}, {89, 110, 156, 222, 300, 380, 453, 510}, {85, 65, 45, 25, 5, -15, -35, -55}}, // centre_dessus
    {512, 320, 3, 1, 8, false, {294, 334, 396, 472, 552, 628, 690, 730}, {241, 171, 119, 92, 92, 119, 171, 241}, {160, 140, 120, 100, 80, 60, 40, 20}}, // centre_dessus
    {624, 376, 6, 3, 5, false, {526, 604, 684, 757, 814, 0, 0, 0}, {166, 145, 152, 186, 243, 0, 0, 0}, {115, 95, 75, 55, 35, 0, 0, 0}}, // esquisse
    {160, 400, 6, 1, 6, false, {44, 120, 200, 276, 338, 378, 0, 0}, {199, 172, 172, 199, 251, 321, 0, 0}, {120, 100, 80, 60, 40, 20, 0, 0}}, // x160y400
    {880, 240, 6, 4, 8, true, {670, 649, 656, 690, 747, 820, 900, 978}, {142, 220, 300, 373, 430, 464, 471, 450}, {205, 185, 165, 145, 125, 105, 85, 65}}, // x880y240
    {0, 320, 6, 2, 8, false, {39, 39, 39, 39, 39, 60, 133, 190}, {300, 222, 156, 110, 89, 96, 130, 187}, {-185, -205, -225, -245, -265, -285, -305, -325}}, // bord_gauche
    {512, 300, 6, 2, 8, false, {281, 302, 348, 414, 492, 572, 645, 702}, {280, 202, 136, 90, 69, 76, 110, 167}, {175, 155, 135, 115, 95, 75, 55, 35}}, // centre
    {624, 376, 6, 3, 1, false, {684, 0, 0, 0, 0, 0, 0, 0}, {152, 0, 0, 0, 0, 0, 0, 0}, {75, 0, 0, 0, 0, 0, 0, 0}}, // esquisse
};
// Mots : {anneau (1 ou 2), xa, ya, n, largeur demandée[], x du mot[], y du mot[], largeur obtenue[]}
inline constexpr RefMots REF_MOTS_7B_0_8[] = {
    {1, 624, 376, 6, {72, 56, 50, 75, 50, 77}, {417, 480, 574, 675, 767, 833}, {321, 232, 190, 186, 233, 320}, {72, 56, 50, 75, 50, 77}}, // esquisse
    {1, 624, 376, 6, {120, 120, 120, 120, 120, 120}, {395, 464, 571, 677, 784, 853}, {315, 216, 180, 180, 216, 315}, {120, 120, 100, 100, 120, 120}}, // esquisse larges
    {2, 624, 376, 5, {64, 58, 53, 70, 48, 0}, {502, 600, 697, 792, 862, 0}, {115, 99, 104, 136, 210, 0}, {64, 58, 53, 70, 48, 0}}, // esquisse f3
    {2, 624, 376, 5, {120, 120, 120, 120, 120, 0}, {498, 600, 699, 800, 885, 0}, {105, 97, 95, 125, 193, 0}, {96, 93, 93, 120, 120, 0}}, // esquisse f3 larges
    {1, 512, 320, 6, {72, 56, 50, 75, 50, 77}, {305, 368, 462, 563, 655, 721}, {265, 176, 134, 130, 177, 264}, {72, 56, 50, 75, 50, 77}}, // centre_dessus
    {1, 512, 320, 6, {120, 120, 120, 120, 120, 120}, {283, 352, 459, 565, 672, 741}, {259, 160, 124, 124, 160, 259}, {120, 120, 100, 100, 120, 120}}, // centre_dessus larges
    {2, 512, 320, 5, {64, 58, 53, 70, 48, 0}, {390, 488, 585, 680, 750, 0}, {59, 43, 48, 80, 154, 0}, {64, 58, 53, 70, 48, 0}}, // centre_dessus f3
    {2, 512, 320, 5, {120, 120, 120, 120, 120, 0}, {386, 488, 587, 688, 773, 0}, {49, 41, 39, 69, 137, 0}, {96, 93, 93, 120, 120, 0}}, // centre_dessus f3 larges
    {1, 160, 400, 6, {72, 56, 50, 75, 50, 77}, {-13, 76, 176, 278, 345, 374}, {279, 221, 212, 231, 314, 419}, {72, 56, 50, 75, 50, 77}}, // x160y400
    {1, 160, 400, 6, {120, 120, 120, 120, 120, 120}, {-29, 71, 177, 286, 373, 396}, {268, 209, 209, 221, 301, 421}, {120, 100, 100, 103, 120, 120}}, // x160y400 larges
    {2, 160, 400, 5, {64, 58, 53, 70, 48, 0}, {136, 233, 325, 405, 440, 0}, {123, 127, 164, 229, 325, 0}, {64, 58, 53, 70, 48, 0}}, // x160y400 f3
    {2, 160, 400, 5, {120, 120, 120, 120, 120, 0}, {136, 235, 336, 421, 474, 0}, {121, 119, 149, 217, 316, 0}, {93, 93, 120, 120, 120, 0}}, // x160y400 f3 larges
    {1, 512, 300, 6, {72, 56, 50, 75, 50, 77}, {305, 368, 462, 563, 655, 721}, {245, 156, 114, 110, 157, 244}, {72, 56, 50, 75, 50, 77}}, // centre
    {1, 512, 300, 6, {120, 120, 120, 120, 120, 120}, {283, 352, 459, 565, 672, 741}, {239, 140, 104, 104, 140, 239}, {120, 120, 100, 100, 120, 120}}, // centre larges
    {2, 512, 300, 5, {64, 58, 53, 70, 48, 0}, {390, 488, 585, 680, 750, 0}, {39, 23, 28, 60, 134, 0}, {64, 58, 53, 70, 48, 0}}, // centre f3
    {2, 512, 300, 5, {120, 120, 120, 120, 120, 0}, {386, 488, 587, 688, 773, 0}, {29, 21, 19, 49, 117, 0}, {96, 93, 93, 120, 120, 0}}, // centre f3 larges
    {1, 0, 0, 6, {72, 56, 50, 75, 50, 77}, {207, 144, 50, -51, -143, -209}, {-55, -144, -186, -190, -143, -56}, {72, 56, 50, 75, 50, 77}}, // coin_haut_gauche
    {1, 0, 0, 6, {120, 120, 120, 120, 120, 120}, {229, 160, 53, -53, -160, -229}, {-61, -160, -196, -196, -160, -61}, {120, 120, 100, 100, 120, 120}}, // coin_haut_gauche larges
    {2, 0, 0, 5, {64, 58, 53, 70, 48, 0}, {-122, -24, 73, 168, 238, 0}, {261, 277, 272, 240, 166, 0}, {64, 58, 53, 70, 48, 0}}, // coin_haut_gauche f3
    {2, 0, 0, 5, {120, 120, 120, 120, 120, 0}, {-126, -24, 75, 176, 261, 0}, {271, 279, 281, 251, 183, 0}, {96, 93, 93, 120, 120, 0}}, // coin_haut_gauche f3 larges
};

// Premier anneau : {xa, ya, n, dessous, x[], y[], angle[]}
inline constexpr RefPremier REF_PREMIER_7B_1_0[] = {
    {512, 300, 1, true, {512, 0, 0, 0, 0, 0}, {480, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // centre
    {512, 300, 3, true, {422, 512, 602, 0, 0, 0}, {456, 480, 456, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // centre
    {512, 300, 6, true, {338, 385, 465, 559, 639, 686}, {347, 427, 474, 474, 427, 347}, {165, 135, 105, 75, 45, 15}}, // centre
    {512, 320, 1, true, {512, 0, 0, 0, 0, 0}, {500, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // centre_dessus
    {512, 320, 3, true, {422, 512, 602, 0, 0, 0}, {476, 500, 476, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // centre_dessus
    {512, 320, 6, true, {338, 385, 465, 559, 639, 686}, {367, 447, 494, 494, 447, 367}, {165, 135, 105, 75, 45, 15}}, // centre_dessus
    {0, 320, 1, true, {62, 0, 0, 0, 0, 0}, {489, 0, 0, 0, 0, 0}, {70, 0, 0, 0, 0, 0}}, // bord_gauche
    {0, 320, 3, true, {62, 138, 177, 0, 0, 0}, {489, 436, 351, 0, 0, 0}, {70, 40, 10, 0, 0, 0}}, // bord_gauche
    {0, 320, 6, true, {174, 127, 48, 48, 48, 48}, {273, 193, 146, 146, 193, 273}, {-15, -45, -75, -105, -135, -165}}, // bord_gauche
    {1023, 320, 1, true, {976, 0, 0, 0, 0, 0}, {494, 0, 0, 0, 0, 0}, {105, 0, 0, 0, 0, 0}}, // bord_droit
    {1023, 320, 3, true, {849, 896, 976, 0, 0, 0}, {367, 447, 494, 0, 0, 0}, {165, 135, 105, 0, 0, 0}}, // bord_droit
    {1023, 320, 6, true, {976, 896, 849, 849, 896, 976}, {146, 193, 273, 367, 447, 494}, {255, 225, 195, 165, 135, 105}}, // bord_droit
    {512, 0, 1, true, {512, 0, 0, 0, 0, 0}, {180, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // bord_haut
    {512, 0, 3, true, {422, 512, 602, 0, 0, 0}, {156, 180, 156, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // bord_haut
    {512, 0, 6, true, {338, 385, 465, 559, 639, 686}, {48, 127, 174, 174, 127, 48}, {165, 135, 105, 75, 45, 15}}, // bord_haut
    {512, 599, 1, false, {512, 0, 0, 0, 0, 0}, {419, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // bord_bas
    {512, 599, 3, false, {422, 512, 602, 0, 0, 0}, {443, 419, 443, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // bord_bas
    {512, 599, 6, false, {338, 385, 465, 559, 639, 686}, {552, 472, 425, 425, 472, 552}, {165, 135, 105, 75, 45, 15}}, // bord_bas
    {0, 0, 1, true, {62, 0, 0, 0, 0, 0}, {169, 0, 0, 0, 0, 0}, {70, 0, 0, 0, 0, 0}}, // coin_haut_gauche
    {0, 0, 3, true, {62, 138, 177, 0, 0, 0}, {169, 116, 48, 0, 0, 0}, {70, 40, 10, 0, 0, 0}}, // coin_haut_gauche
    {0, 0, 6, true, {174, 127, 48, 48, 48, 48}, {48, 48, 48, 48, 48, 48}, {-15, -45, -75, -105, -135, -165}}, // coin_haut_gauche
    {1023, 0, 1, true, {976, 0, 0, 0, 0, 0}, {174, 0, 0, 0, 0, 0}, {105, 0, 0, 0, 0, 0}}, // coin_haut_droit
    {1023, 0, 3, true, {854, 907, 976, 0, 0, 0}, {62, 138, 177, 0, 0, 0}, {160, 130, 100, 0, 0, 0}}, // coin_haut_droit
    {1023, 0, 6, true, {976, 896, 849, 849, 896, 976}, {48, 48, 48, 48, 127, 174}, {255, 225, 195, 165, 135, 105}}, // coin_haut_droit
    {0, 599, 1, false, {62, 0, 0, 0, 0, 0}, {430, 0, 0, 0, 0, 0}, {70, 0, 0, 0, 0, 0}}, // coin_bas_gauche
    {0, 599, 3, false, {62, 138, 177, 0, 0, 0}, {430, 483, 552, 0, 0, 0}, {70, 40, 10, 0, 0, 0}}, // coin_bas_gauche
    {0, 599, 6, false, {174, 127, 48, 48, 48, 48}, {552, 552, 552, 552, 552, 552}, {-15, -45, -75, -105, -135, -165}}, // coin_bas_gauche
    {1023, 599, 1, false, {976, 0, 0, 0, 0, 0}, {425, 0, 0, 0, 0, 0}, {105, 0, 0, 0, 0, 0}}, // coin_bas_droit
    {1023, 599, 3, false, {849, 896, 976, 0, 0, 0}, {552, 472, 425, 0, 0, 0}, {165, 135, 105, 0, 0, 0}}, // coin_bas_droit
    {1023, 599, 6, false, {976, 896, 849, 849, 896, 976}, {552, 552, 552, 552, 472, 425}, {255, 225, 195, 165, 135, 105}}, // coin_bas_droit
    {624, 376, 1, false, {624, 0, 0, 0, 0, 0}, {196, 0, 0, 0, 0, 0}, {90, 0, 0, 0, 0, 0}}, // esquisse
    {624, 376, 3, false, {534, 624, 714, 0, 0, 0}, {220, 196, 220, 0, 0, 0}, {120, 90, 60, 0, 0, 0}}, // esquisse
    {624, 376, 6, false, {450, 497, 577, 671, 751, 798}, {329, 249, 202, 202, 249, 329}, {165, 135, 105, 75, 45, 15}}, // esquisse
};
// Second anneau : {xa, ya, n du premier, famille, m, dessous, x[], y[], angle[]}
inline constexpr RefSecond REF_SECOND_7B_1_0[] = {
    {512, 320, 6, 2, 8, true, {537, 635, 717, 775, 801, 792, 750, 678}, {552, 552, 525, 443, 345, 245, 154, 82}, {85, 65, 45, 25, 5, -15, -35, -55}}, // centre_dessus
    {624, 376, 6, 3, 5, false, {501, 599, 699, 790, 862, 0, 0, 0}, {113, 87, 96, 138, 210, 0, 0, 0}, {115, 95, 75, 55, 35, 0, 0, 0}}, // esquisse
};
// Mots : {anneau (1 ou 2), xa, ya, n, largeur demandée[], x du mot[], y du mot[], largeur obtenue[]}
inline constexpr RefMots REF_MOTS_7B_1_0[] = {
    {1, 624, 376, 6, {90, 70, 62, 94, 62, 96}, {366, 444, 561, 688, 802, 885}, {307, 196, 142, 138, 198, 306}, {90, 70, 62, 94, 62, 96}}, // esquisse
    {1, 624, 376, 6, {150, 150, 150, 150, 150, 150}, {338, 424, 559, 689, 824, 910}, {299, 176, 132, 132, 176, 299}, {150, 150, 122, 122, 150, 150}}, // esquisse larges
    {2, 624, 376, 5, {80, 72, 66, 88, 60, 0}, {472, 594, 715, 834, 921, 0}, {50, 30, 35, 76, 168, 0}, {80, 72, 66, 88, 60, 0}}, // esquisse f3
    {2, 624, 376, 5, {150, 150, 150, 150, 150, 0}, {466, 593, 718, 844, 951, 0}, {36, 26, 25, 61, 147, 0}, {119, 117, 117, 150, 150, 0}}, // esquisse f3 larges
    {1, 0, 0, 6, {90, 70, 62, 94, 62, 96}, {258, 180, 63, -64, -178, -261}, {-69, -180, -234, -238, -178, -70}, {90, 70, 62, 94, 62, 96}}, // coin_haut_gauche
    {1, 0, 0, 6, {150, 150, 150, 150, 150, 150}, {286, 200, 65, -65, -200, -286}, {-77, -200, -244, -244, -200, -77}, {150, 150, 122, 122, 150, 150}}, // coin_haut_gauche larges
    {2, 0, 0, 5, {80, 72, 66, 88, 60, 0}, {-152, -30, 91, 210, 297, 0}, {326, 346, 341, 300, 208, 0}, {80, 72, 66, 88, 60, 0}}, // coin_haut_gauche f3
    {2, 0, 0, 5, {150, 150, 150, 150, 150, 0}, {-158, -31, 94, 220, 327, 0}, {340, 350, 351, 315, 229, 0}, {119, 117, 117, 150, 150, 0}}, // coin_haut_gauche f3 larges
};
// clang-format on

constexpr Mesures M5 = mesures(ECRAN_TAB5);
constexpr Mesures M7 = mesures(ECRAN_7B);
constexpr Mesures M7_PX =
    mesures(Ecran{1024, 600, 1000}); // le Tab5 sur une carte de 1024 x 600 : mêmes pixels, autre écran

// ---- aides : les invariants, écrits ici une fois pour toutes (rien n'est emprunté à anneau_tient du noyau) ----

constexpr int carre(int v) { return v * v; }

constexpr bool premier_conforme(const Mesures &m, const RefPremier &r) {
    if (passe_dessous(m, r.ya) != r.dessous) {
        return false;
    }
    const Anneau a = disposer(m, Point{r.xa, r.ya}, r.n, m.rayon_1, m.pas_1, 90, m.diametre_1 / 2, r.dessous);
    if (a.n != r.n) {
        return false;
    }
    for (int i = 0; i < r.n; i++) {
        if (a.bouton[i].x != r.x[i] || a.bouton[i].y != r.y[i] || a.bouton[i].angle != r.angle[i]) {
            return false;
        }
    }
    return true;
}

constexpr bool second_conforme(const Mesures &m, const RefSecond &r) {
    if (passe_dessous(m, r.ya) != r.dessous) {
        return false;
    }
    const Anneau p = disposer(m, Point{r.xa, r.ya}, r.n, m.rayon_1, m.pas_1, 90, m.diametre_1 / 2, r.dessous);
    const Anneau a =
        disposer(m, Point{r.xa, r.ya}, r.m, m.rayon_2, m.pas_2, p.bouton[r.famille].angle, m.diametre_2 / 2, r.dessous);
    for (int j = 0; j < r.m; j++) {
        if (a.bouton[j].x != r.x[j] || a.bouton[j].y != r.y[j] || a.bouton[j].angle != r.angle[j]) {
            return false;
        }
    }
    return true;
}

// Les mots : premier anneau de six boutons (anneau 1) ou second anneau déplié sur le quatrième bouton (anneau 2).
constexpr bool mots_conformes(const Mesures &m, const RefMots &r) {
    const bool sous = passe_dessous(m, r.ya);
    const Point moyeu{r.xa, r.ya};
    Anneau a = disposer(m, moyeu, 6, m.rayon_1, m.pas_1, 90, m.diametre_1 / 2, sous);
    int rayon = m.rayon_1;
    int diametre = m.diametre_1;
    if (r.anneau == 2) {
        a = disposer(m, moyeu, r.n, m.rayon_2, m.pas_2, a.bouton[3].angle, m.diametre_2 / 2, sous);
        rayon = m.rayon_2;
        diametre = m.diametre_2;
    }
    placer_mots(m, a, moyeu, sous, rayon, diametre, r.demande);
    for (int i = 0; i < r.n; i++) {
        const Mot &mot = a.bouton[i].mot;
        if (mot.x != r.x[i] || mot.y != r.y[i] || mot.largeur != r.largeur[i]) {
            return false;
        }
    }
    return true;
}

constexpr bool boutons_dans_l_ecran(const Mesures &m, const Anneau &a, int diametre) {
    const int rb = diametre / 2;
    for (int i = 0; i < a.n; i++) {
        const Bouton &b = a.bouton[i];
        if (b.x - rb < m.marge || b.x + rb > m.largeur - m.marge || b.y - rb < m.marge ||
            b.y + rb > m.hauteur - m.marge) {
            return false;
        }
    }
    return true;
}

constexpr bool boutons_sans_chevauchement(const Anneau &a, int diametre) {
    for (int i = 0; i < a.n; i++) {
        for (int j = i + 1; j < a.n; j++) {
            if (carre(a.bouton[i].x - a.bouton[j].x) + carre(a.bouton[i].y - a.bouton[j].y) < carre(diametre)) {
                return false;
            }
        }
    }
    return true;
}

constexpr bool ecart_angulaire_exact(const Anneau &a, int pas) {
    for (int i = 0; i + 1 < a.n; i++) {
        if (a.bouton[i].angle - a.bouton[i + 1].angle != pas) {
            return false;
        }
    }
    return true;
}

// Chaque centre est à `rayon` du moyeu, à un pixel près (l'arrondi de chaque coordonnée vaut au plus 0,5 px).
constexpr bool sur_le_cercle(const Anneau &a, Point moyeu, int rayon) {
    for (int i = 0; i < a.n; i++) {
        const int d2 = carre(a.bouton[i].x - moyeu.x) + carre(a.bouton[i].y - moyeu.y);
        if (d2 < carre(rayon - 1) || d2 > carre(rayon + 1)) {
            return false;
        }
    }
    return true;
}

constexpr bool mots_dans_l_ecran(const Mesures &m, const Anneau &a) {
    for (int i = 0; i < a.n; i++) {
        const Mot &mot = a.bouton[i].mot;
        if (mot.largeur == 0) {
            continue;
        }
        const int gauche = mot.x - mot.largeur / 2;
        const int haut = mot.y - m.legende_h / 2;
        if (gauche < m.marge || gauche + mot.largeur > m.largeur - m.marge || haut < m.marge ||
            haut + m.legende_h > m.hauteur - m.marge) {
            return false;
        }
    }
    return true;
}

// Aucun mot ne touche un bouton, aucun mot ne recouvre un autre mot.
constexpr bool mots_degages(const Mesures &m, const Anneau &a, int diametre) {
    const int rb = diametre / 2;
    for (int i = 0; i < a.n; i++) {
        const Mot &mot = a.bouton[i].mot;
        if (mot.largeur == 0) {
            continue;
        }
        const int gauche = mot.x - mot.largeur / 2;
        const int haut = mot.y - m.legende_h / 2;
        for (int j = 0; j < a.n; j++) {
            // point de la boîte le plus proche du centre du bouton j
            const int px = a.bouton[j].x < gauche
                               ? gauche
                               : (a.bouton[j].x > gauche + mot.largeur ? gauche + mot.largeur : a.bouton[j].x);
            const int py =
                a.bouton[j].y < haut ? haut : (a.bouton[j].y > haut + m.legende_h ? haut + m.legende_h : a.bouton[j].y);
            if (carre(a.bouton[j].x - px) + carre(a.bouton[j].y - py) < carre(rb)) {
                return false;
            }
            const Mot &autre = a.bouton[j].mot;
            if (j > i && autre.largeur != 0) {
                const int g2 = autre.x - autre.largeur / 2;
                const int h2 = autre.y - m.legende_h / 2;
                if (gauche < g2 + autre.largeur && g2 < gauche + mot.largeur && haut < h2 + m.legende_h &&
                    h2 < haut + m.legende_h) {
                    return false;
                }
            }
        }
    }
    return true;
}

constexpr Demande demande(int n, int largeur_mot = 0, int choix = 0, int mot_choix = 0) {
    Demande d;
    d.n = n;
    for (int i = 0; i < n && i < BOUTONS_MAX; i++) {
        d.mot[i] = largeur_mot;
        d.choix[i] = choix;
    }
    d.mot_choix = mot_choix;
    return d;
}

// ---- les tests ----

constexpr int les_constantes_sont_celles_du_tab5() {
    VERIFIER(BOUTONS_MAX == 6 && CHOIX_MAX == 8);
    VERIFIER(M5.largeur == 1280 && M5.hauteur == 720);
    VERIFIER(M5.rayon_1 == 180 && M5.diametre_1 == 72 && M5.pas_1 == 30);
    VERIFIER(M5.rayon_2 == 290 && M5.diametre_2 == 72 && M5.pas_2 == 20);
    VERIFIER(M5.marge == 12); // pas 32 : la mission de ce lot supposait 32, le code du Tab5 dit 12
    VERIFIER(M5.moyeu == 120 && M5.jauge == 148);
    VERIFIER(M5.legende_l == 150 && M5.legende_h == 28 && M5.legende_2 == 60);
    VERIFIER(M5.mot_ecart == 4 && M5.ecart_mots == 8 && M5.mot_min == 40);
    VERIFIER(PIVOT == 5);
    return 0;
}

constexpr int la_7b_a_l_echelle_0_8() {
    VERIFIER(M7.largeur == 1024 && M7.hauteur == 600);
    VERIFIER(M7.rayon_1 == 144 && M7.diametre_1 == 58 && M7.rayon_2 == 232 && M7.diametre_2 == 58);
    VERIFIER(M7.marge == 10 && M7.moyeu == 96 && M7.jauge == 118);
    VERIFIER(M7.legende_l == 120 && M7.legende_h == 22 && M7.legende_2 == 48);
    VERIFIER(M7.mot_ecart == 3 && M7.ecart_mots == 6 && M7.mot_min == 32);
    VERIFIER(M7.pas_1 == 30 && M7.pas_2 == 20); // les angles ne s'échelonnent pas
    // l'arrondi : 72 × 0,8 = 57,6 -> 58 ; 12 × 0,8 = 9,6 -> 10 ; 28 × 0,8 = 22,4 -> 22 ; 4 × 0,8 = 3,2 -> 3 ; 5 × 0,5 = 2,5 -> 3
    VERIFIER(mettre_a_l_echelle(72, 800) == 58 && mettre_a_l_echelle(12, 800) == 10 &&
             mettre_a_l_echelle(28, 800) == 22);
    VERIFIER(mettre_a_l_echelle(4, 800) == 3 && mettre_a_l_echelle(5, 500) == 3 && mettre_a_l_echelle(7, 1000) == 7);
    // le Tab5 sur une carte de 1024 x 600 garde ses pixels : seul l'écran change
    VERIFIER(M7_PX.rayon_1 == 180 && M7_PX.diametre_1 == 72 && M7_PX.largeur == 1024 && M7_PX.hauteur == 600);
    return 0;
}

constexpr int la_table_des_sinus_est_celle_du_tab5() {
    // sin(k × 5°) × 32767, arrondis à l'entier le plus proche, calculés en flottant à part (math.sin) : les 19 valeurs.
    constexpr int ref[19] = {0,     2856,  5690,  8481,  11207, 13848, 16383, 18794, 21062, 23170,
                             25101, 26841, 28377, 29697, 30791, 31650, 32269, 32642, 32767};
    for (int k = 0; k <= 18; k++) {
        VERIFIER(sinus5(k * 5) == ref[k]);
    }
    // quadrants, périodes, angles négatifs
    VERIFIER(sinus5(0) == 0 && sinus5(90) == 32767 && sinus5(180) == 0 && sinus5(270) == -32767);
    VERIFIER(sinus5(120) == sinus5(60) && sinus5(150) == sinus5(30) && sinus5(210) == -sinus5(30));
    VERIFIER(sinus5(-30) == -sinus5(30) && sinus5(390) == sinus5(30) && sinus5(-330) == sinus5(30));
    VERIFIER(cosinus5(0) == 32767 && cosinus5(90) == 0 && cosinus5(180) == -32767 && cosinus5(60) == 16383);
    VERIFIER(cosinus5(120) == -16383 && cosinus5(-60) == 16383 && cosinus5(300) == 16383);
    // sin² + cos² reste à 32767² à 2·32767·0,5 près (chaque table est arrondie à 0,5)
    for (int a = -180; a <= 360; a += 5) {
        const long long s = sinus5(a);
        const long long c = cosinus5(a);
        const long long ecart = s * s + c * c - 32767LL * 32767LL;
        VERIFIER(ecart <= 33000 && ecart >= -33000);
    }
    return 0;
}

constexpr int la_projection_arrondit_au_plus_proche_loin_de_zero() {
    VERIFIER(projeter(180, 32767) == 180 && projeter(180, 0) == 0 && projeter(180, -32767) == -180);
    VERIFIER(projeter(1, 16384) == 1 && projeter(1, 16383) == 0 && projeter(1, -16384) == -1 &&
             projeter(1, -16383) == 0);
    VERIFIER(projeter(180, 28377) == 156 && projeter(180, -28377) == -156); // 155,88
    return 0;
}

constexpr int premier_anneau_valeurs_du_tab5() {
    for (const RefPremier &r : REF_PREMIER_TAB5) {
        VERIFIER(premier_conforme(M5, r));
    }
    return 0;
}

constexpr int second_anneau_valeurs_du_tab5() {
    for (const RefSecond &r : REF_SECOND_TAB5) {
        VERIFIER(second_conforme(M5, r));
    }
    return 0;
}

constexpr int mots_valeurs_du_tab5() {
    for (const RefMots &r : REF_MOTS_TAB5) {
        VERIFIER(mots_conformes(M5, r));
    }
    return 0;
}

constexpr int la_7b_a_l_echelle_0_8_valeurs_du_tab5_mis_a_l_echelle() {
    for (const RefPremier &r : REF_PREMIER_7B_0_8) {
        VERIFIER(premier_conforme(M7, r));
    }
    for (const RefSecond &r : REF_SECOND_7B_0_8) {
        VERIFIER(second_conforme(M7, r));
    }
    for (const RefMots &r : REF_MOTS_7B_0_8) {
        VERIFIER(mots_conformes(M7, r));
    }
    return 0;
}

constexpr int le_tab5_sur_1024_x_600_garde_ses_pixels() {
    for (const RefPremier &r : REF_PREMIER_7B_1_0) {
        VERIFIER(premier_conforme(M7_PX, r));
    }
    for (const RefSecond &r : REF_SECOND_7B_1_0) {
        VERIFIER(second_conforme(M7_PX, r));
    }
    for (const RefMots &r : REF_MOTS_7B_1_0) {
        VERIFIER(mots_conformes(M7_PX, r));
    }
    return 0;
}

// Refait à la main, sans le code ni le Tab5 : moyeu (640, 400), au-dessus.
constexpr int valeurs_refaites_a_la_main() {
    const Point moyeu{640, 400};
    // un bouton : à la verticale, 180 px plus haut
    const Anneau un = disposer(M5, moyeu, 1, 180, 30, 90, 36, false);
    VERIFIER(un.bouton[0].x == 640 && un.bouton[0].y == 220 && un.bouton[0].angle == 90 && un.pivot == 0 && !un.rogne);
    // trois boutons à 120°, 90°, 60° : 180 × cos 60° = 90 ; 180 × sin 60° = 155,88 -> 156
    const Anneau trois = disposer(M5, moyeu, 3, 180, 30, 90, 36, false);
    VERIFIER(trois.bouton[0].x == 550 && trois.bouton[0].y == 244 && trois.bouton[0].angle == 120);
    VERIFIER(trois.bouton[1].x == 640 && trois.bouton[1].y == 220);
    VERIFIER(trois.bouton[2].x == 730 && trois.bouton[2].y == 244 && trois.bouton[2].angle == 60);
    // six boutons à 165°…15° : 180 × cos 15° = 173,87 -> 174 ; 180 × sin 15° = 46,59 -> 47 ; cos 45° = 127,28 -> 127 ; 180 × sin 75° = 173,87
    const Anneau six = disposer(M5, moyeu, 6, 180, 30, 90, 36, false);
    VERIFIER(six.bouton[0].x == 640 - 174 && six.bouton[0].y == 400 - 47 && six.bouton[0].angle == 165);
    VERIFIER(six.bouton[1].x == 640 - 127 && six.bouton[1].y == 400 - 127 && six.bouton[1].angle == 135);
    VERIFIER(six.bouton[2].x == 640 - 47 && six.bouton[2].y == 400 - 174);
    VERIFIER(six.bouton[5].x == 640 + 174 && six.bouton[5].y == 400 - 47 && six.bouton[5].angle == 15);
    // sous le moyeu : la même figure, retournée
    const Anneau sous = disposer(M5, Point{640, 300}, 3, 180, 30, 90, 36, true);
    VERIFIER(sous.bouton[0].x == 550 && sous.bouton[0].y == 300 + 156 && sous.bouton[1].y == 300 + 180);
    // second anneau, trois choix centrés sur 90° : 110°, 90°, 70° à 290 px : 290 × cos 70° = 99,19 -> 99 ; 290 × sin 70° = 272,51 -> 273
    const Anneau deux = disposer(M5, moyeu, 3, 290, 20, 90, 36, false);
    VERIFIER(deux.bouton[0].x == 640 - 99 && deux.bouton[0].y == 400 - 273 && deux.bouton[0].angle == 110);
    VERIFIER(deux.bouton[1].x == 640 && deux.bouton[1].y == 400 - 290);
    VERIFIER(deux.bouton[2].x == 640 + 99 && deux.bouton[2].y == 400 - 273 && deux.bouton[2].angle == 70);
    // l'écart de deux boutons voisins d'un anneau régulier est la corde 2 × R × sin(pas / 2) : 93,2 px à 30° sur 180
    const int corde2 = carre(six.bouton[2].x - six.bouton[3].x) + carre(six.bouton[2].y - six.bouton[3].y);
    VERIFIER(corde2 ==
             carre(2 * 47)); // 92² + 0² : 2 × 47 = 94, la corde 2 × 180 × sin 15° = 93,2 arrondie des deux côtés
    return 0;
}

constexpr int le_seuil_du_dessous_est_celui_du_tab5() {
    // roue_ouvrir : s.dessous = s.ya - (kRayon2 + kLegende2 + kLegendeH / 2) < 0, soit y < 364 au pixel près
    VERIFIER(passe_dessous(M5, 363) && !passe_dessous(M5, 364) && passe_dessous(M5, 0) && !passe_dessous(M5, 719));
    // 7B : 232 + 48 + 11 = 291
    VERIFIER(passe_dessous(M7, 290) && !passe_dessous(M7, 291));
    return 0;
}

constexpr int le_pivotement_va_du_cote_oppose_au_bord() {
    // bord gauche : l'éventail tourne dans le sens horaire (angles qui baissent) de 50° ; bord droit : de 45° dans l'autre
    const Anneau gauche = disposer(M5, Point{0, 400}, 3, 180, 30, 90, 36, false);
    VERIFIER(gauche.pivot == -50 && gauche.bouton[0].angle == 70 && gauche.bouton[0].x == 62);
    VERIFIER(gauche.bouton[0].x - 36 >= 12 && !gauche.rogne);
    const Anneau droite = disposer(M5, Point{1279, 400}, 3, 180, 30, 90, 36, false);
    VERIFIER(droite.pivot == 45 && droite.bouton[2].angle == 105 && droite.bouton[2].x == 1232);
    VERIFIER(droite.bouton[2].x + 36 <= 1280 - 12 && !droite.rogne);
    // bord supérieur, anneau dessous : aucun besoin de pivoter
    VERIFIER(disposer(M5, Point{640, 0}, 3, 180, 30, 90, 36, true).pivot == 0);
    // la frontière au pixel près : un seul bouton à 90° à gauche ne pivote pas à x = 48 (48 - 36 = 12 = marge), pivote d'un cran à 47
    VERIFIER(disposer(M5, Point{48, 400}, 1, 180, 30, 90, 36, false).pivot == 0);
    VERIFIER(disposer(M5, Point{47, 400}, 1, 180, 30, 90, 36, false).pivot == -5);
    // et à droite : 1232 + 36 = 1268 = 1280 - 12 ne pivote pas, 1233 pivote de +5
    VERIFIER(disposer(M5, Point{1232, 400}, 1, 180, 30, 90, 36, false).pivot == 0);
    VERIFIER(disposer(M5, Point{1233, 400}, 1, 180, 30, 90, 36, false).pivot == 5);
    // en haut : y - 180 - 36 = 12 à y = 228 ne pivote pas ; à 227 le bouton, à 90°, tourne vers la verticale d'un cran (angle 85°)
    VERIFIER(disposer(M5, Point{640, 228}, 1, 180, 30, 90, 36, false).pivot == 0);
    const Anneau haut = disposer(M5, Point{640, 227}, 1, 180, 30, 90, 36, false);
    VERIFIER(haut.pivot == -5 && haut.bouton[0].angle == 85 && !haut.rogne);
    // tout pivotement est un multiple de 5°
    for (int x = 0; x < 1280; x += 7) {
        VERIFIER(disposer(M5, Point{x, 400}, 4, 180, 30, 90, 36, false).pivot % 5 == 0);
    }
    return 0;
}

constexpr int un_eventail_trop_large_pour_le_coin_empile_ses_boutons_chez_le_tab5() {
    // La raison de l'écart n° 2 : le Tab5, appuyé en (0, 0) avec six boutons, ramène trois boutons au même point (48, 48).
    const Anneau coin = disposer(M5, Point{0, 0}, 6, 180, 30, 90, 36, true);
    VERIFIER(coin.rogne);
    VERIFIER(coin.bouton[2].x == coin.bouton[3].x && coin.bouton[2].y == coin.bouton[3].y);
    VERIFIER(!boutons_sans_chevauchement(coin, 72));
    return 0;
}

constexpr int ouvrir_garde_le_moyeu_sous_le_doigt_quand_la_roue_tient() {
    for (const Point p : {Point{640, 400}, Point{780, 470}, Point{640, 360}, Point{300, 500}, Point{900, 300}}) {
        for (int n = 1; n <= 6; n++) {
            const Roue r = ouvrir(M5, p, demande(n));
            VERIFIER(r.moyeu == p && !r.moyeu_deplace && r.appui == p);
            VERIFIER(r.dessous == passe_dessous(M5, p.y));
            // et c'est la disposition du Tab5 : celle de disposer, sans rien changer
            const Anneau tab5 = disposer(M5, p, n, 180, 30, 90, 36, r.dessous);
            for (int i = 0; i < n; i++) {
                VERIFIER(r.premier.bouton[i].x == tab5.bouton[i].x && r.premier.bouton[i].y == tab5.bouton[i].y);
                VERIFIER(r.premier.bouton[i].angle == tab5.bouton[i].angle);
            }
        }
    }
    return 0;
}

constexpr int ouvrir_reprend_les_valeurs_du_tab5_la_ou_il_ne_deplace_pas() {
    // Les lignes de référence du Tab5 dont l'ancre est dans l'écran à sa place : « centre », « centre_dessus » et « esquisse ».
    for (const RefPremier &r : REF_PREMIER_TAB5) {
        const Roue roue = ouvrir(M5, Point{r.xa, r.ya}, demande(r.n));
        if (roue.moyeu_deplace) {
            continue;
        }
        for (int i = 0; i < r.n; i++) {
            VERIFIER(roue.premier.bouton[i].x == r.x[i] && roue.premier.bouton[i].y == r.y[i]);
        }
    }
    // et ces trois ancres-là ne sont jamais déplacées
    for (const Point p : {Point{640, 360}, Point{640, 400}, Point{780, 470}}) {
        VERIFIER(!ouvrir(M5, p, demande(6)).moyeu_deplace);
    }
    return 0;
}

constexpr int dans_un_coin_le_moyeu_avance_vers_le_centre_et_tout_tient() {
    for (const Point coin : {Point{0, 0}, Point{1279, 0}, Point{0, 719}, Point{1279, 719}}) {
        const Roue r = ouvrir(M5, coin, demande(6, 90, 8, 110));
        VERIFIER(r.moyeu_deplace && r.appui == coin);
        // le moyeu entier est dans l'écran, à la marge près
        VERIFIER(r.moyeu.x - M5.jauge / 2 >= M5.marge && r.moyeu.x + M5.jauge / 2 <= M5.largeur - M5.marge);
        VERIFIER(r.moyeu.y - M5.jauge / 2 >= M5.marge && r.moyeu.y + M5.jauge / 2 <= M5.hauteur - M5.marge);
        // et il s'est rapproché du centre sur chaque axe, sans le dépasser
        VERIFIER((coin.x == 0 ? r.moyeu.x > 0 && r.moyeu.x <= 640 : r.moyeu.x < 1279 && r.moyeu.x >= 640));
        VERIFIER((coin.y == 0 ? r.moyeu.y > 0 && r.moyeu.y <= 360 : r.moyeu.y < 719 && r.moyeu.y >= 360));
        VERIFIER(!r.premier.rogne && boutons_dans_l_ecran(M5, r.premier, 72) &&
                 boutons_sans_chevauchement(r.premier, 72));
        VERIFIER(ecart_angulaire_exact(r.premier, 30) && sur_le_cercle(r.premier, r.moyeu, 180));
        VERIFIER(mots_dans_l_ecran(M5, r.premier) && mots_degages(M5, r.premier, 72));
    }
    return 0;
}

constexpr int les_quatre_bords_avec_six_boutons_tiennent() {
    for (const Point bord : {Point{0, 360}, Point{1279, 360}, Point{640, 0}, Point{640, 719}}) {
        const Roue r = ouvrir(M5, bord, demande(6, 80, 8, 110));
        VERIFIER(!r.premier.rogne && boutons_dans_l_ecran(M5, r.premier, 72) &&
                 boutons_sans_chevauchement(r.premier, 72));
        VERIFIER(mots_dans_l_ecran(M5, r.premier) && mots_degages(M5, r.premier, 72) &&
                 ecart_angulaire_exact(r.premier, 30));
        // chaque famille, ouverte à huit choix : tout tient aussi
        int largeur[8] = {110, 110, 110, 110, 110, 110, 110, 110};
        for (int f = 0; f < 6; f++) {
            const Anneau a = deplier(M5, r, f, 8, largeur);
            VERIFIER(!a.rogne && boutons_dans_l_ecran(M5, a, 72) && boutons_sans_chevauchement(a, 72));
            VERIFIER(ecart_angulaire_exact(a, 20) && sur_le_cercle(a, r.moyeu, 290));
            VERIFIER(mots_dans_l_ecran(M5, a) && mots_degages(M5, a, 72));
        }
    }
    return 0;
}

constexpr int le_moyeu_ne_sort_pas_de_l_ecran_meme_pour_un_point_hors_de_l_ecran() {
    const Roue r = ouvrir(M5, Point{-50, 9000}, demande(3));
    VERIFIER(r.moyeu.x - M5.jauge / 2 >= M5.marge && r.moyeu.y + M5.jauge / 2 <= M5.hauteur - M5.marge);
    VERIFIER(r.moyeu_deplace && r.appui == (Point{-50, 9000}));
    return 0;
}

constexpr int symetrie_sur_la_verticale_de_l_ecran() {
    // moyeu à x = largeur / 2 : le bouton i et le bouton n - 1 - i sont symétriques ; leurs angles s'additionnent à 180°
    for (const Mesures *m : {&M5, &M7}) {
        const int y = m == &M5 ? 420 : 340;
        for (int n = 1; n <= 6; n++) {
            const Roue r = ouvrir(*m, Point{m->largeur / 2, y}, demande(n, 70));
            VERIFIER(!r.moyeu_deplace && r.premier.pivot == 0);
            for (int i = 0; i < n; i++) {
                const Bouton &a = r.premier.bouton[i];
                const Bouton &b = r.premier.bouton[n - 1 - i];
                VERIFIER(a.x + b.x == m->largeur && a.y == b.y && a.angle + b.angle == 180);
                VERIFIER(a.mot.x + b.mot.x == m->largeur && a.mot.y == b.mot.y && a.mot.largeur == b.mot.largeur);
            }
        }
        // le second anneau d'un bouton central est symétrique aussi (n impair : le bouton du milieu est à 90°)
        const Roue r = ouvrir(*m, Point{m->largeur / 2, y}, demande(3, 0, 8));
        int largeur[8] = {90, 90, 90, 90, 90, 90, 90, 90};
        const Anneau a = deplier(*m, r, 1, 8, largeur);
        for (int j = 0; j < 8; j++) {
            VERIFIER(a.bouton[j].x + a.bouton[7 - j].x == m->largeur && a.bouton[j].y == a.bouton[7 - j].y);
            VERIFIER(a.bouton[j].angle + a.bouton[7 - j].angle == 180);
            VERIFIER(a.bouton[j].mot.x + a.bouton[7 - j].mot.x == m->largeur &&
                     a.bouton[j].mot.y == a.bouton[7 - j].mot.y);
        }
    }
    return 0;
}

constexpr int le_second_anneau_est_centre_sur_sa_famille() {
    const Roue r = ouvrir(M5, Point{780, 470}, demande(6, 0, 5));
    for (int f = 0; f < 6; f++) {
        for (int m = 1; m <= 8; m++) {
            const Anneau a = deplier(M5, r, f, m, nullptr);
            // la moyenne des angles est celle de la famille (l'éventail est régulier), l'écart est exactement 20°
            VERIFIER(a.n == m && ecart_angulaire_exact(a, 20));
            int somme = 0;
            for (int j = 0; j < m; j++) {
                somme += a.bouton[j].angle;
            }
            VERIFIER(somme == m * (r.premier.bouton[f].angle + a.pivot));
            // le bouton du milieu (ou le milieu des deux du milieu) est sur l'axe de la famille, à un pas de pivotement près
            VERIFIER(2 * a.bouton[0].angle - (m - 1) * 20 == 2 * r.premier.bouton[f].angle + 2 * a.pivot);
        }
    }
    return 0;
}

constexpr int les_cotes_des_mots_comme_sur_l_esquisse() {
    // Esquisse : moyeu (780, 470), six boutons 165°…15° : Réglages et Alertes à gauche, Profil et Affichage au-dessus,
    // Pages et Assistant à droite.
    const Roue r = ouvrir(M5, Point{780, 470}, demande(6, 80));
    VERIFIER(r.premier.bouton[0].mot.cote == Cote::Gauche && r.premier.bouton[1].mot.cote == Cote::Gauche);
    VERIFIER(r.premier.bouton[2].mot.cote == Cote::Haut && r.premier.bouton[3].mot.cote == Cote::Haut);
    VERIFIER(r.premier.bouton[4].mot.cote == Cote::Droite && r.premier.bouton[5].mot.cote == Cote::Droite);
    // un mot à gauche est à gauche de son bouton, un mot à droite à sa droite, un mot au-dessus au-dessus
    VERIFIER(r.premier.bouton[0].mot.x < r.premier.bouton[0].x && r.premier.bouton[5].mot.x > r.premier.bouton[5].x);
    VERIFIER(r.premier.bouton[2].mot.y < r.premier.bouton[2].y && r.premier.bouton[3].mot.y < r.premier.bouton[3].y);
    // anneau sous le moyeu : « au-dessus » devient « au-dessous »
    const Roue s = ouvrir(M5, Point{640, 300}, demande(6, 80));
    VERIFIER(s.dessous && s.premier.bouton[2].mot.cote == Cote::Bas &&
             s.premier.bouton[2].mot.y > s.premier.bouton[2].y);
    // une seule frontière de côté : 60° est encore un côté (cos 60° = 0,5), 75° est un haut
    VERIFIER(cote_du_mot(60, false) == Cote::Droite && cote_du_mot(120, false) == Cote::Gauche);
    VERIFIER(cote_du_mot(75, false) == Cote::Haut && cote_du_mot(105, false) == Cote::Haut &&
             cote_du_mot(90, true) == Cote::Bas);
    VERIFIER(cote_du_mot(-90, false) == Cote::Bas && cote_du_mot(0, false) == Cote::Droite &&
             cote_du_mot(180, false) == Cote::Gauche);
    return 0;
}

constexpr int un_mot_absent_ne_laisse_aucune_boite() {
    Demande d = demande(3, 60);
    d.mot[1] = 0;
    const Roue r = ouvrir(M5, Point{640, 400}, d);
    VERIFIER(r.premier.bouton[1].mot.largeur == 0 && r.premier.bouton[1].mot.cote == Cote::Aucun);
    VERIFIER(r.premier.bouton[0].mot.largeur == 60 && r.premier.bouton[2].mot.largeur == 60);
    // un mot plus large que la boîte est ramené à la boîte
    const Roue large = ouvrir(M5, Point{640, 400}, demande(1, 400));
    VERIFIER(large.premier.bouton[0].mot.largeur == 150);
    return 0;
}

constexpr int les_mots_voisins_se_serrent_sans_se_recouvrir() {
    // deux mots de la même rangée à 90 px l'un de l'autre : 90 - 8 = 82 de large chacun au plus ; jamais moins de 40
    Anneau a = disposer(M5, Point{640, 400}, 3, 290, 20, 90, 36, false);
    const int largeur[3] = {150, 150, 150};
    placer_mots(M5, a, Point{640, 400}, false, 290, 72, largeur);
    for (int i = 0; i < 3; i++) {
        VERIFIER(a.bouton[i].mot.largeur >= 40 && a.bouton[i].mot.largeur <= 150);
    }
    VERIFIER(mots_degages(M5, a, 72));
    // un voisin sans mot ne serre personne
    const int seul[3] = {150, 0, 0};
    placer_mots(M5, a, Point{640, 400}, false, 290, 72, seul);
    VERIFIER(a.bouton[0].mot.largeur == 150);
    return 0;
}

constexpr int toucher_la_frontiere_d_un_bouton_pixel_dedans_pixel_dehors() {
    for (const Mesures *m : {&M5, &M7}) {
        const Roue r = ouvrir(*m, Point{m->largeur / 2, m->hauteur / 2 + 80}, demande(1));
        const Bouton &b = r.premier.bouton[0];
        const int rayon = rayon_tactile(*m, m->diametre_1); // 44 sur le Tab5, 35 sur la 7B
        VERIFIER(rayon == m->diametre_1 / 2 + m->toucher_marge && rayon > m->diametre_1 / 2);
        // les quatre axes : le pixel à `rayon` est dedans, celui d'après est dehors
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x, b.y}) == 0);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x + rayon, b.y}) == 0);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x + rayon + 1, b.y}) == -1);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x - rayon, b.y}) == 0);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x - rayon - 1, b.y}) == -1);
        // le pixel (rayon, 1) est à √(rayon² + 1) du centre : juste au-delà du disque, donc dehors
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x + rayon, b.y + 1}) == -1);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x, b.y + rayon}) == 0);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x, b.y + rayon + 1}) == -1);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x, b.y - rayon}) == 0);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x, b.y - rayon - 1}) == -1);
        // les diagonales : (k, k) est dedans tant que 2k² <= rayon²
        int k = 0;
        while (2 * (k + 1) * (k + 1) <= rayon * rayon) {
            k++;
        }
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x + k, b.y + k}) == 0);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x + k + 1, b.y + k + 1}) == -1);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x - k, b.y - k}) == 0);
        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1, Point{b.x - k - 1, b.y + k + 1}) == -1);
    }
    // la valeur exacte : 36 + 8 = 44 sur le Tab5 ; 29 + 6 = 35 sur la 7B
    VERIFIER(rayon_tactile(M5, M5.diametre_1) == 44 && rayon_tactile(M7, M7.diametre_1) == 35);
    return 0;
}

constexpr int la_zone_tactile_n_est_jamais_plus_petite_que_le_bouton() {
    for (const Mesures *m : {&M5, &M7}) {
        const Roue r = ouvrir(*m, Point{m->largeur / 2, m->hauteur / 2 + 90}, demande(6));
        const int rb = m->diametre_1 / 2;
        for (int i = 0; i < 6; i++) {
            for (int dy = -rb; dy <= rb; dy++) {
                for (int dx = -rb; dx <= rb; dx++) {
                    if (dx * dx + dy * dy <= rb * rb) {
                        VERIFIER(bouton_sous(*m, r.premier, m->diametre_1,
                                             Point{r.premier.bouton[i].x + dx, r.premier.bouton[i].y + dy}) == i);
                    }
                }
            }
        }
    }
    return 0;
}

constexpr int les_zones_d_un_eventail_regulier_ne_se_recouvrent_pas() {
    // deux boutons voisins à 30° sur 180 px : corde 93,2, moins les 72 du bouton, soit 21 d'air ; la zone en prend 8 de chaque côté
    for (const Mesures *m : {&M5, &M7}) {
        const Roue r = ouvrir(*m, Point{m->largeur / 2, m->hauteur / 2 + 90}, demande(6));
        const int rayon = rayon_tactile(*m, m->diametre_1);
        for (int i = 0; i + 1 < 6; i++) {
            const int d2 = carre(r.premier.bouton[i].x - r.premier.bouton[i + 1].x) +
                           carre(r.premier.bouton[i].y - r.premier.bouton[i + 1].y);
            VERIFIER(d2 > carre(2 * rayon)); // les disques tactiles sont disjoints
        }
    }
    return 0;
}

constexpr int deux_zones_qui_se_recouvrent_le_centre_le_plus_proche_gagne() {
    Anneau a;
    a.n = 3;
    a.bouton[0].x = 100;
    a.bouton[0].y = 100;
    a.bouton[1].x = 160; // à 60 : les disques de rayon 44 se recouvrent de x = 116 à x = 144
    a.bouton[1].y = 100;
    a.bouton[2].x = 160; // à la même place que le précédent : l'égalité va au premier rang
    a.bouton[2].y = 100;
    VERIFIER(bouton_sous(M5, a, 72, Point{129, 100}) == 0); // 29 du premier, 31 du second
    VERIFIER(bouton_sous(M5, a, 72, Point{130, 100}) == 0); // 30 et 30 : l'égalité va au premier rang
    VERIFIER(bouton_sous(M5, a, 72, Point{131, 100}) == 1); // 31 et 29
    VERIFIER(bouton_sous(M5, a, 72, Point{160, 100}) == 1); // égalité entre 1 et 2 : le rang 1
    VERIFIER(bouton_sous(M5, a, 72, Point{60, 100}) == 0);  // seulement dans la zone du premier
    VERIFIER(bouton_sous(M5, a, 72, Point{204, 100}) == 1 && bouton_sous(M5, a, 72, Point{205, 100}) == -1);
    return 0;
}

constexpr int le_toucher_loin_des_boutons_ne_rend_rien() {
    const Roue r = ouvrir(M5, Point{640, 400}, demande(6));
    VERIFIER(bouton_sous(M5, r.premier, 72, Point{640, 400}) == -1); // le moyeu n'est pas un bouton
    VERIFIER(bouton_sous(M5, r.premier, 72, Point{0, 0}) == -1);
    VERIFIER(bouton_sous(M5, r.premier, 72, Point{1279, 719}) == -1);
    VERIFIER(bouton_sous(M5, Anneau{}, 72, Point{640, 400}) == -1); // anneau vide
    return 0;
}

constexpr int le_toucher_des_deux_anneaux() {
    const Roue r = ouvrir(M5, Point{640, 400}, demande(6, 0, 8));
    const Anneau a = deplier(M5, r, 2, 8, nullptr);
    for (int j = 0; j < 8; j++) {
        VERIFIER(bouton_sous(M5, a, 72, Point{a.bouton[j].x, a.bouton[j].y}) == j);
    }
    // un point du second anneau n'est jamais pris pour un bouton du premier, et réciproquement
    for (int i = 0; i < 6; i++) {
        VERIFIER(bouton_sous(M5, a, 72, Point{r.premier.bouton[i].x, r.premier.bouton[i].y}) == -1);
    }
    for (int j = 0; j < 8; j++) {
        VERIFIER(bouton_sous(M5, r.premier, 72, Point{a.bouton[j].x, a.bouton[j].y}) == -1);
    }
    return 0;
}

constexpr int une_famille_hors_du_premier_anneau_donne_un_anneau_vide() {
    const Roue r = ouvrir(M5, Point{640, 400}, demande(3));
    VERIFIER(deplier(M5, r, 3, 4, nullptr).n == 0 && deplier(M5, r, -1, 4, nullptr).n == 0);
    VERIFIER(deplier(M5, r, 0, 99, nullptr).n == CHOIX_MAX); // borné : jamais plus de huit choix
    VERIFIER(ouvrir(M5, Point{640, 400}, demande(99)).premier.n == BOUTONS_MAX);
    VERIFIER(ouvrir(M5, Point{640, 400}, demande(0)).premier.n == 0);
    return 0;
}

// La 7B à l'échelle 0,8 : mêmes figures, en plus petit ; les mêmes invariants tiennent sur 1024 x 600.
constexpr int la_7b_tient_dans_chaque_coin_et_chaque_bord() {
    for (const Point p : {Point{0, 0}, Point{1023, 0}, Point{0, 599}, Point{1023, 599}, Point{0, 300}, Point{1023, 300},
                          Point{512, 0}, Point{512, 599}, Point{512, 300}}) {
        const Roue r = ouvrir(M7, p, demande(6, 72, 8, 88));
        VERIFIER(!r.premier.rogne && boutons_dans_l_ecran(M7, r.premier, 58) &&
                 boutons_sans_chevauchement(r.premier, 58));
        VERIFIER(ecart_angulaire_exact(r.premier, 30) && mots_dans_l_ecran(M7, r.premier) &&
                 mots_degages(M7, r.premier, 58));
        int largeur[8] = {88, 88, 88, 88, 88, 88, 88, 88};
        for (int f = 0; f < 6; f++) {
            const Anneau a = deplier(M7, r, f, 8, largeur);
            VERIFIER(!a.rogne && boutons_dans_l_ecran(M7, a, 58) && boutons_sans_chevauchement(a, 58));
            VERIFIER(ecart_angulaire_exact(a, 20) && mots_dans_l_ecran(M7, a) && mots_degages(M7, a, 58));
        }
    }
    return 0;
}

// Un écran où le premier anneau seul ne tient nulle part (400 x 300 : les boutons de 72 px à 180 px du moyeu demandent 432 px de
// large et 396 de haut) : `tient` le dit, et la roue est celle du centre de l'écran (règle 2).
constexpr int quand_rien_ne_tient_la_roue_est_au_centre_et_le_dit() {
    const Mesures petit = mesures(Ecran{400, 300, 1000});
    const Point centre{200, 150};
    for (const Point p : {Point{0, 0}, Point{399, 299}, Point{200, 150}, Point{90, 40}}) {
        const Roue r = ouvrir(petit, p, demande(6));
        VERIFIER(!r.tient);
        VERIFIER(r.moyeu == centre && r.appui == p && r.moyeu_deplace == !(p == centre));
        VERIFIER(r.premier.n == 6);
    }
    // et sur le Tab5, là où la roue tient, le drapeau est vrai
    for (const Point p : {Point{640, 400}, Point{780, 470}, Point{0, 0}, Point{1279, 719}}) {
        VERIFIER(ouvrir(M5, p, demande(6, 80, 8, 110)).tient);
    }
    return 0;
}

// Deux mots posés à la main, valeurs du vrai code du Tab5 (largeurs_libres et mot_recul compilés à part, voir plus haut) :
//   - angles 75° et 55°, 80 px chacun, moyeu (640, 400) : leurs mots sont à EXACTEMENT legende_h (28 px) l'un de l'autre en hauteur,
//     donc pas dans la même rangée, donc pas serrés (le Tab5 écrit `>=`) ;
//   - angles 90° et 85° : même rangée et 21 px d'écart en x, serrés jusqu'à la largeur minimale de 40 px.
constexpr int deux_mots_voisins_valeurs_du_tab5() {
    const Point moyeu{640, 400};
    const int largeur[2] = {80, 80};
    Anneau a;
    a.n = 2;
    a.bouton[0].angle = 75;
    a.bouton[1].angle = 55;
    placer_mots(M5, a, moyeu, false, M5.rayon_1, M5.diametre_1, largeur);
    VERIFIER(a.bouton[0].mot.x == 703 && a.bouton[0].mot.y == 164 && a.bouton[0].mot.largeur == 80);
    VERIFIER(a.bouton[1].mot.x == 786 && a.bouton[1].mot.y == 192 && a.bouton[1].mot.largeur == 80);
    VERIFIER(a.bouton[1].mot.y - a.bouton[0].mot.y == 28); // la hauteur d'un mot, en dur : c'est le seuil du Tab5
    Anneau b;
    b.n = 2;
    b.bouton[0].angle = 90;
    b.bouton[1].angle = 85;
    placer_mots(M5, b, moyeu, false, M5.rayon_1, M5.diametre_1, largeur);
    VERIFIER(b.bouton[0].mot.x == 640 && b.bouton[0].mot.y == 166 && b.bouton[0].mot.largeur == 40);
    VERIFIER(b.bouton[1].mot.x == 661 && b.bouton[1].mot.y == 164 && b.bouton[1].mot.largeur == 40);
    return 0;
}

// Un mot n'est jamais plus large que la boîte de 150 px (120 px à la 7B), et sa place en tient compte : au bord droit du bouton
// à 0°, le mot est à 180 + 36 + 4 + 75 du moyeu.
constexpr int un_mot_trop_large_est_borne_a_la_boite() {
    const int large[1] = {200};
    Anneau a;
    a.n = 1;
    a.bouton[0].angle = 0;
    placer_mots(M5, a, Point{640, 400}, false, M5.rayon_1, M5.diametre_1, large);
    VERIFIER(a.bouton[0].mot.largeur == 150 && a.bouton[0].mot.x == 640 + 180 + 36 + 4 + 75 &&
             a.bouton[0].mot.y == 400);
    Anneau b;
    b.n = 1;
    b.bouton[0].angle = 0;
    placer_mots(M7, b, Point{512, 320}, false, M7.rayon_1, M7.diametre_1, large);
    VERIFIER(b.bouton[0].mot.largeur == 120 && b.bouton[0].mot.x == 512 + 144 + 29 + 3 + 60 &&
             b.bouton[0].mot.y == 320);
    return 0;
}

// `anneau_complet` sur des anneaux faits à la main : se toucher n'est pas se recouvrir, un bouton ramené ne tient pas, un mot ne
// recouvre ni un bouton ni un autre mot, un mot sort de l'écran à moins de 12 px d'un bord ne tient pas.
constexpr int anneau_complet_refuse_ce_qui_ne_tient_pas() {
    Anneau a;
    a.n = 2;
    a.bouton[0].x = 300;
    a.bouton[0].y = 300;
    a.bouton[1].x = 372; // 72 px entre les centres : les boutons de 72 px se touchent
    a.bouton[1].y = 300;
    VERIFIER(anneau_complet(M5, a, 72));
    a.bouton[1].x = 371; // 71 px : ils se recouvrent
    VERIFIER(!anneau_complet(M5, a, 72));
    a.bouton[1].x = 372;
    a.rogne = true;
    VERIFIER(!anneau_complet(M5, a, 72));
    // un mot sur un bouton
    Anneau b;
    b.n = 2;
    b.bouton[0].x = 300;
    b.bouton[0].y = 300;
    b.bouton[1].x = 700;
    b.bouton[1].y = 300;
    b.bouton[0].mot = Mot{700, 300, 80, Cote::Droite};
    VERIFIER(!anneau_complet(M5, b, 72));
    b.bouton[0].mot = Mot{800, 300, 80, Cote::Droite}; // à 24 px du bouton (le disque est de 36 px)
    VERIFIER(anneau_complet(M5, b, 72));
    // deux mots qui se touchent tiennent, qui se recouvrent d'un pixel non
    b.bouton[0].mot = Mot{500, 200, 80, Cote::Haut};
    b.bouton[1].mot = Mot{580, 200, 80, Cote::Haut};
    VERIFIER(anneau_complet(M5, b, 72));
    b.bouton[1].mot = Mot{579, 200, 80, Cote::Haut};
    VERIFIER(!anneau_complet(M5, b, 72));
    // un mot à 12 px du bord gauche tient, à 11 px non
    b.bouton[1].mot = Mot{};
    b.bouton[0].mot = Mot{12 + 40, 200, 80, Cote::Haut};
    VERIFIER(anneau_complet(M5, b, 72));
    b.bouton[0].mot = Mot{11 + 40, 200, 80, Cote::Haut};
    VERIFIER(!anneau_complet(M5, b, 72));
    return 0;
}

// Sans exiger le second anneau des familles (`choix` à 0), le moyeu reste sous le doigt plus près des bords ; le second anneau
// peut alors ne pas tenir, et `anneau_complet` le dit. Avec les familles dans la demande, le même appui écarte le moyeu.
constexpr int sans_exiger_les_seconds_anneaux_le_moyeu_reste_plus_pres_du_doigt() {
    const Point p{400, 450};
    const Roue seul = ouvrir(M5, p, demande(6, 80));
    VERIFIER(seul.tient && seul.moyeu == p && !seul.moyeu_deplace);
    VERIFIER(anneau_complet(M5, seul.premier, 72));
    int largeur[8] = {110, 110, 110, 110, 110, 110, 110, 110};
    // la famille 0 (165°) a un choix à 185° : son mot de 110 px sort de l'écran à gauche
    VERIFIER(!anneau_complet(M5, deplier(M5, seul, 0, 8, largeur), 72));
    // la même demande avec ses familles : le moyeu s'écarte du doigt et le second anneau de la famille 0 tient
    const Roue tout = ouvrir(M5, p, demande(6, 80, 8, 110));
    VERIFIER(tout.tient && tout.moyeu_deplace);
    VERIFIER(anneau_complet(M5, deplier(M5, tout, 0, 8, largeur), 72));
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(les_constantes_sont_celles_du_tab5)                                                                              \
    X(la_7b_a_l_echelle_0_8)                                                                                           \
    X(la_table_des_sinus_est_celle_du_tab5)                                                                            \
    X(la_projection_arrondit_au_plus_proche_loin_de_zero)                                                              \
    X(premier_anneau_valeurs_du_tab5)                                                                                  \
    X(second_anneau_valeurs_du_tab5)                                                                                   \
    X(mots_valeurs_du_tab5)                                                                                            \
    X(la_7b_a_l_echelle_0_8_valeurs_du_tab5_mis_a_l_echelle)                                                           \
    X(le_tab5_sur_1024_x_600_garde_ses_pixels)                                                                         \
    X(valeurs_refaites_a_la_main)                                                                                      \
    X(le_seuil_du_dessous_est_celui_du_tab5)                                                                           \
    X(le_pivotement_va_du_cote_oppose_au_bord)                                                                         \
    X(un_eventail_trop_large_pour_le_coin_empile_ses_boutons_chez_le_tab5)                                             \
    X(ouvrir_garde_le_moyeu_sous_le_doigt_quand_la_roue_tient)                                                         \
    X(ouvrir_reprend_les_valeurs_du_tab5_la_ou_il_ne_deplace_pas)                                                      \
    X(dans_un_coin_le_moyeu_avance_vers_le_centre_et_tout_tient)                                                       \
    X(les_quatre_bords_avec_six_boutons_tiennent)                                                                      \
    X(le_moyeu_ne_sort_pas_de_l_ecran_meme_pour_un_point_hors_de_l_ecran)                                              \
    X(symetrie_sur_la_verticale_de_l_ecran)                                                                            \
    X(le_second_anneau_est_centre_sur_sa_famille)                                                                      \
    X(les_cotes_des_mots_comme_sur_l_esquisse)                                                                         \
    X(un_mot_absent_ne_laisse_aucune_boite)                                                                            \
    X(les_mots_voisins_se_serrent_sans_se_recouvrir)                                                                   \
    X(toucher_la_frontiere_d_un_bouton_pixel_dedans_pixel_dehors)                                                      \
    X(la_zone_tactile_n_est_jamais_plus_petite_que_le_bouton)                                                          \
    X(les_zones_d_un_eventail_regulier_ne_se_recouvrent_pas)                                                           \
    X(deux_zones_qui_se_recouvrent_le_centre_le_plus_proche_gagne)                                                     \
    X(le_toucher_loin_des_boutons_ne_rend_rien)                                                                        \
    X(le_toucher_des_deux_anneaux)                                                                                     \
    X(une_famille_hors_du_premier_anneau_donne_un_anneau_vide)                                                         \
    X(la_7b_tient_dans_chaque_coin_et_chaque_bord)                                                                     \
    X(quand_rien_ne_tient_la_roue_est_au_centre_et_le_dit)                                                             \
    X(sans_exiger_les_seconds_anneaux_le_moyeu_reste_plus_pres_du_doigt)                                               \
    X(deux_mots_voisins_valeurs_du_tab5)                                                                               \
    X(un_mot_trop_large_est_borne_a_la_boite)                                                                          \
    X(anneau_complet_refuse_ce_qui_ne_tient_pas)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
