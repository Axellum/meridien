// Écran « Bonjour » de la phase 1a : prouve l'affichage, les accents et le tactile. Complété en 1b par l'heure, la date et l'état
// du Wi-Fi (rafraîchis par un minuteur LVGL). Pas de fonction produit au-delà.
#pragma once

#include "board.h"
#include "boot_info.h"

namespace ecran {

// Construit l'écran sur l'affichage de la carte. À appeler après board_init().
void ui_bonjour_creer(const BootInfo &boot);

// Rappel appelé (dans la tâche LVGL, verrou pris) sur un appui long du fond de l'écran. Sert à revenir à l'écran principal
// quand « Bonjour » est devenu l'écran de diagnostic. Sans rappel, l'appui long ne fait rien.
void ui_bonjour_sur_appui_long(void (*rappel)());

} // namespace ecran
