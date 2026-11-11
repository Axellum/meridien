#include "sntp_rapide.h"

#include <cstdint>
#include <cstring>
#include <sys/time.h>

#include "ecran/sntp.hpp"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

namespace ecran {

namespace {

const char *TAG = "sntp_rapide";

// Trois serveurs interrogés ensemble : deux du pool français et un du pool mondial (des adresses différentes à chaque nom).
constexpr const char *kNoms[] = {"0.fr.pool.ntp.org", "1.fr.pool.ntp.org", "2.pool.ntp.org"};
constexpr int kNombreNoms = sizeof(kNoms) / sizeof(kNoms[0]);
constexpr int kEssaisResolution = 5;
constexpr int kMancheMs = 1500; // entre deux envois (les serveurs sont tous interrogés à chaque manche)
constexpr int kManchesMax = 8;  // 12 s au plus, puis relance programmée (ecran/sntp.hpp) ; lwIP continue de son côté
constexpr int kEcouteMs = 200;  // pas d'écoute du socket
constexpr uint32_t kPileOctets = 6144; // la marge réellement restante est journalisée en fin de tâche

// État partagé entre trois tâches : la boucle d'événements (nouvelle IP), la tâche de demande (fin) et celle des minuteurs
// (relance). Les DÉCISIONS sont prises par le noyau (ecran/sntp.hpp, testé sur le PC) sous ce verrou ; leurs effets (tâche,
// minuteur) sont appliqués hors du verrou.
portMUX_TYPE g_verrou = portMUX_INITIALIZER_UNLOCKED;
sntp::Planificateur g_plan;
esp_timer_handle_t g_minuteur_relance = nullptr;
void (*g_quand_regle)() = nullptr;
int64_t g_ip_us = 0;           // instant de la dernière adresse IP (esp_timer), pour mesurer « IP -> heure »
uint32_t g_numero_demande = 0; // compte des demandes depuis le démarrage, pour lire le journal

int64_t depuis_ms(int64_t instant_us) { return (esp_timer_get_time() - instant_us) / 1000; }

bool resoudre(const char *nom, sockaddr_in &adresse) {
    addrinfo indice = {};
    indice.ai_family = AF_INET;
    indice.ai_socktype = SOCK_DGRAM;
    addrinfo *resultat = nullptr;
    if (getaddrinfo(nom, "123", &indice, &resultat) != 0 || resultat == nullptr) {
        if (resultat != nullptr) {
            freeaddrinfo(resultat);
        }
        return false;
    }
    std::memcpy(&adresse, resultat->ai_addr, sizeof(adresse));
    freeaddrinfo(resultat);
    return true;
}

bool deja_presente(const sockaddr_in *serveurs, int nombre, const sockaddr_in &adresse) {
    for (int i = 0; i < nombre; ++i) {
        if (serveurs[i].sin_addr.s_addr == adresse.sin_addr.s_addr) {
            return true;
        }
    }
    return false;
}

void texte_adresse(const sockaddr_in &adresse, char (&texte)[16]) {
    esp_ip4addr_ntoa(reinterpret_cast<const esp_ip4_addr_t *>(&adresse.sin_addr), texte, sizeof(texte));
}

// Une demande : résoudre les noms, interroger les serveurs par manches, régler l'horloge à la première réponse vérifiée. Vrai si
// l'horloge a été réglée. `numero` et `ip_us` ne servent qu'au journal.
bool acquerir(uint32_t numero, int64_t ip_us) {
    const int64_t debut = esp_timer_get_time();
    sockaddr_in serveurs[kNombreNoms] = {};
    int nombre = 0;
    for (int essai = 0; essai < kEssaisResolution && nombre == 0; ++essai) {
        for (const char *nom : kNoms) {
            sockaddr_in adresse = {};
            if (resoudre(nom, adresse) && !deja_presente(serveurs, nombre, adresse)) {
                serveurs[nombre++] = adresse;
            }
        }
        if (nombre == 0) {
            vTaskDelay(pdMS_TO_TICKS(1000)); // DNS pas encore prêt : on réessaie
        }
    }
    if (nombre == 0) {
        ESP_LOGW(TAG, "demande n° %u : aucun serveur de temps résolu après %d essais", static_cast<unsigned>(numero),
                 kEssaisResolution);
        return false;
    }
    const int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        ESP_LOGW(TAG, "demande n° %u : socket impossible", static_cast<unsigned>(numero));
        return false;
    }
    timeval delai = {};
    delai.tv_sec = 0;
    delai.tv_usec = kEcouteMs * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &delai, sizeof(delai));

