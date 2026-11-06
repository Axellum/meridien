// L'hôte des réglages sur l'appareil (ADR-0007) : ce que les écrans de réglages et d'installation guidée reçoivent comme
// `HoteReglages`. La logique est dans le noyau pur (ecran/hote_appareil.hpp, testée sur le PC) ; main/hote_appareil_esp.cpp ne fait
// que brancher ses deux portes sur ESP-IDF : la mémoire permanente (NVS) et le reste de l'appareil (réglages de la page web, Wi-Fi
// de main/reseau_wifi.h, téléchargement du géocodage, version, redémarrage).
#pragma once

#include "ecran/hote_reglages.hpp"

namespace ecran {

// L'hôte unique de l'appareil, créé au premier appel. À appeler depuis la tâche de l'interface (verrou d'affichage pris) : la
// progression de l'installation et le Wi-Fi retenu sont lus et écrits dans cette tâche seulement.
HoteReglages &hote_de_l_appareil();

} // namespace ecran
