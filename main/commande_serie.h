// Commande « redemarrer » reçue sur la console USB-Serial/JTAG. Sert au banc de la phase 1b (20 redémarrages logiciels de suite
// pilotés depuis le PC, sans toucher à la tablette). TEMPORAIRE : à retirer avant la phase 2, comme la mire de test du tactile.
#pragma once

namespace ecran {

// Lance la tâche qui lit la console. Rend la main tout de suite. Une ligne « redemarrer » redémarre la carte par board_restart().
void commande_serie_demarrer();

// Relevé de la mémoire interne libre à une étape du démarrage (budget de mémoire du jalon 1b, TEMPORAIRE comme la commande
// « memoire »). Une ligne « JALON_MEM … » avec le delta depuis le relevé précédent.
void memoire_jalon(const char *etape);

// Parcours du tas interne (totaux, blocs par classe de taille, plus gros blocs), comme la commande série « blocs ».
void memoire_blocs();

} // namespace ecran
