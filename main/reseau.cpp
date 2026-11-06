#include "reseau.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "board.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "commande_serie.h"
#include "horloge.h"
#include "sntp_rapide.h"
// Généré par CMake depuis reglages_locaux.cmake (hors git, voir main/CMakeLists.txt) : MERIDIEN_WIFI_SSID et
// MERIDIEN_WIFI_MOT_DE_PASSE. Sans ce fichier de réglages locaux, ce sont des chaînes vides.
#include "identifiants_wifi.h"

static_assert(sizeof(MERIDIEN_WIFI_SSID) <= 33, "SSID trop long : 32 octets au plus");
static_assert(sizeof(MERIDIEN_WIFI_MOT_DE_PASSE) <= 65, "mot de passe Wi-Fi trop long : 64 octets au plus");
static_assert(CONFIG_LWIP_SNTP_MAX_SERVERS >= 2, "deux serveurs SNTP attendus (sdkconfig.defaults)");

namespace ecran {

namespace {

const char *TAG = "reseau";
// Ligne unique par démarrage, que le banc de test (20 redémarrages) cherche : « BOOT <n> … ».
const char *TAG_BOOT = "BOOT";

constexpr uint32_t kDelaiLienMs = 25000;         // lien avec le coprocesseur (deux essais de la couche carte au plus)
constexpr int32_t kDelaiHeureMs = 60000;         // au-delà, la ligne BOOT est écrite sans l'heure
constexpr int64_t kDelaiReessaiWifiUs = 2000000; // entre deux tentatives de connexion après une coupure
constexpr EventBits_t kEvenementHeure = BIT0;

portMUX_TYPE g_verrou = portMUX_INITIALIZER_UNLOCKED;
EtatReseau g_etat = {};
EventGroupHandle_t g_evenements = nullptr;
esp_timer_handle_t g_minuteur_reessai = nullptr;
bool g_sntp_demarre = false;
uint32_t g_numero_demarrage = 0;
bool g_numero_garde = false;

int32_t maintenant_ms() { return static_cast<int32_t>(esp_timer_get_time() / 1000); }

template <typename F> void modifier(F &&fonction) {
    portENTER_CRITICAL(&g_verrou);
    fonction(g_etat);
    portEXIT_CRITICAL(&g_verrou);
}

void relancer_connexion(void *) { esp_wifi_connect(); }

void sur_evenement_wifi(void *, esp_event_base_t, int32_t id, void *donnees) {
    switch (id) {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "Wi-Fi démarré ; connexion au point d'accès");
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_CONNECTED: {
        const int32_t ms = maintenant_ms();
        modifier([ms](EtatReseau &e) {
            e.wifi_connecte = true;
            if (e.wifi_ms < 0) {
                e.wifi_ms = ms;
            }
        });
        const auto *infos = static_cast<const wifi_event_sta_connected_t *>(donnees);
        // Ni l'SSID ni le BSSID : seulement le canal.
        ESP_LOGI(TAG, "associé au point d'accès à %d ms (canal %d)", static_cast<int>(ms),
                 infos != nullptr ? static_cast<int>(infos->channel) : -1);
        break;
    }
    case WIFI_EVENT_STA_DISCONNECTED: {
        const auto *infos = static_cast<const wifi_event_sta_disconnected_t *>(donnees);
        uint32_t total = 0;
        modifier([&total](EtatReseau &e) {
            e.wifi_connecte = false;
            e.ip_obtenue = false;
            e.ip[0] = '\0';
            total = ++e.deconnexions;
        });
        ESP_LOGW(TAG, "Wi-Fi coupé (raison %d, coupure n° %u) ; nouvelle tentative dans 2 s",
                 infos != nullptr ? static_cast<int>(infos->reason) : -1, static_cast<unsigned>(total));
        if (g_minuteur_reessai != nullptr) {
            esp_timer_start_once(g_minuteur_reessai, kDelaiReessaiWifiUs); // déjà armé : l'appel est refusé, sans effet
        }
        break;
    }
    default:
        break;
    }
}

// Première heure reçue, par l'acquisition rapide (sntp_rapide) ou par le SNTP de lwIP : la première des deux compte.
void marquer_heure_recue() {
    const int32_t ms = maintenant_ms();
    modifier([ms](EtatReseau &e) {
        e.heure_synchronisee = true;
        if (e.heure_ms < 0) {
            e.heure_ms = ms;
        }
    });
    if (g_evenements != nullptr) {
        xEventGroupSetBits(g_evenements, kEvenementHeure);
    }
}

void sur_heure_recue(struct timeval *) { marquer_heure_recue(); }

void demarrer_sntp() {
    if (g_sntp_demarre) {
        return; // déjà en marche : une nouvelle adresse IP ne le relance pas, lwIP garde ses serveurs
    }
    esp_sntp_config_t config = {};
    config.smooth_sync = false;
    config.server_from_dhcp = false;
    config.wait_for_sync = false;
    config.start = true;
    config.sync_cb = sur_heure_recue;
    config.renew_servers_after_new_IP = false;
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
    config.index_of_first_server = 0;
    config.num_of_servers = 2;
    config.servers[0] = "fr.pool.ntp.org";
    config.servers[1] = "pool.ntp.org";
    const esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP non démarré : %s", esp_err_to_name(err));
        return;
    }
    g_sntp_demarre = true;
    ESP_LOGI(TAG, "SNTP démarré (deux serveurs du pool, premier : fr.pool.ntp.org)");
}

