// BOUCHON — corps provisoire de l'interface réseau des réglages (bouchons/reseau_wifi.h) : aucune recherche ne trouve rien, aucun essai
// ne démarre. À supprimer avec ce dossier quand jalon-1b livre la vraie version dans main/reseau.*. N'est PAS dans main/CMakeLists.txt
// tant que la colle des réglages (milestone « binaire flashable ») ne l'ajoute pas ; jamais compilé par le rendu PC.
#include "reseau_wifi.h"

#include "esp_log.h"

namespace ecran {

namespace {
constexpr const char *kEtiquette = "reseau_wifi";
bool g_dit = false;

void le_dire_une_fois() {
    if (!g_dit) {
        g_dit = true;
        ESP_LOGW(kEtiquette, "BOUCHON : l'interface Wi-Fi des reglages n'est pas encore fournie par reseau.cpp");
    }
}
} // namespace

void reseau_scanner_lancer() { le_dire_une_fois(); }

bool reseau_scanner_en_cours() { return false; }

std::size_t reseau_scanner_resultats(PointAcces *, std::size_t) { return 0; }

bool reseau_connecter(const char *, const char *, SurResultatConnexion, void *) {
    le_dire_une_fois();
    return false;
}

void reseau_abandonner() {}

std::size_t reseau_ssid_courant(char *sortie, std::size_t capacite) {
    if (sortie != nullptr && capacite > 0) {
        sortie[0] = '\0';
    }
    return 0;
}

} // namespace ecran
