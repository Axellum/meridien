#include "diagnostic_sntp.h"

#include <cstdint>
#include <cstring>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip_addr.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "reseau.h"

namespace ecran {

namespace {

const char *TAG = "diag_sntp";

// Noms différents de ceux du SNTP de lwIP (fr.pool.ntp.org, pool.ntp.org) : sinon notre résolution remplirait le cache DNS de lwIP
// avant la sienne et masquerait un défaut de DNS.
constexpr const char *kNoms[2] = {"0.fr.pool.ntp.org", "0.pool.ntp.org"};
constexpr int kSondes = 5;          // requêtes SNTP directes au plus
constexpr int kDelaiSondeMs = 2000; // attente d'une réponse
constexpr int kPasMs = 3000;        // cadence des constats
constexpr int kPasMax = 20;         // 60 s au plus
constexpr int kTailleSntp = 48;

bool g_demarre = false;

int64_t depuis_ms(int64_t instant_us) { return (esp_timer_get_time() - instant_us) / 1000; }

void journaliser_serveurs_dns() {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr) {
        ESP_LOGW(TAG, "interface réseau WIFI_STA_DEF introuvable");
        return;
    }
    const esp_netif_dns_type_t types[2] = {ESP_NETIF_DNS_MAIN, ESP_NETIF_DNS_BACKUP};
    const char *noms[2] = {"principal", "secours"};
    for (int i = 0; i < 2; ++i) {
        esp_netif_dns_info_t info = {};
        char texte[16] = "aucun";
        if (esp_netif_get_dns_info(netif, types[i], &info) == ESP_OK && info.ip.type == ESP_IPADDR_TYPE_V4 &&
            info.ip.u_addr.ip4.addr != 0) {
            esp_ip4addr_ntoa(&info.ip.u_addr.ip4, texte, sizeof(texte));
        }
        ESP_LOGI(TAG, "serveur DNS %s de la carte : %s", noms[i], texte);
    }
}

bool resoudre(const char *nom, sockaddr_in &adresse) {
    addrinfo indice = {};
    indice.ai_family = AF_INET;
    indice.ai_socktype = SOCK_DGRAM;
    addrinfo *resultat = nullptr;
    const int64_t debut = esp_timer_get_time();
    const int err = getaddrinfo(nom, "123", &indice, &resultat);
    const int64_t duree = depuis_ms(debut);
    if (err != 0 || resultat == nullptr) {
        ESP_LOGW(TAG, "résolution de %s : échec (code %d) après %d ms", nom, err, static_cast<int>(duree));
        if (resultat != nullptr) {
            freeaddrinfo(resultat);
        }
        return false;
    }
    std::memcpy(&adresse, resultat->ai_addr, sizeof(adresse));
    freeaddrinfo(resultat);
    char texte[16];
    esp_ip4addr_ntoa(reinterpret_cast<const esp_ip4_addr_t *>(&adresse.sin_addr), texte, sizeof(texte));
    ESP_LOGI(TAG, "résolution de %s : %s en %d ms", nom, texte, static_cast<int>(duree));
    return true;
}

// Une requête SNTP directe : vrai si une réponse d'au moins 48 octets arrive avant le délai.
bool sonde(int numero, const sockaddr_in &serveur, int64_t t_ip_us) {
    char texte[16];
    esp_ip4addr_ntoa(reinterpret_cast<const esp_ip4_addr_t *>(&serveur.sin_addr), texte, sizeof(texte));
    const int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        ESP_LOGW(TAG, "sonde %d : socket impossible", numero);
        return false;
    }
    timeval delai = {};
    delai.tv_sec = kDelaiSondeMs / 1000;
    delai.tv_usec = (kDelaiSondeMs % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &delai, sizeof(delai));
    uint8_t requete[kTailleSntp] = {};
    requete[0] = 0x1B; // LI 0, version 3, mode client
    const int64_t envoi = esp_timer_get_time();
    const ssize_t envoye =
        sendto(s, requete, sizeof(requete), 0, reinterpret_cast<const sockaddr *>(&serveur), sizeof(serveur));
    bool recue = false;
    if (envoye == static_cast<ssize_t>(sizeof(requete))) {
        uint8_t reponse[64];
        const ssize_t lu = recv(s, reponse, sizeof(reponse), 0);
        recue = lu >= kTailleSntp;
    }
    close(s);
    if (recue) {
        ESP_LOGI(TAG, "sonde %d vers %s à +%d ms : réponse en %d ms", numero, texte,
                 static_cast<int>(depuis_ms(t_ip_us)), static_cast<int>(depuis_ms(envoi)));
    } else {
        ESP_LOGW(TAG, "sonde %d vers %s à +%d ms : %s", numero, texte, static_cast<int>(depuis_ms(t_ip_us)),
                 envoye == static_cast<ssize_t>(sizeof(requete)) ? "perdue (délai écoulé)" : "envoi impossible");
    }
    return recue;
}

