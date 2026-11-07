#include "reseau.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "board.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
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
#include "nvs.h"
#include "sdkconfig.h"

#include "commande_serie.h"
#include "ecran/essai_wifi.hpp"
#include "ecran/points_acces.hpp"
#include "horloge.h"
#include "reseau_wifi.h"
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

// --- Changement de Wi-Fi depuis l'écran (ADR-0007, point 7) : état -------------------------------------------------------------
// Tout se DÉCIDE dans la boucle d'événements du système (tâche « sys_evt »), la même où arrivent les événements Wi-Fi et IP : les
// transitions de la machine à états (ecran/essai_wifi.hpp, testée sur le PC) n'ont donc besoin d'aucun verrou. Les appelants
// (écran, serveur web) ne font que réclamer l'essai sous g_verrou puis poster un message. Rien de ce bloc ne s'écrit au journal :
// ni SSID, ni mot de passe.
constexpr std::size_t kLongSsid = 32;
constexpr std::size_t kLongMotDePasse =
    64; // 63 pour l'écran ; 64 (clé hexadécimale) tolérés pour les identifiants de compilation
constexpr std::size_t kReseauxGardes = 24;  // lignes de la liste de l'écran
constexpr std::size_t kPointsAccesLus = 64; // points d'accès lus à la radio (un réseau peut en avoir plusieurs)
constexpr int64_t kDelaiScanUs = 15000000;  // une recherche sans réponse est abandonnée
constexpr int64_t kDelaiBasculeUs = 300000; // entre la coupure de l'ancien réseau et la connexion au nouveau
constexpr int32_t kMsgDemarrer = 1;         // messages postés à la boucle d'événements
constexpr int32_t kMsgAbandonner = 2;
constexpr int32_t kMsgDelaiEssai = 3;
constexpr int32_t kMsgDelaiRetour = 4;
constexpr int32_t kMsgScanner = 5;
constexpr int32_t kMsgDelaiScan = 6;
// Une « base » d'événements se compare par son adresse : un seul tableau, dans cette unité de compilation.
const char kBaseReseau[] = "ECRAN_RESEAU";

struct Identifiants {
    char ssid[kLongSsid + 1];
    char mot_de_passe[kLongMotDePasse + 1];
    bool present; // un SSID est connu
};

Identifiants g_actuels = {}; // réseau en service (sous g_verrou)
Identifiants g_nouveau = {}; // réseau à l'essai (réclamé sous g_verrou, ensuite la boucle d'événements seule)
wifi::Essai g_essai = {};    // boucle d'événements seulement
bool g_occupe = false;       // sous g_verrou : un essai est réclamé et pas encore terminé
bool g_wifi_pret = false;    // sous g_verrou : esp_wifi_start() a réussi
bool g_reconnexion = true;   // sous g_verrou : réessayer le réseau après une coupure (faux après un premier essai raté)
bool g_scan_en_cours = false;            // sous g_verrou
SurResultatConnexion g_rappel = nullptr; // sous g_verrou
void *g_contexte = nullptr;
int64_t g_debut_essai_us = 0; // sous g_verrou ; pour les durées du journal
int64_t g_debut_scan_us = 0;
PointAcces g_resultats[kReseauxGardes] = {}; // sous g_verrou : dernière recherche terminée
std::size_t g_nb_resultats = 0;
esp_timer_handle_t g_minuteur_essai = nullptr;
esp_timer_handle_t g_minuteur_retour = nullptr;
esp_timer_handle_t g_minuteur_scan = nullptr;

int32_t maintenant_ms() { return static_cast<int32_t>(esp_timer_get_time() / 1000); }

template <typename F> void modifier(F &&fonction) {
    portENTER_CRITICAL(&g_verrou);
    fonction(g_etat);
    portEXIT_CRITICAL(&g_verrou);
}

// Efface sans que le compilateur puisse juger l'écriture inutile (identifiants sur la pile ou dans une structure qui ne sert plus).
void effacer(void *debut, std::size_t octets) {
    volatile unsigned char *p = static_cast<volatile unsigned char *>(debut);
    for (std::size_t i = 0; i < octets; ++i) {
        p[i] = 0;
    }
}

bool reconnexion_permise() {
    portENTER_CRITICAL(&g_verrou);
    const bool ok = g_reconnexion && !g_scan_en_cours;
    portEXIT_CRITICAL(&g_verrou);
    return ok;
}

