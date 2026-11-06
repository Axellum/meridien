// Commande « redemarrer » reçue sur la console USB-Serial/JTAG. Sert au banc de la phase 1b (20 redémarrages logiciels de suite
// pilotés depuis le PC, sans toucher à la tablette). TEMPORAIRE : à retirer avant la phase 2, comme la mire de test du tactile.
#pragma once

namespace ecran {

// Lance la tâche qui lit la console. Rend la main tout de suite. Une ligne « redemarrer » redémarre la carte par board_restart().
void commande_serie_demarrer();

} // namespace ecran
