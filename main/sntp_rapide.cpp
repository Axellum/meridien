#include "sntp_rapide.h"

#include <cstdint>
#include <cstring>
#include <sys/time.h>

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
constexpr int kManchesMax = 8;  // 12 s au plus, puis le SNTP de lwIP reste seul
constexpr int kEcouteMs = 200;  // pas d'écoute du socket
constexpr int kTailleSntp = 48;
constexpr uint32_t kPileOctets = 6144; // la marge réellement restante est journalisée en fin de tâche
constexpr uint64_t kSecondesEntre1900Et1970 = 2208988800ULL;
constexpr int64_t kUnixMinimum = 1767225600; // 1er janvier 2026 : en dessous, la réponse est absurde

void (*g_quand_regle)() = nullptr;
bool g_demarre = false;

int64_t depuis_ms(int64_t instant_us) { return (esp_timer_get_time() - instant_us) / 1000; }

uint32_t lire_u32(const uint8_t *octets) {
    return (static_cast<uint32_t>(octets[0]) << 24) | (static_cast<uint32_t>(octets[1]) << 16) |
           (static_cast<uint32_t>(octets[2]) << 8) | static_cast<uint32_t>(octets[3]);
}

// Vrai si `reponse` est une réponse SNTP valide à la requête portant `nonce` (nombre aléatoire placé dans l'horodatage d'émission
// du client, que le serveur doit recopier dans l'horodatage d'origine : un paquet falsifié ou en retard ne le connaît pas).
// Remplit `instant` avec l'heure du serveur.
bool lire_reponse(const uint8_t *reponse, int taille, const uint8_t *nonce, timeval &instant) {
    if (taille < kTailleSntp) {
        return false;
    }
    const int mode = reponse[0] & 7;
    const int indicateur_alerte = reponse[0] >> 6;
    const int strate = reponse[1];
    if (mode != 4 || indicateur_alerte == 3 || strate < 1 || strate > 15) {
        return false; // pas une réponse de serveur, horloge du serveur non synchronisée, ou « Kiss-of-Death » (strate 0)
    }
    if (std::memcmp(reponse + 24, nonce, 8) != 0) {
        return false;
    }
    const uint32_t secondes = lire_u32(reponse + 40);
    const uint32_t fraction = lire_u32(reponse + 44);
    if (secondes == 0) {
        return false;
    }
    // Les secondes NTP repartent de zéro en 2036 (bit de poids fort à 0 : ère suivante).
    const uint64_t depuis_1900 =
        (secondes >= kSecondesEntre1900Et1970) ? secondes : (static_cast<uint64_t>(secondes) + (1ULL << 32));
    const int64_t unix_s = static_cast<int64_t>(depuis_1900 - kSecondesEntre1900Et1970);
    if (unix_s < kUnixMinimum) {
        return false;
    }
    instant.tv_sec = static_cast<time_t>(unix_s);
    instant.tv_usec = static_cast<suseconds_t>((static_cast<uint64_t>(fraction) * 1000000ULL) >> 32);
    return true;
}

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

void tache_acquisition(void *) {
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
        ESP_LOGW(TAG, "aucun serveur de temps résolu après %d essais : le SNTP de lwIP reste seul", kEssaisResolution);
        vTaskDelete(nullptr);
        return;
    }
    const int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        ESP_LOGW(TAG, "socket impossible : le SNTP de lwIP reste seul");
        vTaskDelete(nullptr);
        return;
    }
    timeval delai = {};
    delai.tv_sec = 0;
    delai.tv_usec = kEcouteMs * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &delai, sizeof(delai));

    uint8_t requete[kTailleSntp] = {};
    requete[0] = 0x23; // pas d'alerte, version 4, mode client

    bool regle = false;
    for (int manche = 0; manche < kManchesMax && !regle; ++manche) {
        // Nonce neuf à chaque manche, dans l'horodatage d'émission : une réponse en retard d'une manche précédente est refusée,
        // l'erreur possible sur l'heure reste donc inférieure à la durée d'une manche.
        esp_fill_random(requete + 40, 8);
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
            timeval instant = {};
            if (!lire_reponse(reponse, lu, requete + 40, instant)) {
                continue;
            }
            if (settimeofday(&instant, nullptr) != 0) {
                ESP_LOGW(TAG, "settimeofday refusé");
                continue;
            }
            char texte[16];
            texte_adresse(source, texte);
            ESP_LOGI(TAG,
                     "horloge réglée à +%d ms après le lancement (manche %d sur %d, serveur %s, aller-retour %d ms) ; "
                     "utc=%lld",
                     static_cast<int>(depuis_ms(debut)), manche + 1, kManchesMax, texte,
                     static_cast<int>(depuis_ms(debut_manche)), static_cast<long long>(instant.tv_sec));
            regle = true;
        }
        if (!regle) {
            ESP_LOGI(TAG, "manche %d : aucune réponse valide des %d serveurs", manche + 1, nombre);
        }
    }
    close(s);
    if (regle) {
        if (g_quand_regle != nullptr) {
            g_quand_regle();
        }
    } else {
        ESP_LOGW(TAG, "aucune réponse après %d manches : le SNTP de lwIP reste seul", kManchesMax);
    }
    ESP_LOGI(TAG, "pile : %u octets libres au minimum sur %u",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)), static_cast<unsigned>(kPileOctets));
    vTaskDelete(nullptr);
}

} // namespace

void sntp_rapide_demarrer(void (*quand_regle)()) {
    if (g_demarre) {
        return;
    }
    g_demarre = true;
    g_quand_regle = quand_regle;
    if (xTaskCreate(tache_acquisition, "sntp_rapide", kPileOctets, nullptr, 3, nullptr) != pdPASS) {
        ESP_LOGW(TAG, "tâche non créée : le SNTP de lwIP reste seul");
    }
}

} // namespace ecran
