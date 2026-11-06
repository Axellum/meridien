// Recherche de lieu côté appareil (contrat main/geocodage.h).
//
// Ce fichier ne décide de rien : la lecture de la réponse, les règles d'écart et l'état d'une recherche (« la dernière demande
// gagne ») sont dans le noyau pur ecran/geocodage.hpp, testé sur le PC avec de vraies réponses d'Open-Meteo. Ici : la tâche, le
// téléchargement HTTPS et le verrou.
//
// UNE tâche, créée à la demande et supprimée quand il n'y a plus rien à faire (pas de pile de 12 Ko gardée en permanence : la RAM
// interne manque). Elle tourne à la priorité d'arrière-plan : la prise de contact TLS calcule plusieurs secondes sans dormir ; à une
// priorité plus haute que celle de la tâche d'inactivité, le chien de garde des tâches se déclenche (constaté par jalon-1b sur la
// tâche météo). Elle traite d'abord la demande de la page web (un navigateur attend), puis celle de l'écran.
//
// Les gros tampons (réponse brute de 16 Ko, huit lieux, adresses) sont en PSRAM, alloués au premier usage ; la RAM interne ne sert
// qu'à la pile de la tâche, à deux sémaphores, et à ce que mbedtls et esp_http_client prennent pendant la poignée de main (≈ 65 Ko,
// sous le verrou TLS commun : verrou_tls.h). Le texte cherché n'est jamais écrit au journal (c'est un lieu, donc une donnée privée).
#include "geocodage.h"

#include <cstdint>
#include <cstring>
#include <new>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "reglages.h"
#include "reseau.h"
#include "verrou_tls.h"

