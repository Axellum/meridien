// Réglages de la page (contrat main/reglages.h) : langue de l'écran, langue de l'assistant, thème, luminosité en NVS (espace
// « reglages », clé « affichage », un blob de 5 octets écrit d'un seul coup), et le complément de la ville (région, pays) qui
// s'ajoute à la zone de meteo_zone.h sans en créer une seconde : la zone elle-même (nom, position, fuseau) n'est lue et écrite
// que par zone_lire / zone_ecrire.
//
// Toutes les règles (bornes, valeurs par défaut quand la NVS est abîmée) sont dans le noyau pur ecran/reglages_valeurs.hpp,
// testé sur le PC ; ce fichier ne fait que lire, écrire et appliquer.
#include "reglages.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "nvs.h"

#include "ecran/i18n.hpp"
#include "meteo_zone.h"

namespace {

const char *TAG = "reglages";
constexpr const char *kEspace = "reglages";
constexpr const char *kCleAffichage = "affichage";
constexpr const char *kCleZoneExtra = "zone_extra";
constexpr uint8_t kVersionAffichage = 1;
constexpr uint8_t kVersionExtra = 1;

// Contenu du blob « affichage ». Indices (et non codes) : fr0 en1 de2 nl3 es4 it5 tr6 ; auto0 clair1 sombre2.
struct AffichageNvs {
    uint8_t version;
    uint8_t langue_ecran;
    uint8_t langue_assistant;
    uint8_t theme;
    uint8_t luminosite;
};
static_assert(sizeof(AffichageNvs) == 5, "format de l'affichage en NVS : 5 octets");

// Complément de la ville. Il n'est valable que pour la zone qui porte ce nom à cette position : si la zone a été changée par un
// autre chemin, la région et le pays seraient faux, donc on ne les rend pas.
struct ZoneExtraNvs {
    uint8_t version;
    char nom[48];
    char region[64];
    char pays[64];
    char code_pays[4];
    uint8_t reserve[3];
    int32_t latitude_e5;
    int32_t longitude_e5;
};
static_assert(sizeof(ZoneExtraNvs) == 192, "format du complément de zone en NVS : 192 octets");

portMUX_TYPE g_verrou = portMUX_INITIALIZER_UNLOCKED;
ecran::reglages::Affichage g_courant{};
ecran::RappelReglages g_rappel = nullptr;

ecran::reglages::Affichage affichage_depuis_nvs(const AffichageNvs &b) {
    ecran::reglages::Affichage a{};
    a.langue_ecran = ecran::i18n::langue_depuis_indice(b.langue_ecran);
    a.langue_assistant = ecran::i18n::langue_depuis_indice(b.langue_assistant);
    a.theme = ecran::reglages::theme_depuis_indice(b.theme);
    a.luminosite = ecran::reglages::luminosite_depuis_nvs(b.luminosite);
    return a;
}

int32_t en_e5(float degres) { return static_cast<int32_t>(std::lround(static_cast<double>(degres) * 100000.0)); }

// Mêmes arrondis des deux côtés (écriture et relecture) : la comparaison est exacte.
bool memes_lieux(const ZoneExtraNvs &extra, const ZoneMeteo &zone) {
    return std::strncmp(extra.nom, zone.nom, sizeof(extra.nom)) == 0 && extra.latitude_e5 == en_e5(zone.latitude) &&
           extra.longitude_e5 == en_e5(zone.longitude);
}

bool lire_extra(ZoneExtraNvs &extra) {
    nvs_handle_t h = 0;
    if (nvs_open(kEspace, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t taille = sizeof(extra);
    const esp_err_t err = nvs_get_blob(h, kCleZoneExtra, &extra, &taille);
    nvs_close(h);
    if (err != ESP_OK || taille != sizeof(extra) || extra.version != kVersionExtra) {
        return false;
    }
    // Les textes reviennent de la NVS : on s'assure qu'ils finissent par un zéro avant de les lire.
    extra.nom[sizeof(extra.nom) - 1] = '\0';
    extra.region[sizeof(extra.region) - 1] = '\0';
    extra.pays[sizeof(extra.pays) - 1] = '\0';
    extra.code_pays[sizeof(extra.code_pays) - 1] = '\0';
    return true;
}

} // namespace

namespace ecran {

void reglages_demarrer() {
    reglages::Affichage lu{}; // défauts : français, thème auto, 75 %
    nvs_handle_t h = 0;
    const esp_err_t ouvert = nvs_open(kEspace, NVS_READONLY, &h);
    if (ouvert == ESP_OK) {
        AffichageNvs brut = {};
        size_t taille = sizeof(brut);
        const esp_err_t err = nvs_get_blob(h, kCleAffichage, &brut, &taille);
        nvs_close(h);
        if (err == ESP_OK && taille == sizeof(brut) && brut.version == kVersionAffichage) {
            lu = affichage_depuis_nvs(brut);
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "réglages d'affichage non lus (%s, %u octets) : valeurs par défaut", esp_err_to_name(err),
                     static_cast<unsigned>(taille));
        }
    } else if (ouvert != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS « %s » non ouverte (%s) : valeurs par défaut", kEspace, esp_err_to_name(ouvert));
    }
    portENTER_CRITICAL(&g_verrou);
    g_courant = lu;
    portEXIT_CRITICAL(&g_verrou);
    i18n::definir_langue(lu.langue_ecran);
    ESP_LOGI(TAG, "affichage : écran %.*s, assistant %.*s, thème %.*s, luminosité %d %%",
             static_cast<int>(i18n::code(lu.langue_ecran).size()), i18n::code(lu.langue_ecran).data(),
             static_cast<int>(i18n::code(lu.langue_assistant).size()), i18n::code(lu.langue_assistant).data(),
             static_cast<int>(reglages::code(lu.theme).size()), reglages::code(lu.theme).data(), lu.luminosite);
}

reglages::Affichage reglages_affichage() {
    portENTER_CRITICAL(&g_verrou);
    const reglages::Affichage a = g_courant;
    portEXIT_CRITICAL(&g_verrou);
    return a;
}

bool reglages_enregistrer_affichage(const reglages::Affichage &nouveau) {
    if (!reglages::luminosite_valide(nouveau.luminosite)) {
        ESP_LOGW(TAG, "luminosité %d %% refusée (5 à 100) : rien n'est changé", nouveau.luminosite);
        return false;
    }
    AffichageNvs brut = {};
    brut.version = kVersionAffichage;
    brut.langue_ecran = static_cast<uint8_t>(i18n::indice(nouveau.langue_ecran));
    brut.langue_assistant = static_cast<uint8_t>(i18n::indice(nouveau.langue_assistant));
    brut.theme = static_cast<uint8_t>(nouveau.theme);
    brut.luminosite = static_cast<uint8_t>(nouveau.luminosite);
    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(kEspace, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, kCleAffichage, &brut, sizeof(brut));
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "affichage non enregistré en NVS : %s (l'ancien état reste en service)", esp_err_to_name(err));
        return false;
    }