void relancer_connexion(void *) {
    if (reconnexion_permise()) {
        esp_wifi_connect();
    }
}

int32_t depuis_debut_essai_ms() {
    portENTER_CRITICAL(&g_verrou);
    const int64_t debut = g_debut_essai_us;
    portEXIT_CRITICAL(&g_verrou);
    return static_cast<int32_t>((esp_timer_get_time() - debut) / 1000);
}

// ---- messages et minuteurs ---------------------------------------------------------------------------------------------------

bool poster(int32_t message) { return esp_event_post(kBaseReseau, message, nullptr, 0, pdMS_TO_TICKS(50)) == ESP_OK; }

void armer(esp_timer_handle_t minuteur, int64_t microsecondes) {
    if (minuteur == nullptr) {
        return;
    }
    esp_timer_stop(minuteur); // refusé (sans effet) s'il ne tournait pas
    esp_timer_start_once(minuteur, microsecondes);
}

// Les minuteurs ne font que poster : la décision est prise dans la boucle d'événements. Si la file est pleine, on réessaie dans 1 s
// (sans quoi l'essai ne se terminerait jamais).
void sur_minuteur_essai(void *) {
    if (!poster(kMsgDelaiEssai)) {
        esp_timer_start_once(g_minuteur_essai, 1000000);
    }
}
void sur_minuteur_retour(void *) {
    if (!poster(kMsgDelaiRetour)) {
        esp_timer_start_once(g_minuteur_retour, 1000000);
    }
}
void sur_minuteur_scan(void *) {
    if (!poster(kMsgDelaiScan)) {
        esp_timer_start_once(g_minuteur_scan, 1000000);
    }
}

esp_err_t creer_minuteur(esp_timer_cb_t rappel, const char *nom, esp_timer_handle_t *poignee) {
    const esp_timer_create_args_t args = {.callback = rappel,
                                          .arg = nullptr,
                                          .dispatch_method = ESP_TIMER_TASK,
                                          .name = nom,
                                          .skip_unhandled_events = true};
    return esp_timer_create(&args, poignee);
}

// ---- configuration de la station ---------------------------------------------------------------------------------------------

void remplir_config(wifi_config_t &config, const Identifiants &id) {
    effacer(&config, sizeof(config));
    std::memcpy(config.sta.ssid, id.ssid, std::strlen(id.ssid));
    std::memcpy(config.sta.password, id.mot_de_passe, std::strlen(id.mot_de_passe));
    config.sta.threshold.authmode = id.mot_de_passe[0] != '\0' ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
}

// Quitte le réseau actuel (s'il y en a un) et se connecte à celui-ci, dans 300 ms : le temps que la coupure soit traitée.
// Les coupures que cela provoque (raison 8, « nous avons quitté le réseau ») ne comptent pas comme un échec de l'essai.
bool basculer_vers(const Identifiants &id) {
    wifi_config_t config;
    remplir_config(config, id);
    const esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config);
    effacer(&config, sizeof(config)); // ne pas laisser les identifiants sur la pile
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config : %s", esp_err_to_name(err));
        return false;
    }
    esp_wifi_disconnect(); // sans effet, et c'est normal, si l'on n'était associé à rien
    armer(g_minuteur_reessai, kDelaiBasculeUs);
    return true;
}

// ---- fin d'un essai et application des décisions de la machine à états ------------------------------------------------------

void terminer_essai(const wifi::Sortie &s) {
    esp_timer_stop(g_minuteur_essai);
    esp_timer_stop(g_minuteur_retour);
    effacer(&g_nouveau, sizeof(g_nouveau));
    SurResultatConnexion rappel = nullptr;
    void *contexte = nullptr;
    portENTER_CRITICAL(&g_verrou);
    rappel = g_rappel;
    contexte = g_contexte;
    g_rappel = nullptr;
    g_contexte = nullptr;
    g_occupe = false; // avant le rappel : il peut très bien relancer un essai
    portEXIT_CRITICAL(&g_verrou);
    const bool sans_reseau = !s.etat.avait_ancien && s.resultat != wifi::Resultat::Connecte;
    if (sans_reseau) {
        // Appareil neuf, essai raté (ou abandonné) : il n'y a rien à reprendre. Cesser de réessayer les mauvais identifiants (la
        // recherche suivante échouerait pendant une tentative de connexion).
        portENTER_CRITICAL(&g_verrou);
        g_reconnexion = false;
        portEXIT_CRITICAL(&g_verrou);
        esp_timer_stop(g_minuteur_reessai);
        esp_wifi_disconnect();
    }
    ESP_LOGI(TAG, "essai Wi-Fi terminé à +%d ms : résultat %d, rappel %s", static_cast<int>(depuis_debut_essai_ms()),
             static_cast<int>(s.resultat), (s.rappeler && rappel != nullptr) ? "appelé" : "non appelé");
    if (s.rappeler && rappel != nullptr) {
        rappel(s.resultat, contexte);
    }
}