    uint8_t requete[sntp::kTaillePaquet] = {};
    uint8_t nonce[sntp::kTailleNonce] = {};
    bool regle = false;
    for (int manche = 0; manche < kManchesMax && !regle; ++manche) {
        // Nonce neuf à chaque manche, dans l'horodatage d'émission : une réponse en retard d'une manche précédente est refusée,
        // l'erreur possible sur l'heure reste donc inférieure à la durée d'une manche.
        esp_fill_random(nonce, sizeof(nonce));
        sntp::preparer_requete(requete, nonce);
        const int64_t debut_manche = esp_timer_get_time();
        for (int i = 0; i < nombre; ++i) {
            sendto(s, requete, sizeof(requete), 0, reinterpret_cast<const sockaddr *>(&serveurs[i]),
                   sizeof(serveurs[i]));
        }
        while (!regle && depuis_ms(debut_manche) < kMancheMs) {
            uint8_t reponse[64];
            sockaddr_in source = {};
            socklen_t taille_source = sizeof(source);
            const int lu =
                recvfrom(s, reponse, sizeof(reponse), 0, reinterpret_cast<sockaddr *>(&source), &taille_source);
            if (lu < 0 || !deja_presente(serveurs, nombre, source)) {
                continue; // délai d'écoute écoulé, ou paquet d'une adresse que nous n'avons pas interrogée
            }
            sntp::Instant heure;
            if (sntp::lire_reponse(reponse, lu, nonce, heure) != sntp::Refus::Aucun) {
                continue;
            }
            timeval instant = {};
            instant.tv_sec = static_cast<time_t>(heure.secondes);
            instant.tv_usec = static_cast<suseconds_t>(heure.microsecondes);
            if (settimeofday(&instant, nullptr) != 0) {
                ESP_LOGW(TAG, "settimeofday refusé");
                continue;
            }
            char texte[16];
            texte_adresse(source, texte);
            // Ligne de mesure « IP -> heure » (docs/mesures/1b.md) : aucune donnée privée (ni nom du réseau, ni adresse de l'appareil).
            ESP_LOGI(TAG,
                     "heure reçue %d ms après l'IP (demande n° %u, +%d ms après son lancement, manche %d sur %d, "
                     "serveur %s, "
                     "aller-retour %d ms) ; utc=%lld",
                     static_cast<int>(depuis_ms(ip_us)), static_cast<unsigned>(numero),
                     static_cast<int>(depuis_ms(debut)), manche + 1, kManchesMax, texte,
                     static_cast<int>(depuis_ms(debut_manche)), static_cast<long long>(instant.tv_sec));
            regle = true;
        }
        if (!regle) {
            ESP_LOGI(TAG, "demande n° %u, manche %d : aucune réponse valide des %d serveurs",
                     static_cast<unsigned>(numero), manche + 1, nombre);
        }
    }
    close(s);
    if (!regle) {
        ESP_LOGW(TAG, "demande n° %u : aucune réponse après %d manches", static_cast<unsigned>(numero), kManchesMax);
    }
    return regle;
}

void appliquer(const sntp::Decision &d);