    const reglages::Affichage avant = reglages_affichage();
    portENTER_CRITICAL(&g_verrou);
    g_courant = nouveau;
    RappelReglages rappel = g_rappel;
    portEXIT_CRITICAL(&g_verrou);

    i18n::definir_langue(nouveau.langue_ecran);
    if (nouveau.luminosite != avant.luminosite) {
        board_set_backlight_percent(nouveau.luminosite);
    }
    ESP_LOGI(TAG, "affichage enregistré : écran %.*s, assistant %.*s, thème %.*s, luminosité %d %%",
             static_cast<int>(i18n::code(nouveau.langue_ecran).size()), i18n::code(nouveau.langue_ecran).data(),
             static_cast<int>(i18n::code(nouveau.langue_assistant).size()), i18n::code(nouveau.langue_assistant).data(),
             static_cast<int>(reglages::code(nouveau.theme).size()), reglages::code(nouveau.theme).data(),
             nouveau.luminosite);
    if (rappel != nullptr) {
        rappel(nouveau);
    }
    return true;
}

void reglages_definir_rappel(RappelReglages rappel) {
    portENTER_CRITICAL(&g_verrou);
    g_rappel = rappel;
    portEXIT_CRITICAL(&g_verrou);
}

bool reglages_zone_lire(reglages::Zone &zone) {
    ZoneMeteo m = {};
    if (!::zone_lire(&m)) {
        return false;
    }
    zone = reglages::Zone{};
    reglages::copier_texte(m.nom, zone.nom, sizeof(zone.nom));
    reglages::copier_texte(m.fuseau, zone.fuseau, sizeof(zone.fuseau));
    zone.latitude = static_cast<double>(en_e5(m.latitude)) / 100000.0;
    zone.longitude = static_cast<double>(en_e5(m.longitude)) / 100000.0;
    ZoneExtraNvs extra = {};
    if (lire_extra(extra) && memes_lieux(extra, m)) {
        reglages::copier_texte(extra.region, zone.region, sizeof(zone.region));
        reglages::copier_texte(extra.pays, zone.pays, sizeof(zone.pays));
        reglages::copier_texte(extra.code_pays, zone.code_pays, sizeof(zone.code_pays));
    }
    return true;
}

bool reglages_zone_enregistrer(const reglages::Zone &zone) {
    ZoneMeteo m = {};
    reglages::copier_texte(zone.nom, m.nom, sizeof(m.nom));
    reglages::copier_texte(zone.fuseau, m.fuseau, sizeof(m.fuseau));
    m.latitude = static_cast<float>(zone.latitude);
    m.longitude = static_cast<float>(zone.longitude);
    if (!::zone_ecrire(m)) {
        return false;
    }
    // La zone relue (arrondie comme elle le sera à chaque démarrage) : c'est elle que le complément doit suivre.
    ZoneMeteo relue = {};
    if (!::zone_lire(&relue)) {
        return true;
    }
    ZoneExtraNvs extra = {};
    extra.version = kVersionExtra;
    reglages::copier_texte(relue.nom, extra.nom, sizeof(extra.nom));
    reglages::copier_texte(zone.region, extra.region, sizeof(extra.region));
    reglages::copier_texte(zone.pays, extra.pays, sizeof(extra.pays));
    reglages::copier_texte(zone.code_pays, extra.code_pays, sizeof(extra.code_pays));
    extra.latitude_e5 = en_e5(relue.latitude);
    extra.longitude_e5 = en_e5(relue.longitude);
    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(kEspace, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, kCleZoneExtra, &extra, sizeof(extra));
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        // La ville est enregistrée ; seule la région et le pays manqueront à l'affichage de la page.
        ESP_LOGW(TAG, "région et pays non enregistrés (%s) : la ville est quand même enregistrée",
                 esp_err_to_name(err));
    }
    return true;
}

} // namespace ecran
