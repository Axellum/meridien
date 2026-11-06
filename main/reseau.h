// Réseau de l'application : lien avec le coprocesseur Wi-Fi (couche carte), connexion au point d'accès, heure par SNTP.
// Phase 1b. Les identifiants Wi-Fi sont ceux de la compilation (identifiants_wifi.h, hors git) ; la saisie par l'utilisateur
// et la NVS viennent avec la page de réglages (phase 3). Rien d'ici n'écrit l'SSID ni le mot de passe, ni à l'écran ni au journal.
#pragma once

#include <cstdint>

#include "boot_info.h"

namespace ecran {

struct EtatReseau {
    bool identifiants_presents; // un SSID a été compilé dans le firmware (sa valeur n'est jamais exposée)
    bool coprocesseur_pret;     // lien P4 <-> ESP32-C6 établi
    bool coprocesseur_echec;    // lien impossible après les essais de la couche carte
    bool wifi_connecte;         // associé au point d'accès
    bool ip_obtenue;
    bool heure_synchronisee;       // au moins une réponse SNTP reçue depuis le démarrage
    char version_coprocesseur[24]; // « 1.4.1 » ; vide si non lue
    char ip[16];                   // « 192.168.x.y » ; vide sans adresse
    uint32_t deconnexions;         // pertes du lien Wi-Fi depuis le démarrage
    // Jalons en millisecondes depuis le lancement de l'application (esp_timer) ; -1 tant que non atteints.
    int32_t lien_ms;
    int32_t wifi_ms;
    int32_t ip_ms;
    int32_t heure_ms;
};

// Lance la tâche réseau et rend la main tout de suite. À appeler une fois, après board_init().
void reseau_demarrer(const BootInfo &boot);

// Copie de l'état courant (sûre depuis n'importe quelle tâche).
EtatReseau reseau_etat();

} // namespace ecran
