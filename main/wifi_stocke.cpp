// Voir wifi_stocke.h. Mêmes espace et clés que l'hôte de l'appareil (ecran/hote_appareil.hpp : stockage::ESPACE_WIFI).
#include "wifi_stocke.h"

#include "nvs.h"

#include "ecran/hote_appareil.hpp"

namespace ecran {

bool wifi_stocke_lire(char *ssid, std::size_t capacite_ssid, char *mot_de_passe, std::size_t capacite_mdp) {
    if (ssid == nullptr || capacite_ssid == 0 || mot_de_passe == nullptr || capacite_mdp == 0) {
        return false;
    }
    ssid[0] = '\0';
    mot_de_passe[0] = '\0';
    nvs_handle_t h = 0;
    if (nvs_open(stockage::ESPACE_WIFI, NVS_READONLY, &h) != ESP_OK) {
        return false; // rien n'a jamais été retenu
    }
    size_t n = capacite_ssid;
    const bool ssid_lu = nvs_get_str(h, stockage::CLE_WIFI_SSID, ssid, &n) == ESP_OK && ssid[0] != '\0';
    size_t m = capacite_mdp;
    const bool mdp_lu = nvs_get_str(h, stockage::CLE_WIFI_MOT_DE_PASSE, mot_de_passe, &m) == ESP_OK;
    nvs_close(h);
    if (!ssid_lu) {
        ssid[0] = '\0';
        mot_de_passe[0] = '\0';
        return false;
    }
    if (!mdp_lu) {
        mot_de_passe[0] = '\0'; // réseau ouvert, ou mot de passe illisible : l'essai dira si ça passe
    }
    return true;
}

} // namespace ecran