void tache_acquisition(void *) {
    portENTER_CRITICAL(&g_verrou);
    const uint32_t numero = g_numero_demande;
    const int64_t ip_us = g_ip_us;
    void (*const quand_regle)() = g_quand_regle;
    portEXIT_CRITICAL(&g_verrou);
    const bool regle = acquerir(numero, ip_us);
    if (regle && quand_regle != nullptr) {
        quand_regle();
    }
    ESP_LOGI(TAG, "pile : %u octets libres au minimum sur %u",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)), static_cast<unsigned>(kPileOctets));
    portENTER_CRITICAL(&g_verrou);
    const sntp::Decision d = sntp::sur_fin(g_plan, regle);
    portEXIT_CRITICAL(&g_verrou);
    // Peut armer la relance ; une nouvelle IP arrivée à cet instant peut lancer la demande suivante avant que cette tâche ne soit
    // effacée : deux piles un court instant, jamais deux demandes.
    appliquer(d);
    vTaskDelete(nullptr);
}

// Effets d'une décision du noyau, hors du verrou.
void appliquer(const sntp::Decision &d) {
    if (d.annuler_relance && g_minuteur_relance != nullptr) {
        esp_timer_stop(
            g_minuteur_relance); // déjà arrêté : erreur sans effet ; s'il sonne quand même, le noyau l'ignore
    }
    if (d.relance_dans_ms > 0) {
        if (g_minuteur_relance != nullptr) {
            esp_timer_stop(g_minuteur_relance);
            esp_timer_start_once(g_minuteur_relance, static_cast<uint64_t>(d.relance_dans_ms) * 1000ULL);
            ESP_LOGI(TAG, "nouvelle demande dans %u s ; en attendant, le SNTP de lwIP continue seul",
                     static_cast<unsigned>(d.relance_dans_ms / 1000));
        } else {
            ESP_LOGW(TAG, "pas de minuteur de relance : prochaine demande à la prochaine adresse IP");
        }
    }
    if (d.lancer) {
        portENTER_CRITICAL(&g_verrou);
        ++g_numero_demande;
        portEXIT_CRITICAL(&g_verrou);
        if (xTaskCreate(tache_acquisition, "sntp_rapide", kPileOctets, nullptr, 3, nullptr) != pdPASS) {
            ESP_LOGW(TAG, "tâche non créée : relance prévue");
            portENTER_CRITICAL(&g_verrou);
            const sntp::Decision echec = sntp::sur_fin(g_plan, false);
            portEXIT_CRITICAL(&g_verrou);
            appliquer(echec); // n'arme qu'un minuteur (sur_fin ne lance jamais) : pas de boucle
        }
    }
}

void sur_minuteur_relance(void *) {
    portENTER_CRITICAL(&g_verrou);
    const sntp::Decision d = sntp::sur_minuteur(g_plan);
    portEXIT_CRITICAL(&g_verrou);
    appliquer(d);
}

} // namespace

void sntp_rapide_sur_ip(void (*quand_regle)()) {
    if (g_minuteur_relance ==
        nullptr) { // appelée depuis la boucle d'événements seulement : jamais deux créations à la fois
        const esp_timer_create_args_t args = {.callback = sur_minuteur_relance,
                                              .arg = nullptr,
                                              .dispatch_method = ESP_TIMER_TASK,
                                              .name = "sntp_relance",
                                              .skip_unhandled_events = true};
        if (esp_timer_create(&args, &g_minuteur_relance) != ESP_OK) {
            g_minuteur_relance = nullptr;
            ESP_LOGW(TAG, "minuteur de relance non créé : une demande par adresse IP seulement");
        }
    }
    portENTER_CRITICAL(&g_verrou);
    g_quand_regle = quand_regle;
    g_ip_us = esp_timer_get_time();
    const sntp::Decision d = sntp::sur_nouvelle_ip(g_plan);
    portEXIT_CRITICAL(&g_verrou);
    if (!d.lancer) {
        ESP_LOGI(TAG, "nouvelle adresse IP pendant une demande : elle continue, pas de seconde demande");
    }
    appliquer(d);
}

} // namespace ecran
