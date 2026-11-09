// Zone météo de l'appareil (contrat meteo_zone.h) : lieu dont on affiche la météo. Défaut Lyon ; enregistrée en NVS (espace
// « meteo », clé « zone »). Les règles de validité (UTF-8 du nom, bornes des coordonnées, caractères du fuseau) sont dans le noyau
// pur (ecran/meteo_zone_regles.hpp, testé sur le PC) : rien d'invraisemblable ne part vers Open-Meteo ni vers la NVS.
//
// Les coordonnées sont gardées en e5 (centièmes de millième de degré) : c'est la valeur exacte de la requête, sans aller-retour
// par un flottant. Un changement de POSITION (pas de simple nom) efface la météo de l'ancien lieu et relance une requête.
#include "meteo_zone.h"

#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "nvs.h"

#include "ecran/meteo_zone_regles.hpp"
#include "meteo_interne.h"

namespace {

const char *TAG = "meteo_zone";
constexpr const char *kEspace = "meteo";
constexpr const char *kCle = "zone";
constexpr uint8_t kVersion = 1;

// Contenu du blob de la NVS (la NVS protège déjà chaque entrée par une somme de contrôle). Taille fixe, relue telle quelle.
struct ZoneNvs {
    uint8_t version;
    char nom[48];
    char fuseau[40];
    uint8_t reserve[3]; // zéros, alignement des entiers
    int32_t latitude_e5;
    int32_t longitude_e5;
};
static_assert(sizeof(ZoneNvs) == 100, "format de la zone en NVS : 100 octets");
static_assert(sizeof(ZoneNvs::nom) == sizeof(ZoneMeteo::nom) && sizeof(ZoneNvs::fuseau) == sizeof(ZoneMeteo::fuseau),
              "les textes de la NVS et du contrat ont la même taille");

struct Courante {
    ZoneMeteo zone;
    ecran::PositionE5 position;
};

portMUX_TYPE g_verrou = portMUX_INITIALIZER_UNLOCKED;
Courante g_courante = {};
bool g_chargee = false;

Courante par_defaut() {
    Courante c = {};
    std::strcpy(c.zone.nom, "Lyon");
    std::strcpy(c.zone.fuseau, "Europe/Paris");
    c.position = {4574906, 484789};
    c.zone.latitude = static_cast<float>(c.position.latitude_e5) / 100000.0F;
    c.zone.longitude = static_cast<float>(c.position.longitude_e5) / 100000.0F;
    return c;
}

// Copie un texte déjà validé (terminé par un zéro) en mettant à zéro tout le reste du tampon, pour ne rien écrire d'autre en NVS.
// La copie s'arrête à N - 1 octets (le dernier reste à zéro) : la coupe de strncpy, écrite pour que le compilateur la voie.
template <std::size_t N> void copier_texte(char (&vers)[N], const char *depuis) {
    std::memset(vers, 0, N);
    std::size_t n = 0;
    while (n < N - 1 && depuis[n] != '\0') {
        ++n;
    }
    std::memcpy(vers, depuis, n);
}

bool depuis_nvs(const ZoneNvs &brut, Courante &sortie) {
    if (brut.version != kVersion || !ecran::meteo::nom_de_zone_valide(brut.nom, sizeof(brut.nom)) ||
        !ecran::meteo::fuseau_de_zone_valide(brut.fuseau, sizeof(brut.fuseau))) {
        return false;
    }
    if (brut.latitude_e5 < -ecran::meteo::LATITUDE_E5_MAX || brut.latitude_e5 > ecran::meteo::LATITUDE_E5_MAX ||
        brut.longitude_e5 < -ecran::meteo::LONGITUDE_E5_MAX || brut.longitude_e5 > ecran::meteo::LONGITUDE_E5_MAX) {
        return false;
    }
    Courante c = {};
    copier_texte(c.zone.nom, brut.nom);
    copier_texte(c.zone.fuseau, brut.fuseau);
    c.position = {brut.latitude_e5, brut.longitude_e5};
    c.zone.latitude = static_cast<float>(brut.latitude_e5) / 100000.0F;
    c.zone.longitude = static_cast<float>(brut.longitude_e5) / 100000.0F;
    sortie = c;
    return true;
}

void charger_si_besoin() {
    portENTER_CRITICAL(&g_verrou);
    const bool deja = g_chargee;
    portEXIT_CRITICAL(&g_verrou);
    if (deja) {
        return;
    }
    Courante lue = par_defaut();
    nvs_handle_t h = 0;
    const esp_err_t ouvert = nvs_open(kEspace, NVS_READONLY, &h);
    if (ouvert == ESP_OK) {
        ZoneNvs brut = {};
        size_t taille = sizeof(brut);
        const esp_err_t err = nvs_get_blob(h, kCle, &brut, &taille);
        nvs_close(h);
        if (err == ESP_OK && taille == sizeof(brut)) {
            if (!depuis_nvs(brut, lue)) {
                ESP_LOGW(TAG, "zone enregistrée illisible ou invraisemblable : zone par défaut (Lyon)");
                lue = par_defaut();
            }
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "zone enregistrée non lue (%s, %u octets) : zone par défaut (Lyon)", esp_err_to_name(err),
                     static_cast<unsigned>(taille));
        }
    } else if (ouvert != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS non ouverte (%s) : zone par défaut (Lyon)", esp_err_to_name(ouvert));
    }
    portENTER_CRITICAL(&g_verrou);
    if (!g_chargee) { // deux tâches ont pu charger en même temps : la première gagne, les deux ont lu la même chose
        g_courante = lue;
        g_chargee = true;
    }
    portEXIT_CRITICAL(&g_verrou);
}

} // namespace