void appliquer(const wifi::Sortie &s) {
    const bool etait_occupe = wifi::occupe(g_essai);
    g_essai = s.etat;
    if (s.appliquer_ancien) {
        esp_timer_stop(g_minuteur_essai);
        Identifiants ancien;
        portENTER_CRITICAL(&g_verrou);
        ancien = g_actuels;
        portEXIT_CRITICAL(&g_verrou);
        ESP_LOGW(TAG, "essai Wi-Fi : retour arrière vers l'ancien réseau, à +%d ms",
                 static_cast<int>(depuis_debut_essai_ms()));
        basculer_vers(ancien);
        effacer(&ancien, sizeof(ancien));
        armer(g_minuteur_retour, wifi::DELAI_RETOUR_MS * 1000);
    }
    if (s.adopter_nouveau) {
        portENTER_CRITICAL(&g_verrou);
        g_actuels = g_nouveau; // c'est lui, désormais, l'« ancien » du prochain essai
        portEXIT_CRITICAL(&g_verrou);
        modifier([](EtatReseau &e) { e.identifiants_presents = true; });
    }
    if (etait_occupe && !wifi::occupe(g_essai)) {
        terminer_essai(s);
    }
}

// Message « démarrer » : la personne a demandé un essai (reseau_connecter a déjà rangé le réseau dans g_nouveau).
void demarrer_essai() {
    bool ancien = false;
    bool recherche = false;
    portENTER_CRITICAL(&g_verrou);
    ancien = g_actuels.present;
    recherche = g_scan_en_cours;
    g_scan_en_cours = false;
    g_reconnexion = true;
    portEXIT_CRITICAL(&g_verrou);
    const wifi::Sortie s = wifi::demarrer(g_essai, ancien);
    if (!s.appliquer_nouveau) {
        ESP_LOGE(TAG, "essai Wi-Fi refusé par la machine à états (un essai était déjà en cours)");
        return;
    }
    g_essai = s.etat;
    if (recherche) {
        esp_timer_stop(g_minuteur_scan);
        esp_wifi_scan_stop(); // une recherche ne tient pas avec une connexion en cours
    }
    armer(g_minuteur_essai, wifi::DELAI_ESSAI_MS * 1000);
    ESP_LOGI(TAG, "essai Wi-Fi démarré (ancien réseau : %s ; délai %d s)", ancien ? "oui" : "non",
             static_cast<int>(wifi::DELAI_ESSAI_MS / 1000));
    if (!basculer_vers(g_nouveau)) {
        appliquer(
            wifi::sur_delai_essai(g_essai)); // réglage refusé par la radio : échec « autre », l'ancien réseau revient
    }
}

// ---- recherche de réseaux ----------------------------------------------------------------------------------------------------

void terminer_recherche() {
    portENTER_CRITICAL(&g_verrou);
    g_scan_en_cours = false;
    const bool reessayer = g_reconnexion;
    portEXIT_CRITICAL(&g_verrou);
    esp_timer_stop(g_minuteur_scan);
    if (reessayer && !reseau_etat().wifi_connecte) {
        armer(g_minuteur_reessai, kDelaiBasculeUs); // la recherche a interrompu les tentatives : les reprendre
    }
}

// Message « scanner ».
void lancer_recherche() {
    if (wifi::occupe(g_essai)) {
        terminer_recherche();
        return;
    }
    esp_timer_stop(g_minuteur_reessai); // pas de tentative de connexion pendant la recherche
    esp_err_t err = esp_wifi_scan_start(nullptr, false);
    if (err == ESP_ERR_WIFI_STATE) {
        // La radio est en train de se connecter (réseau absent, mot de passe changé…) : on interrompt la tentative, la recherche
        // passe, et terminer_recherche() relance les tentatives.
        esp_wifi_disconnect();
        err = esp_wifi_scan_start(nullptr, false);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "recherche de réseaux refusée par la radio : %s", esp_err_to_name(err));
        terminer_recherche();
        return;
    }
    portENTER_CRITICAL(&g_verrou);
    g_debut_scan_us = esp_timer_get_time();
    portEXIT_CRITICAL(&g_verrou);
    armer(g_minuteur_scan, kDelaiScanUs);
}