void sur_evenement_ip(void *, esp_event_base_t, int32_t id, void *donnees) {
    if (id != IP_EVENT_STA_GOT_IP) {
        return;
    }
    const auto *evenement = static_cast<const ip_event_got_ip_t *>(donnees);
    char ip[16] = "";
    if (evenement != nullptr) {
        esp_ip4addr_ntoa(&evenement->ip_info.ip, ip, sizeof(ip));
    }
    const int32_t ms = maintenant_ms();
    modifier([&](EtatReseau &e) {
        e.ip_obtenue = true;
        std::memcpy(e.ip, ip, sizeof(e.ip));
        if (e.ip_ms < 0) {
            e.ip_ms = ms;
        }
    });
    ESP_LOGI(TAG, "adresse IP obtenue : %s à %d ms", ip, static_cast<int>(ms));
    demarrer_sntp();
    sntp_rapide_demarrer(marquer_heure_recue); // l'heure arrive sans attendre la relance de 15 s de lwIP
}

// Initialise la pile IP et le Wi-Fi (routé vers le coprocesseur par esp_wifi_remote) et lance la connexion.
bool demarrer_wifi() {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init : %s", esp_err_to_name(err));
        return false;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { // INVALID_STATE : déjà créée par la couche carte
        ESP_LOGE(TAG, "boucle d'événements : %s", esp_err_to_name(err));
        return false;
    }
    if (esp_netif_create_default_wifi_sta() == nullptr) {
        ESP_LOGE(TAG, "interface réseau Wi-Fi non créée");
        return false;
    }

    const esp_timer_create_args_t args = {.callback = relancer_connexion,
                                          .arg = nullptr,
                                          .dispatch_method = ESP_TIMER_TASK,
                                          .name = "wifi_reessai",
                                          .skip_unhandled_events = true};
    err = esp_timer_create(&args, &g_minuteur_reessai);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "minuteur de reconnexion : %s", esp_err_to_name(err));
        return false;
    }

    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init : %s", esp_err_to_name(err));
        return false;
    }
    // Les identifiants viennent de la compilation en phase 1b : on ne les recopie pas en NVS (la bibliothèque le ferait en clair).
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, sur_evenement_wifi, nullptr);
    if (err == ESP_OK) {
        err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, sur_evenement_ip, nullptr);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gestionnaires d'événements : %s", esp_err_to_name(err));
        return false;
    }

    wifi_config_t config = {};
    std::memcpy(config.sta.ssid, MERIDIEN_WIFI_SSID, sizeof(MERIDIEN_WIFI_SSID) - 1);
    std::memcpy(config.sta.password, MERIDIEN_WIFI_MOT_DE_PASSE, sizeof(MERIDIEN_WIFI_MOT_DE_PASSE) - 1);
    config.sta.threshold.authmode = sizeof(MERIDIEN_WIFI_MOT_DE_PASSE) > 1 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_STA, &config);
    }
    std::memset(&config, 0, sizeof(config)); // ne pas laisser les identifiants sur la pile
    if (err == ESP_OK) {
        err = esp_wifi_set_ps(WIFI_PS_NONE); // comme le firmware ESPHome : pas d'économie d'énergie radio
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "démarrage du Wi-Fi : %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

// La ligne que le banc de test lit : un seul « BOOT n … » par démarrage, avec tout ce qui dit si le lien est bon.
void journal_demarrage() {
    const EtatReseau etat = reseau_etat();
    board_wifi_info_t carte;
    board_wifi_get_info(&carte);

    int rssi = 0;
    wifi_ap_record_t point = {};
    if (etat.wifi_connecte && esp_wifi_sta_get_ap_info(&point) == ESP_OK) {
        rssi = point.rssi; // on ne lit que la puissance, jamais l'SSID du point d'accès
    }

    TexteHorloge texte = {};
    const int64_t maintenant = static_cast<int64_t>(std::time(nullptr));
    const bool horloge_ok = etat.heure_synchronisee && horloge_formater(maintenant, texte);

    ESP_LOGI(TAG_BOOT,
             "BOOT %u%s cause=%d rom=0x%02x lien=%d essais=%d c6=%s hote=%s lien_ms=%d wifi_ms=%d ip_ms=%d ntp_ms=%d "
             "coupures=%u echecs_transport=%u rssi=%d utc=%lld local=\"%s, %s\"",
             static_cast<unsigned>(g_numero_demarrage), g_numero_garde ? "" : "(compteur non enregistré)",
             static_cast<int>(esp_reset_reason()), static_cast<unsigned>(esp_rom_get_reset_reason(0)),
             etat.coprocesseur_pret ? 1 : 0, static_cast<int>(carte.attempts),
             etat.version_coprocesseur[0] != '\0' ? etat.version_coprocesseur : "inconnue",
             carte.host_version[0] != '\0' ? carte.host_version : "inconnue", static_cast<int>(etat.lien_ms),
             static_cast<int>(etat.wifi_ms), static_cast<int>(etat.ip_ms), static_cast<int>(etat.heure_ms),
             static_cast<unsigned>(etat.deconnexions), static_cast<unsigned>(carte.transport_failures), rssi,
             static_cast<long long>(etat.heure_synchronisee ? maintenant : -1LL), horloge_ok ? texte.date : "-",
             horloge_ok ? texte.heure : "-");
}

void tache_reseau(void *) {
    ESP_LOGI(TAG, "lien avec le coprocesseur Wi-Fi : demandé à %d ms", static_cast<int>(maintenant_ms()));
    const esp_err_t lien = board_wifi_link_up(kDelaiLienMs);
    board_wifi_info_t carte;
    board_wifi_get_info(&carte);
    modifier([&](EtatReseau &e) {
        e.coprocesseur_pret = (lien == ESP_OK);
        e.coprocesseur_echec = (lien != ESP_OK);
        e.lien_ms = carte.link_up_ms;
        std::memcpy(e.version_coprocesseur, carte.coprocessor_version, sizeof(e.version_coprocesseur));
    });
    memoire_jalon("après le lien avec le C6 (esp_hosted, SDIO)");
    if (lien != ESP_OK) {
        ESP_LOGE(TAG, "lien avec le coprocesseur impossible : %s (essais : %d)", esp_err_to_name(lien),
                 static_cast<int>(carte.attempts));
        journal_demarrage();
        vTaskDelete(nullptr);
        return;
    }

    if (!reseau_etat().identifiants_presents) {
        ESP_LOGW(
            TAG,
            "aucun réglage Wi-Fi dans ce firmware (reglages_locaux.cmake absent à la compilation) : pas de connexion");
        journal_demarrage();
        vTaskDelete(nullptr);
        return;
    }
    if (!demarrer_wifi()) {
        journal_demarrage();
        vTaskDelete(nullptr);
        return;
    }

    memoire_jalon("après le démarrage du Wi-Fi (esp_wifi, lwIP, netif)");
    // Attendre la première réponse SNTP, au plus kDelaiHeureMs après le début de l'attente.
    const EventBits_t bits =
        xEventGroupWaitBits(g_evenements, kEvenementHeure, pdFALSE, pdTRUE, pdMS_TO_TICKS(kDelaiHeureMs));
    if ((bits & kEvenementHeure) == 0) {
        ESP_LOGW(TAG, "heure du réseau non reçue après %d s", static_cast<int>(kDelaiHeureMs / 1000));
    }
    memoire_jalon("heure reçue (ou délai écoulé) : IP, SNTP et serveur web en route");
    journal_demarrage();
    vTaskDelete(nullptr);
}

} // namespace

void reseau_demarrer(const BootInfo &boot) {
    g_numero_demarrage = boot.boot_count;
    g_numero_garde = boot.counter_persistent;
    g_evenements = xEventGroupCreate();
    modifier([](EtatReseau &e) {
        e = EtatReseau{};
        e.identifiants_presents = MERIDIEN_WIFI_SSID[0] != '\0';
        e.lien_ms = -1;
        e.wifi_ms = -1;
        e.ip_ms = -1;
        e.heure_ms = -1;
    });
    if (g_evenements == nullptr || xTaskCreate(tache_reseau, "reseau", 8192, nullptr, 4, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "tâche réseau non créée");
    }
}

EtatReseau reseau_etat() {
    EtatReseau copie;
    portENTER_CRITICAL(&g_verrou);
    copie = g_etat;
    portEXIT_CRITICAL(&g_verrou);
    return copie;
}

} // namespace ecran
