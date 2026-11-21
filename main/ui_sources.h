// Page « Sources » (ADR-0029) : ce que le bouton « Sources » de la roue d'appui long ouvre. La liste de ce que l'écran affiche VRAIMENT
// (la météo, créditée au service qui l'a donnée, les lieux, les alertes de Vigie, les normales climatiques de Météo-France, la mer et les
// spots, les polices et icônes) avec, pour chacun, la phrase de sa licence. Les phrases viennent de ecran/credits.hpp, la même table que
// la page de réglages : une seule source de vérité.
//
// LVGL PUR, comme ui_alertes.h : aucun en-tête ESP-IDF, FreeRTOS ni journal. La page reçoit tout par paramètre (la liste composée par
// credits::composer) pour que le rendu PC (tools/rendu) la compile telle quelle. Elle est une IMAGE du moment de l'ouverture. Pas de
// fondu : l'écran change d'un coup (AGENTS.md, préférences du produit).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/credits.hpp"
#include "ecran/visuel_themes.hpp"
#include "lvgl.h"

namespace ecran {

struct ContexteSources {
    lv_obj_t *racine = nullptr;        // l'écran LVGL (vide) qui reçoit la page, de la taille de l'écran
    const std::uint8_t *ttf = nullptr; // Roboto Bold embarquée (main/assets/roboto_700.ttf)
    std::size_t ttf_taille = 0;
    int32_t largeur = 0; // taille LOGIQUE de l'écran (rotation comprise)
    int32_t hauteur = 0;
    const visuel::ThemePrincipal *theme = &visuel::kThemeCiel;
    void (*fermer)() = nullptr; // « Fermer » : rend la main à l'écran principal ; peut être nul (rendu PC)
};

// Construit la page sur `ctx.racine`. Rend faux si une police n'a pas pu être créée ou si les paramètres manquent.
bool ui_sources_creer(const ContexteSources &ctx, const credits::Liste &liste);

} // namespace ecran
