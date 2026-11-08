#include "reglages_mer.h"

#include <cstdint>
#include <cstring>
#include <new>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#include "mer_client.h"

namespace {

const char *TAG = "mer";
constexpr const char *kEspace = "mer";
constexpr const char *kCle = "registre";
constexpr uint8_t kVersion = 1;

// Contenu du blob : les mêmes cases que ecran::mer::Registre, mais rangées à plat pour que la taille du blob ne dépende pas de
// l'alignement du compilateur (le PC en annonce d'autres).
struct RegistreNvs {
    uint8_t version;
    uint8_t nombre;
    uint8_t choisi;
    uint8_t reserve;
    int16_t vent_min_kn;
    int16_t vent_max_kn;
    char id[ecran::mer::REGISTRE_MAX][ecran::mer::ID_OCTETS];
    char nom[ecran::mer::REGISTRE_MAX][ecran::mer::NOM_OCTETS];
};
static_assert(sizeof(RegistreNvs) == 8 + 12 * 65 + 12 * 129, "format du registre des spots en NVS : 2 336 octets");
static_assert(ecran::mer::REGISTRE_MAX == 12 && ecran::mer::ID_OCTETS == 65 && ecran::mer::NOM_OCTETS == 129,
              "le format en NVS dépend de ces trois nombres : le changer demande une nouvelle version du blob");

// Mutex statique (aucune allocation) créé au premier usage ; le registre lui-même vit en PSRAM (2,3 Ko), alloué par
// reglages_mer_demarrer().
StaticSemaphore_t g_mutex_donnees;
SemaphoreHandle_t g_mutex = nullptr;
ecran::mer::Registre *g_courant = nullptr;

// Ce que reglages_mer_demarrer() lit : tout en PSRAM (≈ 7 Ko), rendu avant de sortir.
struct Lecture {
    RegistreNvs brut;
    ecran::mer::Registre intermediaire;
    ecran::mer::Registre lu;
};

SemaphoreHandle_t verrou() {
    if (g_mutex == nullptr) {
        g_mutex = xSemaphoreCreateMutexStatic(&g_mutex_donnees);
    }
    return g_mutex;
}

void vers_nvs(const ecran::mer::Registre &r, RegistreNvs &brut) {
    std::memset(&brut, 0, sizeof brut);
    brut.version = kVersion;
    brut.nombre = r.nombre;
    brut.choisi = r.choisi;
    brut.vent_min_kn = r.vent_min_kn;
    brut.vent_max_kn = r.vent_max_kn;
    for (std::size_t i = 0; i < ecran::mer::REGISTRE_MAX; ++i) {
        std::memcpy(brut.id[i], r.spots[i].id, sizeof brut.id[i]);
        std::memcpy(brut.nom[i], r.spots[i].nom, sizeof brut.nom[i]);
    }
}

void depuis_nvs(const RegistreNvs &brut, ecran::mer::Registre &r) {
    r = ecran::mer::Registre{};
    r.nombre = brut.nombre;
    r.choisi = brut.choisi;
    r.vent_min_kn = brut.vent_min_kn;
    r.vent_max_kn = brut.vent_max_kn;
    for (std::size_t i = 0; i < ecran::mer::REGISTRE_MAX; ++i) {
        std::memcpy(r.spots[i].id, brut.id[i], sizeof r.spots[i].id);
        std::memcpy(r.spots[i].nom, brut.nom[i], sizeof r.spots[i].nom);
    }
}

} // namespace