// Message « délai de la recherche ».
void abandonner_recherche() {
    portENTER_CRITICAL(&g_verrou);
    const bool en_cours = g_scan_en_cours;
    portEXIT_CRITICAL(&g_verrou);
    if (!en_cours) {
        return;
    }
    ESP_LOGW(TAG, "recherche de réseaux sans réponse après %d s : abandonnée",
             static_cast<int>(kDelaiScanUs / 1000000));
    esp_wifi_scan_stop();
    esp_wifi_clear_ap_list();
    terminer_recherche();
}

// Événement WIFI_EVENT_SCAN_DONE : lire la liste de la radio, la classer (noyau pur), la garder.
void traiter_recherche() {
    portENTER_CRITICAL(&g_verrou);
    const bool attendue = g_scan_en_cours;
    const int64_t debut = g_debut_scan_us;
    portEXIT_CRITICAL(&g_verrou);
    if (!attendue) {
        esp_wifi_clear_ap_list(); // recherche interrompue ou qui n'est pas la nôtre
        return;
    }
    uint16_t total = 0;
    esp_wifi_scan_get_ap_num(&total);
    const uint16_t n = total < kPointsAccesLus ? total : static_cast<uint16_t>(kPointsAccesLus);
    std::size_t gardes = 0;
    bool lu = false;
    if (n > 0) {
        // Tout en PSRAM, le temps de la lecture : ni sur la pile de la boucle d'événements, ni en mémoire interne.
        const std::size_t octets =
            n * (sizeof(wifi_ap_record_t) + sizeof(PointAcces)) + kReseauxGardes * sizeof(PointAcces);
        auto *zone = static_cast<unsigned char *>(heap_caps_calloc(1, octets, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (zone != nullptr) {
            auto *radio = reinterpret_cast<wifi_ap_record_t *>(zone);
            auto *bruts = reinterpret_cast<PointAcces *>(zone + n * sizeof(wifi_ap_record_t));
            auto *tries = bruts + n;
            uint16_t lus = n;
            if (esp_wifi_scan_get_ap_records(&lus, radio) == ESP_OK) {
                for (uint16_t i = 0; i < lus; ++i) {
                    PointAcces &p = bruts[i];
                    std::size_t k = 0;
                    while (k < 32 && radio[i].ssid[k] != 0) {
                        p.ssid[k] = static_cast<char>(radio[i].ssid[k]);
                        ++k;
                    }
                    p.ssid[k] = '\0';
                    p.rssi_dbm = radio[i].rssi;
                    p.securise = radio[i].authmode != WIFI_AUTH_OPEN && radio[i].authmode != WIFI_AUTH_OWE;
                }
                gardes = wifi::classer_points_acces(bruts, lus, tries, kReseauxGardes);
                portENTER_CRITICAL(&g_verrou);
                std::memcpy(g_resultats, tries, gardes * sizeof(PointAcces));
                g_nb_resultats = gardes;
                portEXIT_CRITICAL(&g_verrou);
                lu = true;
            } else {
                esp_wifi_clear_ap_list();
            }
            effacer(zone, octets); // les noms de réseaux voisins ne traînent pas
            heap_caps_free(zone);
        } else {
            esp_wifi_clear_ap_list();
            ESP_LOGW(TAG, "recherche de réseaux : PSRAM insuffisante pour lire %u points d'accès",
                     static_cast<unsigned>(n));
        }
    } else {
        esp_wifi_clear_ap_list();
        lu = true; // aucun réseau : c'est un résultat
        portENTER_CRITICAL(&g_verrou);
        g_nb_resultats = 0;
        portEXIT_CRITICAL(&g_verrou);
    }
    ESP_LOGI(TAG, "recherche de réseaux terminée en %d ms : %u point(s) d'accès, %u réseau(x) listé(s)%s",
             static_cast<int>((esp_timer_get_time() - debut) / 1000), static_cast<unsigned>(total),
             static_cast<unsigned>(gardes), lu ? "" : " (lecture échouée)");
    terminer_recherche();
}

// ---- le point d'entrée de tous les messages ----------------------------------------------------------------------------------

void sur_message_reseau(void *, esp_event_base_t, int32_t id, void *) {
    switch (id) {
    case kMsgDemarrer:
        demarrer_essai();
        break;
    case kMsgAbandonner:
        ESP_LOGW(TAG, "essai Wi-Fi : abandon demandé à +%d ms", static_cast<int>(depuis_debut_essai_ms()));
        appliquer(wifi::abandonner(g_essai));
        break;
    case kMsgDelaiEssai:
        ESP_LOGW(TAG, "essai Wi-Fi : délai de %d s écoulé à +%d ms", static_cast<int>(wifi::DELAI_ESSAI_MS / 1000),
                 static_cast<int>(depuis_debut_essai_ms()));
        appliquer(wifi::sur_delai_essai(g_essai));
        break;
    case kMsgDelaiRetour:
        ESP_LOGW(TAG, "essai Wi-Fi : l'ancien réseau n'est pas revenu après %d s (à +%d ms) ; l'écran reprend la main",
                 static_cast<int>(wifi::DELAI_RETOUR_MS / 1000), static_cast<int>(depuis_debut_essai_ms()));
        appliquer(wifi::sur_delai_retour(g_essai));
        break;
    case kMsgScanner:
        lancer_recherche();
        break;
    case kMsgDelaiScan:
        abandonner_recherche();
        break;
    default:
        break;
    }
}

void sur_evenement_wifi(void *, esp_event_base_t, int32_t id, void *donnees) {
    switch (id) {
    case WIFI_EVENT_STA_START: {
        portENTER_CRITICAL(&g_verrou);
        const bool connu = g_actuels.present;
        portEXIT_CRITICAL(&g_verrou);
        if (connu) {
            ESP_LOGI(TAG, "Wi-Fi démarré ; connexion au point d'accès");
            esp_wifi_connect();
        } else {
            ESP_LOGI(TAG, "Wi-Fi démarré sans réseau connu : pas de connexion");
        }
        break;
    }
    case WIFI_EVENT_SCAN_DONE:
        traiter_recherche();
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
        const int raison = infos != nullptr ? static_cast<int>(infos->reason) : -1;
        const bool reessai = reconnexion_permise();
        if (wifi::occupe(g_essai)) {
            ESP_LOGW(TAG, "Wi-Fi coupé (raison %d, coupure n° %u) à +%d ms de l'essai", raison,
                     static_cast<unsigned>(total), static_cast<int>(depuis_debut_essai_ms()));
        } else {
            ESP_LOGW(TAG, "Wi-Fi coupé (raison %d, coupure n° %u)%s", raison, static_cast<unsigned>(total),
                     reessai ? " ; nouvelle tentative dans 2 s" : "");
        }
        if (reessai && g_minuteur_reessai != nullptr) {
            esp_timer_start_once(g_minuteur_reessai, kDelaiReessaiWifiUs); // déjà armé : l'appel est refusé, sans effet
        }
        appliquer(wifi::sur_deconnexion(g_essai, raison)); // sans effet hors d'un essai
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
    appliquer(wifi::sur_adresse_ip(g_essai));  // sans effet hors d'un essai ou d'un retour arrière
}

// Initialise la pile IP et le Wi-Fi (routé vers le coprocesseur par esp_wifi_remote) et lance la connexion au réseau `depart` (sans
// réseau connu, le Wi-Fi est démarré quand même : la recherche et la saisie à l'écran en ont besoin).
bool demarrer_wifi(const Identifiants &depart) {
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

    err = creer_minuteur(relancer_connexion, "wifi_reessai", &g_minuteur_reessai);
    if (err == ESP_OK) {
        err = creer_minuteur(sur_minuteur_essai, "wifi_essai", &g_minuteur_essai);
    }
    if (err == ESP_OK) {
        err = creer_minuteur(sur_minuteur_retour, "wifi_retour", &g_minuteur_retour);
    }
    if (err == ESP_OK) {
        err = creer_minuteur(sur_minuteur_scan, "wifi_scan", &g_minuteur_scan);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "minuteurs du Wi-Fi : %s", esp_err_to_name(err));
        return false;
    }

    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init : %s", esp_err_to_name(err));
        return false;
    }
    // Le réseau vient de la mémoire permanente (tenue par l'hôte des réglages) ou de la compilation : la bibliothèque ne doit pas le
    // recopier elle-même en NVS (elle le ferait en clair).
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, sur_evenement_wifi, nullptr);
    if (err == ESP_OK) {
        err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, sur_evenement_ip, nullptr);
    }
    if (err == ESP_OK) {
        err = esp_event_handler_register(kBaseReseau, ESP_EVENT_ANY_ID, sur_message_reseau, nullptr);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gestionnaires d'événements : %s", esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK && depart.present) {
        wifi_config_t config;
        remplir_config(config, depart);
        err = esp_wifi_set_config(WIFI_IF_STA, &config);
        effacer(&config, sizeof(config)); // ne pas laisser les identifiants sur la pile
    }
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
    portENTER_CRITICAL(&g_verrou);
    g_wifi_pret = true;
    portEXIT_CRITICAL(&g_verrou);
    return true;
}

