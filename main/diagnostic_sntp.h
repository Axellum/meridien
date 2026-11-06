// Diagnostic TEMPORAIRE (jalon 1b) de l'attente de l'heure : l'heure SNTP n'arrive pas toujours vite (12,7 s à plus de 60 s,
// 2 démarrages sur 20 sans heure dans les 60 s). Mesure, juste après l'obtention de l'adresse IP, trois choses séparément :
// la résolution DNS, la perte de paquets UDP (sondes SNTP directes, hors lwIP) et l'état du SNTP de lwIP.
// À retirer dès que la cause est trouvée (comme la commande série et le point d'essai tactile).
#pragma once

namespace ecran {

// Lance, une seule fois, une tâche de mesure qui écrit ses constats au journal (étiquette « diag_sntp »). Aucun effet sur l'heure.
void diagnostic_sntp_demarrer();

} // namespace ecran
