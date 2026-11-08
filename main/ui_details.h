// Page « Détails » : les SEPT GRANDEURS que l'accueil refait n'affiche plus (vent, pluie, humidité, pression, indice UV, soleil, lune),
// en grand, sur une page plein écran atteinte par la roue d'appui long (« Pages ▸ Détails »). Rien ne se règle ici : un seul bouton,
// « Fermer ». Aucune animation ; l'écran change d'un coup (AGENTS.md, préférences du produit).
//
// LVGL PUR, comme ui_principal.h et ui_alertes.h : ui_details.cpp n'inclut aucun en-tête ESP-IDF, FreeRTOS ni journal. Il reçoit la page
// déjà composée (ecran/grandeurs.hpp : valeurs, unités, « — » pour une grandeur inconnue, grisé d'une météo périmée), la police par ses
// octets, le thème ; le rendu PC (tools/rendu) le compile tel quel. Le branchement sur l'appareil (lecture de la météo, de l'heure et du
// réglage de thème, écran à charger et à rendre, minuteur) est dans ui_details_ouvrir.cpp.
//
// Trois objets LVGL en tout, sous l'écran (la barre, la grille des sept tuiles, le bouton « Fermer ») : chacun dessine tout son contenu
// dans LV_EVENT_DRAW_MAIN, et la grille n'est redessinée que si l'empreinte de la page a changé (grandeurs::empreinte). L'état de la page
// vit en PSRAM le temps de l'ouverture (LVGL alloue en PSRAM, lvgl_memoire.cpp) : seuls les pointeurs des trois polices restent en RAM
// interne entre deux ouvertures.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/grandeurs.hpp"
#include "ecran/visuel_themes.hpp"
#include "lvgl.h"

namespace ecran {

struct ContexteDetails {
    lv_obj_t *racine = nullptr;        // l'écran LVGL (vide) qui reçoit la page, de la taille de l'écran
    const std::uint8_t *ttf = nullptr; // Roboto Bold embarquée (main/assets/roboto_700.ttf)
    std::size_t ttf_taille = 0;
    int32_t largeur = 0; // taille LOGIQUE de l'écran (rotation comprise)
    int32_t hauteur = 0;
    const visuel::ThemePrincipal *theme =
        &visuel::kThemeCiel;    // doit vivre aussi longtemps que la page (les thèmes sont des constantes)
    void (*fermer)() = nullptr; // « Fermer » : rend la main à l'écran d'avant ; peut être nul (rendu PC)
};

// Construit la page sur `ctx.racine`. Rend faux si une police n'a pas pu être créée, si la mémoire manque ou si le contexte est incomplet.
// L'état de la page est libéré tout seul quand `ctx.racine` est supprimé (LV_EVENT_DELETE).
bool ui_details_creer(const ContexteDetails &ctx, const grandeurs::Page &page);

// Met à jour la page montrée (la dernière créée) : ne redessine que si l'empreinte a changé. Sans effet si aucune page n'est ouverte.
// À appeler dans la tâche LVGL, verrou pris.
void ui_details_mettre_a_jour(const grandeurs::Page &page);

} // namespace ecran

// Ouvre la page « Détails » par-dessus l'écran actif : lit la météo, l'heure et le thème, construit la page, la charge d'un coup et
// lance un minuteur de mise à jour (500 ms). Sans effet si la page est déjà ouverte. C'est la roue d'appui long (ADR-0021) qui l'appelle :
// le bouton « Détails » de son premier anneau (ecran_principal.cpp, composer_entrees). À appeler dans la tâche LVGL, verrou pris. Hors de `ecran`, comme
// ui_reglages_ouvrir() (ui_reglages.h) : c'est un pointeur de fonction que l'écran principal ou la roue reçoit. Défini dans
// ui_details_ouvrir.cpp (colle du firmware), jamais compilé par le rendu PC.
void ui_details_ouvrir();

// Referme la page : l'écran qui était actif à l'ouverture revient d'un coup, le minuteur s'arrête, la page est supprimée. Sans effet si
// elle n'est pas ouverte. C'est aussi ce que fait le bouton « Fermer ».
void ui_details_fermer();
