// Recherche de lieu côté appareil (contrat main/geocodage.h).
//
// Ce fichier ne décide de rien : la lecture de la réponse et les règles d'écart sont dans le noyau pur (ecran/lieux_geocodage.hpp
// pour l'écran, ecran/geocodage.hpp pour la page), testé sur le PC avec de vraies réponses d'Open-Meteo. Ici : la tâche, le
// téléchargement HTTPS et le verrou. Avant, l'écran avait son propre téléchargeur (tâche « lieu_dl », priorité 3, SANS le verrou
// TLS) : une recherche de ville pendant une requête météo faisait deux poignées de main à la fois.
//
// UNE tâche, créée à la demande et supprimée quand il n'y a plus rien à faire (pas de pile de 12 Ko gardée en permanence : la RAM
// interne manque). Elle tourne à la priorité d'arrière-plan : la prise de contact TLS calcule plusieurs secondes sans dormir ; à une
// priorité plus haute que celle de la tâche d'inactivité, le chien de garde des tâches se déclenche (constaté par jalon-1b sur la
// tâche météo). Elle traite d'abord la demande de la page web (un navigateur attend), puis celle de l'écran.
//
// Le téléchargement passe par le client HTTPS commun (main/http_commun.h : verrou TLS commun, gzip avec CRC-32 vérifié, redirections
// refusées). Les gros tampons (deux réponses de 16 Ko : celle en cours, celle que l'écran lit ; adresses) sont en PSRAM, alloués au
// premier usage, plus les deux tampons du client commun, en PSRAM le temps d'une requête ; la RAM interne ne sert qu'à la pile de
// la tâche, à deux sémaphores, et à ce que mbedtls et esp_http_client prennent pendant la poignée de main (≈ 65 Ko, sous le verrou
// TLS commun : verrou_tls.h). Le texte cherché n'est jamais écrit au journal (c'est un lieu, donc une donnée privée).
#include "geocodage.h"

#include <cstdint>
#include <cstring>
#include <new>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "http_commun.h"
#include "reseau.h"