// Copie une chaîne dans un tableau de `capacite` octets (zéro final compris) ; faux si elle n'y tient pas.
bool copier_texte(char *vers, std::size_t capacite, const char *depuis) {
    const std::size_t n = std::strlen(depuis);
    if (n >= capacite) {
        vers[0] = '\0';
        return false;
    }
    std::memcpy(vers, depuis, n + 1);
    return true;
}

// Le réseau retenu en mémoire permanente par l'hôte des réglages (NVS, espace « wifi », clés « ssid » et « mdp » : voir
// ecran/hote_appareil.hpp de la branche des réglages, main/wifi_stocke.h). Lecture directe tant que cette branche n'est pas fusionnée.
bool lire_reseau_retenu(Identifiants &id) {
    nvs_handle_t h = 0;
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) {
        return false; // rien n'a jamais été retenu
    }
    std::size_t n = sizeof(id.ssid);
    const bool ssid_lu = nvs_get_str(h, "ssid", id.ssid, &n) == ESP_OK && id.ssid[0] != '\0';
    std::size_t m = sizeof(id.mot_de_passe);
    const bool mdp_lu = nvs_get_str(h, "mdp", id.mot_de_passe, &m) == ESP_OK;
    nvs_close(h);
    const std::size_t longueur_mdp = mdp_lu ? std::strlen(id.mot_de_passe) : 0;
    if (!ssid_lu || (longueur_mdp > 0 && longueur_mdp < 8)) { // un mot de passe WPA fait 8 caractères au moins
        effacer(&id, sizeof(id));
        return false;
    }
    if (!mdp_lu) {
        id.mot_de_passe[0] = '\0'; // réseau ouvert
    }
    id.present = true;
    return true;
}

