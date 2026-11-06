// Écran « Bonjour » de la phase 1a : prouve l'affichage, les accents et le tactile. Pas de fonction produit.
#pragma once

#include "board.h"
#include "boot_info.h"

namespace ecran {

// Construit l'écran sur l'affichage de la carte. À appeler après board_init().
void ui_bonjour_creer(const BootInfo &boot);

} // namespace ecran
