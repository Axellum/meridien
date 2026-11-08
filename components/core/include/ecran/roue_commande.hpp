// Noyau pur : ce qui se décide dans la roue d'appui long SANS dessiner (ADR-0021). Entre la géométrie (roue_geometrie.hpp : où est
// quoi) et l'interface (main/ui_roue.cpp : les objets LVGL), trois choses se calculent en nombres et se testent sur le PC :
//   1. l'écran de la roue d'après la taille de l'écran réel (échelle de la 7B, du Tab5) ;
//   2. la demande faite à `ouvrir` d'après les entrées de la roue, et ce qu'on exige à l'ouverture du second anneau d'une famille
//      (le réglage que l'ADR-0017 laisse à l'appelant : `EXIGENCE_A_L_OUVERTURE`) ;
//   3. ce que fait un toucher : choisir, déplier, replier, fermer, ou rien (un glissement ne fait rien : ADR-0036 du dépôt Tab5,
//      « a swipe on it does nothing »).
// Aucun LVGL, aucune police, aucune couleur, aucun texte : des numéros de boutons et des pixels. Aucune géométrie recalculée ici :
// tout passe par `ecran::roue` (roue_geometrie.hpp, jamais modifié par ce fichier).
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation, aucune allocation.
#pragma once

#include <cstdint>

#include "ecran/roue_geometrie.hpp"

