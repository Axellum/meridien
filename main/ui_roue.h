// La roue d'appui long (ADR-0021) : un appui long sur l'écran principal l'ouvre AU POINT D'APPUI. Le premier anneau porte les entrées
// que l'appelant donne (aujourd'hui « Réglages », « Détails » et, quand une alerte est en vigueur, « Alertes »), le moyeu au centre dit « Aller à »
// (ou le nom de la famille dépliée), le second anneau montre les choix d'une famille.
//
// LVGL PUR (contrat d'interface n° 3) : aucun en-tête ESP-IDF, le banc de rendu PC (tools/rendu) compile ce fichier tel quel.
//
// Un seul objet LVGL : le voile plein écran, posé sur la couche supérieure, qui dessine tout (disques, anneaux, mots, icônes) dans son
// événement DRAW_MAIN et reçoit les touchers. Toutes les positions et les zones tactiles viennent de `ecran::roue` (roue_geometrie.hpp),
// jamais recalculées ici ; ce qu'un toucher décide vient de `roue_commande.hpp` (testé sur le PC). Aucune animation : la roue
// apparaît, se replie ou se ferme d'un coup. Les objets sont créés à l'ouverture et détruits à la fermeture ; les polices (Roboto et le
// sous-ensemble d'icônes mdi_roue.ttf, en PSRAM) sont gardées d'une ouverture à l'autre.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/roue_commande.hpp"
#include "ecran/visuel_themes.hpp"
#include "lvgl.h"

namespace ecran {

// Les glyphes de main/assets/mdi_roue.ttf (ICONE_REGLAGES, ICONE_ALERTES, ICONE_MOYEU...), générés par tools/icones/generer_police_roue.py
// d'après tools/icones/roue_glyphes.json. Pour une nouvelle entrée de la roue : une ligne dans ce manifeste, puis régénérer.
#include "ui_roue_icones.inc"

// Un choix du second anneau d'une famille : son mot (déjà traduit), son icône (un glyphe de ui_roue_icones.inc : jamais nul, une icône
// n'est jamais un rectangle vide) et ce qu'il fait. `actif` : c'est le choix en vigueur (le profil épinglé, ou « Auto »), marqué comme
// la famille dépliée du premier anneau : filet et icône à la couleur d'accent ; il reste touchable (le choisir de nouveau ne change rien).
struct ChoixRoue {
    const char *texte;
    const char *icone;
    void (*action)();
    bool actif = false;
};

// Une entrée du premier anneau. UNE commande (`action`, `choix` nul) ou UNE famille (`choix` : `nb_choix` choix, `action` nul).
// Les chaînes doivent vivre aussi longtemps que la roue est ouverte (des littéraux, ou la table de traduction : tr()).
struct EntreeRoue {
    const char *texte;      // le mot du bouton, déjà traduit ; nul : pas de mot
    const char *icone;      // un glyphe de ui_roue_icones.inc (ICONE_*) ; jamais nul
    void (*action)();       // appelé dans la tâche LVGL APRÈS la fermeture de la roue (il peut charger un autre écran)
    const ChoixRoue *choix; // famille : ses choix ; nul pour une commande
    int nb_choix;
};

inline constexpr int ENTREES_ROUE_MAX = roue::BOUTONS_MAX;

// Ce que la roue lit pour s'ouvrir. `couche` : la couche supérieure de l'affichage (lv_display_get_layer_top), coordonnées d'écran
// exactes. `ttf` : Roboto Bold ; `mdi` : main/assets/mdi_roue.ttf. `theme` : doit vivre aussi longtemps que la roue (les thèmes de
// visuel_themes.hpp sont des constantes).
struct ContexteRoue {
    lv_obj_t *couche = nullptr;
    const std::uint8_t *ttf = nullptr;
    std::size_t ttf_taille = 0;
    const std::uint8_t *mdi = nullptr;
    std::size_t mdi_taille = 0;
    int32_t largeur = 0;
    int32_t hauteur = 0;
    const visuel::ThemePrincipal *theme = nullptr;
};

// Ouvre la roue au point (x, y) avec `n` entrées (1 à ENTREES_ROUE_MAX). Rend faux, sans rien créer, si elle est déjà ouverte, si un
// argument est invalide ou si une police n'a pas pu être créée. À appeler dans la tâche LVGL, verrou pris.
bool ui_roue_ouvrir(const ContexteRoue &ctx, const EntreeRoue *entrees, int n, int32_t x, int32_t y);

// Ferme la roue (le voile est supprimé au passage suivant du minuteur de LVGL). Sans effet si elle est fermée.
void ui_roue_fermer();

bool ui_roue_ouverte();

// Change le thème de la roue ouverte (le thème de l'écran principal vient de changer). Sans effet si elle est fermée.
void ui_roue_changer_theme(const visuel::ThemePrincipal &theme);

// Pour les essais et le banc de rendu : un toucher complet (posé puis levé au même point, sans appui long) comme si le doigt le faisait.
// Rend vrai si la roue est encore ouverte après.
bool ui_roue_simuler_toucher(int32_t x, int32_t y);

// Centre du bouton `rang` du premier anneau (rang < 0 ou trop grand : faux). Pour viser un bouton avec ui_roue_simuler_toucher.
bool ui_roue_centre_bouton(int rang, int32_t *x, int32_t *y);

// Où la roue s'est posée : le centre du moyeu, sa distance au point d'appui (0 : sous le doigt), si tout tient à l'écran (`tient`, la
// géométrie le dit) et si les boutons sont en dessous du moyeu. Pour les mesures et le banc de rendu ; faux si la roue est fermée.
struct PositionRoue {
    int32_t x;
    int32_t y;
    int32_t ecart;
    bool tient;
    bool dessous;
};
bool ui_roue_position(PositionRoue *position);

// Centre du choix `rang` du second anneau, quand une famille est dépliée (sinon, ou rang hors limites : faux).
bool ui_roue_centre_choix(int rang, int32_t *x, int32_t *y);

// Ce que la roue coûte : objets LVGL vivants (0 fermée, 1 ouverte), minuteurs, polices gardées, et la RAM statique de son état en
// octets (sizeof de l'état : 2 576 sur le PC, 1 628 sur la carte RISC-V 32 bits, pointeurs de 4 octets, depuis `ChoixRoue::actif` ; les polices TinyTTF et leurs glyphes sont en PSRAM, non comptés ici).
struct CoutRoue {
    int objets;
    int minuteurs;
    int polices;
    std::size_t ram_statique;
};
CoutRoue ui_roue_cout();

} // namespace ecran