namespace ecran {

void reglages_mer_demarrer() {
    if (g_courant == nullptr) {
        void *place = heap_caps_calloc(1, sizeof(mer::Registre), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (place == nullptr) {
            ESP_LOGE(TAG, "pas de place en PSRAM pour le registre des spots (%u octets) : la page mer reste vide",
                     static_cast<unsigned>(sizeof(mer::Registre)));
            return;
        }
        g_courant = new (place) mer::Registre();
    }
    // Le blob lu, le registre brut qu'il donne et le registre nettoyé : 7 Ko en tout, pris en PSRAM le temps de la lecture, pas sur la pile.
    auto *l = static_cast<Lecture *>(heap_caps_calloc(1, sizeof(Lecture), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (l == nullptr) {
        ESP_LOGE(TAG, "pas de place en PSRAM pour lire le registre des spots (%u octets) : la page mer reste vide",
                 static_cast<unsigned>(sizeof(Lecture)));
        return;
    }
    new (l) Lecture();
    nvs_handle_t h = 0;
    const esp_err_t ouvert = nvs_open(kEspace, NVS_READONLY, &h);
    if (ouvert == ESP_OK) {
        size_t taille = sizeof(RegistreNvs);
        const esp_err_t err = nvs_get_blob(h, kCle, &l->brut, &taille);
        nvs_close(h);
        if (err == ESP_OK && taille == sizeof(RegistreNvs) && l->brut.version == kVersion) {
            depuis_nvs(l->brut, l->intermediaire);
            l->lu = mer::registre_depuis_nvs(l->intermediaire);
            if (!mer::egaux(l->lu, l->intermediaire)) {
                ESP_LOGW(TAG, "registre des spots relu avec des entrées abîmées : %u gardée(s) sur %u annoncée(s)",
                         static_cast<unsigned>(l->lu.nombre), static_cast<unsigned>(l->intermediaire.nombre));
            }
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "registre des spots non lu (%s, %u octets) : registre vide", esp_err_to_name(err),
                     static_cast<unsigned>(taille));
        }
    } else if (ouvert != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS « %s » non ouverte (%s) : registre vide", kEspace, esp_err_to_name(ouvert));
    }
    xSemaphoreTake(verrou(), portMAX_DELAY);
    *g_courant = l->lu;
    xSemaphoreGive(g_mutex);
    const mer::SpotMer *choisi = mer::spot_choisi(*g_courant);
    ESP_LOGI(TAG, "registre des spots : %u spot(s), choisi : %s, vent du kite %d à %d nœuds",
             static_cast<unsigned>(g_courant->nombre),
             choisi != nullptr ? choisi->id : "aucun (rien n'est demandé à Vigie)",
             static_cast<int>(g_courant->vent_min_kn), static_cast<int>(g_courant->vent_max_kn));
    heap_caps_free(l);
}

bool reglages_mer(mer::Registre &sortie) {
    if (g_courant == nullptr) {
        return false;
    }
    xSemaphoreTake(verrou(), portMAX_DELAY);
    sortie = *g_courant;
    xSemaphoreGive(g_mutex);
    return true;
}

mer::DemandeMer reglages_mer_demande() {
    if (g_courant == nullptr) {
        return mer::DemandeMer{};
    }
    xSemaphoreTake(verrou(), portMAX_DELAY);
    const mer::DemandeMer d = mer::demande_du_registre(*g_courant);
    xSemaphoreGive(g_mutex);
    return d;
}

bool reglages_mer_enregistrer(const mer::Registre &nouveau) {
    if (g_courant == nullptr) {
        ESP_LOGW(TAG, "registre des spots non enregistré : pas de mémoire pour le garder");
        return false;
    }
    if (!mer::registre_valide(nouveau)) {
        ESP_LOGW(TAG, "registre des spots refusé (entrée à corriger, doublon, choix hors de la liste ou vent refusé) : "
                      "rien n'est changé");
        return false;
    }
    auto *brut =
        static_cast<RegistreNvs *>(heap_caps_calloc(1, sizeof(RegistreNvs), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (brut == nullptr) {
        ESP_LOGE(TAG, "pas de place en PSRAM pour écrire le registre des spots");
        return false;
    }
    vers_nvs(nouveau, *brut);
    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(kEspace, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, kCle, brut, sizeof(RegistreNvs));
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    heap_caps_free(brut);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "registre des spots non enregistré en NVS : %s (l'ancien état reste en service)",
                 esp_err_to_name(err));
        return false;
    }
    xSemaphoreTake(verrou(), portMAX_DELAY);
    *g_courant = nouveau;
    xSemaphoreGive(g_mutex);
    const mer::SpotMer *choisi = mer::spot_choisi(nouveau);
    ESP_LOGI(TAG, "registre des spots enregistré : %u spot(s), choisi : %s, vent du kite %d à %d nœuds",
             static_cast<unsigned>(nouveau.nombre), choisi != nullptr ? choisi->id : "aucun",
             static_cast<int>(nouveau.vent_min_kn), static_cast<int>(nouveau.vent_max_kn));
    mer_client_a_change();
    return true;
}

} // namespace ecran