namespace ecran::roue {

// ---- Ce qu'on peut vouloir changer en regardant la vraie carte ------------------------------------------------------------------

// Ce que l'ouverture de la roue exige de l'écran (ADR-0017, point 4).
//   PremierAnneauSeul : le moyeu reste au plus près du doigt ; le second anneau d'une famille n'est exigé qu'au moment où on la
//                       déplie, et si, à cet endroit, il ne tient pas, la roue entière se REPOSE (`deplier_famille`) plus près du
//                       centre. Prix : un saut de la roue au dépliement, seulement quand il le faut.
//   TousLesAnneaux    : l'ouverture exige le second anneau de chaque famille. Prix, mesuré à l'ADR-0017 sur 1280 x 720 : un
//                       moyeu écarté du doigt pour 92 % des appuis (264 px en moyenne, 676 au pire, dans un coin).
enum class Exigence : std::uint8_t { PremierAnneauSeul, TousLesAnneaux };
inline constexpr Exigence EXIGENCE_A_L_OUVERTURE = Exigence::PremierAnneauSeul;

// Au-delà de cette distance, entre l'endroit où le doigt se pose et l'endroit où il est (à l'appui long) ou se lève (sur la roue
// ouverte), c'est un glissement, pas un toucher. Mesure du Tab5 (1280 x 720), mise à l'échelle comme les autres.
inline constexpr int GLISSEMENT_PX = 24;

// Une roue laissée ouverte se ferme seule après ce délai sans toucher (le Tab5 la ferme au délai d'inactivité de ses popups).
inline constexpr std::int32_t INACTIVITE_MS = 15000;

// ---- 1. L'écran de la roue ------------------------------------------------------------------------------------------------------

// Les mesures du Tab5 (1280 x 720, ou 720 x 1280 en portrait) ramenées à l'écran réel : l'échelle est la plus petite des deux
// rapports, comme l'écran principal (ui_principal.cpp, `g.echelle`). 1024 x 600 : 800 ; 1280 x 720 : 1000. Un écran sans taille
// (0) garde l'échelle 1000.
constexpr Ecran ecran_de(int largeur, int hauteur) noexcept {
    if (largeur <= 0 || hauteur <= 0) {
        return Ecran{TAB5_LARGEUR, TAB5_HAUTEUR, 1000};
    }
    const bool portrait = hauteur > largeur;
    const int ref_l = portrait ? TAB5_HAUTEUR : TAB5_LARGEUR;
    const int ref_h = portrait ? TAB5_LARGEUR : TAB5_HAUTEUR;
    const int sur_l = largeur * 1000 / ref_l;
    const int sur_h = hauteur * 1000 / ref_h;
    return Ecran{largeur, hauteur, sur_l < sur_h ? sur_l : sur_h};
}

// Le seuil de glissement en pixels de l'écran réel.
constexpr int glissement(const Ecran &e) noexcept { return mettre_a_l_echelle(GLISSEMENT_PX, e.echelle_pm); }

// Vrai si le doigt a bougé de plus que `seuil` pixels (distance à vol d'oiseau, sans racine carrée : le carré est comparé).
constexpr bool a_glisse(Point depart, Point arrivee, int seuil) noexcept {
    const int dx = arrivee.x - depart.x;
    const int dy = arrivee.y - depart.y;
    return dx * dx + dy * dy > seuil * seuil;
}

// ---- 2. La demande faite à la géométrie ------------------------------------------------------------------------------------------

// Ce que l'appelant sait d'une entrée du premier anneau : la largeur de son mot, mesurée sur la police (0 : pas de mot), et le
// nombre de choix de sa famille (0 : ce n'est pas une famille).
struct Forme {
    int mot = 0;
    int choix = 0;
};

// La demande d'ouverture. `mot_choix` : la largeur du plus large mot d'un choix (toutes familles), 0 s'il n'y en a pas.
constexpr Demande demande(const Forme *formes, int n, int mot_choix, Exigence exigence) noexcept {
    Demande d;
    d.n = detail::limiter(n, 0, BOUTONS_MAX);
    d.mot_choix = mot_choix;
    for (int i = 0; i < d.n && formes != nullptr; i++) {
        d.mot[i] = formes[i].mot;
        d.choix[i] = exigence == Exigence::TousLesAnneaux ? detail::limiter(formes[i].choix, 0, CHOIX_MAX) : 0;
    }
    return d;
}

// Le dépliement de la famille `famille` : la roue (la même, ou reposée), le second anneau et s'il tient.
struct Depliage {
    Roue roue;
    Anneau second;
    bool tient = false;   // le second anneau tient dans l'écran (anneau_complet)
    bool reposee = false; // la roue a été reposée ailleurs pour que le second anneau tienne
};

// Déplie la famille `famille` (`n` choix, `largeur[j]` : largeur du mot du choix j, nullptr : pas de mot) sur la roue ouverte.
// Si le second anneau tient autour du moyeu actuel, rien ne bouge. Sinon la roue est reposée par `ouvrir` sur le point d'appui
// d'origine, avec ce second anneau EXIGÉ en plus de ce que la demande d'ouverture exigeait : le moyeu avance vers le centre de
// l'écran jusqu'à la première position où il tient (ADR-0017, règle 2). Si rien ne tient, le second anneau est rendu tel quel
// (`tient` faux) : à l'appelant de décider (ui_roue.cpp le montre quand même).
constexpr Depliage deplier_famille(const Mesures &m, const Roue &roue, const Demande &d, int famille, int n,
                                   const int *largeur) noexcept {
    Depliage r;
    r.roue = roue;
    r.second = deplier(m, roue, famille, n, largeur);
    r.tient = anneau_complet(m, r.second, m.diametre_2);
    if (r.tient || famille < 0 || famille >= roue.premier.n) {
        return r;
    }
    Demande exigee = d;
    exigee.choix[famille] = detail::limiter(n, 0, CHOIX_MAX);
    const Roue neuve = ouvrir(m, roue.appui, exigee);
    if (neuve.moyeu == roue.moyeu) {
        return r; // le moyeu ne bouge pas : rien ne tient mieux ailleurs
    }
    r.roue = neuve;
    r.second = deplier(m, neuve, famille, n, largeur);
    r.tient = anneau_complet(m, r.second, m.diametre_2);
    r.reposee = true;
    return r;
}

// ---- 3. Ce que fait un toucher ---------------------------------------------------------------------------------------------------

enum class Geste : std::uint8_t {
    Rien,         // un glissement, un doigt posé sur un bouton et levé sur un autre, un toucher annulé
    Fermer,       // un toucher hors des boutons (voile, moyeu, bande) quand aucune famille n'est dépliée
    Replier,      // un toucher hors des boutons, ou sur la famille dépliée, quand une famille est dépliée
    Deplier,      // le bouton `rang` est une famille : son second anneau s'ouvre (un autre se replie)
    Choisir,      // le bouton `rang` du premier anneau : la roue se ferme, puis son action part
    ChoisirChoix, // le choix `rang` du second anneau de la famille `famille` : la roue se ferme, puis il part
};

struct Decision {
    Geste geste = Geste::Rien;
    int rang = -1;    // Deplier, Choisir : le bouton ; ChoisirChoix : le choix
    int famille = -1; // ChoisirChoix : la famille dépliée
};

// Ce que la roue ouverte fait d'un toucher qui a commencé en `appui` et fini en `relache` (le même doigt).
// `famille` : le bouton du premier anneau dont le second anneau est déplié (-1 : replié) ; `second` : cet anneau.
// `est_famille[i]` : le bouton i du premier anneau est une famille. `seuil` : `glissement(ecran)`. `annule` : le toucher a duré
// assez pour être un appui long.
//   - un glissement ou un toucher annulé ne fait rien ;
//   - un bouton ne répond que si le doigt s'est posé ET levé sur lui (comme un clic) ;
//   - le second anneau est regardé d'abord, seulement quand une famille est dépliée ;
//   - une famille : se déplie ; si elle l'est déjà, se replie ; ailleurs que sur un bouton : replie, puis ferme.
constexpr Decision decider(const Mesures &m, const Roue &roue, int famille, const Anneau &second,
                           const bool *est_famille, Point appui, Point relache, int seuil, bool annule) noexcept {
    Decision d;
    if (annule || a_glisse(appui, relache, seuil)) {
        return d;
    }
    const bool second_ouvert = famille >= 0 && famille < roue.premier.n;
    if (second_ouvert) {
        const int a = bouton_sous(m, second, m.diametre_2, appui);
        const int b = bouton_sous(m, second, m.diametre_2, relache);
        if (a >= 0 || b >= 0) {
            if (a == b) {
                d.geste = Geste::ChoisirChoix;
                d.rang = a;
                d.famille = famille;
            }
            return d;
        }
    }
    const int a = bouton_sous(m, roue.premier, m.diametre_1, appui);
    const int b = bouton_sous(m, roue.premier, m.diametre_1, relache);
    if (a >= 0 || b >= 0) {
        if (a != b) {
            return d;
        }
        d.rang = a;
        if (est_famille != nullptr && est_famille[a]) {
            d.geste = second_ouvert && famille == a ? Geste::Replier : Geste::Deplier;
        } else {
            d.geste = Geste::Choisir;
        }
        return d;
    }
    d.geste = second_ouvert ? Geste::Replier : Geste::Fermer;
    return d;
}

} // namespace ecran::roue