// Le réseau de départ : celui de la mémoire permanente s'il y en a un, sinon celui de la compilation (identifiants_wifi.h).
Identifiants reseau_de_depart(bool *depuis_memoire) {
    Identifiants id = {};
    *depuis_memoire = lire_reseau_retenu(id);
    if (*depuis_memoire) {
        return id;
    }
    id.present = copier_texte(id.ssid, sizeof(id.ssid), MERIDIEN_WIFI_SSID) && id.ssid[0] != '\0' &&
                 copier_texte(id.mot_de_passe, sizeof(id.mot_de_passe), MERIDIEN_WIFI_MOT_DE_PASSE);
    if (!id.present) {
        effacer(&id, sizeof(id));
    }
    return id;
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

    Identifiants depart;
    portENTER_CRITICAL(&g_verrou);
    depart = g_actuels;
    portEXIT_CRITICAL(&g_verrou);
    if (!depart.present) {
        ESP_LOGW(TAG, "aucun réseau Wi-Fi connu (ni retenu en mémoire permanente, ni compilé : reglages_locaux.cmake "
                      "absent) : le "
                      "Wi-Fi démarre sans connexion, la recherche et la saisie à l'écran restent possibles");
    }
    const bool wifi_demarre = demarrer_wifi(depart);
    effacer(&depart, sizeof(depart));
    if (!wifi_demarre) {
        journal_demarrage();
        vTaskDelete(nullptr);
        return;
    }
    if (!reseau_etat().identifiants_presents) {
        journal_demarrage(); // pas de réseau : aucune heure à attendre
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
    bool depuis_memoire = false;
    Identifiants depart = reseau_de_depart(&depuis_memoire);
    portENTER_CRITICAL(&g_verrou);
    g_actuels = depart;
    portEXIT_CRITICAL(&g_verrou);
    ESP_LOGI(TAG, "réseau Wi-Fi de départ : %s",
             !depart.present ? "aucun" : (depuis_memoire ? "retenu en mémoire permanente" : "compilé"));
    modifier([&depart](EtatReseau &e) {
        e = EtatReseau{};
        e.identifiants_presents = depart.present;
        e.lien_ms = -1;
        e.wifi_ms = -1;
        e.ip_ms = -1;
        e.heure_ms = -1;
    });
    effacer(&depart, sizeof(depart));
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

// ---- interface demandée par les réglages à l'écran (reseau_wifi.h) -----------------------------------------------------------

void reseau_scanner_lancer() {
    portENTER_CRITICAL(&g_verrou);
    const bool possible = g_wifi_pret && !g_scan_en_cours && !g_occupe;
    if (possible) {
        g_scan_en_cours = true;
        g_debut_scan_us = esp_timer_get_time();
    }
    portEXIT_CRITICAL(&g_verrou);
    if (possible && !poster(kMsgScanner)) {
        portENTER_CRITICAL(&g_verrou);
        g_scan_en_cours = false;
        portEXIT_CRITICAL(&g_verrou);
    }
}

bool reseau_scanner_en_cours() {
    portENTER_CRITICAL(&g_verrou);
    const bool oui = g_scan_en_cours;
    portEXIT_CRITICAL(&g_verrou);
    return oui;
}

std::size_t reseau_scanner_resultats(PointAcces *sortie, std::size_t max) {
    if (sortie == nullptr || max == 0) {
        return 0;
    }
    portENTER_CRITICAL(&g_verrou);
    const std::size_t n = g_nb_resultats < max ? g_nb_resultats : max;
    std::memcpy(sortie, g_resultats, n * sizeof(PointAcces));
    portEXIT_CRITICAL(&g_verrou);
    return n;
}

bool reseau_connecter(const char *ssid, const char *mot_de_passe, SurResultatConnexion sur_resultat, void *contexte) {
    if (ssid == nullptr) {
        return false;
    }
    const std::size_t longueur_ssid = strnlen(ssid, kLongSsid + 1);
    const std::size_t longueur_mdp = mot_de_passe != nullptr ? strnlen(mot_de_passe, 64) : 0;
    if (longueur_ssid == 0 || longueur_ssid > kLongSsid || longueur_mdp > 63 ||
        (longueur_mdp > 0 && longueur_mdp < 8)) {
        return false;
    }
    portENTER_CRITICAL(&g_verrou);
    const bool libre = g_wifi_pret && !g_occupe;
    if (libre) {
        g_occupe = true;
        effacer(&g_nouveau, sizeof(g_nouveau));
        std::memcpy(g_nouveau.ssid, ssid, longueur_ssid);
        if (longueur_mdp > 0) {
            std::memcpy(g_nouveau.mot_de_passe, mot_de_passe, longueur_mdp);
        }
        g_nouveau.present = true;
        g_rappel = sur_resultat;
        g_contexte = contexte;
        g_debut_essai_us = esp_timer_get_time();
    }
    portEXIT_CRITICAL(&g_verrou);
    if (!libre) {
        return false;
    }
    if (!poster(kMsgDemarrer)) {
        portENTER_CRITICAL(&g_verrou);
        g_occupe = false;
        g_rappel = nullptr;
        g_contexte = nullptr;
        effacer(&g_nouveau, sizeof(g_nouveau));
        portEXIT_CRITICAL(&g_verrou);
        return false;
    }
    return true;
}

void reseau_abandonner() {
    portENTER_CRITICAL(&g_verrou);
    const bool en_cours = g_occupe;
    portEXIT_CRITICAL(&g_verrou);
    if (en_cours) {
        poster(kMsgAbandonner);
    }
}

std::size_t reseau_ssid_courant(char *sortie, std::size_t capacite) {
    if (sortie == nullptr || capacite == 0) {
        return 0;
    }
    std::size_t n = 0;
    portENTER_CRITICAL(&g_verrou);
    if (g_etat.wifi_connecte && g_actuels.present && !g_occupe) {
        n = std::strlen(g_actuels.ssid);
        if (n > capacite - 1) {
            n = capacite - 1;
        }
        std::memcpy(sortie, g_actuels.ssid, n);
    }
    portEXIT_CRITICAL(&g_verrou);
    sortie[n] = '\0';
    return n;
}

} // namespace ecran