namespace ecran {

namespace {

const char *TAG = "geocodage";

// Seule adresse joignable d'ici.
constexpr std::string_view kAutorise = "https://geocoding-api.open-meteo.com/";
constexpr size_t kTailleReponse = 16384; // réponse d'Open-Meteo, décompressée : ≈ 5 Ko pour huit lieux
constexpr int kDelaiHttpMs = 8000;       // connexion, TLS et lecture : au-delà, la recherche est un échec
constexpr uint32_t kAttenteVerrouMs =
    15000;                         // une requête météo en cours (15 s au plus) ne fait pas échouer la recherche
constexpr int kTailleEnvoi = 1536; // adresse (jusqu'à 1 Ko avec 80 caractères accentués) + en-têtes
constexpr const char *kAgentUtilisateur = "Meridien/ESP32-P4 (recherche de ville)";
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

// La demande de l'écran : la dernière gagne (chaque demande ou abandon change `generation` ; une réponse arrivée pour une génération
// dépassée est jetée).
struct Ecran {
    bool a_faire = false; // demandée et pas encore prise par la tâche
    std::uint32_t generation = 0;
    EtatTelechargement etat = EtatTelechargement::Inactif;
    size_t taille = 0;
    char adresse[api::ADRESSE_GEOCODAGE_MAX + 1] = {};
    char corps[kTailleReponse] = {}; // la réponse que l'écran lit (copiée depuis `travail` sous le verrou)
};

struct Contexte {
    SemaphoreHandle_t verrou = nullptr;     // protège `ecran`, `page` et `tache_active`
    SemaphoreHandle_t page_finie = nullptr; // donné par la tâche quand la demande de la page est finie
    Ecran ecran;
    Page page;
    bool tache_active = false;
    // Propres à la tâche (une seule à la fois) : pas besoin du verrou.
    char travail[kTailleReponse];
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

// Où la lecture du corps copie la réponse (client_http::LireCorps).
struct Copie {
    char *tampon;
    size_t capacite;
    size_t taille;
};

bool copier_corps(std::string_view corps, void *utilisateur) {
    auto *c = static_cast<Copie *>(utilisateur);
    if (corps.size() > c->capacite) {
        return false; // ne peut pas arriver : la borne de la requête est la capacité du tampon
    }
    std::memcpy(c->tampon, corps.data(), corps.size());
    c->taille = corps.size();
    return true;
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
    Copie copie = {tampon, capacite, 0};
    client_http::Requete requete;
    requete.adresse = adresse;
    requete.user_agent = kAgentUtilisateur;
    requete.accept = "application/json";
    requete.delai_ms = kDelaiHttpMs;
    requete.taille_max = capacite;
    requete.attente_verrou_tls_ms = kAttenteVerrouMs;
    requete.taille_envoi = kTailleEnvoi;
    const client_http::Reponse r = client_http::obtenir(requete, copier_corps, &copie);
    // Pour le budget de mémoire interne (jalon-1b) : ce qui reste, et le plus bas atteint depuis le démarrage.
    ESP_LOGI(TAG,
             "téléchargement fini en %d ms (HTTP %d, %u octets sur le fil%s, %u lus) ; RAM interne libre %u, minimum "
             "depuis le démarrage %u",
             r.duree_ms, r.statut_http, static_cast<unsigned>(r.octets_fil), r.gzip ? " (gzip)" : "",
             static_cast<unsigned>(r.octets), static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)));
    switch (r.issue) {
    case client_http::Issue::Ok:
        taille = copie.taille;
        return api::Telechargement::Ok;
    case client_http::Issue::Reseau:
        // DNS, connexion, TLS, délai, ou verrou TLS occupé plus de 15 s (une requête météo en cours) : comme avant.
        ESP_LOGW(TAG, "requête échouée (réseau, ou verrou TLS occupé plus de %u ms)",
                 static_cast<unsigned>(kAttenteVerrouMs));
        return api::Telechargement::SansInternet;
    default:
        ESP_LOGW(TAG, "réponse refusée (HTTP %d, issue %d)", r.statut_http, static_cast<int>(r.issue));
        return api::Telechargement::Echec;
    }
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

void travailler_pour_l_ecran(Contexte *c, std::uint32_t numero) {
    size_t taille = 0;
    const api::Telechargement r = telecharger(c->adresse_ecran, c->travail, kTailleReponse, taille);
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    if (numero ==
        c->ecran.generation) { // sinon une demande plus récente (ou un abandon) est arrivée entre-temps : jetée
        if (r == api::Telechargement::Ok) {
            std::memcpy(c->ecran.corps, c->travail, taille);
            c->ecran.taille = taille;
            c->ecran.etat = EtatTelechargement::Termine;
        } else {
            c->ecran.etat =
                r == api::Telechargement::SansInternet ? EtatTelechargement::SansInternet : EtatTelechargement::Echec;
        }
    }
    xSemaphoreGive(c->verrou);
}

void tache_geocodage(void *) {
    Contexte *c = contexte();
    for (;;) {
        enum class Quoi { Rien, Page, Ecran } quoi = Quoi::Rien;
        std::uint32_t numero = 0;
        xSemaphoreTake(c->verrou, portMAX_DELAY);
        if (c->page.en_attente && c->page.abandonne) {
            c->page.en_attente = false; // abandonnée avant d'avoir été prise : rien à télécharger
        }
        if (c->page.en_attente) {
            quoi = Quoi::Page;
        } else if (c->ecran.a_faire) {
            std::memcpy(c->adresse_ecran, c->ecran.adresse,
                        sizeof c->adresse_ecran); // copie : la demande peut être remplacée
            numero = c->ecran.generation;
            c->ecran.a_faire = false;
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
            travailler_pour_l_ecran(c, numero);
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

void geocodage_ecran_lancer(const char *adresse) {
    Contexte *c = contexte();
    if (c == nullptr) {
        return;
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    ++c->ecran.generation;
    c->ecran.taille = 0;
    const size_t n = adresse != nullptr ? std::strlen(adresse) : 0;
    if (n == 0 || n > api::ADRESSE_GEOCODAGE_MAX) {
        c->ecran.a_faire = false;
        c->ecran.etat = EtatTelechargement::Echec;
    } else {
        std::memcpy(c->ecran.adresse, adresse, n);
        c->ecran.adresse[n] = '\0';
        c->ecran.a_faire = true;
        c->ecran.etat = EtatTelechargement::EnCours;
        if (!lancer_tache(c)) { // pas de tâche possible : l'écran ne reste pas sur « en cours »
            c->ecran.a_faire = false;
            c->ecran.etat = EtatTelechargement::Echec;
        }
    }
    xSemaphoreGive(c->verrou);
}

EtatTelechargement geocodage_ecran_etat() {
    Contexte *c = contexte();
    if (c == nullptr) {
        return EtatTelechargement::Echec;
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    const EtatTelechargement e = c->ecran.etat;
    xSemaphoreGive(c->verrou);
    return e;
}

std::string_view geocodage_ecran_corps() {
    Contexte *c = contexte();
    if (c == nullptr) {
        return {};
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    const std::string_view v = c->ecran.etat == EtatTelechargement::Termine
                                   ? std::string_view(c->ecran.corps, c->ecran.taille)
                                   : std::string_view();
    xSemaphoreGive(c->verrou);
    return v;
}

void geocodage_ecran_liberer() {
    Contexte *c = contexte();
    if (c == nullptr) {
        return;
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    ++c->ecran.generation; // une réponse encore en route sera jetée
    c->ecran.a_faire = false;
    c->ecran.etat = EtatTelechargement::Inactif;
    c->ecran.taille = 0;
    xSemaphoreGive(c->verrou);
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