namespace ecran {

namespace {

const char *TAG = "geocodage";

// Seule adresse joignable d'ici.
constexpr std::string_view kAutorise = "https://geocoding-api.open-meteo.com/";
constexpr size_t kTailleReponse = 16384; // réponse brute d'Open-Meteo : ≈ 5 Ko pour huit lieux
constexpr int kDelaiHttpMs = 8000;       // connexion, TLS et lecture : au-delà, la recherche est un échec
constexpr uint32_t kAttenteVerrouMs =
    15000;                                 // une requête météo en cours (15 s au plus) ne fait pas échouer la recherche
constexpr uint32_t kAttentePageMs = 30000; // attente maximale du serveur HTTP : verrou + téléchargement + marge
constexpr uint32_t kPileTache = 12288;     // la poignée de main TLS demande plusieurs Ko de pile
constexpr UBaseType_t kPrioriteTache = tskIDLE_PRIORITY;

// La demande de la page web (une seule à la fois : le serveur n'a qu'un fil).
struct Page {
    bool en_attente = false; // demandée et pas encore finie (la tâche s'en occupe)
    bool abandonne = false;  // l'appelant a renoncé : la tâche jette le résultat au lieu de le copier chez lui
    char *tampon = nullptr;
    size_t capacite = 0;
    size_t taille = 0;
    api::Telechargement resultat = api::Telechargement::Echec;
    char adresse[api::ADRESSE_GEOCODAGE_MAX + 1] = {};
};

struct Contexte {
    SemaphoreHandle_t verrou = nullptr;     // protège `recherche`, `page` et `tache_active`
    SemaphoreHandle_t page_finie = nullptr; // donné par la tâche quand la demande de la page est finie
    geocodage::Recherche recherche;
    Page page;
    bool tache_active = false;
    // Propres à la tâche (une seule à la fois) : pas besoin du verrou.
    char travail[kTailleReponse];
    geocodage::Lieu lieux[geocodage::LIEUX_MAX];
    char adresse_ecran[api::ADRESSE_GEOCODAGE_MAX + 1];
};

Contexte *creer() {
    void *memoire = heap_caps_calloc(1, sizeof(Contexte), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (memoire == nullptr) {
        ESP_LOGE(TAG, "mémoire PSRAM non obtenue (%u octets) : pas de recherche de ville",
                 static_cast<unsigned>(sizeof(Contexte)));
        return nullptr;
    }
    Contexte *c = new (memoire) Contexte();
    c->verrou = xSemaphoreCreateMutex();
    c->page_finie = xSemaphoreCreateBinary();
    if (c->verrou == nullptr || c->page_finie == nullptr) {
        ESP_LOGE(TAG, "sémaphores non créés : pas de recherche de ville");
        return nullptr; // le contexte reste alloué : il ne sera jamais utilisé, et l'erreur est rare
    }
    return c;
}

// Créé au premier appel (l'initialisation d'une variable locale statique est protégée contre deux fils).
Contexte *contexte() {
    static Contexte *c = creer();
    return c;
}

// ---- téléchargement ---------------------------------------------------------------------------------------------------------------

struct Reception {
    char *donnees;
    size_t capacite;
    size_t n;
    bool trop;
};

esp_err_t sur_evenement_http(esp_http_client_event_t *evt) {
    auto *rec = static_cast<Reception *>(evt->user_data);
    if (rec != nullptr && evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0 && evt->data != nullptr) {
        const size_t len = static_cast<size_t>(evt->data_len);
        if (rec->n + len > rec->capacite) {
            rec->trop = true; // plus grand que prévu : on ne lit pas une réponse à moitié
        } else {
            std::memcpy(rec->donnees + rec->n, evt->data, len);
            rec->n += len;
        }
    }
    return ESP_OK;
}

// Télécharge `adresse` (zéro final) dans `tampon`. Appelé seulement depuis la tâche du module.
api::Telechargement telecharger(const char *adresse, char *tampon, size_t capacite, size_t &taille) {
    taille = 0;
    if (std::strncmp(adresse, kAutorise.data(), kAutorise.size()) != 0) {
        return api::Telechargement::Echec;
    }
    if (!reseau_etat().ip_obtenue) {
        return api::Telechargement::SansInternet;
    }
    const int64_t debut_us = esp_timer_get_time();
    VerrouTls verrou(kAttenteVerrouMs);
    if (!verrou.pris()) {
        ESP_LOGW(TAG, "verrou TLS occupé depuis plus de %u ms : recherche abandonnée",
                 static_cast<unsigned>(kAttenteVerrouMs));
        return api::Telechargement::SansInternet;
    }

    Reception rec = {tampon, capacite, 0, false};
    esp_http_client_config_t config = {};
    config.url = adresse;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = kDelaiHttpMs;
    config.event_handler = sur_evenement_http;
    config.user_data = &rec;
    config.crt_bundle_attach = esp_crt_bundle_attach; // le serveur est vérifié (certificats racine d'ESP-IDF)
    config.buffer_size = 2048;
    config.buffer_size_tx = 1536; // adresse (jusqu'à 1 Ko avec 80 caractères accentués) + en-têtes
    config.keep_alive_enable = false;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGW(TAG, "client HTTP non créé (mémoire interne libre : %u octets)",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
        return api::Telechargement::Echec;
    }
    esp_http_client_set_header(client, "Accept", "application/json");
    const esp_err_t err = esp_http_client_perform(client);
    const int statut = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    // Pour le budget de mémoire interne (jalon-1b) : ce qui reste, et le plus bas atteint depuis le démarrage.
    ESP_LOGI(TAG,
             "téléchargement fini en %d ms (HTTP %d, %u octets) ; RAM interne libre %u, minimum depuis le démarrage %u",
             static_cast<int>((esp_timer_get_time() - debut_us) / 1000), statut, static_cast<unsigned>(rec.n),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "requête échouée (%s)", esp_err_to_name(err));
        return api::Telechargement::SansInternet;
    }
    if (statut != 200 || rec.trop) {
        ESP_LOGW(TAG, "réponse refusée (HTTP %d%s)", statut, rec.trop ? ", trop grande" : "");
        return api::Telechargement::Echec;
    }
    taille = rec.n;
    return api::Telechargement::Ok;
}

// ---- la tâche ---------------------------------------------------------------------------------------------------------------------

void travailler_pour_la_page(Contexte *c) {
    size_t taille = 0;
    api::Telechargement r = telecharger(c->page.adresse, c->travail, kTailleReponse, taille);
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    if (!c->page.abandonne) {
        if (r == api::Telechargement::Ok && taille > c->page.capacite) {
            r = api::Telechargement::Echec; // le tampon du serveur est plus petit que la réponse
        }
        if (r == api::Telechargement::Ok) {
            std::memcpy(c->page.tampon, c->travail, taille);
            c->page.taille = taille;
        }
        c->page.resultat = r;
        xSemaphoreGive(c->page_finie);
    }
    c->page.en_attente = false;
    xSemaphoreGive(c->verrou);
}

void travailler_pour_l_ecran(Contexte *c, const char *texte, std::uint32_t numero) {
    geocodage::Issue issue = geocodage::Issue::SansConnexion;
    size_t n = 0;
    api::Ecrivain adresse(c->adresse_ecran,
                          api::ADRESSE_GEOCODAGE_MAX); // un octet de plus dans le tampon : le zéro final
    reglages::adresse_geocodage(texte, reglages_affichage().langue_ecran, adresse);
    if (adresse.ok()) {
        c->adresse_ecran[adresse.vue().size()] = '\0';
        size_t taille = 0;
        if (telecharger(c->adresse_ecran, c->travail, kTailleReponse, taille) == api::Telechargement::Ok) {
            if (geocodage::analyser(std::string_view(c->travail, taille), c->lieux, geocodage::LIEUX_MAX, n)) {
                issue = geocodage::Issue::Trouvee;
                ESP_LOGI(TAG, "recherche de lieu : %u résultat(s)", static_cast<unsigned>(n));
            } else {
                ESP_LOGW(TAG, "réponse illisible (ce n'est pas un objet JSON)");
            }
        }
    } else {
        ESP_LOGW(TAG, "adresse de la recherche trop longue");
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    c->recherche.terminer(numero, issue, c->lieux, n); // jetée si une demande plus récente est arrivée entre-temps
    xSemaphoreGive(c->verrou);
}

void tache_geocodage(void *) {
    Contexte *c = contexte();
    for (;;) {
        enum class Quoi { Rien, Page, Ecran } quoi = Quoi::Rien;
        char texte[reglages::RECHERCHE_OCTETS_MAX + 1] = {};
        std::uint32_t numero = 0;
        xSemaphoreTake(c->verrou, portMAX_DELAY);
        if (c->page.en_attente && c->page.abandonne) {
            c->page.en_attente = false; // abandonnée avant d'avoir été prise : rien à télécharger
        }
        if (c->page.en_attente) {
            quoi = Quoi::Page;
        } else if (c->recherche.prendre(texte, numero)) {
            quoi = Quoi::Ecran;
        } else {
            c->tache_active = false; // sous le verrou : une demande qui arrive maintenant relancera une tâche
        }
        xSemaphoreGive(c->verrou);
        if (quoi == Quoi::Rien) {
            vTaskDelete(nullptr);
            return;
        }
        if (quoi == Quoi::Page) {
            travailler_pour_la_page(c);
        } else {
            travailler_pour_l_ecran(c, texte, numero);
        }
    }
}

// À appeler verrou tenu. Vrai si une tâche tourne (ou vient d'être lancée) pour traiter les demandes.
bool lancer_tache(Contexte *c) {
    if (c->tache_active) {
        return true;
    }
    if (xTaskCreate(tache_geocodage, "geocodage", kPileTache, nullptr, kPrioriteTache, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "tâche non créée (mémoire interne libre : %u octets)",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
        return false;
    }
    c->tache_active = true;
    return true;
}

} // namespace

// ---- l'écran ----------------------------------------------------------------------------------------------------------------------

void geocodage_chercher(std::string_view texte) {
    Contexte *c = contexte();
    if (c == nullptr) {
        return;
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    const reglages::ErreurRecherche refus = c->recherche.demander(texte);
    if (refus == reglages::ErreurRecherche::Aucune && !lancer_tache(c)) {
        // pas de tâche possible : la demande est close tout de suite, l'écran ne reste pas sur « en cours »
        char pris[reglages::RECHERCHE_OCTETS_MAX + 1] = {};
        std::uint32_t numero = 0;
        if (c->recherche.prendre(pris, numero)) {
            c->recherche.terminer(numero, geocodage::Issue::SansConnexion, nullptr, 0);
        }
    }
    xSemaphoreGive(c->verrou);
}

geocodage::Etat geocodage_etat() {
    Contexte *c = contexte();
    if (c == nullptr) {
        return geocodage::Etat::SansConnexion;
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    const geocodage::Etat etat = c->recherche.etat();
    xSemaphoreGive(c->verrou);
    return etat;
}

std::size_t geocodage_resultats(geocodage::Lieu *sortie, std::size_t max) {
    Contexte *c = contexte();
    if (c == nullptr || sortie == nullptr) {
        return 0;
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    const std::size_t n = c->recherche.resultats(sortie, max);
    xSemaphoreGive(c->verrou);
    return n;
}

// ---- la page web ------------------------------------------------------------------------------------------------------------------

api::Telechargement geocodage_telecharger(std::string_view adresse, char *tampon, std::size_t capacite,
                                          std::size_t &taille) {
    taille = 0;
    Contexte *c = contexte();
    if (c == nullptr || tampon == nullptr || adresse.size() > api::ADRESSE_GEOCODAGE_MAX) {
        return api::Telechargement::Echec;
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    if (c->page.en_attente) {
        xSemaphoreGive(c->verrou);
        return api::Telechargement::
            Echec; // une demande de page précédente est encore en route (abandonnée mais pas finie)
    }
    std::memcpy(c->page.adresse, adresse.data(), adresse.size());
    c->page.adresse[adresse.size()] = '\0';
    c->page.tampon = tampon;
    c->page.capacite = capacite;
    c->page.taille = 0;
    c->page.resultat = api::Telechargement::Echec;
    c->page.abandonne = false;
    c->page.en_attente = true;
    xSemaphoreTake(c->page_finie, 0); // vide le sémaphore d'une demande précédente
    if (!lancer_tache(c)) {
        c->page.en_attente = false;
        xSemaphoreGive(c->verrou);
        return api::Telechargement::Echec;
    }
    xSemaphoreGive(c->verrou);

    const bool fini = xSemaphoreTake(c->page_finie, pdMS_TO_TICKS(kAttentePageMs)) == pdTRUE;
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    if (!fini) {
        if (c->page.en_attente) {
            c->page.abandonne = true; // la tâche jettera son résultat sans toucher à `tampon`
            xSemaphoreGive(c->verrou);
            ESP_LOGW(TAG, "la page n'a pas reçu de réponse en %u ms", static_cast<unsigned>(kAttentePageMs));
            return api::Telechargement::Echec;
        }
        xSemaphoreTake(c->page_finie, 0); // la réponse est arrivée juste après le délai : on la prend
    }
    const api::Telechargement resultat = c->page.resultat;
    taille = c->page.taille;
    xSemaphoreGive(c->verrou);
    return resultat;
}

} // namespace ecran
