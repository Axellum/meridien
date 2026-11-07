#include "banc_wifi.h"

#include <cstdint>
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "reseau.h"
#include "reseau_wifi.h"
// MERIDIEN_WIFI_SSID / MERIDIEN_WIFI_MOT_DE_PASSE : le réseau d'Axel, compilé dans le binaire du banc (jamais écrit au journal).
#include "identifiants_wifi.h"

namespace ecran {

namespace {

const char *TAG = "banc_wifi";

enum Mode : int { kScan = 1, kFaux = 2, kFauxAbandon = 3, kInconnu = 4, kBon = 5 };

constexpr const char *kMotDePasseFaux = "faux-mot-de-passe-1"; // 8 caractères au moins : accepté par reseau_connecter
constexpr const char *kSsidInconnu = "meridien-essai-inexistant";
constexpr int kAbandonApresMs = 4000;

volatile bool g_en_cours = false;
volatile bool g_fini = false;
volatile int g_resultat = -1;
volatile int64_t g_rappel_us = 0;

void sur_resultat(wifi::Resultat resultat, void *) {
    g_resultat = static_cast<int>(resultat);
    g_rappel_us = esp_timer_get_time();
    g_fini = true;
}

int64_t depuis_ms(int64_t t0_us) { return (esp_timer_get_time() - t0_us) / 1000; }

void recherche() {
    const int64_t t0 = esp_timer_get_time();
    const uint32_t coupures_avant = reseau_etat().deconnexions;
    reseau_scanner_lancer();
    if (!reseau_scanner_en_cours()) {
        ESP_LOGW(TAG, "WIFI_SCAN refusee (Wi-Fi pas demarre, ou essai en cours)");
        return;
    }
    while (reseau_scanner_en_cours() && depuis_ms(t0) < 25000) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    PointAcces liste[24];
    const std::size_t n = reseau_scanner_resultats(liste, 24);
    ESP_LOGI(TAG, "WIFI_SCAN duree_ms=%d reseaux=%u coupures_pendant=%u connecte_apres=%d",
             static_cast<int>(depuis_ms(t0)), static_cast<unsigned>(n),
             static_cast<unsigned>(reseau_etat().deconnexions - coupures_avant), reseau_etat().wifi_connecte ? 1 : 0);
    for (std::size_t i = 0; i < n && i < 10; ++i) {
        // Ni le nom du réseau voisin, ni le nôtre : sa longueur, son niveau, s'il demande un mot de passe, si c'est le nôtre.
        ESP_LOGI(TAG, "WIFI_RESEAU rang=%u rssi=%d securise=%d longueur=%u courant=%d", static_cast<unsigned>(i + 1),
                 static_cast<int>(liste[i].rssi_dbm), liste[i].securise ? 1 : 0,
                 static_cast<unsigned>(std::strlen(liste[i].ssid)),
                 (MERIDIEN_WIFI_SSID[0] != '\0' && std::strcmp(liste[i].ssid, MERIDIEN_WIFI_SSID) == 0) ? 1 : 0);
    }
}

void essai(const char *nom, const char *ssid, const char *mot_de_passe, int abandon_apres_ms) {
    const uint32_t coupures_avant = reseau_etat().deconnexions;
    g_fini = false;
    g_resultat = -1;
    const int64_t t0 = esp_timer_get_time();
    if (!reseau_connecter(ssid, mot_de_passe, sur_resultat, nullptr)) {
        ESP_LOGW(TAG, "WIFI_ESSAI mode=%s refuse", nom);
        return;
    }
    int64_t abandon_ms = -1;
    while (!g_fini && depuis_ms(t0) < 100000) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (abandon_apres_ms > 0 && abandon_ms < 0 && depuis_ms(t0) >= abandon_apres_ms) {
            abandon_ms = depuis_ms(t0);
            reseau_abandonner();
            break; // pas de rappel après un abandon
        }
    }
    const int64_t rappel_ms = g_fini ? (g_rappel_us - t0) / 1000 : -1;
    const bool ip_au_rappel = reseau_etat().ip_obtenue;
    // Après un abandon, ou si le rappel est venu avant l'adresse IP : attendre le retour de l'ancien réseau pour le mesurer.
    int64_t ip_retrouvee_ms = -1;
    while (depuis_ms(t0) < 100000) {
        const EtatReseau e = reseau_etat();
        if (e.ip_obtenue && e.wifi_connecte) {
            ip_retrouvee_ms = depuis_ms(t0);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    char courant[40];
    const std::size_t longueur = reseau_ssid_courant(courant, sizeof(courant));
    const int meme =
        (longueur > 0 && MERIDIEN_WIFI_SSID[0] != '\0' && std::strcmp(courant, MERIDIEN_WIFI_SSID) == 0) ? 1 : 0;
    std::memset(courant, 0, sizeof(courant));
    ESP_LOGI(TAG,
             "WIFI_ESSAI mode=%s resultat=%d rappel_ms=%d abandon_ms=%d ip_au_rappel=%d ip_retrouvee_ms=%d coupures=%u "
             "ssid_courant_longueur=%u ssid_courant_egal_compile=%d",
             nom, g_resultat, static_cast<int>(rappel_ms), static_cast<int>(abandon_ms), ip_au_rappel ? 1 : 0,
             static_cast<int>(ip_retrouvee_ms), static_cast<unsigned>(reseau_etat().deconnexions - coupures_avant),
             static_cast<unsigned>(longueur), meme);
}

void tache_banc(void *argument) {
    switch (static_cast<int>(reinterpret_cast<intptr_t>(argument))) {
    case kScan:
        recherche();
        break;
    case kFaux:
        essai("faux", MERIDIEN_WIFI_SSID, kMotDePasseFaux, 0);
        break;
    case kFauxAbandon:
        essai("faux_abandon", MERIDIEN_WIFI_SSID, kMotDePasseFaux, kAbandonApresMs);
        break;
    case kInconnu:
        essai("inconnu", kSsidInconnu, kMotDePasseFaux, 0);
        break;
    case kBon:
        essai("bon", MERIDIEN_WIFI_SSID, MERIDIEN_WIFI_MOT_DE_PASSE, 0);
        break;
    default:
        break;
    }
    g_en_cours = false;
    vTaskDelete(nullptr);
}

} // namespace

bool banc_wifi_lancer(const char *ligne) {
    int mode = 0;
    if (std::strcmp(ligne, "wifi_scan") == 0) {
        mode = kScan;
    } else if (std::strcmp(ligne, "wifi_faux") == 0) {
        mode = kFaux;
    } else if (std::strcmp(ligne, "wifi_faux_abandon") == 0) {
        mode = kFauxAbandon;
    } else if (std::strcmp(ligne, "wifi_inconnu") == 0) {
        mode = kInconnu;
    } else if (std::strcmp(ligne, "wifi_bon") == 0) {
        mode = kBon;
    } else {
        return false;
    }
    if (MERIDIEN_WIFI_SSID[0] == '\0' && (mode == kFaux || mode == kFauxAbandon || mode == kBon)) {
        ESP_LOGW(TAG, "ce binaire n'a pas de reseau compile : essai impossible");
        return true;
    }
    if (g_en_cours) {
        ESP_LOGW(TAG, "un essai du banc est deja en cours");
        return true;
    }
    g_en_cours = true;
    if (xTaskCreate(tache_banc, "banc_wifi", 4096, reinterpret_cast<void *>(static_cast<intptr_t>(mode)), 1, nullptr) !=
        pdPASS) {
        g_en_cours = false;
        ESP_LOGE(TAG, "tache du banc non creee");
    }
    return true;
}

} // namespace ecran