void journaliser_etat_lwip(int64_t t_ip_us) {
    char texte[2][20];
    uint8_t accessibilite[2];
    for (uint8_t i = 0; i < 2; ++i) {
        const ip_addr_t *adresse = esp_sntp_getserver(i);
        if (adresse == nullptr || ip_addr_isany(adresse)) {
            std::strcpy(texte[i], "non résolu");
        } else {
            ipaddr_ntoa_r(adresse, texte[i], sizeof(texte[i]));
        }
        accessibilite[i] = esp_sntp_getreachability(i);
    }
    ESP_LOGI(TAG, "SNTP de lwIP à +%d ms : serveur 0 %s (accessibilité 0x%02x) ; serveur 1 %s (accessibilité 0x%02x)",
             static_cast<int>(depuis_ms(t_ip_us)), texte[0], accessibilite[0], texte[1], accessibilite[1]);
}

void tache_diagnostic(void *) {
    const int64_t t_ip = esp_timer_get_time();
    journaliser_serveurs_dns();
    sockaddr_in adresses[2] = {};
    bool resolue[2] = {};
    for (int i = 0; i < 2; ++i) {
        resolue[i] = resoudre(kNoms[i], adresses[i]);
    }
    int envoyees = 0;
    int recues = 0;
    int32_t synchro_ms = -1;
    for (int pas = 0; pas < kPasMax; ++pas) {
        const int64_t debut_pas = esp_timer_get_time();
        journaliser_etat_lwip(t_ip);
        if (reseau_etat().heure_synchronisee) {
            synchro_ms = static_cast<int32_t>(depuis_ms(t_ip));
            break;
        }
        if (envoyees < kSondes && resolue[envoyees % 2]) {
            const int numero = envoyees + 1;
            ++envoyees;
            if (sonde(numero, adresses[(numero - 1) % 2], t_ip)) {
                ++recues;
            }
        }
        const int64_t reste = kPasMs - depuis_ms(debut_pas);
        if (reste > 0) {
            vTaskDelay(pdMS_TO_TICKS(reste));
        }
    }
    ESP_LOGI(TAG, "bilan : sondes UDP directes reçues %d sur %d ; heure de lwIP %s", recues, envoyees,
             synchro_ms >= 0 ? "synchronisée" : "non synchronisée dans les 60 s");
    if (synchro_ms >= 0) {
        ESP_LOGI(TAG, "heure de lwIP synchronisée à +%d ms après l'adresse IP", static_cast<int>(synchro_ms));
    }
    vTaskDelete(nullptr);
}

} // namespace

void diagnostic_sntp_demarrer() {
    if (g_demarre) {
        return;
    }
    g_demarre = true;
    if (xTaskCreate(tache_diagnostic, "diag_sntp", 4096, nullptr, 3, nullptr) != pdPASS) {
        ESP_LOGW(TAG, "tâche de diagnostic non créée");
    }
}

} // namespace ecran
