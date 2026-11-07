#include "reglages_alertes.h"

#include <cstdint>
#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "nvs.h"

#include "vigie_client.h"

namespace {

const char *TAG = "alertes";
constexpr const char *kEspace = "alertes";
constexpr const char *kCle = "reglage";
constexpr uint8_t kVersion = 1;

// Contenu du blob. Indices (et non codes) pour le niveau : jaune 0, orange 1, rouge 2 (ne jamais renuméroter). L'adresse est celle que
// vigie::normaliser_adresse a rendue, ou vide (instance publique) ; zéro final compris.
struct AlertesNvs {
    uint8_t version;
    uint8_t actives;
    uint8_t niveau_min;
    uint8_t reserve;
    int32_t periode_min;
    char adresse[ecran::vigie::ADRESSE_MAX + 1 + 3]; // 84 octets : la taille du blob ne dépend pas de l'alignement
};
static_assert(sizeof(AlertesNvs) == 92, "format des réglages d'alertes en NVS : 92 octets");
static_assert(sizeof(AlertesNvs::adresse) >= ecran::vigie::ADRESSE_MAX + 1,
              "l'adresse et son zéro tiennent dans le blob");

portMUX_TYPE g_verrou = portMUX_INITIALIZER_UNLOCKED;
ecran::reglages::Alertes g_courant{};

} // namespace

namespace ecran {

void reglages_alertes_demarrer() {
    reglages::Alertes lu{}; // défauts : activées, instance publique, 15 minutes, jaune
    nvs_handle_t h = 0;
    const esp_err_t ouvert = nvs_open(kEspace, NVS_READONLY, &h);
    if (ouvert == ESP_OK) {
        AlertesNvs brut = {};
        size_t taille = sizeof(brut);
        const esp_err_t err = nvs_get_blob(h, kCle, &brut, &taille);
        nvs_close(h);
        if (err == ESP_OK && taille == sizeof(brut) && brut.version == kVersion) {
            brut.adresse[sizeof(brut.adresse) - 1] = '\0';
            lu = reglages::alertes_depuis_nvs(brut.actives, brut.periode_min, brut.niveau_min,
                                              std::string_view(brut.adresse));
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "réglages des alertes non lus (%s, %u octets) : valeurs par défaut", esp_err_to_name(err),
                     static_cast<unsigned>(taille));
        }
    } else if (ouvert != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS « %s » non ouverte (%s) : valeurs par défaut", kEspace, esp_err_to_name(ouvert));
    }
    portENTER_CRITICAL(&g_verrou);
    g_courant = lu;
    portEXIT_CRITICAL(&g_verrou);
    ESP_LOGI(TAG, "alertes : %s, période %d min, niveau minimal %.*s, adresse %s",
             lu.actives ? "activées" : "désactivées", static_cast<int>(lu.periode_min),
             static_cast<int>(reglages::code(lu.niveau_min).size()), reglages::code(lu.niveau_min).data(),
             lu.adresse[0] != '\0' ? "personnalisée" : "instance publique");
}

reglages::Alertes reglages_alertes() {
    portENTER_CRITICAL(&g_verrou);
    const reglages::Alertes a = g_courant;
    portEXIT_CRITICAL(&g_verrou);
    return a;
}

bool reglages_alertes_enregistrer(const reglages::Alertes &nouveau) {
    // Les mêmes règles que celles de la page : une valeur refusée par l'API l'est aussi ici (l'écran de l'appareil ne passe pas par l'API).
    if (!reglages::periode_alertes_valide(nouveau.periode_min)) {
        ESP_LOGW(TAG, "période de %d min refusée (10 à 180) : rien n'est changé",
                 static_cast<int>(nouveau.periode_min));
        return false;
    }
    char propre[vigie::ADRESSE_MAX + 1] = {};
    if (nouveau.adresse[0] != '\0' &&
        vigie::normaliser_adresse(std::string_view(nouveau.adresse), propre, sizeof propre) == 0) {
        ESP_LOGW(TAG, "adresse de Vigie refusée (https:// obligatoire) : rien n'est changé");
        return false;
    }
    if (static_cast<std::size_t>(nouveau.niveau_min) >= vigie::NOMBRE_NIVEAUX_MIN) {
        ESP_LOGW(TAG, "niveau minimal inconnu : rien n'est changé");
        return false;
    }
    reglages::Alertes retenu = nouveau;
    std::memcpy(retenu.adresse, propre, sizeof(retenu.adresse)); // la forme normalisée (ou vide) est celle qu'on garde

    AlertesNvs brut = {};
    brut.version = kVersion;
    brut.actives = retenu.actives ? 1 : 0;
    brut.niveau_min = static_cast<uint8_t>(retenu.niveau_min);
    brut.periode_min = retenu.periode_min;
    std::memcpy(brut.adresse, retenu.adresse, sizeof(retenu.adresse)); // le reste du blob est resté à zéro
    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(kEspace, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, kCle, &brut, sizeof(brut));
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "réglages des alertes non enregistrés en NVS : %s (l'ancien état reste en service)",
                 esp_err_to_name(err));
        return false;
    }
    portENTER_CRITICAL(&g_verrou);
    g_courant = retenu;
    portEXIT_CRITICAL(&g_verrou);
    ESP_LOGI(TAG, "alertes enregistrées : %s, période %d min, niveau minimal %.*s, adresse %s",
             retenu.actives ? "activées" : "désactivées", static_cast<int>(retenu.periode_min),
             static_cast<int>(reglages::code(retenu.niveau_min).size()), reglages::code(retenu.niveau_min).data(),
             retenu.adresse[0] != '\0' ? "personnalisée" : "instance publique");
    vigie_client_a_change();
    return true;
}

} // namespace ecran