namespace ecran {

void zone_lire_e5(ZoneMeteo *zone, PositionE5 *position) {
    charger_si_besoin();
    portENTER_CRITICAL(&g_verrou);
    const Courante c = g_courante;
    portEXIT_CRITICAL(&g_verrou);
    if (zone != nullptr) {
        *zone = c.zone;
    }
    if (position != nullptr) {
        *position = c.position;
    }
}

} // namespace ecran

bool zone_lire(ZoneMeteo *sortie) {
    if (sortie == nullptr) {
        return false;
    }
    ecran::zone_lire_e5(sortie, nullptr);
    return true;
}

bool zone_ecrire(const ZoneMeteo &zone) {
    using namespace ecran::meteo;
    int32_t lat_e5 = 0;
    int32_t lon_e5 = 0;
    if (!nom_de_zone_valide(zone.nom, sizeof(zone.nom)) || !fuseau_de_zone_valide(zone.fuseau, sizeof(zone.fuseau)) ||
        !latitude_vers_e5(static_cast<double>(zone.latitude), lat_e5) ||
        !longitude_vers_e5(static_cast<double>(zone.longitude), lon_e5)) {
        ESP_LOGW(TAG, "zone refusée (nom, fuseau ou coordonnées invalides) : rien n'est enregistré");
        return false;
    }

    charger_si_besoin();
    Courante nouvelle = {};
    copier_texte(nouvelle.zone.nom, zone.nom);
    copier_texte(nouvelle.zone.fuseau, zone.fuseau);
    nouvelle.position = {lat_e5, lon_e5};
    nouvelle.zone.latitude = static_cast<float>(lat_e5) / 100000.0F;
    nouvelle.zone.longitude = static_cast<float>(lon_e5) / 100000.0F;

    portENTER_CRITICAL(&g_verrou);
    const Courante ancienne = g_courante;
    portEXIT_CRITICAL(&g_verrou);
    const bool position_changee = !(ancienne.position == nouvelle.position);
    if (!position_changee && std::strcmp(ancienne.zone.nom, nouvelle.zone.nom) == 0 &&
        std::strcmp(ancienne.zone.fuseau, nouvelle.zone.fuseau) == 0) {
        return true; // identique : rien à écrire, aucune nouvelle requête
    }

    ZoneNvs brut = {};
    brut.version = kVersion;
    copier_texte(brut.nom, nouvelle.zone.nom);
    copier_texte(brut.fuseau, nouvelle.zone.fuseau);
    brut.latitude_e5 = lat_e5;
    brut.longitude_e5 = lon_e5;
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
        ESP_LOGE(TAG, "zone non enregistrée en NVS : %s (l'ancienne zone reste en service)", esp_err_to_name(err));
        return false;
    }

    portENTER_CRITICAL(&g_verrou);
    g_courante = nouvelle;
    g_chargee = true;
    portEXIT_CRITICAL(&g_verrou);
    ESP_LOGI(TAG, "zone enregistrée : « %s »%s", nouvelle.zone.nom,
             position_changee ? " ; nouvelle requête météo tout de suite" : " (même position)");
    if (position_changee) {
        ecran::meteo_client_zone_changee();
    }
    return true;
}
