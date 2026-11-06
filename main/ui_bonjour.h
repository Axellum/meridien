// Écran « Bonjour » de la phase 1a : prouve l'affichage, les accents et le tactile. Complété en 1b par l'heure, la date et l'état
// du Wi-Fi (rafraîchis par un minuteur LVGL). Pas de fonction produit au-delà.
#pragma once

#include "board.h"
#include "boot_info.h"

namespace ecran {

// Construit l'écran sur l'affichage de la carte. À appeler après board_init().
void ui_bonjour_creer(const BootInfo &boot);

} // namespace ecran
