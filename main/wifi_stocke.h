// Le réseau Wi-Fi que la personne a choisi sur l'écran de réglages (ou dans l'installation guidée), retenu en mémoire permanente
// pour le prochain démarrage. Écrit par l'hôte de l'appareil (ecran/hote_appareil.hpp) SEULEMENT après une connexion réussie ; lu
// par main/reseau.* au démarrage, AVANT les identifiants de la compilation (identifiants_wifi.h).
//
// NVS : espace « wifi », clés « ssid » et « mdp » (textes). Le mot de passe est en clair dans la NVS de l'appareil (la NVS n'est pas
// chiffrée, voir docs/mesures/6-reglages-appareil.md) ; il ne va ni au journal, ni à l'écran, ni dans une réponse de l'API.
#pragma once

#include <cstddef>

namespace ecran {

// Copie le réseau retenu. Rend vrai si un nom de réseau (non vide) est retenu ; le mot de passe est alors dans `mot_de_passe` (vide
// pour un réseau ouvert). Faux sinon : `ssid` et `mot_de_passe` sont vidés, utiliser les identifiants de la compilation.
bool wifi_stocke_lire(char *ssid, std::size_t capacite_ssid, char *mot_de_passe, std::size_t capacite_mdp);

} // namespace ecran
